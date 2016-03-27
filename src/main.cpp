#include "MultiTuProcessor.hpp"
#include "annotate.hpp"
#include "cgWrappers.hpp"
#include "cmdline.hpp"
#include "CgStr.hpp"

#include <iostream>
#include <boost/filesystem.hpp>

using namespace synth;

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

static int executeCmdLine(CmdLineArgs const& args)
{
    CgIdxHandle hcidx(clang_createIndex(
            /*excludeDeclarationsFromPCH:*/ true,
            /*displayDiagnostics:*/ true));
    auto state(MultiTuProcessor::forRootdir(std::move(args.rootdir)));

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

        for (unsigned i = 0; i < nCmds; ++i) {
            CXCompileCommand cmd = clang_CompileCommands_getCommand(
                cmds.get(), i);
            std::vector<CgStr> clArgsHandles = getClArgs(cmd);
            std::vector<char const*> clArgs;
            clArgs.reserve(clArgsHandles.size() + args.clangArgs.size());
            for (CgStr const& s : clArgsHandles)
                clArgs.push_back(s.get());
            clArgs.insert(
                clArgs.end(), args.clangArgs.begin(), args.clangArgs.end());
            CgStr dir = clang_CompileCommand_getDirectory(cmd);
            if (!dir.empty())
                fs::current_path(dir.get());
            processTu(
                hcidx.get(),
                state,
                clArgs.data(),
                static_cast<int>(clArgs.size()));
        }
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
    state.writeOutput(args.outdir);
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
