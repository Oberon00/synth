#ifndef SYNTH_BASICHL_HPP_INCLUDED
#define SYNTH_BASICHL_HPP_INCLUDED

#include <iosfwd>
#include <vector>

namespace synth {

struct Markup;

void basicHighlightFile(std::istream& f, std::vector<Markup>& markups);

} // namespace synth

#endif // SYNTH_BASICHL_HPP_INCLUDED
