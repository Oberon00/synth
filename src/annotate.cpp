#include "annotate.hpp"

#include "CgStr.hpp"
#include "MultiTuProcessor.hpp"
#include "cgWrappers.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "fileIdSupport.hpp"
#include "output.hpp"
#include "xref.hpp"

#include <boost/assert.hpp>

#include <climits>
#include <cstring>
#include <iostream>
#include <string>

using namespace synth;

static const unsigned kMaxRefRecursion = 16;

static bool equalFileLocations(CXSourceLocation loc1, CXSourceLocation loc2)
{
    CXFile f1;
    unsigned off1;
    clang_getFileLocation(loc1, &f1, nullptr, nullptr, &off1);

    CXFile f2;
    unsigned off2;
    clang_getFileLocation(loc2, &f2, nullptr, nullptr, &off2);

    return off1 == off2 && clang_File_isEqual(f1, f2);
}

static unsigned getLocOffset(CXSourceLocation loc)
{
    unsigned r;
    clang_getFileLocation(loc, nullptr, nullptr, nullptr, &r);
    return r;
}

namespace {

struct FileState {
    HighlightedFile& hlFile;
    MultiTuProcessor& multiTuProcessor;
    bool prevWasDtorStart;
};

struct FileAnnotationState {
    CXFile file;
    HighlightedFile* hlFile;
    CgTokensHandle tokens;
    std::vector<CXCursor> annotations;
    std::vector<bool> annotationBad;

    // Maps from token begin offsets to indices (in tokens and tokCurs).
    std::unordered_map<unsigned, std::size_t> locationMap;

    void populateLocationMap(CXTranslationUnit tu)
    {
        for (std::size_t i = 0; i < tokens.size(); ++i) {
            CXToken tok = tokens.tokens()[i];
            unsigned off = getLocOffset(clang_getTokenLocation(tu, tok));
            BOOST_VERIFY(locationMap.insert({off, i}).second);
        }
    }
};

using TuAnnotationMap = std::unordered_map<CXFileUniqueID, FileAnnotationState>;

FileAnnotationState* lookupFileAnnotations(TuAnnotationMap& m, CXFile f)
{
    CXFileUniqueID fuid;
    if (!f || clang_getFileUniqueID(f, &fuid) != 0)
        return nullptr;
    auto it = m.find(fuid);
    return it == m.end() ? nullptr : &it->second;
}

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
    CXToken tok, CXCursor cur, CXTranslationUnit tu, unsigned recursionDepth = 0)
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
                    bool recErr = recursionDepth > kMaxRefRecursion;
                    if (recErr) {
                        CgStr kindSp(clang_getCursorKindSpelling(k));
                        CgStr rKindSp(clang_getCursorKindSpelling(
                                clang_getCursorKind(refd)));
                        std::clog << "When trying to highlight token "
                                << clang_getTokenExtent(tu, tok) << " "
                                << CgStr(clang_getTokenSpelling(tu, tok))
                                << ":\n"
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

                    return getTokenAttributes(
                        tok, refd, tu, recursionDepth + 1);
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
    Markup* m = &markups.back();
    CXTranslationUnit tu = clang_Cursor_getTranslationUnit(cur);
    CXSourceRange rng = clang_getTokenExtent(tu, tok);
    unsigned lineno;
    clang_getFileLocation(
        clang_getRangeStart(rng), nullptr, &lineno, nullptr, &m->beginOffset);
    m->endOffset = getLocOffset(clang_getRangeEnd(rng));
    if (m->beginOffset == m->endOffset) {
        markups.pop_back();
        return;
    }

    m->refd.file = nullptr;
    m->attrs = getTokenAttributes(tok, cur, tu);

    CXTokenKind tk = clang_getTokenKind(tok);
    if (tk == CXToken_Comment || tk == CXToken_Literal)
        return;

    CXCursorKind k = clang_getCursorKind(cur);
    if (state.prevWasDtorStart) {
        assert (k == CXCursor_Destructor);
        assert(tk == CXToken_Identifier);
        state.prevWasDtorStart = false;
        Markup dtorLnk = {};
        dtorLnk.beginOffset = getLocOffset(clang_getCursorLocation(cur));
        dtorLnk.endOffset = m->endOffset;
        markups.push_back(std::move(dtorLnk));
        m = &markups.back();
    } else if (!equalFileLocations(
            clang_getRangeStart(rng), clang_getCursorLocation(cur))
    ) {
        return;
    } else if (k == CXCursor_InclusionDirective) {
        Markup incLnk = {};
        CXSourceRange incrng = clang_getCursorExtent(cur);
        incLnk.beginOffset = getLocOffset(clang_getRangeStart(incrng));
        incLnk.endOffset = getLocOffset(clang_getRangeEnd(incrng));
        if (linkInclude(incLnk, cur, state.multiTuProcessor))
            state.hlFile.markups.push_back(std::move(incLnk));
        return;
    } else if (k == CXCursor_Destructor) {
        assert(tk == CXToken_Punctuation); // "~"
        state.prevWasDtorStart = true;
        return;
    }

    if (clang_isDeclaration(k))
        m->attrs |= TokenAttributes::flagDecl;

    // clang_isReference() sometimes reports false negatives, e.g. for
    // overloaded operators, so check manually.
    CXCursor referenced = clang_getCursorReferenced(cur);
    bool isref = !clang_Cursor_isNull(referenced)
        && !clang_equalCursors(cur, referenced);
    if (isref)
        linkCursorIfIncludedDst(*m, referenced, state.multiTuProcessor);

    CXCursor defcur = clang_getCursorDefinition(cur);
    if (clang_equalCursors(defcur, cur)) { // This is a definition:
        m->attrs |= TokenAttributes::flagDef;
        CgStr usr(clang_getCursorUSR(cur));
        if (!usr.empty()) {
            SourceLocation decl {&state.hlFile, lineno};
            state.multiTuProcessor.registerDef(usr.get(), std::move(decl));
        }
    } else if (!isref) {
        if (clang_Cursor_isNull(defcur)) {
            CgStr usr(clang_getCursorUSR(cur));
            if (!usr.empty()) {
                state.multiTuProcessor.registerMissingDefLink(
                    state.hlFile,
                    state.hlFile.markups.size() - 1,
                    usr.get());
            }
        } else {
            linkCursorIfIncludedDst(*m, defcur, state.multiTuProcessor);
        }
    }
}

