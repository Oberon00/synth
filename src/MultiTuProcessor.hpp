#ifndef SYNTH_MULTI_TU_PROCESSOR_HPP_INCLUDED
#define SYNTH_MULTI_TU_PROCESSOR_HPP_INCLUDED

#include "FileIdSupport.hpp"
#include "output.hpp"

#include <boost/filesystem/path.hpp>
#include <clang-c/Index.h>

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace synth {

class SimpleTemplate;

namespace fs = boost::filesystem;

struct FileEntry {
    std::atomic_flag processed;
    HighlightedFile hlFile;
};

using PathMap = std::vector<std::pair<fs::path, fs::path>>;

// Trys to link m to an external URL that represents what refcur references.
// The callee must be thread safe.
using ExternalRefLinker = std::function<void(Markup& m, CXCursor mcur)>;

class MultiTuProcessor {
public:
    explicit MultiTuProcessor(
        PathMap const& rootdir_, ExternalRefLinker&& refLinker);

    bool isFileIncluded(fs::path const& p) const;

    // Returns nullptr if f should be ignored.
    HighlightedFile const* referenceFilename(CXFile f);

    HighlightedFile* prepareToProcess(CXFile f);

    void registerDef(std::string&& usr, SourceLocation&& def)
    {
        std::lock_guard<std::mutex> lock(m_mut);
        m_defs.insert({std::move(usr), std::move(def)});
    }

    // Not threadsafe!
    void writeOutput(SimpleTemplate const& tpl);

    // Not threadsafe!
    SourceLocation const* findMissingDef(std::string const& usr)
    {
        auto it = m_defs.find(usr);
        return it == m_defs.end() ? nullptr : &it->second;
    }

    void linkExternalRef(Markup& m, CXCursor mcur)
    {
        m_refLinker(m, mcur);
    }

private:
    using FileEntryMap = std::unordered_map<CXFileUniqueID, FileEntry>;

    // Returns nullptr if f should be ignored.
    FileEntry* obtainFileEntry(CXFile f);

    PathMap::value_type const* getFileMapping(fs::path const& p) const;


    FileEntryMap m_processedFiles;
    PathMap m_dirs;

    // Maps from USRs to source location
    std::unordered_map<std::string, SourceLocation> m_defs;

    // Common prefix of all keys in m_dirs
    fs::path m_rootInDir;

    ExternalRefLinker m_refLinker;

    std::mutex m_mut;
};

} // namespace synth

#endif
