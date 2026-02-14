#ifndef FLUX_SCOPE_H
#define FLUX_SCOPE_H

#include "symbol.h"
#include <string>
#include <unordered_map>

namespace flux::semantic {

class Scope {
  public:
    explicit Scope(Scope* parent = nullptr, uint32_t depth = 0) : parent_(parent), depth_(depth) {}
    uint32_t depth() const {
        return depth_;
    }

    bool declare(Symbol symbol) {
        symbol.scope_depth = depth_;
        auto [_, inserted] = symbols_.emplace(symbol.name, symbol);
        return inserted;
    }

    const Symbol* lookup(const std::string& name) const {
        auto it = symbols_.find(name);
        if (it != symbols_.end())
            return &it->second;

        if (parent_)
            return parent_->lookup(name);

        return nullptr;
    }

    Symbol* lookup_mut(const std::string& name) {
        auto it = symbols_.find(name);
        if (it != symbols_.end())
            return &it->second;

        if (parent_)
            return parent_->lookup_mut(name);

        return nullptr;
    }

    Scope* parent() const {
        return parent_;
    }

    const std::unordered_map<std::string, Symbol>& get_symbols() const {
        return symbols_;
    }
    std::unordered_map<std::string, Symbol>& get_symbols_mut() {
        return symbols_;
    }

  private:
    Scope* parent_;
    uint32_t depth_;
    std::unordered_map<std::string, Symbol> symbols_;
};

} // namespace flux::semantic

#endif // FLUX_SCOPE_H
