#ifndef SYNTH_HIGHLIGTHED_FILE_HPP_INCLUDED
#define SYNTH_HIGHLIGTHED_FILE_HPP_INCLUDED

#include <string>
#include <vector>
#include <unordered_map>
#include <boost/filesystem/path.hpp>
#include <iosfwd>
#include <cstdint>

namespace synth {

namespace fs = boost::filesystem;

using TokenAttributesUnderlying = std::uint16_t;

enum class TokenAttributes : TokenAttributesUnderlying {
    none,

    attr,
    cmmt,
    constant, // Enumerator, non-type template param.
    func,
    kw,
    kwDecl,
    lbl,
    lit,
    litChr,
    litKw, // true, false, nullptr
    litNum,
    litNumFlt,
    litNumIntBin,
    litNumIntDecLong,
    litNumIntHex,
    litNumIntOct,
    litStr,
    namesp,
    op,
    opWord,
    pre,
    preIncludeFile,
    punct,
    ty,
    tyBuiltin,
    varGlobal,
    varLocal,
    varNonstaticMember,
    varStaticMember,

    maskKind = 0x3ff, // 1023 kinds

    flagDecl = 1 << 15,
    flagDef = 1 << 14
};

#define SYNTH_DEF_TOKATTR_OP(op)                            \
    constexpr TokenAttributes operator op (                 \
        TokenAttributes lhs, TokenAttributes rhs)           \
    {                                                       \
        return static_cast<TokenAttributes>(                \
            static_cast<TokenAttributesUnderlying>(lhs)     \
            op static_cast<TokenAttributesUnderlying>(rhs)); \
    }

SYNTH_DEF_TOKATTR_OP(|)
SYNTH_DEF_TOKATTR_OP(&)
SYNTH_DEF_TOKATTR_OP(^)

#define SYNTH_DEF_TOKATTR_ASSIGN_OP(op)                       \
    constexpr TokenAttributes operator op (                   \
        TokenAttributes& lhs, TokenAttributes rhs)            \
    {                                                         \
        auto r = static_cast<TokenAttributesUnderlying>(lhs); \
        r op static_cast<TokenAttributesUnderlying>(rhs);     \
        return lhs = static_cast<TokenAttributes>(r);         \
    }

SYNTH_DEF_TOKATTR_ASSIGN_OP(|=)
SYNTH_DEF_TOKATTR_ASSIGN_OP(&=)
SYNTH_DEF_TOKATTR_ASSIGN_OP(^=)

constexpr TokenAttributes operator~ (TokenAttributes t)
{
    return static_cast<TokenAttributes>(
        ~static_cast<TokenAttributesUnderlying>(t));
}

struct SourceLocation {
    fs::path const* filename;
    unsigned lineno;

    bool valid() const { return filename != nullptr; }
};

struct Markup {
    unsigned beginOffset;
    unsigned endOffset;

    TokenAttributes attrs;

    SourceLocation refd;

    bool empty() const;
    bool isRef() const { return refd.valid(); }
};

struct HighlightedFile {
    fs::path const* originalPath;
    std::vector<Markup> markups;

    // May invalidate references and indexes into markups.
    void prepareOutput();

    void writeTo(std::ostream& out) const;
};

}

#endif
