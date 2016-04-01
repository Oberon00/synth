#include "MultiTuProcessor.hpp"
#include "SimpleTemplate.hpp"
#include "annotate.hpp"
#include "cgWrappers.hpp"
#include "cmdline.hpp"
#include "CgStr.hpp"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <thread>
#include <condition_variable>
#include <boost/filesystem.hpp>
#include <boost/io/ios_state.hpp>

using namespace synth;

static char const kDefaultTemplateText[] =
R"EOT(<!DOCTYPE html>
<html>
    <head>
        <meta charset="utf-8">
        <meta name="viewport" content="width=device-width">
        <title>@@filename@@</title>
        <link rel="stylesheet" href="@@rootpath@@/code.css">
    </head>
    <body>
        <div class="highlight"><pre>@@code@@</pre></div>
    </body>
</html>)EOT";

namespace {

struct InitialPathResetter {
    ~InitialPathResetter() {
        boost::system::error_code ec;
        fs::current_path(fs::initial_path(), ec);
        if (ec) {
            std::cerr << "Failed resetting working directory: " << ec;
        }
    }
};

struct ThreadSharedState {
    CXIndex cidx;
    MultiTuProcessor& multiTuProcessor;
    std::mutex workingDirMut;
    std::mutex outputMut;
    std::condition_variable workingDirChangedOrFree;
    unsigned nWorkingDirUsers;
    std::atomic_bool cancel;
};

class UIntRef {
public:
    UIntRef(
        unsigned& refd, std::condition_variable& zeroSignal, std::mutex& mut)
        : m_refd(refd)
        , m_zeroSignal(zeroSignal)
        , m_mut(mut)
        , m_acquired(false)
    { }

    // Note: The referenced unsigned must be syncronized by the caller.
    void acquire()
    {
        assert(!m_acquired);
        ++m_refd;
        m_acquired = true;
    }

    ~UIntRef()
    {
        if (m_acquired) {
            bool zeroReached;
            {
                std::lock_guard<std::mutex> lock(m_mut);
                zeroReached = --m_refd == 0;
            }
            if (zeroReached)
                m_zeroSignal.notify_all();
        }
    }

private:
    unsigned& m_refd;
    std::condition_variable& m_zeroSignal;
    std::mutex& m_mut;
    bool m_acquired;
};

} // anonyomous namespace

static std::vector<CgStr> getClArgs(CXCompileCommand cmd)
{
    std::vector<CgStr> result;
    unsigned nArgs = clang_CompileCommand_getNumArgs(cmd);
    result.reserve(nArgs);
    for (unsigned i = 0; i < nArgs; ++i)
        result.push_back(clang_CompileCommand_getArg(cmd, i));
    return result;
}

static bool processCompileCommand(
    CXCompileCommand cmd,
    std::vector<char const*> extraArgs,
    float pct,
    ThreadSharedState& state)
{
    CgStr file(clang_CompileCommand_getFilename(cmd));
    if (!file.empty() && !state.multiTuProcessor.isFileIncluded(file.get()))
        return false;

    std::vector<CgStr> clArgsHandles = getClArgs(cmd);
    std::vector<char const*> clArgs;
    clArgs.reserve(clArgsHandles.size() + extraArgs.size());
    for (CgStr const& s : clArgsHandles)
        clArgs.push_back(s.get());
    clArgs.insert(clArgs.end(), extraArgs.begin(), extraArgs.end());
    CgStr dirStr = clang_CompileCommand_getDirectory(cmd);
    UIntRef dirRef(
        state.nWorkingDirUsers,
        state.workingDirChangedOrFree,
        state.workingDirMut);

    bool dirChanged = false;
    if (!dirStr.empty()) {
        fs::path dir = std::move(dirStr).gets();
        bool dirOk;
        std::unique_lock<std::mutex> lock(state.workingDirMut);
        state.workingDirChangedOrFree.wait(lock, [&]() {
            if (state.cancel)
                return true;
            dirOk = fs::current_path() == dir;
            return dirOk || state.nWorkingDirUsers == 0;
        });
        if (state.cancel)
            return false;
        dirRef.acquire();
        if (!dirOk) {
            fs::current_path(dir);
            dirChanged = true;
        }
    } else {
        std::lock_guard<std::mutex> lock(state.workingDirMut);
        dirRef.acquire();
    }

    {
        std::lock_guard<std::mutex> lock(state.outputMut);

        if (dirChanged)
            std::clog << "Entered directory " << dirStr.get() << '\n';
        boost::io::ios_all_saver saver(std::clog);
        std::clog.flags(std::clog.flags() | std::ios::fixed);
        std::clog.precision(2);
        std::clog << '[' << std::setw(6) << pct << "%]: " << file << "...\n";
    }


    return processTu(
        state.cidx,
        state.multiTuProcessor,
        clArgs.data(),
        static_cast<int>(clArgs.size())) == EXIT_SUCCESS;
}

