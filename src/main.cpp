#include <clang-c/CXCompilationDatabase.h>
#include <clang-c/Index.h>

#ifdef __unix__
  #include <limits.h>
  #include <stdlib.h>
  #include <cstring>
#endif

#include <iostream>
#include <string>
#include <type_traits>
#include <iomanip>
#include <memory>
#include <vector>

#include "CgStr.hpp"
#include "MultiTuProcessor.hpp"

char const tokenMap[] = "pkilc";


static CXChildVisitResult hlVisitor( CXCursor cursor, CXCursor /* parent */, CXClientData /* clientData */ )
{
    if (!clang_Location_isFromMainFile(clang_getCursorLocation(cursor)))
        return CXChildVisit_Continue;
    
    CXTranslationUnit tu = clang_Cursor_getTranslationUnit(cursor);
    CXSourceRange range = clang_getCursorExtent(cursor);


    CXToken* tokens;
    unsigned int numTokens;
    clang_tokenize(tu, range, &tokens, &numTokens);

    if ( numTokens > 0 ) {
        std::vector<CXCursor> tokCurs(numTokens);
        clang_annotateTokens(tu, tokens, numTokens, tokCurs.data());
        for (unsigned i = 0; i < numTokens - 1; ++i) {
            CgStr tokensp(clang_getTokenSpelling(tu,
                tokens[i]));
            CXSourceLocation tl = clang_getTokenLocation(tu, tokens[i]);
    
            unsigned line, column, offset;
            clang_getFileLocation(tl, nullptr, &line, &column, &offset);
            CgStr usr(clang_getCursorUSR(tokCurs[i]));
            CXCursorKind k = clang_getCursorKind(tokCurs[i]);
            CXCursor refd = clang_isReference(k) ? clang_getCursorReferenced(tokCurs[i]) : clang_getNullCursor();
            CgStr dname(clang_getCursorDisplayName(tokCurs[i]));
            std::cout << tokenMap[clang_getTokenKind(tokens[i])]
                      << " K:" << CgStr(clang_getCursorKindSpelling(k)).get()
                      << " D:" << dname.get()
                      << " U:" << usr.get()
                      << ' ' << line
                      << ':' << column
                      << " " << tokensp.get() << "\n";
            if (!clang_Cursor_isNull(refd))
                std::cout << "  -> U:" << CgStr(clang_getCursorUSR(refd)).get() << '\n';
            //std::cout << "  SUB: ";
            //clang_visitChildren(tokCurs[i], &visitSubtokens, nullptr);
            //std::cout << '\n';
        }
    }
    clang_disposeTokens(tu, tokens, numTokens);
    return CXChildVisit_Continue;
}

// static CXChildVisitResult astDumper(CXCursor c, CXCursor /* parent */, CXClientData ud) {
//     if (!clang_Location_isFromMainFile(clang_getCursorLocation(c)))
//         return CXChildVisit_Continue;
//     int ind = *static_cast<int*>(ud);
//     CXCursorKind kind = clang_getCursorKind(c);
//     CgStr spelling(clang_getCursorSpelling(c));
//     for (int i = 0; i < ind; ++i)
//         std::cout.put(' ');
//     std::cout  << cursorKindNames().at(kind) << ' ' << spelling.get() << '\n';
//     ind += 2;
//     clang_visitChildren(c, astDumper, &ind);
//     return CXChildVisit_Continue;
// }

static void printLoc(CXSourceLocation loc)
{
    unsigned line, col, off;
    CXFile file;
    clang_getFileLocation(loc, &file, &line, &col, &off);
    CgStr fname(clang_getFileName(file));
    std::cout << fname.get() << ":" << line << ":" << col << "+" << off << "\n";
}

int main( int argc, char** argv )
{
    CXIndex index = clang_createIndex( 0, 1 );
    CXTranslationUnit tu;
    CXErrorCode err = clang_parseTranslationUnit2FullArgv(index, nullptr, argv, argc, nullptr, 0, CXTranslationUnit_DetailedPreprocessingRecord, &tu);
    if (err == CXError_Success) {
        CXCursor rootCursor = clang_getTranslationUnitCursor(tu);
        CXSourceRange rng = clang_getCursorExtent(rootCursor);
        CXSourceLocation beg = clang_getRangeStart(rng);
        CXSourceLocation end = clang_getRangeEnd(rng);
        printLoc(beg);
        printLoc(end);
        //int ind = 0;
        CXToken* tokens;
        unsigned int numTokens;
        clang_tokenize(tu, clang_getCursorExtent(rootCursor), &tokens, &numTokens);

        if ( numTokens > 0 ) {
            std::vector<CXCursor> tokCurs(numTokens);
            clang_annotateTokens(tu, tokens, numTokens, tokCurs.data());
            for (unsigned i = 0; i < numTokens - 1; ++i) {
                CgStr tokensp(clang_getTokenSpelling(tu,
                    tokens[i]));
                CXSourceLocation tl = clang_getTokenLocation(tu, tokens[i]);
        
                unsigned line, column, offset;
                clang_getFileLocation(tl, nullptr, &line, &column, &offset);
                CgStr usr(clang_getCursorUSR(tokCurs[i]));
                CXCursorKind k = clang_getCursorKind(tokCurs[i]);
                CXCursor refd = clang_getCursorReferenced(tokCurs[i]);
                CgStr dname(clang_getCursorDisplayName(tokCurs[i]));
                std::cout << tokenMap[clang_getTokenKind(tokens[i])]
                        << " K:" << CgStr(clang_getCursorKindSpelling(k)).get()
                        << " S:" << CgStr(clang_getCursorSpelling(tokCurs[i])).get()
                        << " D:" << dname.get()
                        << " U:" << usr.get()
                        << ' ' << line
                        << ':' << column
                        << " " << tokensp.get() << "\n";
                if (!clang_equalCursors(refd, tokCurs[i]) && !clang_Cursor_isNull(refd)) {
                    std::cout << "  -> U:" << CgStr(clang_getCursorUSR(refd)).get() << '\n';
                    //printLoc(clang_getCursorLocation(refd));
                }
                //std::cout << "  SUB: ";
                //clang_visitChildren(tokCurs[i], &visitSubtokens, nullptr);
                //std::cout << '\n';
            }
        }
        clang_disposeTokens(tu, tokens, numTokens);
    } else {
        return err;
    }
    
    clang_disposeTranslationUnit(tu);
    clang_disposeIndex( index );
}
