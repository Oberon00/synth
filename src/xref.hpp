#ifndef SYNTH_XREF_HPP_INCLUDED
#define SYNTH_XREF_HPP_INCLUDED

#include <clang-c/Index.h>
#include <string>
#include <boost/filesystem/path.hpp>

namespace synth {

namespace fs = boost::filesystem;

struct Markup;
struct SourceLocation;
class MultiTuProcessor;

void linkCursorIfIncludedDst(
    Markup& m, CXCursor dst, unsigned srcLineno, MultiTuProcessor& state);

void linkSymbol(Markup& m, SourceLocation const& sym);

bool linkInclude(Markup& m, CXCursor incCursor, MultiTuProcessor& state);

} // namespace synth

#endif // SYNTH_XREF_HPP_INCLUDED
