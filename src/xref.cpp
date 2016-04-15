#include "xref.hpp"

#include "CgStr.hpp"
#include "MultiTuProcessor.hpp"
#include "output.hpp"

#include <boost/filesystem/path.hpp>

using namespace synth;

static std::string makeEmptyString()
{
    return std::string();
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
    linkSymbol(
        m, state.referenceSymbol(file, offset, lineno, &makeEmptyString));
}

static void linkInclude(Markup& m, CXCursor incCursor, MultiTuProcessor& state)
{
    CXFile file = clang_getIncludedFile(incCursor);
    linkSymbol(m, state.referenceSymbol(file, UINT_MAX, 0, &makeEmptyString));
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
