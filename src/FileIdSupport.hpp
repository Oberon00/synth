#ifndef FILE_ID_SUPPORT_HPP_INCLUDED
#define FILE_ID_SUPPORT_HPP_INCLUDED

#include <clang-c/Index.h>
#include <algorithm>

namespace std {
    template <>
    struct hash<CXFileUniqueID> {
        std::size_t operator() (CXFileUniqueID const& uid) const {
            int nelems = sizeof(uid.data) / sizeof(uid.data[0]);
            std::size_t h = std::hash<unsigned long long>()(uid.data[0]);
            for (int i = 1; i < nelems; ++i)
                hash_combine(h, uid.data[i]);
            return h;
        }
    private:
        // From boost.
        template <class T>
        static void hash_combine(std::size_t& seed, const T& v)
        {
            std::hash<T> hasher;
            seed ^= hasher(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
        }
    };

    template <>
    struct equal_to<CXFileUniqueID> {
        bool operator() (CXFileUniqueID const& lhs, CXFileUniqueID const& rhs) const {
            return std::equal(
                std::begin(lhs.data),
                std::end(lhs.data),
                std::begin(rhs.data));
        }
    };
}

#endif
