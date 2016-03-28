#include "HighlightedFile.hpp"
#include <utility>
#include <fstream>
#include <climits>
#include <iostream>

using namespace synth;

PrimitiveTag Markup::begin_tag() const
{
    std::string content("<");
    content += tag;
    for (auto const& kv : attrs) {
        content += ' ';
        content += kv.first;
        content += "=\"";
        content += kv.second; // TODO? Escape?
        content += '"';
    }
    content += '>';
    return {begin_offset, content};
}

PrimitiveTag Markup::end_tag() const
{
    return {end_offset, std::string("</") + tag + ">"};
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

static void writeAllEnds(
    std::ostream& out, std::vector<Markup const*> const& activeTags)
{

    auto rit = activeTags.rbegin();
    auto rend = activeTags.rend();
    for (; rit != rend; ++rit)
        out << (*rit)->end_tag().content;
}

static bool copyWithLinenosUntil(
    std::istream& in,
    std::ostream& out,
    unsigned offset,
    unsigned& lineno,
    std::vector<Markup const*> const& activeTags)
{
    if (lineno == 0) {
        ++lineno;
        out << "<span id=\"L1\" class=\"Ln\">";
    }

    while (in && in.tellg() < offset) {
        int ch = in.get();
        if (ch == std::istream::traits_type::eof()) {
            out << "</span>\n";
            return false;
        } else {
            switch (ch) {
                case '\n': {
                    writeAllEnds(out, activeTags);
                    ++lineno;
                    out << "</span>\n<span id=\"L" 
                        << lineno
                        << "\" class=\"Ln\">";

                    for (auto const& m: activeTags)
                        out << m->begin_tag().content;
                } break;

                case '<':
                    out << "&lt;";
                    break;
                case '>':
                    out << "&gt;";
                    break;
                case '&':
                    out << "&amp;";
                    break;
                case '\r':
                    // Discard.
                    break;
                default:
                    out.put(static_cast<char>(ch));
                    break;
            }
        }
    }
    return true;
}

static void copyWithLinenosUntilNoEof(
    std::istream& in,
    std::ostream& out,
    unsigned offset,
    unsigned& lineno,
    std::vector<Markup const*> const& activeTags)
{
    bool eof = !copyWithLinenosUntil(in, out, offset, lineno, activeTags);
    if (eof) {
        throw std::runtime_error(
            "unexpected EOF in input source at line " + std::to_string(lineno));
    }
}

void HighlightedFile::writeTo(std::ostream& out) const
{
    std::ifstream in(originalPath);
    if (!in)
        throw std::runtime_error("Could not reopen source " + originalPath);
    std::vector<Markup const*> activeTags;
    unsigned lineno = 0;
    for (auto const& m : markups) {
        while (!activeTags.empty()
            && m.begin_offset >= activeTags.back()->end_offset
        ) {
            PrimitiveTag endTag = activeTags.back()->end_tag();
            copyWithLinenosUntilNoEof(
                in, out, endTag.offset, lineno, activeTags);
            activeTags.pop_back();
            out << endTag.content;
        }

        copyWithLinenosUntilNoEof(
            in, out, m.begin_offset, lineno, activeTags);
        out << m.begin_tag().content;
        activeTags.push_back(&m);
    }
    bool eof = !copyWithLinenosUntil(in, out, UINT_MAX, lineno, activeTags);
    assert(eof);
    writeAllEnds(out, activeTags);
}
