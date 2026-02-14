#ifndef FLUX_SYMBOL_H
#define FLUX_SYMBOL_H

#include "ast/ast.h"
#include <string>
#include <vector>

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
    bool is_moved = false;
    ast::Visibility visibility = ast::Visibility::None;
    std::string module_name;

    // For variables: declared type name (e.g. "Int32")
    // For functions: return type name (e.g. "Int32")
    std::string type; // return type

    // Only meaningful for functions:
    std::vector<std::string> param_types;
};
} // namespace flux::semantic

#endif // FLUX_SYMBOL_H
