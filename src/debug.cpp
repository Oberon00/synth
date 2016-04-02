#include "debug.hpp"

#include "CgStr.hpp"
#include <iostream>

using namespace synth;

void synth::writeLoc(std::ostream& out, CXSourceLocation loc, CXFile f)
{
    CXFile locF;
    unsigned lineno, col;
    clang_getFileLocation(loc, &locF, &lineno, &col, nullptr);
    if (!f || !clang_File_isEqual(f, locF))
        out << CgStr(clang_getFileName(locF)) << ':';
    out << lineno << ':' << col;
}

void synth::writeExtent(std::ostream& out, CXSourceRange rng, CXFile f)
{
    CXSourceLocation beg = clang_getRangeStart(rng);
    CXFile begF;
    unsigned begLn;
    clang_getFileLocation(beg, &begF, &begLn, nullptr, nullptr);
    writeLoc(out, beg, f);

    CXSourceLocation end = clang_getRangeEnd(rng);
    out << '-';
    CXFile endF;
    unsigned endLn, endCol;
    clang_getFileLocation(end, &endF, &endLn, &endCol, nullptr);
    if (!clang_File_isEqual(begF, endF) || begLn != endLn)
        writeLoc(out, end, begF);
    else
        out << endCol;
}

std::ostream& synth::operator<< (std::ostream& out, CXSourceRange rng)
{
    writeExtent(out, rng);
    return out;
}

static void writeCursor(std::ostream& out, CXCursor c, CXFile f)
{
    out << CgStr(clang_getCursorKindSpelling(clang_getCursorKind(c))) << ' ';
    writeExtent(out, clang_getCursorExtent(c), f);
    CgStr dn(clang_getCursorDisplayName(c));
    if (!dn.empty())
        out << " D:" << dn;
    CgStr sp(clang_getCursorSpelling(c));
    if (!sp.empty())
        out << " S:" << sp;
}

std::ostream& synth::operator<< (std::ostream& out, CXCursor c)
{
    writeCursor(out, c, nullptr);
    return out;
}

void synth::writeIndent(int ind)
{
    for (int i = 0; i < ind; ++i)
        std::clog.put(' ');
}

void synth::dumpSingleCursor(CXCursor c, int ind, CXFile f)
{
    writeIndent(ind);
    writeCursor(std::clog, c, f);
    CXCursor refd = clang_getCursorReferenced(c);
    if (!clang_Cursor_isNull(refd)) {
        if (clang_equalCursors(c, refd)) {
            std::clog << " (self-ref)";
        } else {
            std::clog << '\n';
            writeIndent(ind + 4);
            std::clog << "-> ";
            writeCursor(std::clog, refd, f);
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
