#ifndef SYNTH_DEBUG_HPP_INCLUDED
#define SYNTH_DEBUG_HPP_INCLUDED

#include <clang-c/Index.h>
#include <iosfwd>

namespace synth {

void writeIndent(int ind);
void dumpSingleCursor(CXCursor c, int ind);
void dumpAst(CXCursor c, int ind);
std::ostream& operator<< (std::ostream& out, CXCursor c);
std::ostream& operator<< (std::ostream& out, CXSourceRange rng);

} // namespace synth

#endif // SYNTH_DEBUG_HPP_INCLUDED
