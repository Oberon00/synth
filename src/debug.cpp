#include "debug.hpp"

#include "CgStr.hpp"

#include <cstring>
#include <iostream>

using namespace synth;

static char const kTokenKindChrs[] = "pkilc";

void synth::writeLoc(std::ostream& out, CXSourceLocation loc, CXFile f)
{
    CXFile locF;
    unsigned lineno, col, off;
    clang_getFileLocation(loc, &locF, &lineno, &col, &off);
    if (!f || !clang_File_isEqual(f, locF))
        out << CgStr(clang_getFileName(locF)) << ':';
    out << lineno << ':' << col << '@' << off;
}


static void writeExtentWithLoc(
    std::ostream& out, CXSourceRange rng, CXSourceLocation loc, CXFile f)
{
    CXSourceLocation beg = clang_getRangeStart(rng);
    CXFile begF;
    unsigned begLn, begOff;
    clang_getFileLocation(beg, &begF, &begLn, nullptr, &begOff);
    writeLoc(out, beg, f);

    CXFile locF;
    unsigned locLn, locOff;
    clang_getFileLocation(loc, &locF, &locLn, nullptr, &locOff);
    if (!clang_File_isEqual(begF, locF) || begOff != locOff) {
            out << '!';
        if (!clang_File_isEqual(begF, locF) || begLn != locLn)
            writeLoc(out, loc, begF);
        else
            out << locOff - begOff;
    }

    CXSourceLocation end = clang_getRangeEnd(rng);
    CXFile endF;
    unsigned endLn, endOff;
    clang_getFileLocation(end, &endF, &endLn, nullptr, &endOff);
    if (!clang_File_isEqual(locF, endF) || locLn != endLn) {
        out << '-';
        writeLoc(out, end, locF);
    } else {
        out << '+' << endOff - locOff;
    }
}

void synth::writeExtent(std::ostream& out, CXSourceRange rng, CXFile f)
{
    writeExtentWithLoc(out, rng, clang_getRangeStart(rng), f);
}

void synth::writeToken(std::ostream& out, CXToken tok, CXTranslationUnit tu, CXFile f)
{
    out << kTokenKindChrs[clang_getTokenKind(tok)] << ' ';
    out << '"' << CgStr(clang_getTokenSpelling(tu, tok)) << "\" ";
    CXSourceRange tokExt = clang_getTokenExtent(tu, tok);
    writeExtentWithLoc(std::clog, tokExt, clang_getTokenLocation(tu, tok), f);
}

std::ostream& synth::operator<< (std::ostream& out, CXSourceRange rng)
{
    writeExtent(out, rng);
    return out;
}

static void writeCursor(std::ostream& out, CXCursor c, CXFile f)
{
    CXCursorKind k = clang_getCursorKind(c);
    if (k == 0)
        out << "<bad kind> ";
    else
        out << CgStr(clang_getCursorKindSpelling(k)) << ' ';
    writeExtentWithLoc(
        out, clang_getCursorExtent(c), clang_getCursorLocation(c), f);
    CgStr dn(clang_getCursorDisplayName(c));
    if (!dn.empty())
        out << " D:" << dn;
    CgStr sp(clang_getCursorSpelling(c));
    if (!sp.empty()) {
        if (!dn.empty() && !std::strcmp(dn.get(), sp.get()))
            out << " S=D";
        else
            out << " S:" << sp;
    }
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
            std::clog << " (<-)";
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
