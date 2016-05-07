#include "output.hpp"

#include "config.hpp"

#include <boost/assert.hpp>
#include <boost/filesystem.hpp>
#include <boost/utility/string_ref.hpp>

#include <climits>
#include <utility>


using namespace synth;

bool Markup::empty() const
{
    return beginOffset == endOffset
        || (attrs == TokenAttributes::none && !isRef());
}

fs::path HighlightedFile::dstPath() const
{
    fs::path r = inOutDir->second / fname;
    r += ".html";
    return r;
}

static boost::string_ref htmlEscape(char const& c, bool inAttr)
{
    switch (c) {
        case '<':
            if (!inAttr)
                return "&lt;";
            break;
        // Note: '>' does not need to be escaped.
        case '&':
            return "&amp;";
        case '"':
            if (inAttr)
                return "&quot;";
            break;
    }
    return boost::string_ref(&c, 1);
}

static std::string htmlEscape(boost::string_ref s, bool inAttr)
{
    std::string r;
    r.reserve(s.size());
    for (char c : s) {
        boost::string_ref escaped = htmlEscape(c, inAttr);
        r.append(escaped.data(), escaped.size());
    }
    return r;
}

static char const* getTokenKindCssClass(TokenAttributes attrs)
{
    // These CSS classes are the ones Pygments uses.
    using Tk = TokenAttributes;
    SYNTH_DISCLANGWARN_BEGIN("-Wswitch-enum")
    switch(attrs & Tk::maskKind) {
        case Tk::attr: return "nd"; // Name.Decorator
        case Tk::cmmt: return "c"; // Comment
        case Tk::constant: return "no"; // Name.Constant
        case Tk::func: return "nf"; // Name.Function
        case Tk::kw: return "k"; // Keyword
        case Tk::kwDecl: return "kd"; // Keyword.Declaration
        case Tk::lbl: return "nl"; // Name.Label
        case Tk::lit: return "l"; // Literal
        case Tk::litChr: return "sc"; // String.Char
        case Tk::litKw: return "kc"; // Keyword.Constant
        case Tk::litNum: return "m"; // Number
        case Tk::litNumFlt: return "mf"; // Number.Float
        case Tk::litNumIntBin: return "mb"; // Number.Binary
        case Tk::litNumIntDecLong: return "ml"; // Number.Integer.Long
        case Tk::litNumIntHex: return "mh"; // Number.Hex
        case Tk::litNumIntOct: return "mo"; // Number.Oct
        case Tk::litStr: return "s"; // String
        case Tk::namesp: return "nn"; // Name.Namespace
        case Tk::op: return "o"; // Operator
        case Tk::opWord: return "ow"; // Operator.Word
        case Tk::pre: return "cp"; // Comment.Preproc
        case Tk::preIncludeFile: return "cpf"; // Comment.PreprocFile
        case Tk::punct: return "p"; // Punctuation
        case Tk::ty: return "nc"; // Name.Class
        case Tk::tyBuiltin: return "kt"; // Keyword.Type
        case Tk::varGlobal: return "vg"; // Name.Variable.Global
        case Tk::varLocal: return "nv"; // Name.Variable
        case Tk::varNonstaticMember: return "vi"; // Name.Variable.Instance
        case Tk::varStaticMember: return "vc"; // Name.Variable.Class
        case Tk::none: return "";

        default:
            assert(false && "Unexpected token kind!");
            return "";
    }
    SYNTH_DISCLANGWARN_END
}

static void writeCssClasses(TokenAttributes attrs, std::ostream& out)
{
    if ((attrs & TokenAttributes::flagDef) != TokenAttributes::none)
        out << "def ";
    if ((attrs & TokenAttributes::flagDecl) != TokenAttributes::none)
        out << "decl ";
    out << getTokenKindCssClass(attrs);
}

// return: The written tag was a reference.
static bool writeBeginTag(
    Markup const& m,
    fs::path const& outPath,
    MultiTuProcessor& multiTuProcessor,
    bool reopened,
    std::ostream& out)
{
    std::string href = m.isRef()
        ? m.refd(outPath, multiTuProcessor) : std::string();

    if (href.empty() && m.attrs == TokenAttributes::none)
        return false;

    out << '<';

    if (href.empty()) {
        out << "span";
    } else {
        href = htmlEscape(std::move(href), /*inAttr:*/ true);
        out << "a href=\"" << href << "\"";
    }

    if (m.attrs != TokenAttributes::none) {
        out << " class=\"";
        writeCssClasses(m.attrs, out);
        out << '\"';
    }
    
    if (!reopened && m.fileUniqueName && !m.fileUniqueName->empty()) {
        out << " id=\""
            << htmlEscape(*m.fileUniqueName, /*inAttr:*/ true) << '\"';
    }
    out << '>';

    return !href.empty();
}


namespace {

struct MarkupInfo {
    Markup const* markup;
    bool wasRef;
};

struct OutputState {
    std::istream& in;
    std::ostream& out;
    unsigned lineno;
    std::vector<MarkupInfo> const& activeTags;
    fs::path const& outPath;
    std::vector<std::pair<unsigned, unsigned>> const& disabledLines;
    std::size_t disabledLineIdx;
    MultiTuProcessor& multiTuProcessor;
};

} // anonymous namespace


