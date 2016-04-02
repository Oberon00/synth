#ifndef SYNTH_DEBUG_HPP_INCLUDED
#define SYNTH_DEBUG_HPP_INCLUDED

#include <clang-c/Index.h>
#include <iosfwd>

namespace synth {

void writeIndent(int ind);

// If the locatio of the cursor is in f, no filename is printed.
void dumpSingleCursor(CXCursor c, int ind = 0, CXFile f = nullptr);
void dumpAst(CXCursor c, int ind = 0);
void writeLoc(std::ostream& out, CXSourceLocation loc, CXFile f = nullptr);
void writeExtent(std::ostream& out, CXSourceRange rng, CXFile f = nullptr);
std::ostream& operator<< (std::ostream& out, CXCursor c);
std::ostream& operator<< (std::ostream& out, CXSourceRange rng);

} // namespace synth

#endif // SYNTH_DEBUG_HPP_INCLUDED
