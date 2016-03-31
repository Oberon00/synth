#ifndef SYNTH_MULTI_TU_PROCESSOR_HPP_INCLUDED
#define SYNTH_MULTI_TU_PROCESSOR_HPP_INCLUDED

#include <string>
#include <vector>
#include <unordered_set>
#include <cassert>
#include <clang-c/Index.h>
#include <unordered_map>
#include <unordered_set>
#include <boost/optional.hpp>
#include <boost/filesystem/path.hpp>
#include "HighlightedFile.hpp"
#include "SymbolDeclaration.hpp"
#include "FileIdSupport.hpp"

namespace synth {

class SimpleTemplate;

namespace fs = boost::filesystem;

struct MissingDef {
    std::size_t hlFileIdx;
    std::size_t markupIdx;
};

struct FileEntry {
    std::string fileName;
    bool processed;
};

class MultiTuProcessor {
public:
    static MultiTuProcessor forRootdir(fs::path const& rootdir_)
    {
        return MultiTuProcessor(rootdir_);
    }

    // Returns nullptr if f is not under the rootdir.
    std::string const* internFilename(CXFile f);

    std::pair<HighlightedFile*, unsigned> prepareToProcess(CXFile f);

    std::vector<HighlightedFile> const& outputs() const
    {
        return m_outputs;
    }

    void registerDef(SymbolDeclaration&& def)
    {
        m_defs.insert({def.usr, std::move(def)});
    }

    void registerMissingDefLink(
        std::size_t hlFileIdx,
        std::size_t markupIdx,
        std::string&& dstUsr)
    {
        m_missingDefs[dstUsr].push_back({hlFileIdx, markupIdx});
    }

    void resolveMissingRefs();

    void writeOutput(fs::path const& outpath, SimpleTemplate const& tpl);

private:
    using FileEntryMap = std::unordered_map<CXFileUniqueID, FileEntry>;

    explicit MultiTuProcessor(fs::path const& rootdir_);

    Markup& markupFromMissingDef(MissingDef const& def);

    // Returns nullptr if the file is not under the rootdir.
    FileEntry* getFileEntry(CXFile f);


    FileEntryMap m_processedFiles;
    fs::path m_rootdir;
    std::vector<HighlightedFile> m_outputs;
    std::unordered_map<std::string, SymbolDeclaration> m_defs;
    std::unordered_map<std::string, std::vector<MissingDef>> m_missingDefs;
};

} // namespace synth

#endif