static void writeEndTag(MarkupInfo const& mi, std::ostream& out)
{
    if (mi.wasRef)
        out << "</a>";
    else if (mi.markup->attrs != TokenAttributes::none)
        out << "</span>";
}

static void writeAllEnds(
    std::ostream& out, std::vector<MarkupInfo> const& activeTags)
{

    auto rit = activeTags.rbegin();
    auto rend = activeTags.rend();
    for (; rit != rend; ++rit)
        writeEndTag(*rit, out);
}

static void writeLineStart(OutputState& state)
{
    if (state.disabledLineIdx < state.disabledLines.size()
        && state.disabledLines[state.disabledLineIdx].first == state.lineno
    ) {
        state.out << "<div class=\"disabled-code\">";
    }
    state.out << "<span id=\"" << lineId(state.lineno) << "\" class=\"Ln\">";
}

static bool copyWithLinenosUntil(OutputState& state, unsigned offset)
{
    if (state.lineno == 0) {
        ++state.lineno;
        writeLineStart(state);
    }

    while (state.in && state.in.tellg() < offset) {
        int ch = state.in.get();
        if (ch == std::istream::traits_type::eof()) {
            state.out << "</span>\n"; // end tag for lineno.
            return false;
        } else {
            switch (ch) {
                case '\n': {
                    writeAllEnds(state.out, state.activeTags);
                    ++state.lineno;
                    state.out << "</span>";
                    if (state.disabledLineIdx < state.disabledLines.size()
                        && state.disabledLines[state.disabledLineIdx].second
                            == state.lineno
                    ) {
                        state.out << "</div>";
                        ++state.disabledLineIdx;
                    }
                    state.out << '\n';
                    writeLineStart(state);

                    for (auto const& mi : state.activeTags) {
                        writeBeginTag(
                            *mi.markup,
                            state.outPath,
                            state.multiTuProcessor,
                            /*reopened:*/ true,
                            state.out);
                    }
                } break;

                case '\r':
                    // Discard.
                    break;
                default:
                    state.out << htmlEscape(static_cast<char>(ch), false);
                    break;
            }
        }
    }
    return true;
}

static void copyWithLinenosUntilNoEof(OutputState& state, unsigned offset)
{
    bool eof = !copyWithLinenosUntil(state, offset);
    if (eof) {
        throw std::runtime_error(
            "unexpected EOF in input source " + state.outPath.string()
            + " at line " + std::to_string(state.lineno));
    }
}

// ORDER BY beginOffset ASC, endOffset DESC
static bool rangeLessThan(Markup const& lhs, Markup const& rhs)
{
    return lhs.beginOffset != rhs.beginOffset
        ? lhs.beginOffset < rhs.beginOffset
        : lhs.endOffset > rhs.endOffset;
}

static bool rangesOverlap(Markup const& lhs, Markup const& rhs)
{
    return lhs.beginOffset < rhs.endOffset
        && rhs.beginOffset < lhs.endOffset;
}

void synth::sortMarkups(std::vector<Markup>& markups)
{
    std::sort(markups.begin(), markups.end(), &rangeLessThan);
}

void synth::HighlightedFile::supplementMarkups(
    std::vector<Markup> const& supplementary)
{
    std::size_t i = 0;
    unsigned nextSupps = 0;
    for (auto const& supp : supplementary) {
        while (i < markups.size() && markups[i].endOffset < supp.beginOffset) {
            ++i;
            i += nextSupps;
            nextSupps = 0;
        }
        if (i >= markups.size()) {
            markups.push_back(supp);
        } else if (!rangesOverlap(supp, markups[i])) {
            bool const before = rangeLessThan(supp, markups[i]);
            // Silence clang warning.
            auto idiff = static_cast<std::vector<Markup>::difference_type>(i)
                + (before ? 0 : 1);
            markups.insert(markups.begin() + idiff, supp);
            if (before)
                ++i;
            else
                ++nextSupps;
        }
    }
}

void HighlightedFile::writeTo(
    std::ostream& out, MultiTuProcessor& multiTuProcessor, std::ifstream& selfIn) const
{
    fs::path outPath = dstPath();
    std::vector<MarkupInfo> activeTags;
    OutputState state {
        selfIn,
        out,
        0,
        activeTags,
        outPath,
        disabledLines,
        0,
        multiTuProcessor
    };
    for (auto const& m : markups) {
        while (!activeTags.empty()
            && m.beginOffset >= activeTags.back().markup->endOffset
        ) {
            MarkupInfo const& miEnd = activeTags.back();
            copyWithLinenosUntilNoEof(state, miEnd.markup->endOffset);
            writeEndTag(miEnd, out);
            activeTags.pop_back();
        }

        copyWithLinenosUntilNoEof(state, m.beginOffset);
        bool wasRef = writeBeginTag(
            m, state.outPath, state.multiTuProcessor, /*reopened:*/ false, out);
        activeTags.push_back({ &m, wasRef });
    }
    while (!activeTags.empty()) {
        copyWithLinenosUntilNoEof(state, activeTags.back().markup->endOffset);
        writeEndTag(activeTags.back(), out);
        activeTags.pop_back();
    }
    BOOST_VERIFY(!copyWithLinenosUntil(state, UINT_MAX));
}
