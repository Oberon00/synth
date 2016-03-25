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
    std::string tag;
    std::unordered_map<std::string, std::string> attrs;
    unsigned begin_offset;
    unsigned end_offset;
    
    PrimitiveTag begin_tag() const;
    PrimitiveTag end_tag() const;
};

struct HighlightedFile {
    std::string originalPath;
    std::vector<Markup> markups;
    
    std::vector<PrimitiveTag> flatten() const;
};    

}

#endif