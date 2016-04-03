#ifndef SYNTH_HIGHLIGHT_HPP_INCLUDED
#define SYNTH_HIGHLIGHT_HPP_INCLUDED

#include "output.hpp" // For TokenAttribues
#include <clang-c/Index.h>

namespace synth {

TokenAttributes getTokenAttributes(
    CXToken tok, CXCursor cur, char const* tokSpelling);

} // namespace synth

#endif // SYNTH_HIGHLIGHT_HPP_INCLUDED
