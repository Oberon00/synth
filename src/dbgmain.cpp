#include "debug.hpp"
#include "cgWrappers.hpp"
#include "CgStr.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>

using namespace synth;

static char const kTokenKindChrs[] = "pkilc";

static void dumpToken(
    CXTranslationUnit tu, CXToken tok, CXFile f = nullptr)
{
    std::clog << kTokenKindChrs[clang_getTokenKind(tok)] << ' ';
    std::clog << '"' << CgStr(clang_getTokenSpelling(tu, tok)) << "\" ";
    CXSourceRange tokExt = clang_getTokenExtent(tu, tok);
    writeExtent(std::clog, tokExt, f);
}

static void dumpAnnotatedToken(
    CXTranslationUnit tu, CXToken tok, CXCursor cur, CXFile f = nullptr)
{
    dumpToken(tu, tok, f);
    std::clog << '\t';
    dumpSingleCursor(cur, 0, f);
    CXCursor c2 = clang_getCursor(tu, clang_getTokenLocation(tu, tok));
    if (!clang_equalCursors(cur, c2))
        dumpSingleCursor(c2, 2, f);
}

static void dumpTokens(CXCursor root, bool annotate)
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

    CgTokensCleanup tokCleanup(tokens, numTokens, tu);

    if (numTokens > 0) {
        if (annotate) {
            std::vector<CXCursor> tokCurs(numTokens);
            clang_annotateTokens(tu, tokens, numTokens, tokCurs.data());
            for (unsigned i = 0; i < numTokens; ++i) {
                CXCursor cur = tokCurs[i];
                dumpAnnotatedToken(tu, tokens[i], cur, f);
            }
        } else {
            for (unsigned i = 0; i < numTokens; ++i)
                dumpToken(tu, tokens[i], f);
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
            std::strchr(argv[1], 'c') != nullptr);
    }

    return 0;
}
