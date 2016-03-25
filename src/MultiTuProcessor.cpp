#include "MultiTuProcessor.hpp"
#include <algorithm>
#include <boost/filesystem.hpp>
#include "CgStr.hpp"

// Adapted from http://stackoverflow.com/a/15549954/2128694, user Rob Kennedy
static bool isInDir(std::string const& dir, char const* p) {
    namespace fs = boost::filesystem;
    auto cdir = fs::absolute(dir);
    auto cp = fs::absolute(p);
    // If dir ends with "/" and isn't the root directory, then the final
    // component returned by iterators will include "." and will interfere
    // with the std::equal check below, so we strip it before proceeding.
    if (cdir.filename() == ".")
        cdir.remove_filename();
    // We're also not interested in the file's name.
    if(cp.has_filename())
        cp.remove_filename();

    // If dir has more components than file, then file can't possibly
    // reside in dir.
    auto dir_len = std::distance(cdir.begin(), cdir.end());
    auto file_len = std::distance(cp.begin(), cp.end());
    if (dir_len > file_len)
        return false;

    // This stops checking when it reaches dir.end(), so it's OK if file
    // has more directory components afterward. They won't be checked.
    return std::equal(cdir.begin(), cdir.end(), cp.begin());
}

boost::optional<synth::HighlightedFile> synth::MultiTuProcessor::prepareToProcess(CXFile f)
{
    CgStr fpath(clang_getFileName(f));
    if (!isInDir(m_rootdir, fpath.get()))
        return {};
    HighlightedFile r;
    r.originalPath = fpath;
    return r;
}