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

static std::string getVarCssClasses(CXCursor cur)
{
    if (clang_getCursorLinkage(cur) == CXLinkage_NoLinkage)
        return "nv"; // Name.Variable (local)
    if (clang_getCXXAccessSpecifier(cur) == CX_CXXInvalidAccessSpecifier)
        return "vg"; // Name.Variable.Global
    if (clang_Cursor_getStorageClass(cur) == CX_SC_Static)
        return "vc"; // Name.Variable.Class (static member variable)
    return "vi"; // Name.Variable.Instance (nonstatic member variable)
}

static std::string getCssClasses(CXToken tok, CXCursor cur, CXTranslationUnit tu)
{
    CXCursorKind k = clang_getCursorKind(cur);
    CXTokenKind tk = clang_getTokenKind(tok);
    if (clang_isPreprocessing(k)) {
        if (k == CXCursor_InclusionDirective
            && (tk == CXToken_Literal || tk == CXToken_Identifier)
        ) {
            CgStr spelling = clang_getTokenSpelling(tu, tok);
            if (!std::strcmp(spelling.gets(), "cpf")
            ) {
                return "cpf";
            }
        }
        return "cp";
    }
    switch (tk) {
        case CXToken_Punctuation:
            if (k == CXCursor_BinaryOperator || k == CXCursor_UnaryOperator)
                return "o";
            return "p";

        case CXToken_Comment:
            return "c";

        case CXToken_Literal:
            SYNTH_DISCLANGWARN_BEGIN("-Wswitch-enum")
            switch (k) {
                case CXCursor_ObjCStringLiteral:
                case CXCursor_StringLiteral:
                    return "s";
                case CXCursor_CharacterLiteral:
                    return "sc";
                case CXCursor_FloatingLiteral:
                    return "mf";
                case CXCursor_IntegerLiteral:
                    return "mi";
                case CXCursor_ImaginaryLiteral:
                    return "m"; // Number
                default:
                    return "l";
            }
            SYNTH_DISCLANGWARN_END

        case CXToken_Keyword:
            if (clang_isDeclaration(k))
                return "kd";
            if (k == CXCursor_TypeRef)
                return "kt";
            if (k == CXCursor_BinaryOperator || k == CXCursor_UnaryOperator)
                return "ow"; // Operator.Word
            return "k";

        case CXToken_Identifier:
            if (isTypeKind(k))
                return "nc"; // Name.Class
            SYNTH_DISCLANGWARN_BEGIN("-Wswitch-enum")
            switch (k) {
                case CXCursor_MemberRef:
                case CXCursor_DeclRefExpr:
                case CXCursor_MemberRefExpr:
                case CXCursor_UsingDeclaration:
                case CXCursor_TemplateRef: {
                    CXCursor refd = clang_getCursorReferenced(cur);
                    return getCssClasses(tok, refd, tu);
                }

                case CXCursor_ObjCPropertyDecl:
                    return "py"; // Name.Variable.Property

                case CXCursor_ObjCIvarDecl:
                case CXCursor_FieldDecl:
                    return "vi"; // Name.Variable.Instance

                case CXCursor_EnumConstantDecl:
                case CXCursor_NonTypeTemplateParameter:
                    return "no"; // Name.Constant

                case CXCursor_FunctionDecl:
                case CXCursor_ObjCInstanceMethodDecl:
                case CXCursor_ObjCClassMethodDecl:
                case CXCursor_CXXMethod:
                case CXCursor_FunctionTemplate:
                case CXCursor_Constructor:
                case CXCursor_Destructor:
                case CXCursor_ConversionFunction:
                case CXCursor_OverloadedDeclRef:
                    return "nf"; // Name.Function

                case CXCursor_VarDecl:
                    return getVarCssClasses(cur);
                case CXCursor_ParmDecl:
                    return "nv"; // Name.Variable

                case CXCursor_Namespace:
                case CXCursor_NamespaceAlias:
                case CXCursor_UsingDirective:
                    return "nn"; // Name.Namespace

                case CXCursor_LabelStmt:
                    return "nl"; // Name.Label

                case CXCursor_LinkageSpec: // Handled by other tokens.
                case CXCursor_CXXAccessSpecifier: // Handled by other tokens.
                    return std::string();
                default:
                    if (clang_isAttribute(k))
                        return "nd"; // Name.Decorator
                    return std::string();
            }
            SYNTH_DISCLANGWARN_END
            assert("unreachable" && false);
    }
}

