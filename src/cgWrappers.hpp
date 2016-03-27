#ifndef SYNTH_CGWRAPPERS_HPP_INCLUDED
#define SYNTH_CGWRAPPERS_HPP_INCLUDED

#include <clang-c/Index.h>
#include <boost/noncopyable.hpp>
#include <memory>
#include <vector>

namespace synth {

struct DeleterForCXIndex {
    void operator() (CXIndex cidx) const { clang_disposeIndex(cidx); }
};

struct DeleterForCXTranslationUnit {
    void operator() (CXTranslationUnit tu) const
    {
        clang_disposeTranslationUnit(tu);
    }
};

class CgTokensCleanup : private boost::noncopyable {
public:
    CgTokensCleanup(CXToken* data, unsigned ntokens, CXTranslationUnit tu_)
        : m_data(data), m_ntokens(ntokens), m_tu(tu_)
    {}

    ~CgTokensCleanup() {
        clang_disposeTokens(m_tu, m_data, m_ntokens);
    }

private:
    CXToken* m_data;
    unsigned m_ntokens;
    CXTranslationUnit m_tu;
};

using CgIdxHandle = std::unique_ptr<
    std::remove_pointer_t<CXIndex>, DeleterForCXIndex>;
using CgTuHandle = std::unique_ptr<
    std::remove_pointer_t<CXTranslationUnit>, DeleterForCXTranslationUnit>;

} // namespace synth

#endif // SYNTH_CGWRAPPERS_HPP_INCLUDED
