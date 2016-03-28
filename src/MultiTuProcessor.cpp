#include "MultiTuProcessor.hpp"
#include "SimpleTemplate.hpp"
#include <algorithm>
#include <boost/filesystem.hpp>
#include <boost/variant/variant.hpp>
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

    static std::pair<synth::HighlightedFile*, unsigned> const null = {
        nullptr, UINT_MAX};
    if (!f)
        return null;
    CXFileUniqueID fuid;
    if (clang_getFileUniqueID(f, &fuid) != 0)
        return null;
    if (m_processedFiles.find(fuid) != m_processedFiles.end())
        return null;
    CgStr fpath(clang_getFileName(f));
    if (fpath.empty() || !isInDir(m_rootdir, fpath.get()))
        return null;
    m_processedFiles.insert(fuid);
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

void synth::MultiTuProcessor::writeOutput(
    fs::path const& outpath, SimpleTemplate const& tpl)
{
    m_missingDefs.clear(); // Will be invalidated by the below operations.
    SimpleTemplate::Context ctx;
    for (auto& hlfile : m_outputs) {
        hlfile.prepareOutput();
        auto relpath = fs::relative(hlfile.originalPath, m_rootdir);
        auto hlpath = outpath / relpath ;
        hlpath += ".html";
        auto hldir = hlpath.parent_path();
        if (hldir != ".")
            fs::create_directories(hldir);
        std::ofstream outfile(hlpath.c_str());
        outfile.exceptions(std::ios::badbit | std::ios::failbit);
        ctx["code"] = SimpleTemplate::ValCallback(
            std::bind(&HighlightedFile::writeTo, &hlfile, std::placeholders::_1));
        ctx["filename"] = relpath.string();
        ctx["rootpath"] = fs::relative(outpath, hlpath.parent_path())
            .lexically_normal().string();
        tpl.writeTo(outfile, ctx);
        std::cout << hlpath << " written\n";
    }
}

