#include "CgStr.hpp"
#include "HighlightedFile.hpp"
#include "MultiTuProcessor.hpp"
#include "annotate.hpp"
#include "cgWrappers.hpp"
#include "config.hpp"
#include "xref.hpp"

#include <climits>
#include <cstring>
#include <iostream>
#include <string>

using namespace synth;

namespace {

struct FileState {
    HighlightedFile& hlFile;
    std::size_t hlFileIdx;
    MultiTuProcessor& multiTuProcessor;
};

} // anonymous namespace

static bool isTypeKind(CXCursorKind k)
{
    SYNTH_DISCLANGWARN_BEGIN("-Wswitch-enum")
    switch (k) {
        case CXCursor_ClassDecl:
        case CXCursor_ClassTemplate:
        case CXCursor_ClassTemplatePartialSpecialization:
        case CXCursor_StructDecl:
        case CXCursor_UnionDecl:
        case CXCursor_EnumDecl:
        case CXCursor_TypedefDecl:
        case CXCursor_ObjCInterfaceDecl:
        case CXCursor_ObjCCategoryDecl:
        case CXCursor_ObjCProtocolDecl:
        case CXCursor_ObjCImplementationDecl:
        case CXCursor_TemplateTypeParameter:
        case CXCursor_TemplateTemplateParameter:
        case CXCursor_TypeAliasDecl:
        case CXCursor_TypeAliasTemplateDecl:
        case CXCursor_TypeRef:
        case CXCursor_ObjCSuperClassRef:
        case CXCursor_ObjCProtocolRef:
        case CXCursor_ObjCClassRef:
        case CXCursor_CXXBaseSpecifier:
            return true;

        default:
            return false;
    }
    SYNTH_DISCLANGWARN_END
}

static TokenAttributes getVarTokenAttributes(CXCursor cur)
{
    if (clang_getCursorLinkage(cur) == CXLinkage_NoLinkage)
        return TokenAttributes::varLocal;
    if (clang_getCXXAccessSpecifier(cur) == CX_CXXInvalidAccessSpecifier)
        return TokenAttributes::varGlobal;
    if (clang_Cursor_getStorageClass(cur) == CX_SC_Static)
        return TokenAttributes::varStaticMember;
    return TokenAttributes::varNonstaticMember;
}

static TokenAttributes getIntCssClasses(CXToken tok, CXTranslationUnit tu)
{
    std::string sp = CgStr(clang_getTokenSpelling(tu, tok)).gets();
    if (!sp.empty()) {
        if (sp.size() >= 2 && sp[0] == '0') {
            if (sp[1] == 'x' || sp[1] == 'X')
                return TokenAttributes::litNumIntHex;
            if (sp[1] == 'b' || sp[1] == 'B')
                return TokenAttributes::litNumIntBin;
            return TokenAttributes::litNumIntOct;
        }
        char suffix = sp[sp.size() - 1];
        if (suffix == 'l' || suffix == 'L')
            return TokenAttributes::litNumIntDecLong;
    }
    return TokenAttributes::litNum;
}

static bool startsWith(std::string const& s, std::string const& p) {
    return s.compare(0, p.size(), p) == 0;
}

static bool isBuiltinTypeKw(std::string const& t) {
    return startsWith(t, "unsigned ")
        || t == "unsigned"
        || startsWith(t, "signed ")
        || t == "signed"
        || startsWith(t, "short ")
        || t == "short"
        || startsWith(t, "long ")
        || t == "long"
        || t == "int"
        || t == "float"
        || t == "double"
        || t == "bool"
        || t == "char"
        || t == "char16_t"
        || t == "char32_t"
        || t == "wchar_t";
}

