#include "xref.hpp"

#include "CgStr.hpp"
#include "MultiTuProcessor.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "output.hpp"
#include "highlight.hpp"

#include <boost/filesystem/path.hpp>

#include <iostream>
#include <regex>

using namespace synth;

// A cursor is the main cursor for a declaration in its file iff:
//  - It is a definition; or
//  - It is the cannonical cursor (as defined by libclang) and the definition
//    is not in the same file (≠ translation unit).
static bool isMainCursor(CXCursor cur)
{
    CXCursor def = clang_getCursorDefinition(cur);
    if (clang_equalCursors(cur, def))
        return true;

    CXCursor canon = clang_getCanonicalCursor(cur);
    if (!clang_equalCursors(cur, canon))
        return false;
    CXFile defF;
    clang_getFileLocation(
        clang_getCursorLocation(def), &defF, nullptr, nullptr, nullptr);
    CXFile curF;
    clang_getFileLocation(
        clang_getCursorLocation(canon), &curF, nullptr, nullptr, nullptr);
    return !clang_File_isEqual(defF, curF);
}

static std::pair<bool, CXCursor> typeAliasRedeclares(CXCursor decl)
{
    CXType aliasTy = clang_getCursorType(decl);
    CXType canonTy = clang_getCanonicalType(aliasTy);
    CXCursor canonDecl = clang_getTypeDeclaration(canonTy);
    std::string aliasQName = simpleQualifiedName(decl);
    std::string canonQName = simpleQualifiedName(canonDecl);
    
    return {aliasQName == canonQName, canonDecl};
}

// Returns clang_getCursorReferenced(cur), unless the referenced cursor is a
// type alias or typedef used in an occurence of the "typedef struct S { } S;"
// pattern. In this case, it returns the declaration cursor of the struct
// instead of the type alias.
static CXCursor effectiveReferencedCursor(CXCursor cur)
{
    CXCursor refd = clang_getCursorReferenced(cur);
    CXCursorKind k = clang_getCursorKind(refd);
    if (k != CXCursor_TypedefDecl && k != CXCursor_TypeAliasDecl)
        return refd;
    CXCursor tyRefd = clang_getCursorReferenced(refd);
    if (!clang_equalCursors(refd, tyRefd))
        return refd;
    
    auto sameAndCanon = typeAliasRedeclares(refd);
    return sameAndCanon.first ? sameAndCanon.second : refd;
}


static std::string relativeUrl(fs::path const& from, fs::path const& to)
{
    if (to == from)
        return std::string();
    fs::path r = to.lexically_relative(from.parent_path());
    return r == "." ? std::string() : r.string();
}

static std::string locationUrl(
    fs::path const& outPath, SymbolDeclaration const& dst)
{
    std::string r = relativeUrl(outPath, dst.file->dstPath());
    if (!dst.fileUniqueName.empty()) {
        r.reserve(r.size() + 1 + dst.fileUniqueName.size());
        r += '#';
        r += dst.fileUniqueName;
    } else if (dst.lineno != 0) {
        std::string anchor = lineId(dst.lineno);
        r.reserve(r.size() + 1 + anchor.size());
        r += "#";
        r += std::move(anchor);
    }
    return r;
}

static void linkSymbol(Markup& m, SymbolDeclaration const* sym)
{
    if (!sym)
        return;
    m.refd = [sym](fs::path const& outPath, MultiTuProcessor&) {
        return locationUrl(outPath, *sym);
    };
}

static void linkDeclCursor(Markup& m, CXCursor decl, MultiTuProcessor& state, bool isC)
{
    CXFile file;
    unsigned lineno, offset;
    clang_getFileLocation(
        clang_getCursorLocation(decl), &file, &lineno, nullptr, &offset);
    linkSymbol(m, state.referenceSymbol(
        file, lineno, offset, [&]() { return fileUniqueName(decl, isC); }));
}

static void linkInclude(Markup& m, CXCursor incCursor, MultiTuProcessor& state)
{
    CXFile file = clang_getIncludedFile(incCursor);
    linkSymbol(m, state.referenceSymbol(
        file, 0, UINT_MAX, []() { return std::string(); }));
}

static void linkExternalDef(Markup& m, CXCursor cur, MultiTuProcessor& state)
{
    CgStr hUsr(clang_getCursorUSR(cur));
    if (hUsr.empty())
        return;
    state.linkExternalRef(m, cur);
    m.refd = [usr = hUsr.copy(), extRef = std::move(m.refd)] (
        fs::path const& outPath, MultiTuProcessor& state_)
    {
        SymbolDeclaration const* sym = state_.findMissingDef(usr);
        if (sym)
            return locationUrl(outPath, *sym);
        if (extRef)
            return extRef(outPath, state_);
        return std::string();
    };
}

