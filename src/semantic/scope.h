#ifndef FLUX_SCOPE_H
#define FLUX_SCOPE_H

#include <unordered_map>
#include <string>
#include "symbol.h"

namespace flux::semantic {

    class Scope {
    public:
        explicit Scope(Scope* parent = nullptr)
            : parent_(parent) {}

        bool declare(const Symbol& symbol) {
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

        Scope* parent() const { return parent_; }

    private:
        Scope* parent_;
        std::unordered_map<std::string, Symbol> symbols_;
    };

} // namespace flux::semantic

#endif // FLUX_SCOPE_H
