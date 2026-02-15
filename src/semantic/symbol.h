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
    bool is_initialized = false;
    uint32_t borrow_count = 0;
    bool is_mutably_borrowed = false;
    std::string borrowed_symbol_name;
    uint32_t scope_depth = 0;
    ast::Visibility visibility = ast::Visibility::None;
    bool is_async = false;
    std::string module_name;

    // For variables: declared type name (e.g. "Int32")
    // For functions: return type name (e.g. "Int32")
    std::string type; // return type

    // Only meaningful for functions:
    std::vector<std::string> param_types;

    Symbol() = default;
    Symbol(std::string name, SymbolKind kind, bool mut = false, bool is_const = false,
           bool moved = false, bool initialized = false,
           ast::Visibility vis = ast::Visibility::None, std::string mod = "", std::string t = "",
           std::vector<std::string> params = {}, bool async_fn = false)
        : name(std::move(name)), kind(kind), is_mutable(mut), is_const(is_const), is_moved(moved),
          is_initialized(initialized), borrow_count(0), is_mutably_borrowed(false),
          borrowed_symbol_name(""), scope_depth(0), visibility(vis), is_async(async_fn),
          module_name(std::move(mod)), type(std::move(t)), param_types(std::move(params)) {}
};
} // namespace flux::semantic

#endif // FLUX_SYMBOL_H
