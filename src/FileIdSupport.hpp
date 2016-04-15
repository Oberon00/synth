#ifndef FILE_ID_SUPPORT_HPP_INCLUDED
#define FILE_ID_SUPPORT_HPP_INCLUDED

#include <clang-c/Index.h>
#include <algorithm>
#include <functional>
#include <boost/functional/hash/hash.hpp>

namespace std {
    template <>
    struct hash<CXFileUniqueID> {
        std::size_t operator() (CXFileUniqueID const& uid) const {
            int constexpr nelems = sizeof(uid.data) / sizeof(uid.data[0]);
            std::hash<unsigned long long> hasher;
            std::size_t h = hasher(uid.data[0]);
            for (int i = 1; i < nelems; ++i)
                boost::hash_combine(h, hasher(uid.data[i]));
            return h;
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
