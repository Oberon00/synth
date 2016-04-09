#include "highlight.hpp"

#include "CgStr.hpp"
#include "config.hpp"
#include "debug.hpp"

#include <cstring>
#include <iostream>

using namespace synth;


static unsigned const kMaxRefRecursion = 16;

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

static TokenAttributes getIntTokenAttributes(boost::string_ref sp)
{
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

static bool isBuiltinTypeKw(boost::string_ref t) {
    return t.starts_with("unsigned ")
        || t == "unsigned"
        || t.starts_with("signed ")
        || t == "signed"
        || t.starts_with("short ")
        || t == "short"
        || t.starts_with("long ")
        || t == "long"
        || t == "int"
        || t == "float"
        || t == "double"
        || t == "bool"
        || t == "char"
        || t == "char16_t"
        || t == "char32_t"
        || t == "wchar_t"
        || t == "void";
}
static TokenAttributes getTokenAttributesImpl(
    CXToken tok,
    CXCursor cur,
    boost::string_ref sp, // token spelling
    CXTranslationUnit tu,
    unsigned recursionDepth)
{
    CXCursorKind k = clang_getCursorKind(cur);
    CXTokenKind tk = clang_getTokenKind(tok);
    if (clang_isPreprocessing(k)) {
        if (k == CXCursor_InclusionDirective && sp != "include" && sp != "#")
            return TokenAttributes::preIncludeFile;
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
                    return getIntTokenAttributes(sp);
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
            if (k == CXCursor_TypeRef || isBuiltinTypeKw(sp))
                return TokenAttributes::tyBuiltin;
            if (clang_isDeclaration(k) || k == CXCursor_DeclStmt)
                return TokenAttributes::kwDecl;
            if (sp == "sizeof" || sp == "alignof")
                return TokenAttributes::opWord;
            if (sp == "this")
                return TokenAttributes::litKw;
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
                    bool recErr = recursionDepth > kMaxRefRecursion;
                    if (recErr) {
                        CgStr kindSp(clang_getCursorKindSpelling(k));
                        CgStr rKindSp(clang_getCursorKindSpelling(
                                clang_getCursorKind(refd)));
                        std::clog << "When trying to highlight token "
                                << clang_getTokenExtent(tu, tok) << " "
                                << sp << ":\n"
                                << "  Cursor " << clang_getCursorExtent(cur)
                                << " " << kindSp << " references "
                                << clang_getCursorExtent(refd)
                                << " " << rKindSp
                                << "  Maximum depth exceeded with "
                                << recursionDepth << ".\n";
                        return TokenAttributes::none;
                    }

                    if (clang_equalCursors(cur, refd))
                        return TokenAttributes::none;

                    return getTokenAttributesImpl(
                        tok, refd, sp, tu, recursionDepth + 1);
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
    }
    SYNTH_DISCLANGWARN_END
    assert("unreachable" && false);
    return TokenAttributes::none;
}

TokenAttributes synth::getTokenAttributes(
    CXToken tok, CXCursor cur, boost::string_ref tokSpelling)
{
    return getTokenAttributesImpl(
        tok, cur, tokSpelling, clang_Cursor_getTranslationUnit(cur), 0);
}
