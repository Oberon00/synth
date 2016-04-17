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

} // namespace synth

#endif // SYNTH_XREF_HPP_INCLUDED
