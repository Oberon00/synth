#include "annotate.hpp"

#include "CgStr.hpp"
#include "MultiTuProcessor.hpp"
#include "cgWrappers.hpp"
#include "FileIdSupport.hpp"
#include "highlight.hpp"
#include "output.hpp"
#include "xref.hpp"

#include <boost/assert.hpp>

#include <climits>
#include <iostream>

using namespace synth;

// This is needed for macro arguments: clang_equalLocations is only true if two
// locations are truly equal. That is if either of spelling-, source- or
// file-location is different, it returns false. However we are only interested
// in the file location, hence this function.
// We must work with the offset here because it is in fact often different (and
// more "correct" for our purposes) from what line:column would suggest. See
// also comment inside processToken().
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
    bool lnkPending;
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

static FileAnnotationState* lookupFileAnnotations(TuAnnotationMap& m, CXFile f)
{
    CXFileUniqueID fuid;
    if (!f || clang_getFileUniqueID(f, &fuid) != 0)
        return nullptr;
    auto it = m.find(fuid);
    return it == m.end() ? nullptr : &it->second;
}

} // anonymous namespace

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

    CgStr hsp(clang_getTokenSpelling(tu, tok));
    boost::string_ref sp = hsp.gets();
    m->attrs = getTokenAttributes(tok, cur, sp);

    CXTokenKind tk = clang_getTokenKind(tok);
    if (tk == CXToken_Comment || tk == CXToken_Literal)
        return;

    CXCursorKind k = clang_getCursorKind(cur);
    if (state.lnkPending) {
        if (tk == CXToken_Punctuation && (sp == "(" || sp == "[")) {
            // This is the "("/"[" of an operator overload and we also want
            // to highlight the closing ")"/"]".
            return;
        }

        state.lnkPending = false;
        Markup lnk = {};
        lnk.beginOffset = getLocOffset(clang_getCursorLocation(cur));
        lnk.endOffset = m->endOffset;
        markups.push_back(std::move(lnk));
        m = &markups.back();
    }
    else if (!equalFileLocations(
        clang_getRangeStart(rng), clang_getCursorLocation(cur))
    ) {
        // Note that there is magic in the offset with which equalFileLocations
        // works (and clang_equalLocations too); it is sometimes different from
        // what line:column would suggest. Otherwise this condition would not
        // correctly work because e.g. for a function with built-in return type,
        // the cursor location of the FunctionDecl starts at the return type
        // declaration according to line:column, but at the function name (or
        // operator keyword or dtor tilde) according to the offset.
        return;
    }
    else if (k == CXCursor_InclusionDirective) {
        Markup incLnk = {};
        CXSourceRange incrng = clang_getCursorExtent(cur);
        incLnk.beginOffset = getLocOffset(clang_getRangeStart(incrng));
        incLnk.endOffset = getLocOffset(clang_getRangeEnd(incrng));
        linkCursor(incLnk, cur, state.multiTuProcessor);
        if (incLnk.isRef())
            state.hlFile.markups.push_back(std::move(incLnk));
        return;
    }
    else if (k == CXCursor_Destructor) {
        // This is the "~" of a dtor. Include the next part in the link.
        state.lnkPending = true;
        return;
    }
    else if (tk == CXToken_Keyword
        && (k == CXCursor_FunctionDecl || k == CXCursor_CXXMethod)
        && sp == "operator"
    ) {
        state.lnkPending = true;
        return;
    }

    SymbolDeclaration const* decl = nullptr;
    auto const loadDecl = [&]() {
        if (decl)
            return;
        decl = state.multiTuProcessor.referenceSymbol(
            &state.hlFile,
            lineno,
            m->beginOffset,
            [&cur]() { return fileUniqueName(cur); });

        // This dereference is safe even if other threads modify decl since it
        // is basically just pointer arithmetic.
        m->fileUniqueName = &decl->fileUniqueName;
    };

    if (clang_isDeclaration(k)) {
        m->attrs |= TokenAttributes::flagDecl;
        if (isMainCursor(cur))
            loadDecl();
    }

    CXCursor defcur = clang_getCursorDefinition(cur);
    if (clang_equalCursors(defcur, cur)) { // This is a definition:
        m->attrs |= TokenAttributes::flagDef;
        loadDecl();
        CgStr usr(clang_getCursorUSR(cur));
        if (!usr.empty())
            state.multiTuProcessor.registerDef(usr.get(), decl);
    }

    linkCursor(*m, cur, state.multiTuProcessor);
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