static void processToken(
    HighlightedFile& out,
    unsigned outIdx,
    MultiTuProcessor& state,
    CXToken tok,
    CXCursor cur)
{
    out.markups.emplace_back();
    Markup& m = out.markups.back();
    m.tag = Markup::kTagSpan;
    CXTranslationUnit tu = clang_Cursor_getTranslationUnit(cur);
    CXSourceRange rng = clang_getTokenExtent(tu, tok);
    //std::cout << rng << ": " << CgStr(clang_getTokenSpelling(tu, tok)).gets() << '\n';
    CXFile file;
    unsigned lineno;
    clang_getFileLocation(
        clang_getRangeStart(rng), &file, &lineno, nullptr, &m.begin_offset);
    clang_getFileLocation(
        clang_getRangeEnd(rng), nullptr, nullptr, nullptr, &m.end_offset);
    if (m.begin_offset == m.end_offset) {
        out.markups.pop_back();
        return;
    }
    CgStr srcFname(clang_getFileName(file)); // TODO? make relative to root
    m.attrs.insert({"class", getCssClasses(tok, cur, tu)});


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
        Markup im = {};
        CXSourceRange incrng = clang_getCursorExtent(cur);
        clang_getFileLocation(
            clang_getRangeStart(incrng),
            nullptr,
            nullptr,
            nullptr,
            &im.begin_offset);
        clang_getFileLocation(
            clang_getRangeEnd(incrng),
            nullptr,
            nullptr,
            nullptr,
            &im.end_offset);
        if (linkInclude(im, cur, srcFname.get(), state))
            out.markups.push_back(std::move(im));
        return;
    }

    // std::cout << CgStr(clang_getTokenSpelling(tu, tok)).gets()
    //     << " " << CgStr(clang_getCursorKindSpelling(k)).gets() << '\n';

    // clang_isReference() sometimes reports false negatives, e.g. for
    // overloaded operators, so check manually.
    CXCursor referenced = clang_getCursorReferenced(cur);
    bool isref = !clang_Cursor_isNull(referenced)
        && !clang_equalCursors(cur, referenced);
    if (isref) {
        linkCursorIfIncludedDst(
            m, referenced, srcFname.get(), lineno, state, /*byUsr:*/ false);
    }

    CXCursor defcur = clang_getCursorDefinition(cur);
    if (clang_equalCursors(defcur, cur)) { // This is a definition:
        m.attrs["class"] += " dfn";
        CgStr usr(clang_getCursorUSR(cur));
        if (!usr.empty()) {
            SymbolDeclaration decl {
                usr.get(),
                srcFname.get(),
                lineno,
                /*isdef=*/ true
            };
            m.attrs["id"] = decl.usr; // Escape?
            state.registerDef(std::move(decl));
        }
    } else if (!isref) {
        if (clang_Cursor_isNull(defcur)) {
            CgStr usr(clang_getCursorUSR(cur));
            if (!usr.empty()) {
                state.registerMissingDefLink(
                    outIdx,
                    out.markups.size() - 1,
                    srcFname.get(),
                    usr.get());
            }
        } else {
            linkCursorIfIncludedDst(
                m, defcur, srcFname.get(), lineno, state, /*byUsr:*/ true);
        }
    }
}

namespace {

struct IncVisitorData {
    MultiTuProcessor& state;
    CXTranslationUnit tu;
};

} // anonymous namespace

static void processFile(
    CXFile file, CXSourceLocation*, unsigned, CXClientData ud)
{
    auto& cdata = *static_cast<IncVisitorData*>(ud);
    CXTranslationUnit tu = cdata.tu;
    MultiTuProcessor& state = cdata.state;

    CXSourceLocation beg = clang_getLocationForOffset(tu, file, 0);
    CXSourceLocation end = clang_getLocation(tu, file, UINT_MAX, UINT_MAX);

    auto output = state.prepareToProcess(file);
    if (!output.first)
        return;

    CXToken* tokens;
    unsigned numTokens;
    clang_tokenize(tu, clang_getRange(beg, end), &tokens, &numTokens);
    CgTokensCleanup tokCleanup(tokens, numTokens, tu);

    if (numTokens > 0) {
        std::vector<CXCursor> tokCurs(numTokens);
        clang_annotateTokens(tu, tokens, numTokens, tokCurs.data());
        for (unsigned i = 0; i < numTokens - 1; ++i) {
            processToken(
                *output.first, output.second, state, tokens[i], tokCurs[i]);
        }
    }
    std::cout << "Processed " << numTokens << " tokens in "
              << CgStr(clang_getFileName(file)).gets() << '\n';
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
