#include "MultiTuProcessor.hpp"
#include <algorithm>
#include <boost/filesystem.hpp>
#include "CgStr.hpp"
#include <climits>

namespace fs = boost::filesystem;

// Adapted from http://stackoverflow.com/a/15549954/2128694, user Rob Kennedy
// dir must be an absolute path without filename component.
static bool isInDir(fs::path const& dir, fs::path p)
{
    p = fs::absolute(std::move(p));

    return std::mismatch(dir.begin(), dir.end(), p.begin(), p.end()).first
        == dir.end();
}


synth::MultiTuProcessor::MultiTuProcessor(fs::path const& rootdir)
    : m_rootdir(fs::absolute(rootdir))
{
    if (m_rootdir.filename() == ".")
        m_rootdir.remove_filename();
}

bool synth::MultiTuProcessor::underRootdir(fs::path const& p) const
{
    return isInDir(m_rootdir, p);
}

std::pair<synth::HighlightedFile*, unsigned>
synth::MultiTuProcessor::prepareToProcess(CXFile f)
{
    CgStr fpath(clang_getFileName(f));
    if (!isInDir(m_rootdir, fpath.get()))
        return {nullptr, UINT_MAX};
    m_outputs.emplace_back();
    HighlightedFile* r = &m_outputs.back();
    r->originalPath = fpath;
    return {r, m_outputs.size() - 1};
}

std::string synth::MultiTuProcessor::relativeUrl(
        fs::path const& from, fs::path const& to) const
{
    fs::path r = fs::relative(to, from);
    return r == "." ? std::string() : r.string();
}
