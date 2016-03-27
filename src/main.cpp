#include "MultiTuProcessor.hpp"
#include "annotate.hpp"
#include "cgWrappers.hpp"
#include "cmdline.hpp"

#include <iostream>

using namespace synth;

static int executeCmdLine(CmdLineArgs const& args)
{
    CgIdxHandle hcidx(clang_createIndex(
            /*excludeDeclarationsFromPCH:*/ true,
            /*displayDiagnostics:*/ true));
    auto state(MultiTuProcessor::forRootdir(std::move(args.rootdir)));
    int r = synth::processTu(
        hcidx.get(), state, args.clangArgs.data(), args.nClangArgs);
    if (r)
        return r;
    state.resolveMissingRefs();
    state.writeOutput(args.outdir);
    return r;
}

int main(int argc, char* argv[])
{
    try {
        return executeCmdLine(CmdLineArgs::parse(argc, argv));
    } catch (std::exception const& e) {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
