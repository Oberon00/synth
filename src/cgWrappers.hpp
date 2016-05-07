#ifndef SYNTH_CGWRAPPERS_HPP_INCLUDED
#define SYNTH_CGWRAPPERS_HPP_INCLUDED

#include <clang-c/CXCompilationDatabase.h>
#include <clang-c/Index.h>

#include <boost/noncopyable.hpp>

#include <memory>
#include <type_traits> // remove_pointer_t
#include <vector>

namespace synth {

class CgTokensHandle {
public:
    CgTokensHandle(CXToken* data, unsigned ntokens, CXTranslationUnit tu_)
        : m_data(data), m_ntokens(ntokens), m_tu(tu_)
    {}

    ~CgTokensHandle() { destroy(); }

    CgTokensHandle(CgTokensHandle&& other)
        : m_data(other.m_data)
        , m_ntokens(other.m_ntokens)
        , m_tu(other.m_tu)
    {
        other.release();
    }

    CgTokensHandle& operator= (CgTokensHandle&& other)
    {
        destroy();
        m_data = other.m_data;
        m_ntokens = other.m_ntokens;
        m_tu = other.m_tu;
        other.release();
        return *this;
    }

    CXTranslationUnit tu() const { return m_tu; }
    unsigned size() const { return m_ntokens; }
    CXToken* tokens() { return m_data; }

private:
    void destroy()
    {
        if (m_data)
            clang_disposeTokens(m_tu, m_data, m_ntokens);
    }

    void release()
    {
        m_ntokens = 0;
        m_data = nullptr;
    }

    CXToken* m_data;
    unsigned m_ntokens;
    CXTranslationUnit m_tu;
};

#define SYNTH_DEF_DELETER(n, t, f)               \
    struct DeleterFor##n {                    \
        void operator() (t v) const { f(v); } \
    };

#define SYNTH_DEF_HANDLE(n, t, delf) \
    SYNTH_DEF_DELETER(n, t, delf)       \
    using n = std::unique_ptr<       \
        std::remove_pointer_t<t>, DeleterFor##n>;

SYNTH_DEF_HANDLE(CgIdxHandle, CXIndex, clang_disposeIndex)
SYNTH_DEF_HANDLE(CgTuHandle, CXTranslationUnit, clang_disposeTranslationUnit)
SYNTH_DEF_HANDLE(
    CgDbHandle, CXCompilationDatabase, clang_CompilationDatabase_dispose)
SYNTH_DEF_HANDLE(
    CgCmdsHandle, CXCompileCommands, clang_CompileCommands_dispose)
SYNTH_DEF_HANDLE(CgSourceRangesHandle, CXSourceRangeList*, clang_disposeSourceRangeList)
} // namespace synth

#endif // SYNTH_CGWRAPPERS_HPP_INCLUDED
