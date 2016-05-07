#ifndef SYNTH_HIGHLIGTHED_FILE_HPP_INCLUDED
#define SYNTH_HIGHLIGTHED_FILE_HPP_INCLUDED

#include <boost/filesystem/path.hpp>

#include <cstdint>
#include <iosfwd>
#include <string>
#include <unordered_map>
#include <vector>

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
    inline TokenAttributes operator op (                   \
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

struct HighlightedFile;

struct SymbolDeclaration {
    HighlightedFile const* file;
    unsigned lineno; // 0: Whole file referenced. Implies fileUniueName.empty().
    std::string fileUniqueName; // Can be empty.

    bool valid() const { return file != nullptr; }
};

inline bool operator== (
    SymbolDeclaration const& lhs, SymbolDeclaration const& rhs)
{
    return lhs.lineno == rhs.lineno
        && lhs.file == rhs.file
        && lhs.fileUniqueName == rhs.fileUniqueName;
}

class MultiTuProcessor;

// return.empty(): No reference.
using CodeRef = std::function<std::string(fs::path const&, MultiTuProcessor&)>;

struct Markup {
    unsigned beginOffset;
    unsigned endOffset;

    TokenAttributes attrs;

    std::string const* fileUniqueName;

    CodeRef refd;

    bool empty() const;
    bool isRef() const { return static_cast<bool>(refd); }
};

struct HighlightedFile {
    // Relative to inOutDir: first / fname: original, second / fname: dst.
    fs::path fname;
    std::pair<fs::path, fs::path> const* inOutDir;

    std::vector<Markup> markups;

    std::vector<std::pair<unsigned, unsigned>> disabledLines;

    fs::path dstPath() const;
    fs::path srcPath() const { return inOutDir->first / fname; }

    void supplementMarkups(std::vector<Markup> const& supplementary);
    void writeTo(
        std::ostream& out,
        MultiTuProcessor& multiTuProcessor,
        std::ifstream& selfIn) const;
};

 // Must be called before writeTo() or supplementMarkups()
void sortMarkups(std::vector<Markup>& markups);

inline std::string lineId(unsigned lineno)
{
    return std::to_string(lineno) + "L";
}

} // namespace synth

#endif
