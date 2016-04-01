#include "xref.hpp"
#include "MultiTuProcessor.hpp"
#include "output.hpp"

#include <boost/filesystem/path.hpp>

using namespace synth;

void synth::linkSymbol(Markup& m, SourceLocation const& sym)
{
    if (sym.valid())
        m.refd = sym;
}

void synth::linkCursorIfIncludedDst(
    Markup& m, CXCursor dst, MultiTuProcessor& state)
{
    CXFile file;
    unsigned lineno;
    clang_getFileLocation(
        clang_getCursorLocation(dst), &file, &lineno, nullptr, nullptr);
    HighlightedFile const* hlFile = state.referenceFilename(file);
    if (!hlFile)
        return;
    return linkSymbol(m, {hlFile, lineno});
}

bool synth::linkInclude(
    Markup& m,
    CXCursor incCursor,
    MultiTuProcessor& state)
{
    CXFile file = clang_getIncludedFile(incCursor);
    HighlightedFile const* hlFile = state.referenceFilename(file);
    if (!hlFile)
        return false;
    m.refd.file = hlFile;
    m.refd.lineno = 0;
    return true;
}
