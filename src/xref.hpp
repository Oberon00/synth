#ifndef SYNTH_XREF_HPP_INCLUDED
#define SYNTH_XREF_HPP_INCLUDED

#include <clang-c/Index.h>
#include <string>

namespace synth {

struct Markup;
struct SourceLocation;
class MultiTuProcessor;

void linkCursorIfIncludedDst(Markup& m, CXCursor dst, MultiTuProcessor& state);
void linkSymbol(Markup& m, SourceLocation const& sym);
bool linkInclude(Markup& m, CXCursor incCursor, MultiTuProcessor& state);

} // namespace synth

#endif // SYNTH_XREF_HPP_INCLUDED
