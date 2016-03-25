#ifndef SYNTH_LIBCLANG_HPP_INCLUDED
#define SYNTH_LIBCLANG_HPP_INCLUDED

#include "clang-c/CXString.h"
#include <string>
#include <cassert>

// Bla
class CgStr {
public:
    CgStr(CXString&& s)
        : m_data(s)
    { }
    
    
    CgStr(CgStr&& other)
    {
        *this = std::move(other);
    }
    
    CgStr& operator=(CgStr&& other) {
        m_data = std::move(other.m_data);
        other.m_data.data = nullptr; // HACK Undocumented behavior.
        assert(!other.valid());
    }
    
    ~CgStr() {
        if (valid())
            clang_disposeString(m_data);
    }
    
    char const* get() const { return clang_getCString(m_data); }
    
    operator char const* () const { return get(); }
    
    std::string copy() const
    {
        auto s = get();
        return s ? s : std::string();
    }
    
    bool valid() const { return m_data.data != nullptr; } // HACK
    
private:
    CXString m_data;
};

#endif