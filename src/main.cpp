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
#include <atomic>
#include <boost/filesystem.hpp>

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

static void processCompileCommand(
    CXCompileCommand cmd,
    CXIndex cidx,
    std::vector<char const*> extraArgs,
    MultiTuProcessor& state,
    std::mutex& outputMut,
    float pct)
{
    CgStr file(clang_CompileCommand_getFilename(cmd));
    if (!file.empty() && !state.isFileIncluded(file.get()))
        return;

    {
        std::lock_guard<std::mutex> lock(outputMut);

        std::clog.flags(std::clog.flags() | std::ios::fixed);
        std::clog.precision(2);
        std::clog << '[' << std::setw(6) << pct << "%]: " << file << "...\n";
    }

    std::vector<CgStr> clArgsHandles = getClArgs(cmd);
    std::vector<char const*> clArgs;
    clArgs.reserve(clArgsHandles.size() + extraArgs.size());
    for (CgStr const& s : clArgsHandles)
        clArgs.push_back(s.get());
    clArgs.insert(clArgs.end(), extraArgs.begin(), extraArgs.end());
    //CgStr dir = clang_CompileCommand_getDirectory(cmd);
    //if (!dir.empty())
    //    fs::current_path(dir.get());
    processTu(cidx, state, clArgs.data(), static_cast<int>(clArgs.size()));
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

        //InitialPathResetter pathResetter;

        std::mutex outputMut;

        // It seems [1] that during creation of the first translation,
        // no others may be created or data races occur inside libclang.
        // [1]: Detected by clangs TSan.
        processCompileCommand(
            clang_CompileCommands_getCommand(cmds.get(), 0),
            hcidx.get(),
            args.clangArgs,
            state,
            outputMut,
            0);

        std::atomic_uint sharedCmdIdx(1u);
        std::vector<std::thread> threads;
        threads.reserve(args.nThreads - 1);
        std::clog << "Using " << args.nThreads << " threads.\n";
        auto worker = [&]() {
            for (;;) {
                unsigned cmdIdx = sharedCmdIdx++;
                if (cmdIdx >= nCmds)
                    return;

                processCompileCommand(
                    clang_CompileCommands_getCommand(cmds.get(), cmdIdx),
                    hcidx.get(),
                    args.clangArgs,
                    state,
                    outputMut,
                    static_cast<float>(cmdIdx) / nCmds * 100);
            }
        };
        for (unsigned i = 0; i < args.nThreads - 1; ++i)
            threads.emplace_back(worker);
        worker();
        for (auto& th : threads)
            th.join();
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
