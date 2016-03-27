#include "cmdline.hpp"

#include <stdexcept>
#include <cstring>

using namespace synth;

static char const* getOptVal(char const* const* opt)
{
    if (!opt[1])
        throw std::runtime_error("Missing value for " + std::string(opt[0]));
    return opt[1];
}

CmdLineArgs CmdLineArgs::parse(int argc, char const* const* argv)
{
    if (argc < 3)
        throw std::runtime_error("Too few arguments.");
    CmdLineArgs r = {};
    r.rootdir = argv[1];
    bool foundCmd = false;
    int i;
    for (i = 2; i < argc; ++i) {
        if (!std::strcmp(argv[i], "-o")) {
            r.outdir = getOptVal(argv + i++);
        } else if (!std::strcmp(argv[i], "cmd")) {
            r.clangArgs.assign(argv + i + 1, argv + argc);
            r.nClangArgs = argc - i - 1;
            foundCmd = true;
            i = argc;
            break;
        } else if (!std::strcmp(argv[i], "db")) {
            r.compilationDbDir = getOptVal(argv + i++);
            foundCmd = true;
            break;
        }
    }
    if (!foundCmd)
        throw std::runtime_error("Missing command.");
    if (i != argc)
        throw std::runtime_error("Superfluous commandline arguments.");
    return r;
}

