#ifndef SYNTH_ANNOTATE_HPP_INCLUDED
#define SYNTH_ANNOTATE_HPP_INCLUDED

#include <clang-c/Index.h>

namespace synth {

class MultiTuProcessor;

int processTu(
    CXIndex cidx, MultiTuProcessor& state, char const* const* args, int nargs);

} // namespace synth

#endif // SYNTH_ANNOTATE_HPP_INCLUDED
