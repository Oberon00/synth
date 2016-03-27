#ifndef SYNTH_CMDLINE_HPP_INCLUDED
#define SYNTH_CMDLINE_HPP_INCLUDED

#include <string>
#include <vector>

namespace synth {

struct CmdLineArgs {
    char const* rootdir;
    char const* outdir;

    int nClangArgs;

    // Extra-arguments if in CompilationDb mode, otherwise the whole cmdline.
    std::vector<char const*> clangArgs;

    char const* compilationDbDir;

    static CmdLineArgs parse(int argc, char const* const* argv);
};

} // namespace synth

#endif // SYNTH_CMDLINE_HPP_INCLUDED
