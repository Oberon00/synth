#ifndef SYNTH_XREF_HPP_INCLUDED
#define SYNTH_XREF_HPP_INCLUDED

#include <clang-c/Index.h>

namespace synth {

struct Markup;
class MultiTuProcessor;

void linkCursor(Markup& m, CXCursor mcur, MultiTuProcessor& state);

} // namespace synth

#endif // SYNTH_XREF_HPP_INCLUDED
