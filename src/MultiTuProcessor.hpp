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
#include <unordered_set>
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
    struct SymbolId {
        HighlightedFile const* file;
        unsigned offset; // UINT_MAX: Whole file.

        bool operator== (SymbolId const& other) const {
            return offset == other.offset && file == other.file;
        }
    };

    struct SymbolIdHasher {
        std::size_t operator() (SymbolId const& sym) const {
            std::size_t h = std::hash<decltype(sym.file)>()(sym.file);
            boost::hash_combine(h,
                std::hash<decltype(sym.offset)>()(sym.offset));
            return h;
        }
    };

    using SymbolMap = std::unordered_map<SymbolId, SymbolDeclaration, SymbolIdHasher>;
public:
    explicit MultiTuProcessor(
        PathMap const& rootdir_, ExternalRefLinker&& refLinker);

    // Setter is not threadsafe!
    void setMaxIdSz(std::size_t maxIdSz) noexcept { m_maxIdSz = maxIdSz; }
    std::size_t maxIdSz() const noexcept { return m_maxIdSz; }

    bool isFileIncluded(fs::path const& p) const;

    // Returns nullptr if references to f should be ignored.
    // Pass 0 for lineno and UINT_MAX for offset if referencing
    // the file as a whole.
    SymbolDeclaration const* referenceSymbol(
        CXFile f, unsigned lineno, unsigned offset);

    SymbolDeclaration& createSymbol(
        HighlightedFile const& hlFile, unsigned lineno, unsigned offset);

    HighlightedFile* prepareToProcess(CXFile f);

    void registerDef(std::string&& usr, SymbolDeclaration const* def);

    // Not threadsafe!
    void writeOutput(SimpleTemplate const& tpl);

    // Not threadsafe!
    SymbolDeclaration const* findMissingDef(std::string const& usr)
    {
        auto it = m_defs.find(usr);
        return it == m_defs.end() ? nullptr : it->second;
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

    // Maps from USRs to symbol declarations (referencing m_syms)
    std::unordered_map<std::string, SymbolDeclaration const*> m_defs;
    SymbolMap m_syms;

    // Common prefix of all keys in m_dirs
    fs::path m_rootInDir;

    ExternalRefLinker m_refLinker;

    std::size_t m_maxIdSz; // Maximum length for fileUniqueNames in m_syms.


    std::mutex m_mut;
};

} // namespace synth

#endif
