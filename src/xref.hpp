#ifndef SYNTH_XREF_HPP_INCLUDED
#define SYNTH_XREF_HPP_INCLUDED

#include <clang-c/Index.h>
#include <string>
#include <boost/filesystem/path.hpp>

namespace synth {

namespace fs = boost::filesystem;

struct Markup;
struct SymbolDeclaration;
class MultiTuProcessor;


void linkCursorIfIncludedDst(
    Markup& m,
    CXCursor dst,
    fs::path const& srcurl,
    unsigned srcLineno,
    MultiTuProcessor const& state,
    bool byUsr);

void linkSymbol(
    Markup& m,
    SymbolDeclaration const& sym,
    fs::path const& srcurl);

bool linkInclude(
    Markup& m,
    CXCursor incCursor,
    fs::path const& srcurl,
    MultiTuProcessor const& state);

} // namespace synth

#endif // SYNTH_XREF_HPP_INCLUDED
