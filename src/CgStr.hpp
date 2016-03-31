#ifndef SYNTH_LIBCLANG_HPP_INCLUDED
#define SYNTH_LIBCLANG_HPP_INCLUDED

#include "clang-c/CXString.h"
#include <cassert>
#include <ostream>
#include <string>

namespace synth {

class CgStr {
public:
    CgStr(CXString&& s)
        : m_data(s)
    { }

    CgStr(CgStr&& other)
        : m_data(std::move(other.m_data))
    {
        other.m_data.data = nullptr;
    }

    CgStr& operator=(CgStr&& other) {
        destroy();
        m_data = std::move(other.m_data);
        other.m_data.data = nullptr; // HACK Undocumented behavior.
        assert(!other.valid());
        return *this;
    }

    ~CgStr() {
        destroy();
    }

    char const* get() const { return clang_getCString(m_data); }

    char const* gets() const
    {
        auto s = get();
        return s ? s : "";
    }

    std::string copy() const
    {
        auto s = get();
        return s ? s : std::string();
    }

    bool valid() const { return m_data.data != nullptr; } // HACK
    bool empty() const
    {
        if (!valid())
            return true;
        auto s = get();
        return !s || !*s;
    }

private:
    void destroy() { if (valid()) clang_disposeString(m_data); }

    CXString m_data;
};

inline std::ostream& operator<< (std::ostream& out, CgStr const& s)
{
    return out << s.gets();
}

} // namespace synth

#endif
