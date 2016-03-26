#ifndef SYNTH_SYMBOL_DECLARATION_HPP_INCLUDED
#define SYNTH_SYMBOL_DECLARATION_HPP_INCLUDED

#include <string>

namespace synth {

struct SymbolDeclaration {
    std::string usr;
    std::string filename; // TODO Make reference to reduce memory consumption.
    unsigned lineno;
    bool isdef; // Is this declaration also a definition?
};

}

#endif
