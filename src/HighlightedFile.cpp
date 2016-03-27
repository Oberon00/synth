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

static std::vector<PrimitiveTag> flatten(
    std::vector<Markup> const& sortedMarkups)
{
    std::vector<PrimitiveTag> r;
    std::vector<PrimitiveTag> pendingEnds;
    for (auto const& m : sortedMarkups) {
        while (!pendingEnds.empty()
            && m.begin_offset >= pendingEnds.back().offset
        ) {
            r.push_back(std::move(pendingEnds.back()));
            pendingEnds.pop_back();
        }
        r.push_back(m.begin_tag());
        pendingEnds.push_back(std::move(m).end_tag());
    }

    auto rit = pendingEnds.rbegin();
    auto rend = pendingEnds.rend();
    for (; rit != rend; ++rit)
        r.push_back(*rit);

    return r;
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

static bool copyWithLinenosUntil(
    std::istream& in, std::ostream& out, unsigned& lineno, unsigned offset)
{
    if (lineno == 0) {
        ++lineno;
        out << "<a id=\"L1\"></a>";
    }
    while (in && in.tellg() < offset) {
        int ch = in.get();
        if (ch == std::istream::traits_type::eof()) {
            return false;
        } else {
            switch (ch) {
                case '\n':
                    ++lineno;
                    out << "\n<a id=\"L" << std::to_string(lineno) << "\"></a>";
                    break;
                case '<':
                    out << "&lt;";
                    break;
                case '>':
                    out << "&gt;";
                    break;
                case '&':
                    out << "&amp;";
                    break;
                default:
                    out.put(static_cast<char>(ch));
            }
        }
    }
    return true;
}

void HighlightedFile::writeTo(std::ostream& out) const
{
    std::ifstream in(originalPath);
    if (!in)
        throw std::runtime_error("Could not reopen source " + originalPath);
    auto hls = flatten(markups);
    unsigned lineno = 0;
    bool eof;
    for (auto const& hl : hls) {
        eof = !copyWithLinenosUntil(in, out, lineno, hl.offset);
        if (eof) {
            std::cerr << "target offset " << hl.offset << " for " << hl.content << "\n";
            throw std::runtime_error("unexpected EOF in " + originalPath);
        }
        out << hl.content;
    }
    eof = !copyWithLinenosUntil(in, out, lineno, UINT_MAX);
    assert(eof);
}
