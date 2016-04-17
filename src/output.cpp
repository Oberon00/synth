#include "output.hpp"

#include "config.hpp"

#include <boost/assert.hpp>
#include <boost/filesystem.hpp>

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
        out << "a href=\"" << href << "\" ";
    }

    if (m.attrs != TokenAttributes::none) {
        out << "class=\"";
        writeCssClasses(m.attrs, out);
        out << "\" ";
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

static bool copyWithLinenosUntil(OutputState& state, unsigned offset)
{
    if (state.lineno == 0) {
        ++state.lineno;
        state.out << "<span id=\"L1\" class=\"Ln\">";
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
                    state.out
                        << "</span>\n<span id=\"L" 
                        << state.lineno
                        << "\" class=\"Ln\">";

                    for (auto const& mi : state.activeTags) {
                        writeBeginTag(
                            *mi.markup,
                            state.outPath,
                            state.multiTuProcessor,
                            state.out);
                    }
                } break;

                case '<':
                    state.out << "&lt;";
                    break;
                case '>':
                    state.out << "&gt;";
                    break;
                case '&':
                    state.out << "&amp;";
                    break;
                case '\r':
                    // Discard.
                    break;
                default:
                    state.out.put(static_cast<char>(ch));
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

void HighlightedFile::writeTo(
    std::ostream& out, MultiTuProcessor& multiTuProcessor)
{
    // ORDER BY beginOffset ASC, endOffset DESC
    std::sort(
        markups.begin(), markups.end(),
        [] (Markup const& lhs, Markup const& rhs) {
            return lhs.beginOffset != rhs.beginOffset
                ? lhs.beginOffset < rhs.beginOffset
                : lhs.endOffset > rhs.endOffset;
        });

    fs::path outPath = dstPath();
    fs::ifstream in(srcPath(), std::ios::binary);
    if (!in) {
        throw std::runtime_error(
            "Could not reopen source " + srcPath().string());
    }
    std::vector<MarkupInfo> activeTags;
    OutputState state {in, out, 0, activeTags, outPath, multiTuProcessor};
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
            m, state.outPath, state.multiTuProcessor, out);
        activeTags.push_back({ &m, wasRef });
    }
    BOOST_VERIFY(!copyWithLinenosUntil(state, UINT_MAX));
    writeAllEnds(out, activeTags);
}
