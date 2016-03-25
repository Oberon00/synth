#ifndef SYNTH_MULTI_TU_PROCESSOR_HPP_INCLUDED
#define SYNTH_MULTI_TU_PROCESSOR_HPP_INCLUDED

#include <string>
#include <vector>
#include <unordered_set>
#include <clang-c/Index.h>
#include <unordered_map>
#include <boost/optional.hpp>
#include "HighlightedFile.hpp"
#include "SymbolDeclaration.hpp"
#include "FileIdSet.hpp"

namespace synth {

class MultiTuProcessor {
public:
    static MultiTuProcessor forRootdir(std::string const& rootdir_)
    {
        return MultiTuProcessor(rootdir_);
    }
    
    boost::optional<HighlightedFile> prepareToProcess(CXFile f);
    
    std::vector<HighlightedFile> const& outputs() const
    {
        return m_outputs;
    }
    
private:
    explicit MultiTuProcessor(std::string const& rootdir_)
        : m_rootdir(rootdir_)
    {}
    
    std::string const& m_rootdir;
    FileIdSet m_processedFiles;
    std::vector<HighlightedFile> m_outputs;
    std::unordered_map<std::string, SymbolDeclaration> m_decls;
};    

}

#endif