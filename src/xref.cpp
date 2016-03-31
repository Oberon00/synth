#include "xref.hpp"
#include "MultiTuProcessor.hpp"
#include "output.hpp"

#include <boost/filesystem/path.hpp>

using namespace synth;

void synth::linkSymbol(Markup& m, SourceLocation const& sym)
{
    if (!sym.fname)
        return;
    m.refd = sym;
}

void synth::linkCursorIfIncludedDst(
    Markup& m, CXCursor dst, unsigned srcLineno, MultiTuProcessor& state)
{
    CXFile file;
    unsigned lineno;
    clang_getFileLocation(
        clang_getCursorLocation(dst), &file, &lineno, nullptr, nullptr);
    fs::path const* fname = state.internFilename(file);
    if (!fname || lineno == srcLineno)
        return;
    return linkSymbol(m, {fname, lineno});
}

bool synth::linkInclude(
    Markup& m,
    CXCursor incCursor,
    MultiTuProcessor& state)
{
    CXFile file = clang_getIncludedFile(incCursor);
    fs::path const* fname = state.internFilename(file);
    if (!fname)
        return false;
    m.refd.fname = fname;
    m.refd.lineno = 0;
    return true;
}