// Adapted from
// http://insanecoding.blogspot.co.at/2011/11/how-to-read-in-file-in-c.html
static std::string getFileContents(char const* fname)
{
    std::ifstream in(fname, std::ios::in | std::ios::binary);
    in.exceptions(std::ios::badbit);
    std::ostringstream contents;
    contents << in.rdbuf();
    return std::move(contents).str();
}


static int executeCmdLine(CmdLineArgs const& args)
{
    SimpleTemplate tpl(std::string{});
    if (args.templateFile) {
        try {
            tpl = SimpleTemplate(getFileContents(args.templateFile));
        } catch (std::ios::failure const& e) {
            std::cerr << "Error reading output template: " << e.what() << '\n';
            return EXIT_FAILURE;
        }
    } else {
        tpl = SimpleTemplate(kDefaultTemplateText); 
    }

    CgIdxHandle hcidx(clang_createIndex(
            /*excludeDeclarationsFromPCH:*/ true,
            /*displayDiagnostics:*/ true));

    MultiTuProcessor state(
        PathMap(args.inOutDirs.begin(), args.inOutDirs.end()));

    if (args.compilationDbDir) {
        CXCompilationDatabase_Error err;
        CgDbHandle db(clang_CompilationDatabase_fromDirectory(
                args.compilationDbDir, &err));
        if (err != CXCompilationDatabase_NoError) {
            std::cerr << "Failed loading compilation database (code "
                      << static_cast<int>(err)
                      << ")\n";
            return err + 20;
        }
        CgCmdsHandle cmds(
            clang_CompilationDatabase_getAllCompileCommands(db.get()));
        unsigned nCmds = clang_CompileCommands_getSize(cmds.get());
        if (nCmds == 0) {
            std::cerr << "No compilation commands in DB.\n";
            return EXIT_SUCCESS;
        }

        InitialPathResetter pathResetter;
        ThreadSharedState tstate {
            /*cidx=*/ hcidx.get(),
            /*multiTuProcessor=*/ state,
            /*workindDirMut=*/ {},
            /*outputMut=*/ {},
            /*workingDirChangedOrFree=*/ {},
            /*nWorkingDirUsers=*/ 0u,
            /*cancel=*/ {false}};

        // It seems [1] that during creation of the first translation,
        // no others may be created or data races occur inside libclang.
        // [1]: Detected by clangs TSan.
        unsigned idx = 0;
        while (!processCompileCommand(
            clang_CompileCommands_getCommand(cmds.get(), idx++),
            args.clangArgs,
            0,
            tstate)
        ) {
            assert(tstate.nWorkingDirUsers == 0);
        }

        std::atomic_uint sharedCmdIdx(idx);
        std::vector<std::thread> threads;
        threads.reserve(args.nThreads - 1);
        std::clog << "Using " << args.nThreads << " threads.\n";
        auto const worker = [&]() {
            while (!tstate.cancel) {
                unsigned cmdIdx = sharedCmdIdx++;
                if (cmdIdx >= nCmds)
                    return;

                processCompileCommand(
                    clang_CompileCommands_getCommand(cmds.get(), cmdIdx),
                    args.clangArgs,
                    static_cast<float>(cmdIdx) / nCmds * 100,
                    tstate);
            }
        };
        try {
            for (unsigned i = 0; i < args.nThreads - 1; ++i)
                threads.emplace_back(worker);
            worker();
        } catch (...) {
            tstate.cancel = true; // Do before locking to reduce wait time.
            {
                std::lock_guard<std::mutex> lock(tstate.workingDirMut);
                tstate.cancel = true; // Repeat for condition variable.
            }
            tstate.workingDirChangedOrFree.notify_all();
            for (auto& th : threads)
                th.join();
            assert(tstate.nWorkingDirUsers == 0);
            throw;
        }
        for (auto& th : threads)
            th.join();
        assert(tstate.nWorkingDirUsers == 0);
    } else {
        int r = synth::processTu(
            hcidx.get(),
            state,
            args.clangArgs.data(),
            args.nClangArgs);
        if (r)
            return r;
    }
    state.resolveMissingRefs();
    state.writeOutput(tpl);
    return EXIT_SUCCESS;
}

int main(int argc, char* argv[])
{
    try {
        fs::initial_path(); // Save initial path.
        return executeCmdLine(CmdLineArgs::parse(argc, argv));
    } catch (std::exception const& e) {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
