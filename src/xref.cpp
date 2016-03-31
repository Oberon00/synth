#include "xref.hpp"
#include "MultiTuProcessor.hpp"
#include "HighlightedFile.hpp"

using namespace synth;

void synth::linkSymbol(Markup& m, SourceLocation const& sym)
{
    if (!sym.filename)
        return;
    m.refd.lineno = sym.lineno;
    m.refd.filename = sym.filename;
}

void synth::linkCursorIfIncludedDst(
    Markup& m, CXCursor dst, unsigned srcLineno, MultiTuProcessor& state)
{
    CXFile file;
    unsigned lineno;
    clang_getFileLocation(
        clang_getCursorLocation(dst), &file, &lineno, nullptr, nullptr);
    std::string const* filename = state.internFilename(file);
    if (!filename || lineno == srcLineno)
        return;
    return linkSymbol(m, {filename, lineno});
}

bool synth::linkInclude(
    Markup& m,
    CXCursor incCursor,
    MultiTuProcessor& state)
{
    CXFile file = clang_getIncludedFile(incCursor);
    std::string const* filename = state.internFilename(file);
    if (!filename)
        return false;
    m.refd.filename = filename;
    m.refd.lineno = 0;
    return true;
}
