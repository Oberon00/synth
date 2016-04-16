#ifndef SYNTH_HIGHLIGHT_HPP_INCLUDED
#define SYNTH_HIGHLIGHT_HPP_INCLUDED

#include "output.hpp" // For TokenAttribues

#include <boost/utility/string_ref.hpp>
#include <clang-c/Index.h>

namespace synth {

TokenAttributes getTokenAttributes(
    CXToken tok, CXCursor cur, boost::string_ref tokSpelling);

// This functions always return false for
// MemberRef, DeclRefExpr, MemberRefExpr, UsingDeclaration, TemplateRef.
bool isTypeCursorKind(CXCursorKind k);

// Note that this only returns true for function declarations and
// OverloadedDeclRef.
bool isFunctionCursorKind(CXCursorKind k);

} // namespace synth

#endif // SYNTH_HIGHLIGHT_HPP_INCLUDED
