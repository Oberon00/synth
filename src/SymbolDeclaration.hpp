#ifndef SYNTH_SYMBOL_DECLARATION_HPP_INCLUDED
#define SYNTH_SYMBOL_DECLARATION_HPP_INCLUDED

#include <string>

namespace synth {

struct SymbolDeclaration {
    std::string usr;
    std::string const* filename;
    unsigned lineno;
};

}

#endif
