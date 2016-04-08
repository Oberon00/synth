#include "cmdline.hpp"

#include <cstring>
#include <stdexcept>
#include <thread>
#include <utility>

using namespace synth;

static char const* getOptVal(char const* const* opt)
{
    if (!opt[1])
        throw std::runtime_error("Missing value for " + std::string(opt[0]));
    return opt[1];
}

static void getOptVal(char const* const* opt, char const*& out)
{
    if (out)
        throw std::runtime_error("Duplicate option " + std::string(opt[0]));
    out = getOptVal(opt);
}

CmdLineArgs CmdLineArgs::parse(int argc, char const* const* argv)
{
    if (argc < 3)
        throw std::runtime_error("Too few arguments.");
    CmdLineArgs r = {};
    bool foundCmd = false;
    int i;
    for (i = 1; i < argc; ++i) {
        if (argv[i][0] != '-') {
            r.inOutDirs.push_back({argv[i], nullptr});
        } else if (!std::strcmp(argv[i], "-e")) {
            r.clangArgs.push_back(getOptVal(argv + i++));
        } else if (!std::strcmp(argv[i], "-t")) {
             getOptVal(argv + i++, r.templateFile);
        } else if (!std::strcmp(argv[i], "-j")) {
            if (r.nThreads != 0)
                throw std::runtime_error("Duplicate option -j.");
            char const* nStr = getOptVal(argv + i++);
            int n;
            try {
                n = std::stoi(nStr);
            } catch (std::exception const& e) {
                throw std::runtime_error(
                    std::string("Bad integer for -j: ") + e.what());
            }
            if (n < 0)
                throw std::runtime_error("Value for -j must not be negative");
            r.nThreads = static_cast<unsigned>(n);
        } else if (!std::strcmp(argv[i], "-o")) {
            if (r.inOutDirs.empty()) {
                throw std::runtime_error(
                    "-o without preceeding input directory");
            }
            getOptVal(argv + i++, r.inOutDirs.back().second);
        } else if (!std::strcmp(argv[i], "--cmd")) {
            // These come before any extra-args, thus use insert(begin(), ...).
            r.clangArgs.insert(r.clangArgs.begin(), argv + i + 1, argv + argc);
            r.nClangArgs = argc - i - 1;
            foundCmd = true;
            i = argc;
            break;
        } else if (!std::strcmp(argv[i], "--db")) {
            r.compilationDbDir = getOptVal(argv + i++);
            foundCmd = true;
            ++i;
            break;
        } else {
            throw std::runtime_error(std::string("Bad argument ") + argv[i]);
        }
    }

    if (!foundCmd)
        throw std::runtime_error("Missing command.");
    if (i != argc)
        throw std::runtime_error("Superfluous commandline arguments.");

    if (r.nThreads == 0)
        r.nThreads = std::max(std::thread::hardware_concurrency(), 1u);
    for (auto& dir : r.inOutDirs) {
        if (!dir.second)
            dir.second = ".";
    }

    return r;
}

