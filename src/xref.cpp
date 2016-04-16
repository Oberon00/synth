#include "xref.hpp"

#include "CgStr.hpp"
#include "MultiTuProcessor.hpp"
#include "config.hpp"
#include "output.hpp"
#include "highlight.hpp"

#include <boost/filesystem/path.hpp>

#include <regex>

using namespace synth;

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
        r += "#L";
        r += std::to_string(dst.lineno);
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

static void linkDeclCursor(Markup& m, CXCursor decl, MultiTuProcessor& state)
{
    CXFile file;
    unsigned lineno, offset;
    clang_getFileLocation(
        clang_getCursorLocation(decl), &file, &lineno, nullptr, &offset);
    linkSymbol(m, state.referenceSymbol(
        file, lineno, offset, [&decl]() { return fileUniqueName(decl); }));
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

void synth::linkCursor(Markup& m, CXCursor cur, MultiTuProcessor& state)
{
    CXCursorKind k = clang_getCursorKind(cur);
    bool shouldRef = false;

    if (k == CXCursor_InclusionDirective) {
        linkInclude(m, cur, state);
        shouldRef = true;
    } else {
        // clang_isReference() sometimes reports false negatives, e.g. for
        // overloaded operators, so check manually.
        CXCursor referenced = clang_getCursorReferenced(cur);
        bool isref = !clang_Cursor_isNull(referenced)
            && !clang_equalCursors(cur, referenced);
        shouldRef = isref;
        if (isref) {
            linkDeclCursor(m, referenced, state);
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

std::string synth::fileUniqueName(CXCursor cur)
{
    if (!isNamespaceLevelDeclaration(cur))
        return std::string();
    if (!isMainCursor(cur))
        return std::string();
    CXCursorKind k = clang_getCursorKind(cur);
    if (isTypeCursorKind(k)
        || k == CXCursor_VarDecl
        || k == CXCursor_EnumConstantDecl
    ) {
        return simpleQualifiedName(cur);
    }
    if (isFunctionCursorKind(k)) {
        // Calling clang_Cursor_getTranslationUnit directly on cur erratically
        // returns CXLanguage_C sometimes (e.g. for static functions), thus
        // call it on the TU cursor.
        CXLanguageKind tuLang = clang_getCursorLanguage(
            clang_getTranslationUnitCursor(
                clang_Cursor_getTranslationUnit(cur)));
        if (tuLang == CXLanguage_C) {
            return CgStr(clang_getCursorSpelling(cur)).gets();
        }

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
    if (k != CXCursor_TypeAliasDecl
        && k != CXCursor_TypeAliasTemplateDecl
        && k != CXCursor_TypedefDecl
    ) {
        return false;
    }
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

bool synth::isMainCursor(CXCursor cur)
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