namespace {

struct IncVisitorData {
    MultiTuProcessor& multiTuProcessor;
    CXTranslationUnit tu;
    TuAnnotationMap& annotationMap;
};

} // anonymous namespace


static CXChildVisitResult annotateVisit(
    CXCursor c, CXCursor, CXClientData ud)
{
    auto& amap = *static_cast<TuAnnotationMap*>(ud);
    CXFile f;
    unsigned off;
    clang_getFileLocation(
        clang_getCursorLocation(c), &f, nullptr, nullptr, &off);
    FileAnnotationState* astate = lookupFileAnnotations(amap, f);
    if (!astate)
        return CXChildVisit_Continue; // Should we use Recurse here?

    auto itIdx = astate->locationMap.find(off);
    if (itIdx == astate->locationMap.end()
        || !astate->annotationBad[itIdx->second]
    ) {
        return CXChildVisit_Recurse;
    }

    CXCursor& acur = astate->annotations[itIdx->second];
    acur = c;
    return CXChildVisit_Recurse;
}

static void annotate(TuAnnotationMap& map, CXCursor root)
{
    clang_visitChildren(root, &annotateVisit, &map);
}

static void processFile(
    CXFile file, CXSourceLocation*, unsigned, CXClientData ud)
{
    CXFileUniqueID fuid;
    if (clang_getFileUniqueID(file, &fuid) != 0)
        return;

    auto& cdata = *static_cast<IncVisitorData*>(ud);
    CXTranslationUnit tu = cdata.tu;

    CXSourceLocation beg = clang_getLocationForOffset(tu, file, 0);
    CXSourceLocation end = clang_getLocation(tu, file, UINT_MAX, UINT_MAX);

    HighlightedFile* hlFile = cdata.multiTuProcessor.prepareToProcess(file);
    if (!hlFile)
        return;

    CXToken* tokens;
    unsigned numTokens;
    clang_tokenize(tu, clang_getRange(beg, end), &tokens, &numTokens);
    CgTokensHandle hToks(tokens, numTokens, tu);

    if (numTokens == 0)
        return;

    std::vector<CXCursor> annotations(numTokens);
    clang_annotateTokens(tu, tokens, numTokens, annotations.data());
    std::vector<bool> annotationBad(numTokens);

    for (std::size_t i = 0; i < numTokens; ++i) {
        CXCursor& cur = annotations[i];
        CXSourceLocation tokLoc = clang_getTokenLocation(tu, tokens[i]);
        if (!equalFileLocations(clang_getCursorLocation(cur), tokLoc)) {
            CXCursor c2 = clang_getCursor(tu, tokLoc);
            CXSourceLocation loc2 = clang_getCursorLocation(c2);
            if (equalFileLocations(loc2, tokLoc))
                cur = c2;
            else
                annotationBad[i] = true;
        }
    }

    FileAnnotationState fstate {
        std::move(file),
        std::move(hlFile),
        std::move(hToks),
        std::move(annotations),
        std::move(annotationBad),
        std::unordered_map<unsigned, std::size_t>()
    };
    auto kv = std::make_pair(std::move(fuid), std::move(fstate));
    auto insRes = cdata.annotationMap.insert(std::move(kv));
    assert(insRes.second);
    insRes.first->second.populateLocationMap(tu);
}

static void writeHlTokens(
    TuAnnotationMap& annotations, MultiTuProcessor& multiTuProcessor)
{
    for (auto& fAnnotationsEntry : annotations) {
        FileAnnotationState& fAnnotations = fAnnotationsEntry.second;
        FileState fstate {*fAnnotations.hlFile, multiTuProcessor, false};
        for (std::size_t i = 0; i < fAnnotations.tokens.size(); ++i) {
            CXToken tok = fAnnotations.tokens.tokens()[i];
            CXCursor cur = fAnnotations.annotations[i];
            processToken(fstate, tok, cur);
        }
        fAnnotations.hlFile->markups.shrink_to_fit();
    }
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

    TuAnnotationMap annotations;
    IncVisitorData ud {state, tu, annotations};
    clang_getInclusions(tu, &processFile, &ud);
    annotate(annotations, clang_getTranslationUnitCursor(tu));
    writeHlTokens(annotations, state);

    return EXIT_SUCCESS;
}
