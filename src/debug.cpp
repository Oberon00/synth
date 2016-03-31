#include "debug.hpp"
#include "CgStr.hpp"
#include <iostream>

using namespace synth;

std::ostream& synth::operator<< (std::ostream& out, CXSourceRange rng)
{
    CXSourceLocation beg = clang_getRangeStart(rng);
    CXFile f;
    unsigned lineno, col;
    clang_getFileLocation(beg, &f, &lineno, &col, nullptr);
    out << CgStr(clang_getFileName(f)).gets() << ':' << lineno << ':' << col;

    CXSourceLocation end = clang_getRangeEnd(rng);
    clang_getFileLocation(end, nullptr, &lineno, &col, nullptr);
    return out << '-' << lineno << ':' << col;
}

std::ostream& synth::operator<< (std::ostream& out, CXCursor c)
{
    out << CgStr(clang_getCursorKindSpelling(clang_getCursorKind(c))).gets()
        << ' ' << clang_getCursorExtent(c);
    CgStr dn(clang_getCursorDisplayName(c));
    if (!dn.empty())
        out << " D:" << dn.gets();
    CgStr sp(clang_getCursorSpelling(c));
    if (!sp.empty())
        out << " S:" << dn.gets();
    return out;
}

void synth::writeIndent(int ind)
{
    for (int i = 0; i < ind; ++i)
        std::clog.put(' ');
}

void synth::dumpSingleCursor(CXCursor c, int ind)
{
    writeIndent(ind);
    std::clog << c;
    CXCursor refd = clang_getCursorReferenced(c);
    if (!clang_Cursor_isNull(refd)) {
        if (clang_equalCursors(c, refd)) {
            std::clog << " (self-ref)";
        } else {
            std::clog << '\n';
            writeIndent(ind + 4);
            std::clog << "-> " << refd;
        }
    }
    std::clog << '\n';
}

static CXChildVisitResult dumpAst(CXCursor c, CXCursor, CXClientData ud)
{
    int ind = ud ? *static_cast<int*>(ud) : 0;
    dumpSingleCursor(c, ind);
    ind += 2;
    clang_visitChildren(c, &dumpAst, &ind);
    return CXChildVisit_Continue;
}

void synth::dumpAst(CXCursor c, int ind)
{
    dumpSingleCursor(c, ind);
    ind += 2;
    clang_visitChildren(c, &::dumpAst, &ind);
}
