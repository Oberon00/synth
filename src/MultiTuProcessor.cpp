#include "MultiTuProcessor.hpp"
#include <algorithm>
#include <boost/filesystem.hpp>
#include "CgStr.hpp"
#include <climits>
#include <fstream>
#include <iostream>
#include "xref.hpp"

namespace fs = boost::filesystem;

// Adapted from http://stackoverflow.com/a/15549954/2128694, user Rob Kennedy
// dir must be an absolute path without filename component.
static bool isInDir(fs::path const& dir, fs::path p)
{
    p = fs::absolute(std::move(p)).lexically_normal();

    bool r = std::mismatch(dir.begin(), dir.end(), p.begin(), p.end()).first
        == dir.end();

    return r;
}


synth::MultiTuProcessor::MultiTuProcessor(fs::path const& rootdir)
    : m_rootdir(fs::absolute(rootdir).lexically_normal())
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
    if (!f)
        return {nullptr, UINT_MAX};
    CgStr fpath(clang_getFileName(f));
    if (fpath.empty() || !isInDir(m_rootdir, fpath.get()))
        return {nullptr, UINT_MAX};
    CXFileUniqueID fuid;
    if (clang_getFileUniqueID(f, &fuid) != 0)
        return {nullptr, UINT_MAX};
    if (!m_processedFiles.insert(fuid).second)
        return {nullptr, UINT_MAX};
    m_outputs.emplace_back();
    HighlightedFile* r = &m_outputs.back();
    r->originalPath = fpath.get();
    return {r, m_outputs.size() - 1};
}

void synth::MultiTuProcessor::resolveMissingRefs()
{
    for (auto it = m_missingDefs.begin(); it != m_missingDefs.end(); ) {
        auto def = m_defs.find(it->first);
        if (def != m_defs.end()) { // Definition was resolved:
            for (auto const& ref : it->second) {
                Markup& m = markupFromMissingDef(ref);
                linkSymbol(m, def->second, ref.srcPath);
            }
            it = m_missingDefs.erase(it);
        } else {
            ++it;
        }

    }
}

synth::Markup& synth::MultiTuProcessor::markupFromMissingDef(
    synth::MissingDef const& def)
{
    assert(def.hlFileIdx < m_outputs.size());
    HighlightedFile& hlFile = m_outputs[def.hlFileIdx];
    assert(def.markupIdx < hlFile.markups.size());
    return hlFile.markups[def.markupIdx];
}

void synth::MultiTuProcessor::writeOutput(fs::path const& outpath)
{
    m_missingDefs.clear(); // Will be invalidated by the below operations.
    for (auto& hlfile : m_outputs) {
        hlfile.prepareOutput();
        auto hlpath = outpath / fs::relative(hlfile.originalPath, m_rootdir);
        hlpath += ".html";
        fs::create_directories(hlpath.parent_path());
        std::ofstream outfile(hlpath.c_str());
        outfile.exceptions(std::ios::badbit | std::ios::failbit);
        hlfile.writeTo(outfile);
        std::cout << hlpath << " written\n";
    }
}

