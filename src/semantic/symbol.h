#ifndef FLUX_SYMBOL_H
#define FLUX_SYMBOL_H

#include <string>

namespace flux::semantic {
enum class SymbolKind {
    Variable,
    Function,
};

struct Symbol {
    std::string name;
    SymbolKind kind;
    bool is_mutable = false;
    bool is_const = false;

    // For variables: declared type name (e.g. "Int32")
    // For functions: return type name (e.g. "Int32")
    std::string type; // return type

    // Only meaningful for functions:
    std::vector<std::string> param_types;
};
} // namespace flux::semantic

#endif // FLUX_SYMBOL_H
