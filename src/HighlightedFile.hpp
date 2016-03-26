#ifndef SYNTH_HIGHLIGTHED_FILE_HPP_INCLUDED
#define SYNTH_HIGHLIGTHED_FILE_HPP_INCLUDED

#include <string>
#include <vector>
#include <unordered_map>

namespace synth {

struct PrimitiveTag {
    unsigned offset;
    std::string content;
};

struct Markup {
    char const* tag;
    std::unordered_map<std::string, std::string> attrs;
    unsigned begin_offset;
    unsigned end_offset;

    PrimitiveTag begin_tag() const;
    PrimitiveTag end_tag() const;

    static constexpr char const* kTagSpan = "span";
    static constexpr char const* kTagLink = "a";
};

struct HighlightedFile {
    std::string originalPath;
    std::vector<Markup> markups;

    std::vector<PrimitiveTag> flatten() const;
};

}

#endif
