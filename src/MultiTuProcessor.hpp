#ifndef SYNTH_MULTI_TU_PROCESSOR_HPP_INCLUDED
#define SYNTH_MULTI_TU_PROCESSOR_HPP_INCLUDED

#include "fileidsupport.hpp"
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

struct MissingDef {
    HighlightedFile* hlFile;
    std::size_t markupIdx;
};

struct FileEntry {
    std::atomic_flag processed;
    HighlightedFile hlFile;
};

using PathMap = std::vector<std::pair<fs::path, fs::path>>;

class MultiTuProcessor {
public:
    explicit MultiTuProcessor(PathMap const& rootdir_);

    bool isFileIncluded(fs::path const& p) const;

    // Returns nullptr if f should be ignored.
    HighlightedFile const* referenceFilename(CXFile f);

    HighlightedFile* prepareToProcess(CXFile f);

    void registerDef(std::string&& usr, SourceLocation&& def)
    {
        std::lock_guard<std::mutex> lock(m_mut);
        m_defs.insert({usr, std::move(def)});
    }

    void registerMissingDefLink(
        HighlightedFile& file,
        std::size_t markupIdx,
        std::string&& dstUsr)
    {
        std::lock_guard<std::mutex> lock(m_mut);
        m_missingDefs[dstUsr].push_back({&file, markupIdx});
    }

    void resolveMissingRefs();

    void writeOutput(SimpleTemplate const& tpl);

private:
    using FileEntryMap = std::unordered_map<CXFileUniqueID, FileEntry>;

    Markup& markupFromMissingDef(MissingDef const& def);

    // Returns nullptr if f should be ignored.
    FileEntry* obtainFileEntry(CXFile f);

    PathMap::value_type const* getFileMapping(fs::path const& p) const;


    FileEntryMap m_processedFiles;
    PathMap m_dirs;

    // Maps from USRs to source location
    std::unordered_map<std::string, SourceLocation> m_defs;

    // Maps from USRs to missing definitions
    std::unordered_map<std::string, std::vector<MissingDef>> m_missingDefs;

    // Common prefix of all keys in m_dirs
    fs::path m_rootInDir;

    std::mutex m_mut;
};

} // namespace synth

#endif
