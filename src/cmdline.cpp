#include "cmdline.hpp"

#include <climits>
#include <cstring>
#include <stdexcept>
#include <thread>

using namespace synth;

unsigned const kDefaultMaxIdSz = 128;

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

static unsigned getUintOptVal(char const* const* opt)
{
    char const* nStr = getOptVal(opt);
    int n;
    try {
        n = std::stoi(nStr);
    } catch (std::exception const& e) {
        throw std::runtime_error(
            std::string("Integer expected for ") + opt[0] + ": " + e.what());
    }
    if (n < 0) {
        throw std::runtime_error(
            std::string("Value for ") + opt[0] + " must not be negative.");
    }
    return static_cast<unsigned>(n);
}

CmdLineArgs CmdLineArgs::parse(int argc, char const* const* argv)
{
    if (argc < 3)
        throw std::runtime_error("Too few arguments.");
    CmdLineArgs r = {};
    r.maxIdSz = kDefaultMaxIdSz;
    bool maxIdSzFound = false;
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
            r.nThreads = getUintOptVal(argv + i++);
        } else if (!std::strcmp(argv[i], "--max-id-sz")) {
            if (maxIdSzFound)
                throw std::runtime_error("Duplicate option --max-id-sz.");
            r.maxIdSz = getUintOptVal(argv + i++);
        } else if (!std::strcmp(argv[i], "-o")) {
            if (r.inOutDirs.empty()) {
                throw std::runtime_error(
                    "-o without preceeding input directory");
            }
            getOptVal(argv + i++, r.inOutDirs.back().second);
        } else if (!std::strcmp(argv[i], "--doxytags")) {
            std::pair<char const*, char const*> tagOpts;
            getOptVal(argv + i++, tagOpts.first);
            getOptVal(argv + i++, tagOpts.second);
            r.doxyTagFiles.push_back(std::move(tagOpts));
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
        r.nThreads = std::thread::hardware_concurrency();
    for (auto& dir : r.inOutDirs) {
        if (!dir.second)
            dir.second = ".";
    }

    return r;
}
