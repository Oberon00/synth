#include <clang-c/CXCompilationDatabase.h>
#include <clang-c/Index.h>

#include <iostream>
#include <string>
#include <type_traits>
#include <iomanip>
#include <memory>
#include <vector>
#include <boost/filesystem/path.hpp>
#include <boost/noncopyable.hpp>
#include <climits>

#include "CgStr.hpp"
#include "MultiTuProcessor.hpp"
#include "xref.hpp"

namespace fs = boost::filesystem;

namespace synth {

struct CmdLineArgs {
    std::string rootdir;
    std::string outdir;
    char const* const* firstClangArg;
    int nClangArgs;
};

static CmdLineArgs parseCmdLine(int argc, char** argv)
{
    if (argc < 3)
        throw std::runtime_error("Too few arguments.");
    CmdLineArgs r;
    r.rootdir = argv[1];
    bool foundCmd = false;
    for (int i = 2; i < argc; ++i) {
        if (!std::strcmp(argv[i], "-o")) {
            if (!argv[i + 1])
                throw std::runtime_error("Missing value for -o");
            r.outdir = argv[++i];
        } else if (!std::strcmp(argv[i], "args")) {
            r.firstClangArg = argv + i + 1;
            r.nClangArgs = argc - i - 1;
            foundCmd = true;
            break;
        }
    }
    if (!foundCmd)
        throw std::runtime_error("Missing command.");
    return r;
}

struct DeleterForCXIndex {
    void operator() (CXIndex cidx) const { clang_disposeIndex(cidx); }
};

struct DeleterForCXTranslationUnit {
    void operator() (CXTranslationUnit tu) const
    {
        clang_disposeTranslationUnit(tu);
    }
};

class CgTokensCleanup : private boost::noncopyable {
public:
    CgTokensCleanup(CXToken* data, unsigned ntokens, CXTranslationUnit tu_)
        : m_data(data), m_ntokens(ntokens), m_tu(tu_)
    {}

    ~CgTokensCleanup() {
        clang_disposeTokens(m_tu, m_data, m_ntokens);
    }

private:
    CXToken* m_data;
    unsigned m_ntokens;
    CXTranslationUnit m_tu;
};

using CgIdxHandle = std::unique_ptr<
    std::remove_pointer_t<CXIndex>, DeleterForCXIndex>;
using CgTuHandle = std::unique_ptr<
    std::remove_pointer_t<CXTranslationUnit>, DeleterForCXTranslationUnit>;

static CgStr getCursorFilename(CXCursor c)
{
    CXFile f;
    clang_getFileLocation(
        clang_getCursorLocation(c), &f, nullptr, nullptr, nullptr);
    return clang_getFileName(f);
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
            break;

        case CXToken_Keyword:
            if (clang_isDeclaration(k))
                return "kd";
            if (k == CXCursor_TypeRef)
                return "kt";
            if (k == CXCursor_BinaryOperator || k == CXCursor_UnaryOperator)
                return "ow"; // Operator.Word
            return "k";

        case CXToken_Identifier:
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
                    return "nc"; // Name.Class

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
                    return "nf"; // Name.Function

                case CXCursor_VarDecl:
                    // TODO: Could distinguish globals and class from others.
                    return "nv"; // Name.Variable
                case CXCursor_ParmDecl:
                    return "nv"; // Name.Variable

                case CXCursor_Namespace:
                case CXCursor_NamespaceAlias:
                case CXCursor_UsingDirective:
                    return "nn"; // Name.Namespace

                case CXCursor_LabelStmt:
                    return "nl"; // Name.Label

                case CXCursor_UsingDeclaration: // TODO: Depends on referenced.
                case CXCursor_LinkageSpec: // Handled by other tokens.
                case CXCursor_CXXAccessSpecifier: // Handled by other tokens.
                    return std::string();
                default:
                    if (clang_isAttribute(k))
                        return "nd"; // Name.Decorator
                    return std::string();
            }
            assert(!"unreachable");
        default:
            assert(!"Invalid token kind.");
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