static TokenAttributes getTokenAttributes(
    CXToken tok, CXCursor cur, CXTranslationUnit tu)
{
    CXCursorKind k = clang_getCursorKind(cur);
    CXTokenKind tk = clang_getTokenKind(tok);
    if (clang_isPreprocessing(k)) {
        if (k == CXCursor_InclusionDirective) {
            CgStr spelling = clang_getTokenSpelling(tu, tok);
            if (std::strcmp(spelling.gets(), "include") != 0
                    && std::strcmp(spelling.gets(), "#") != 0
            ) {
                return TokenAttributes::preIncludeFile;
            }
        }
        return TokenAttributes::pre;
    }

    switch (tk) {
        case CXToken_Punctuation:
            if (k == CXCursor_BinaryOperator || k == CXCursor_UnaryOperator)
                return TokenAttributes::op;
            return TokenAttributes::punct;

        case CXToken_Comment:
            return TokenAttributes::cmmt;

        case CXToken_Literal:
            SYNTH_DISCLANGWARN_BEGIN("-Wswitch-enum")
            switch (k) {
                case CXCursor_ObjCStringLiteral:
                case CXCursor_StringLiteral:
                    return TokenAttributes::litStr;
                case CXCursor_CharacterLiteral:
                    return TokenAttributes::litChr;
                case CXCursor_FloatingLiteral:
                    return TokenAttributes::litNumFlt;
                case CXCursor_IntegerLiteral:
                    return getIntCssClasses(tok, tu);
                case CXCursor_ImaginaryLiteral:
                    return TokenAttributes::litNum;
                default:
                    return TokenAttributes::lit;
            }
            SYNTH_DISCLANGWARN_END

        case CXToken_Keyword: {
            if (k == CXCursor_BinaryOperator || k == CXCursor_UnaryOperator)
                return TokenAttributes::opWord;
            if (k == CXCursor_CXXNullPtrLiteralExpr
                || k == CXCursor_CXXBoolLiteralExpr
                || k == CXCursor_ObjCBoolLiteralExpr
            ) {
                return TokenAttributes::litKw;
            }
            std::string sp = CgStr(clang_getTokenSpelling(tu, tok)).gets();
            if (k == CXCursor_TypeRef || isBuiltinTypeKw(sp))
                return TokenAttributes::tyBuiltin;
            if (clang_isDeclaration(k))
                return TokenAttributes::kwDecl;
            return TokenAttributes::kw;
        }

        case CXToken_Identifier:
            if (isTypeKind(k))
                return TokenAttributes::ty;
            SYNTH_DISCLANGWARN_BEGIN("-Wswitch-enum")
            switch (k) {
                case CXCursor_MemberRef:
                case CXCursor_DeclRefExpr:
                case CXCursor_MemberRefExpr:
                case CXCursor_UsingDeclaration:
                case CXCursor_TemplateRef: {
                    CXCursor refd = clang_getCursorReferenced(cur);
                    return getTokenAttributes(tok, refd, tu);
                }

                case CXCursor_ObjCPropertyDecl:
                    return TokenAttributes::varNonstaticMember; // Sorta right.

                case CXCursor_ObjCIvarDecl:
                case CXCursor_FieldDecl:
                    return TokenAttributes::varNonstaticMember; // TODO

                case CXCursor_EnumConstantDecl:
                case CXCursor_NonTypeTemplateParameter:
                    return TokenAttributes::constant;

                case CXCursor_FunctionDecl:
                case CXCursor_ObjCInstanceMethodDecl:
                case CXCursor_ObjCClassMethodDecl:
                case CXCursor_CXXMethod:
                case CXCursor_FunctionTemplate:
                case CXCursor_Constructor:
                case CXCursor_Destructor:
                case CXCursor_ConversionFunction:
                case CXCursor_OverloadedDeclRef:
                    return TokenAttributes::func;

                case CXCursor_VarDecl:
                    return getVarTokenAttributes(cur);
                case CXCursor_ParmDecl:
                    return TokenAttributes::varLocal;

                case CXCursor_Namespace:
                case CXCursor_NamespaceAlias:
                case CXCursor_UsingDirective:
                case CXCursor_NamespaceRef:
                    return TokenAttributes::namesp;

                case CXCursor_LabelStmt:
                    return TokenAttributes::lbl;

                default:
                    if (clang_isAttribute(k))
                        return TokenAttributes::attr;
                    return TokenAttributes::none;
            }
            SYNTH_DISCLANGWARN_END
            assert("unreachable" && false);
    }
}

