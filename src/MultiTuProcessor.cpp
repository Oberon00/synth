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

using namespace synth;

static fs::path normalAbsolute(fs::path const& p)
{
    fs::path r = fs::absolute(p).lexically_normal();
    if (r.filename() == ".")
        r.remove_filename();
    return r;
}

// Idea from http://stackoverflow.com/a/15549954/2128694, user Rob Kennedy
static bool isPathSuffix(fs::path const& dir, fs::path const& p)
{
    return std::mismatch(dir.begin(), dir.end(), p.begin(), p.end()).first
        == dir.end();
}

static fs::path commonPrefix(fs::path const& p1, fs::path const& p2)
{
    auto it1 = p1.begin(), it2 = p2.begin();
    fs::path r;
    while (it1 != p1.end() && it2 != p2.end()) {
        if (*it1 != *it2)
            break;
        r /= *it1;
        ++it1;
        ++it2;
    }
    return r;
}

MultiTuProcessor::MultiTuProcessor(PathMap const& dirs)
{
    if (dirs.empty())
        return;
    m_rootInDir = dirs.begin()->first;
    for (auto const& kv : dirs) {
        fs::path inDir = normalAbsolute(kv.first);
        m_dirs.push_back({inDir, kv.second});
        m_rootInDir = commonPrefix(std::move(m_rootInDir), inDir);
    }
}

bool MultiTuProcessor::isFileIncluded(fs::path const& p) const
{
    return getFileMapping(p) != nullptr;
}

PathMap::value_type const* MultiTuProcessor::getFileMapping(
    fs::path const& p) const
{
    fs::path cleanP = normalAbsolute(p);
    if (!isPathSuffix(m_rootInDir, cleanP))
        return nullptr;

    for (auto const& kv : m_dirs) {
        if (isPathSuffix(kv.first, cleanP))
            return &kv;
    }
    return nullptr;
}

HighlightedFile* MultiTuProcessor::prepareToProcess(CXFile f)
{
    FileEntry* fentry = obtainFileEntry(f);
    if (!fentry || fentry->processed.test_and_set())
        return nullptr;
    return &fentry->hlFile;
}

HighlightedFile const* MultiTuProcessor::referenceFilename(CXFile f)
{
    FileEntry* fentry = obtainFileEntry(f);
    return fentry ? &fentry->hlFile : nullptr;
}


FileEntry* MultiTuProcessor::obtainFileEntry(CXFile f)
{
    CXFileUniqueID fuid;
    if (!f || clang_getFileUniqueID(f, &fuid) != 0)
        return nullptr;

    std::lock_guard<std::mutex> lock(m_mut);
    auto it = m_processedFiles.find(fuid);
    if (it != m_processedFiles.end())
        return &it->second;
    fs::path fname(CgStr(clang_getFileName(f)).gets());
    if (fname.empty())
        return nullptr;
    auto mapping = getFileMapping(fname);
    if (!mapping)
        return nullptr;
    fname = fs::relative(std::move(fname), mapping->first);
    FileEntry& e = m_processedFiles.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(std::move(fuid)),
            std::forward_as_tuple())
        .first->second;
    e.hlFile.fname = std::move(fname);
    e.hlFile.inOutDir = mapping;
    return &e;
}

void MultiTuProcessor::resolveMissingRefs()
{
    for (auto it = m_missingDefs.begin(); it != m_missingDefs.end(); ) {
        auto def = m_defs.find(it->first);
        if (def != m_defs.end()) { // Definition was resolved:
            for (auto const& ref : it->second) {
                Markup& m = markupFromMissingDef(ref);
                linkSymbol(m, def->second);
            }
            it = m_missingDefs.erase(it);
        } else {
            ++it;
        }
    }
}

Markup& MultiTuProcessor::markupFromMissingDef(MissingDef const& def)
{
    assert(def.markupIdx < def.hlFile->markups.size());
    return def.hlFile->markups[def.markupIdx];
}

void MultiTuProcessor::writeOutput(SimpleTemplate const& tpl)
{
    if (m_dirs.empty())
        return;
    auto it = m_dirs.begin();
    fs::path rootOutDir = it->second;
    for (++it; it != m_dirs.end(); ++it)
        rootOutDir = commonPrefix(rootOutDir, it->second);
    bool commonRoot = isPathSuffix(
        normalAbsolute(fs::current_path()), normalAbsolute(rootOutDir));
    SimpleTemplate::Context ctx;
    m_missingDefs.clear(); // Will be invalidated by the below operations.
    std::clog << "Writing " << m_processedFiles.size() << " HTML files...\n";
    for (auto& fentry : m_processedFiles) {
        auto& hlFile = fentry.second.hlFile;
        hlFile.prepareOutput();
        auto dstPath = hlFile.dstPath();
        auto hldir = dstPath.parent_path();
        if (hldir != ".")
            fs::create_directories(hldir);
        std::ofstream outfile(dstPath.c_str());
        outfile.exceptions(std::ios::badbit | std::ios::failbit);
        ctx["code"] = SimpleTemplate::ValCallback(std::bind(
            &HighlightedFile::writeTo, &hlFile, std::placeholders::_1));
        ctx["filename"] = hlFile.fname.string();
        ctx["rootpath"] = fs::relative(
                commonRoot ? rootOutDir : hlFile.inOutDir->second, hldir)
            .lexically_normal().string();
        tpl.writeTo(outfile, ctx);
    }
}