    // clang_isReference() sometimes reports false negatives, e.g. for
    // overloaded operators, so check manually.
    CXCursor referenced = clang_getCursorReferenced(cur);
    bool isref = !clang_Cursor_isNull(referenced)
        && !clang_equalCursors(cur, referenced)
        && state.underRootdir(getCursorFilename(referenced).get());
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

static void processRange(
    MultiTuProcessor& state, CXTranslationUnit tu, CXSourceRange rng);

static CXVisitorResult includeVisitor(void* ud, CXCursor cursor, CXSourceRange)
{
    auto& state = *static_cast<MultiTuProcessor*>(ud);
    CXFile incf = clang_getIncludedFile(cursor);
    CXTranslationUnit tu = clang_Cursor_getTranslationUnit(cursor);

    CXSourceLocation beg = clang_getLocationForOffset(tu, incf, 0);
    CXSourceLocation end = clang_getLocation(tu, incf, UINT_MAX, UINT_MAX);

    processRange(state, tu, clang_getRange(beg, end));

    return CXVisit_Continue;
}

static void processRange(
    MultiTuProcessor& state, CXTranslationUnit tu, CXSourceRange rng)
{
    CXSourceLocation cloc = clang_getRangeStart(rng);
    CXFile cfile;

    clang_getFileLocation(cloc, &cfile, nullptr, nullptr, nullptr);
    auto output = state.prepareToProcess(cfile);
    if (!output.first)
        return;

    CXToken* tokens;
    unsigned numTokens;
    clang_tokenize(tu, rng, &tokens, &numTokens);
    CgTokensCleanup tokCleanup(tokens, numTokens, tu);

    if (numTokens > 0) {
        std::vector<CXCursor> tokCurs(numTokens);
        clang_annotateTokens(tu, tokens, numTokens, tokCurs.data());
        for (unsigned i = 0; i < numTokens - 1; ++i) {
            processToken(
                *output.first, output.second, state, tokens[i], tokCurs[i]);
        }
    }
    std::cout << "Processed " << numTokens << " tokens in " << CgStr(clang_getFileName(cfile)).gets() << '\n';
    clang_findIncludesInFile(tu, cfile, {&state, &includeVisitor});
}

static int processTu(
    CXIndex cidx, MultiTuProcessor& state, char const* const* args, int nargs)
{
    CXTranslationUnit tu;
    CXErrorCode err = clang_parseTranslationUnit2(
        cidx,
        /*source_filename:*/ nullptr,
        args,
        nargs,
        /*unsaved_files:*/ nullptr,
        /*num_unsaved_files:*/ 0,
        CXTranslationUnit_DetailedPreprocessingRecord,
        &tu);
    if (err != CXError_Success) {
        std::cerr << "Failed parsing translation unit (code "
                  << static_cast<int>(err)
                  << ")\n";
        return err + 10;
    }

    CgTuHandle htu(tu);
    CXCursor rootcur = clang_getTranslationUnitCursor(tu);
    processRange(state, tu, clang_getCursorExtent(rootcur));
    //clang_visitChildren(rootcur, &tuVisitor, &state);
    return EXIT_SUCCESS;
}

static int executeCmdLine(CmdLineArgs const& args)
{
    CgIdxHandle hcidx(clang_createIndex(
            /*excludeDeclarationsFromPCH:*/ true,
            /*displayDiagnostics:*/ true));
    auto state(MultiTuProcessor::forRootdir(std::move(args.rootdir)));
    int r = processTu(hcidx.get(), state, args.firstClangArg, args.nClangArgs);
    if (r)
        return r;
    state.resolveMissingRefs();
    state.writeOutput(args.outdir);
    return r;
}

} // namespace synth

int main(int argc, char* argv[])
{
    try {
        return synth::executeCmdLine(synth::parseCmdLine(argc, argv));
    } catch (std::exception const& e) {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
