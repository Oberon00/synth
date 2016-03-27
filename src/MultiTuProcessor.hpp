#ifndef SYNTH_MULTI_TU_PROCESSOR_HPP_INCLUDED
#define SYNTH_MULTI_TU_PROCESSOR_HPP_INCLUDED

#include <string>
#include <vector>
#include <unordered_set>
#include <cassert>
#include <clang-c/Index.h>
#include <unordered_map>
#include <boost/optional.hpp>
#include <boost/filesystem/path.hpp>
#include "HighlightedFile.hpp"
#include "SymbolDeclaration.hpp"
#include "FileIdSet.hpp"

namespace synth {

namespace fs = boost::filesystem;

struct MissingDef {
    fs::path srcPath;
    std::size_t hlFileIdx;
    std::size_t markupIdx;
};

class MultiTuProcessor {
public:
    static MultiTuProcessor forRootdir(fs::path const& rootdir_)
    {
        return MultiTuProcessor(rootdir_);
    }

    bool underRootdir(fs::path const& p) const;

    std::pair<HighlightedFile*, unsigned> prepareToProcess(CXFile f);

    std::vector<HighlightedFile> const& outputs() const
    {
        return m_outputs;
    }

    void registerDef(SymbolDeclaration&& def)
    {
        assert(def.isdef);
        m_defs.insert({def.usr, std::move(def)});
    }

    void registerMissingDefLink(
        std::size_t hlFileIdx,
        std::size_t markupIdx,
        fs::path&& src, std::string&& dstUsr)
    {
        m_missingDefs[dstUsr].push_back({std::move(src), hlFileIdx, markupIdx});
    }

    void resolveMissingRefs();

    void writeOutput(fs::path const& outpath);

private:
    explicit MultiTuProcessor(fs::path const& rootdir_);

    Markup& markupFromMissingDef(MissingDef const& def);

    fs::path m_rootdir;
    FileIdSet m_processedFiles;
    std::vector<HighlightedFile> m_outputs;
    std::unordered_map<std::string, SymbolDeclaration> m_defs;
    std::unordered_map<std::string, std::vector<MissingDef>> m_missingDefs;
};

} // namespace synth

#endif
