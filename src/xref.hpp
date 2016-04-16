#ifndef SYNTH_XREF_HPP_INCLUDED
#define SYNTH_XREF_HPP_INCLUDED

#include <clang-c/Index.h>
#include <string>

namespace synth {

struct Markup;
class MultiTuProcessor;

void linkCursor(Markup& m, CXCursor mcur, MultiTuProcessor& state, bool isC);
std::string fileUniqueName(CXCursor cur, bool isC);
std::string simpleQualifiedName(CXCursor cur);

bool isNamespaceLevelDeclaration(CXCursor cur);

// A cursor is the main cursor for a declaration in its file iff:
//  - It is a definition; or
//  - It is the cannonical cursor (as defined by libclang) and the definition
//    is not in the same file (≠ translation unit).
bool isMainCursor(CXCursor cur);

} // namespace synth

#endif // SYNTH_XREF_HPP_INCLUDED