void synth::linkCursor(Markup& m, CXCursor cur, MultiTuProcessor& state, bool isC)
{
    CXCursorKind k = clang_getCursorKind(cur);
    bool shouldRef = false;

    if (k == CXCursor_InclusionDirective) {
        linkInclude(m, cur, state);
        shouldRef = true;
    } else {
        CXCursor referenced = effectiveReferencedCursor(cur);
        bool isref = !clang_Cursor_isNull(referenced)
            && !clang_equalCursors(cur, referenced);
        shouldRef = isref;
        if (isref) {
            linkDeclCursor(m, referenced, state, isC);
        } else if (
            (m.attrs & (TokenAttributes::flagDef | TokenAttributes::flagDecl))
            != TokenAttributes::none
        ) {
            if ((m.attrs & TokenAttributes::flagDef) == TokenAttributes::none)
                linkExternalDef(m, cur, state);
            shouldRef = true;
        }
    }

    if (shouldRef && !m.isRef())
        state.linkExternalRef(m, std::move(cur));
}

std::string synth::fileUniqueName(CXCursor cur, bool isC)
{
    if (!isNamespaceLevelDeclaration(cur))
        return std::string();
    if (!isMainCursor(cur))
        return std::string();
    CXCursorKind k = clang_getCursorKind(cur);
    if (k == CXCursor_VarDecl || k == CXCursor_EnumConstantDecl)
        return simpleQualifiedName(cur);
    if (isTypeCursorKind(k)) {
        if (!isC) {
            if ((k == CXCursor_TypeAliasDecl || k == CXCursor_TypedefDecl)
                && typeAliasRedeclares(cur).first
            ) {
                return std::string();
            }
            return simpleQualifiedName(cur);
        }

        std::string qname = simpleQualifiedName(cur);
        if (qname.empty())
            return std::string();

        // C allows to have struct S and another non-struct symbol S at the
        // same time. They are distinguished by writing struct in front e.g.
        // "void S(struct S arg) { /* ... */ }"
        // A common pattern is thus "typedef struct S { } S;" We need to make
        // sure that we handle that correctly.
        // Same goes for enum and union.
        std::string prefix;
        SYNTH_DISCLANGWARN_BEGIN("-Wswitch-enum")
        switch (k) {
            case CXCursor_StructDecl:
                prefix = 's';
                break;
            case CXCursor_EnumDecl:
                prefix = 'e';
                break;
            case CXCursor_UnionDecl:
                prefix = 'u';
                break;
            case CXCursor_TypedefDecl:
                break;
            default:
                assert("unreachable" && false);
        }
        SYNTH_DISCLANGWARN_END
        if (!prefix.empty())
            prefix += ':';
        qname.insert(0, std::move(prefix));
        return qname;
    }
    if (isFunctionCursorKind(k)) {
        if (isC)
            return CgStr(clang_getCursorSpelling(cur)).gets();

        std::string r = simpleQualifiedName(cur);
        CXType ty = clang_getCursorType(cur);
        int nargs = clang_getNumArgTypes(ty);
        assert(nargs >= 0);
        if (nargs > 0 || clang_isFunctionTypeVariadic(ty)) {
            r += ':';
            for (int i = 0; i < nargs; ++i) {
                if (i != 0)
                    r += ',';
                CXType argTy = clang_getArgType(ty, static_cast<unsigned>(i));
                r += CgStr(clang_getTypeSpelling(argTy)).gets();
            }
            // Leading space, trailing space and space adjancent to a non-word
            // character is discardable.
            SYNTH_DISCLANGWARN_BEGIN("-Wexit-time-destructors")
            static const std::regex discardableSpace(
                R"((?:^ )|(?: $)|(\W) | (\W))");
            SYNTH_DISCLANGWARN_END
            r = std::regex_replace(std::move(r), discardableSpace, "$1$2");
            std::replace(r.begin(), r.end(), ' ', '-');
            if (clang_isFunctionTypeVariadic(ty)) {
                if (nargs > 0)
                    r += ',';
                r += "...";
            }
        }
        return r;
    }
    return std::string();
}

std::string synth::simpleQualifiedName(CXCursor cur)
{
    CXCursorKind k = clang_getCursorKind(cur);
    if (clang_isInvalid(k) || clang_isTranslationUnit(k))
        return std::string();
    CXCursor parent = clang_getCursorSemanticParent(cur);
    CgStr sp(clang_getCursorSpelling(cur));
    if (sp.empty())
        return simpleQualifiedName(parent);
    std::string name = simpleQualifiedName(parent);
    return name.empty() ? sp.get() : std::move(name) + "::" + sp.get();
}

bool synth::isNamespaceLevelDeclaration(CXCursor cur)
{
    CXLinkageKind linkage = clang_getCursorLinkage(cur);
    if (linkage == CXLinkage_Invalid)
        return false;
    if (linkage != CXLinkage_NoLinkage)
        return true;

    CXCursorKind k = clang_getCursorKind(cur);
    if (!isTypeAliasCursorKind(k))
        return false;

    // We need to walk up the parents to check if they are inside a
    // function.
    do {
        cur = clang_getCursorSemanticParent(cur);
        k = clang_getCursorKind(cur);
        if (isFunctionCursorKind(k))
            return false;
    } while (!clang_isInvalid(k) && !clang_isTranslationUnit(k));
    return true;
}

