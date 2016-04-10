#include "xref.hpp"

#include "CgStr.hpp"
#include "MultiTuProcessor.hpp"
#include "output.hpp"

#include <boost/filesystem/path.hpp>

using namespace synth;

static std::string relativeUrl(fs::path const& from, fs::path const& to)
{
    if (to == from)
        return std::string();
    fs::path r = to.lexically_relative(from.parent_path());
    return r == "." ? std::string() : r.string();
}

static void writeDstUrl(
    std::ostream& out, fs::path const& outPath, SourceLocation const& dst)
{
    out << relativeUrl(outPath, dst.file->dstPath());
    if (dst.lineno != 0)
        out << "#L" << dst.lineno;
}

static bool linkCursorIfIncludedDst(
    Markup& m, CXCursor dst, MultiTuProcessor& state)
{
    CXFile file;
    unsigned lineno;
    clang_getFileLocation(
        clang_getCursorLocation(dst), &file, &lineno, nullptr, nullptr);
    HighlightedFile const* hlFile = state.referenceFilename(file);
    if (!hlFile)
        return false;
    m.refd = [hlFile, lineno](
        std::ostream& out, fs::path const& outPath, MultiTuProcessor&)
    {
        writeDstUrl(out, outPath, { hlFile, lineno });
    };
    return true;
}

static bool linkInclude(
    Markup& m,
    CXCursor incCursor,
    MultiTuProcessor& state)
{
    CXFile file = clang_getIncludedFile(incCursor);
    HighlightedFile const* hlFile = state.referenceFilename(file);
    if (!hlFile)
        return false;
    m.refd = [hlFile](
        std::ostream& out, fs::path const& outPath, MultiTuProcessor&)
    {
        writeDstUrl(out, outPath, { hlFile, 0 });
    };
    return true;
}

static void linkExternalDef(Markup& m, CXCursor cur, MultiTuProcessor&)
{
    CgStr hUsr(clang_getCursorUSR(cur));
    if (hUsr.empty())
        return;
    m.refd = [usr = hUsr.copy()](
        std::ostream& out, fs::path const& outPath, MultiTuProcessor& state)
    {
        SourceLocation const* loc = state.findMissingDef(usr);
        if (loc)
            writeDstUrl(out, outPath, *loc);
    };
}

void synth::linkCursor(Markup& m, CXCursor cur, MultiTuProcessor& state)
{
    CXCursorKind k = clang_getCursorKind(cur);

    if (k == CXCursor_InclusionDirective) {
        linkInclude(m, cur, state);
    } else {
        // clang_isReference() sometimes reports false negatives, e.g. for
        // overloaded operators, so check manually.
        CXCursor referenced = clang_getCursorReferenced(cur);
        bool isref = !clang_Cursor_isNull(referenced)
            && !clang_equalCursors(cur, referenced);
        if (isref) {
            linkCursorIfIncludedDst(m, referenced, state);
        } else if (
            (m.attrs & (TokenAttributes::flagDef | TokenAttributes::flagDecl))
                == TokenAttributes::flagDecl
        ) {
            linkExternalDef(m, cur, state);
        }
    }

}
