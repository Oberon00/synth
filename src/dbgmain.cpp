#include "debug.hpp"
#include "cgWrappers.hpp"
#include "CgStr.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>

using namespace synth;

static char const kTokenKindChrs[] = "pkilc";
static char const* const kLinkageSpellings[] = {
    "Invalid",
    "NoLinkage",
    "Internal",
    "UniqueExternal",
    "External"
};
static char const* const kLangSpellings[] = {
    "Invalid",
    "C",
    "ObjC",
    "C++"
};

static void dumpToken(
    CXTranslationUnit tu, CXToken tok, CXFile f = nullptr)
{
    std::clog << kTokenKindChrs[clang_getTokenKind(tok)] << ' ';
    std::clog << '"' << CgStr(clang_getTokenSpelling(tu, tok)) << "\" ";
    CXSourceRange tokExt = clang_getTokenExtent(tu, tok);
    writeExtent(std::clog, tokExt, f);
}

// Returns decl
static std::pair<CXType, CXCursor> dumpType(CXType ty, CXCursor ref)
{

    if (ty.kind == CXType_Invalid) {
        return {
            clang_getCursorType(clang_getNullCursor()),
            clang_getNullCursor()
        };
    }
    std::clog << CgStr(clang_getTypeKindSpelling(ty.kind))
        << ' ' << CgStr(clang_getTypeSpelling(ty));
    CXCursor decl = clang_getTypeDeclaration(ty);
    if (!clang_equalCursors(ref, decl))
        std::clog << " @ " << decl;
    std::clog << '\n';
    return {ty, decl};
}

static void dumpAnnotatedToken(
    CXTranslationUnit tu,
    CXToken tok,
    CXCursor cur,
    bool extrainfo,
    CXFile f = nullptr)
{
    dumpToken(tu, tok, f);
    std::clog << '\t';
    dumpSingleCursor(cur, 0, f);
    if (extrainfo) {
        CgStr usr = clang_getCursorUSR(cur);
        if (!usr.empty())
            std::clog << "    U: " << usr << '\n';
        std::clog << "    T: ";
        auto tyWithDecl = dumpType(clang_getCursorType(cur), cur);
        CXType canonTy = clang_getCanonicalType(tyWithDecl.first);
        if (!clang_equalTypes(tyWithDecl.first, canonTy)) {
            std::clog << "    CT: ";
            dumpType(canonTy, tyWithDecl.second);
        }
        CXLinkageKind linkage = clang_getCursorLinkage(cur);
        if (linkage != CXLinkage_Invalid)
            std::clog << "    Lnk: " << kLinkageSpellings[linkage];
        CXLanguageKind lang = clang_getCursorLanguage(cur);
        if (lang != CXLanguage_Invalid)
            std::clog << "   Lng: " << kLangSpellings[lang];
        if (lang != CXLanguage_Invalid || linkage != CXLinkage_Invalid)
            std::clog << '\n';
        CXCursor canon = clang_getCanonicalCursor(cur);
        if (!clang_equalCursors(cur, canon))
            std::clog << "    Can: " << canon << '\n';
        CXCursor def = clang_getCursorDefinition(cur);
        if (!clang_equalCursors(cur, def))
            std::clog << "    Def: " << def << '\n';
    }
    CXCursor c2 = clang_getCursor(tu, clang_getTokenLocation(tu, tok));
    if (!clang_equalCursors(cur, c2))
        dumpSingleCursor(c2, 2, f);
}

static void dumpTokens(CXCursor root, bool annotate, bool extrainfo)
{
    // TODO: Duplicate from annotate.cpp processFile()

    CXTranslationUnit tu = clang_Cursor_getTranslationUnit(root);
    CXToken* tokens;
    unsigned numTokens;
    CXSourceRange rng = clang_getCursorExtent(root);
    clang_tokenize(tu, rng, &tokens, &numTokens);
    CXFile f;
    clang_getFileLocation(
        clang_getRangeStart(rng), &f, nullptr, nullptr, nullptr);

    CgTokensHandle tokCleanup(tokens, numTokens, tu);

    if (numTokens > 0) {
        if (annotate) {
            std::vector<CXCursor> tokCurs(numTokens);
            clang_annotateTokens(tu, tokens, numTokens, tokCurs.data());
            for (unsigned i = 0; i < numTokens; ++i) {
                CXCursor cur = tokCurs[i];
                dumpAnnotatedToken(tu, tokens[i], cur, extrainfo, f);
            }
        } else {
            for (unsigned i = 0; i < numTokens; ++i) {
                dumpToken(tu, tokens[i], f);
                std::clog << '\n';
            }
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <flags> <clang-cmd>\n";
        return EXIT_FAILURE;
    }

    CgIdxHandle hcidx(clang_createIndex(true, true));

    // TODO: Duplicated from annotate.cpp synth::processTu() {{{
    CXTranslationUnit tu = nullptr;
    unsigned options = 0;
    if (std::strchr(argv[1], 'p'))
        options |= CXTranslationUnit_DetailedPreprocessingRecord;

    CXErrorCode err = clang_parseTranslationUnit2FullArgv(
        hcidx.get(),
        /*source_filename:*/ nullptr, // Included in commandline.
        argv + 2,
        argc - 2,
        /*unsaved_files:*/ nullptr,
        /*num_unsaved_files:*/ 0,
        options,
        &tu);
    CgTuHandle htu(tu);
    if (err != CXError_Success) {
        std::cerr << "Failed parsing translation unit (code "
                  << static_cast<int>(err)
                  << ")\n";
        return err + 10;
    }
    // }}}

    if (std::strchr(argv[1], 'a'))
        dumpAst(clang_getTranslationUnitCursor(tu), 0);

    if (std::strchr(argv[1], 't')) {
        dumpTokens(
            clang_getTranslationUnitCursor(tu),
            std::strchr(argv[1], 'c') != nullptr,
            std::strchr(argv[1], 'x') != nullptr);
    }

    return 0;
}