static void processToken(FileState& state, CXToken tok, CXCursor cur)
{
    auto& markups = state.hlFile.markups;
    markups.emplace_back();
    Markup& m = markups.back();
    CXTranslationUnit tu = clang_Cursor_getTranslationUnit(cur);
    CXSourceRange rng = clang_getTokenExtent(tu, tok);
    //std::cout << rng << ": " << CgStr(clang_getTokenSpelling(tu, tok)).gets() << '\n';
    unsigned lineno;
    clang_getFileLocation(
        clang_getRangeStart(rng), nullptr, &lineno, nullptr, &m.begin_offset);
    clang_getFileLocation(
        clang_getRangeEnd(rng), nullptr, nullptr, nullptr, &m.end_offset);
    if (m.begin_offset == m.end_offset) {
        markups.pop_back();
        return;
    }

    m.refd.filename = nullptr;
    m.attrs = getTokenAttributes(tok, cur, tu);

    CXTokenKind tk = clang_getTokenKind(tok);
    if (tk == CXToken_Comment || tk == CXToken_Literal)
        return;

    if (!clang_equalLocations(
            clang_getRangeStart(rng), clang_getCursorLocation(cur))
    ) {
        return;
    }

    CXCursorKind k = clang_getCursorKind(cur);
    if (k == CXCursor_InclusionDirective) {
        Markup incLnk = {};
        CXSourceRange incrng = clang_getCursorExtent(cur);
        clang_getFileLocation(
            clang_getRangeStart(incrng),
            nullptr,
            nullptr,
            nullptr,
            &incLnk.begin_offset);
        clang_getFileLocation(
            clang_getRangeEnd(incrng),
            nullptr,
            nullptr,
            nullptr,
            &incLnk.end_offset);
        if (linkInclude(incLnk, cur, state.multiTuProcessor))
            state.hlFile.markups.push_back(std::move(incLnk));
        return;
    }

    if (clang_isDeclaration(k))
        m.attrs |= TokenAttributes::flagDecl;

    // std::cout << CgStr(clang_getTokenSpelling(tu, tok)).gets()
    //     << " " << CgStr(clang_getCursorKindSpelling(k)).gets() << '\n';

    // clang_isReference() sometimes reports false negatives, e.g. for
    // overloaded operators, so check manually.
    CXCursor referenced = clang_getCursorReferenced(cur);
    bool isref = !clang_Cursor_isNull(referenced)
        && !clang_equalCursors(cur, referenced);
    if (isref)
        linkCursorIfIncludedDst(m, referenced, lineno, state.multiTuProcessor);

    CXCursor defcur = clang_getCursorDefinition(cur);
    if (clang_equalCursors(defcur, cur)) { // This is a definition:
        m.attrs |= TokenAttributes::flagDef;
        CgStr usr(clang_getCursorUSR(cur));
        if (!usr.empty()) {
            SourceLocation decl {
                state.hlFile.originalPath,
                lineno
            };
            state.multiTuProcessor.registerDef(usr.get(), std::move(decl));
        }
    } else if (!isref) {
        if (clang_Cursor_isNull(defcur)) {
            CgStr usr(clang_getCursorUSR(cur));
            if (!usr.empty()) {
                state.multiTuProcessor.registerMissingDefLink(
                    state.hlFileIdx,
                    state.hlFile.markups.size() - 1,
                    usr.get());
            }
        } else {
            linkCursorIfIncludedDst(m, defcur, lineno, state.multiTuProcessor);
        }
    }
}

namespace {

struct IncVisitorData {
    MultiTuProcessor& multiTuProcessor;
    CXTranslationUnit tu;
};

} // anonymous namespace

static void processFile(
    CXFile file, CXSourceLocation*, unsigned, CXClientData ud)
{
    auto& cdata = *static_cast<IncVisitorData*>(ud);
    CXTranslationUnit tu = cdata.tu;
    MultiTuProcessor& multiTuProcessor = cdata.multiTuProcessor;

    CXSourceLocation beg = clang_getLocationForOffset(tu, file, 0);
    CXSourceLocation end = clang_getLocation(tu, file, UINT_MAX, UINT_MAX);

    auto output = multiTuProcessor.prepareToProcess(file);
    if (!output.first)
        return;

    CXToken* tokens;
    unsigned numTokens;
    clang_tokenize(tu, clang_getRange(beg, end), &tokens, &numTokens);
    CgTokensCleanup tokCleanup(tokens, numTokens, tu);

    if (numTokens > 0) {
        FileState state {*output.first, output.second, multiTuProcessor};
        std::vector<CXCursor> tokCurs(numTokens);
        clang_annotateTokens(tu, tokens, numTokens, tokCurs.data());
        for (unsigned i = 0; i < numTokens - 1; ++i) {
            CXCursor cur = tokCurs[i];
            CXSourceLocation tokLoc = clang_getTokenLocation(tu, tokens[i]);
            if (!clang_equalLocations(clang_getCursorLocation(cur), tokLoc)) {
                CXCursor c2 = clang_getCursor(tu, tokLoc);
                if (clang_equalLocations(clang_getCursorLocation(c2), tokLoc))
                    cur = c2;
            }
            processToken(state, tokens[i], cur);
        }
    }
    std::cout << "Processed " << numTokens << " tokens in "
              << *output.first->originalPath << '\n';
}

int synth::processTu(
    CXIndex cidx,
    MultiTuProcessor& state,
    char const* const* args,
    int nargs)
{
    CXTranslationUnit tu = nullptr;
    CXErrorCode err = clang_parseTranslationUnit2FullArgv(
        cidx,
        /*source_filename:*/ nullptr, // Included in commandline.
        args,
        nargs,
        /*unsaved_files:*/ nullptr,
        /*num_unsaved_files:*/ 0,
        CXTranslationUnit_DetailedPreprocessingRecord,
        &tu);
    CgTuHandle htu(tu);
    if (err != CXError_Success) {
        std::cerr << "Failed parsing translation unit (code "
                  << static_cast<int>(err)
                  << ")\n";
        std::cerr << "  args:";
        for (int i = 0; i < nargs; ++i)
            std::cerr << ' ' << args[i];
        std::cerr << '\n';
        return err + 10;
    }

    IncVisitorData ud {state, tu};
    clang_getInclusions(tu, &processFile, &ud);
    return EXIT_SUCCESS;
}
