#include "HighlightedFile.hpp"
#include <utility>

synth::PrimitiveTag synth::Markup::begin_tag() const
{
    std::string content("<");
    content += tag;
    content += ' ';
    for (auto const& kv : attrs) {
        content += kv.first;
        content += "=\"";
        content += kv.second; // TODO? Escape?
        content += "\">";
    }
    return {begin_offset, content};
}

synth::PrimitiveTag synth::Markup::end_tag() const
{
    return {end_offset, std::string("<") + tag + ">"};
}

std::vector<synth::PrimitiveTag> synth::HighlightedFile::flatten() const
{
    std::vector<PrimitiveTag> r;
    std::vector<PrimitiveTag> pendingEnds;
    for (auto const& m : markups) {
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
