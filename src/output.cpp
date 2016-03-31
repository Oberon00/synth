#include "output.hpp"
#include "config.hpp"
#include <utility>
#include <fstream>
#include <climits>
#include <iostream>
#include <boost/filesystem/operations.hpp>

using namespace synth;

static std::string relativeUrl(fs::path const& from, fs::path const& to)
{
    if (to == from)
        return std::string();
    fs::path r = fs::relative(to, from.parent_path());
    return r == "." ? std::string() : r.string() + ".html";
}

bool Markup::empty() const
{
    return begin_offset == end_offset
        || (attrs == TokenAttributes::none && !isRef());
}

void HighlightedFile::prepareOutput()
{
    // ORDER BY begin_offset ASC, end_offset DESC
    std::sort(
        markups.begin(), markups.end(),
        [] (Markup const& lhs, Markup const& rhs) {
            return lhs.begin_offset != rhs.begin_offset
                ? lhs.begin_offset < rhs.begin_offset
                : lhs.end_offset > rhs.end_offset;
        });
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

        default:
            assert(false && "Unexpected token kind!");
    }
    SYNTH_DISCLANGWARN_END
}

static void writeCssClasses(TokenAttributes attrs, std::ostream& out)
{
    if ((attrs & TokenAttributes::flagDef) != TokenAttributes::none)
        out << "def ";
    if ((attrs & TokenAttributes::flagDef) != TokenAttributes::none)
        out << "decl ";
    out << getTokenKindCssClass(attrs);
}

static void writeBeginTag(
    Markup const& m, fs::path const& srcPath, std::ostream& out)
{
    if (m.empty())
        return;
    out << '<';
    if (m.isRef()) {
        out << "a href=\"";
        out << relativeUrl(srcPath, *m.refd.filename);
        if (m.refd.lineno != 0)
            out << "#L" << m.refd.lineno;
        out << "\" ";
    } else {
        out << "span ";
    }

    if (m.attrs != TokenAttributes::none) {
        out << "class=\"";
        writeCssClasses(m.attrs, out);
        out << "\" ";
    }
    out << '>';
}

static void writeEndTag(Markup const& m, std::ostream& out)
{
    if (m.isRef())
        out << "</a>";
    else if (!m.empty())
        out << "</span>";
}

static void writeAllEnds(
    std::ostream& out, std::vector<Markup const*> const& activeTags)
{

    auto rit = activeTags.rbegin();
    auto rend = activeTags.rend();
    for (; rit != rend; ++rit)
        writeEndTag(**rit, out);
}

namespace {

struct OutputState {
    std::istream& in;
    std::ostream& out;
    unsigned lineno;
    std::vector<Markup const*> const& activeTags;
    fs::path const& srcPath;
};

} // anonymous namespace


static bool copyWithLinenosUntil(OutputState& state, unsigned offset)
{
    if (state.lineno == 0) {
        ++state.lineno;
        state.out << "<span id=\"L1\" class=\"Ln\">";
    }

    while (state.in && state.in.tellg() < offset) {
        int ch = state.in.get();
        if (ch == std::istream::traits_type::eof()) {
            state.out << "</span>\n";
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

                    for (auto const& m: state.activeTags)
                        writeBeginTag(*m, state.srcPath, state.out);
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
            "unexpected EOF in input source " + state.srcPath.string()
            + " at line " + std::to_string(state.lineno));
    }
}

void HighlightedFile::writeTo(std::ostream& out) const
{
    std::ifstream in(*originalPath);
    if (!in)
        throw std::runtime_error("Could not reopen source " + *originalPath);
    std::vector<Markup const*> activeTags;
    OutputState state {in, out, 0, activeTags, *originalPath};
    for (auto const& m : markups) {
        while (!activeTags.empty()
            && m.begin_offset >= activeTags.back()->end_offset
        ) {
            Markup const& mEnd = *activeTags.back();
            copyWithLinenosUntilNoEof(state, mEnd.end_offset);
            writeEndTag(mEnd, out);
            activeTags.pop_back();
        }

        copyWithLinenosUntilNoEof(state, m.begin_offset);
        writeBeginTag(m, state.srcPath, out);
        activeTags.push_back(&m);
    }
    bool eof = !copyWithLinenosUntil(state, UINT_MAX);
    assert(eof);
    writeAllEnds(out, activeTags);
}
