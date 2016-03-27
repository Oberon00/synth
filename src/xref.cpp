#include "xref.hpp"
#include "SymbolDeclaration.hpp"
#include "MultiTuProcessor.hpp"
#include "HighlightedFile.hpp"
#include "CgStr.hpp"
#include <boost/filesystem/operations.hpp>
#include <iostream>

using namespace synth;

static std::string relativeUrl(
    fs::path const& from, fs::path const& to)
{
    if (to == from)
        return std::string();
    fs::path r = fs::relative(to, from.parent_path());
    return r == "." ? std::string() : r.string() + ".html";
}

static std::string makeSymbolHref(
    SymbolDeclaration sym,
    fs::path const& srcurl)
{
    std::string url = relativeUrl(srcurl, sym.filename);
    url += '#';

    if (sym.isdef) {
        if (!sym.usr.empty()) {
            url += sym.usr;
            return url;
        }
        // else: fall back through to line-number linking.
    }

    url += "L";
    url += std::to_string(sym.lineno);
    return url;
}

void synth::linkSymbol(
    Markup& m,
    SymbolDeclaration const& sym,
    fs::path const& srcurl)
{
    std::string dsturl = makeSymbolHref(sym, srcurl);
    if (dsturl.empty())
        return;
    m.tag = Markup::kTagLink;
    m.attrs["href"] = dsturl;
}

void synth::linkCursorIfIncludedDst(
    Markup& m,
    CXCursor dst,
    fs::path const& srcurl,
    unsigned srcLineno,
    MultiTuProcessor& state,
    bool byUsr)
{
    CXFile file;
    unsigned lineno;
    clang_getFileLocation(
        clang_getCursorLocation(dst), &file, &lineno, nullptr, nullptr);
    std::string filename = CgStr(clang_getFileName(file)).gets();
    if ((filename == srcurl && srcLineno == lineno))
        return;
    if (!state.underRootdir(filename))
        return;
    return linkSymbol(
        m,
        {CgStr(clang_getCursorUSR(dst)).get(), filename, lineno, byUsr},
        srcurl);
}
