#pragma once
#ifndef FLUX_TYPE_H
#define FLUX_TYPE_H

#include <memory>
#include <string>
#include <vector>

namespace flux::semantic {

enum class TypeKind {
    Int,
    Float,
    Bool,
    String,
    Char,
    Enum,
    Struct,
    Ref,
    Tuple,
    Array,
    Slice,
    Function,
    Void,
    Never,
    Unknown,
    Option,
    Result,
    Generic,
};

struct FluxType {
    // For Option/Result generics
    std::vector<FluxType> generic_args;

    FluxType(TypeKind k = TypeKind::Unknown, std::string n = "<unknown>", bool is_mut = false,
             std::vector<FluxType> params = {}, std::unique_ptr<FluxType> ret = nullptr,
             std::vector<FluxType> generics = {})
        : generic_args(std::move(generics)), kind(k), name(std::move(n)), is_mut_ref(is_mut),
          param_types(std::move(params)), return_type(std::move(ret)) {}
    FluxType(const FluxType& other)
        : generic_args(other.generic_args), kind(other.kind), name(other.name),
          is_mut_ref(other.is_mut_ref), param_types(other.param_types),
          return_type(other.return_type ? std::make_unique<FluxType>(*other.return_type)
                                        : nullptr) {}

    FluxType& operator=(const FluxType& other) {
        if (this != &other) {
            kind = other.kind;
            name = other.name;
            is_mut_ref = other.is_mut_ref;
            param_types = other.param_types;
            return_type =
                other.return_type ? std::make_unique<FluxType>(*other.return_type) : nullptr;
            generic_args = other.generic_args;
        }
        return *this;
    }

    ~FluxType() = default;

    TypeKind kind;
    std::string name; // e.g. "Int32", "Float64", "Bool", "Color", etc.
    // For TypeKind::Ref, indicates if this is a mutable reference (&mut T)
    bool is_mut_ref = false;

    // For TypeKind::Function
    std::vector<FluxType> param_types;
    std::unique_ptr<FluxType> return_type;

    bool operator==(const FluxType& other) const {
        if (kind != other.kind || name != other.name || is_mut_ref != other.is_mut_ref)
            return false;
        if (generic_args.size() != other.generic_args.size())
            return false;
        for (size_t i = 0; i < generic_args.size(); ++i) {
            if (generic_args[i] != other.generic_args[i])
                return false;
        }
        if (kind == TypeKind::Function) {
            if (param_types.size() != other.param_types.size())
                return false;
            for (size_t i = 0; i < param_types.size(); ++i) {
                if (param_types[i] != other.param_types[i])
                    return false;
            }
            if (return_type && other.return_type)
                return *return_type == *other.return_type;
            return !return_type && !other.return_type;
        }
        return true;
    }

    bool operator!=(const FluxType& other) const {
        return !(*this == other);
    }
};

inline FluxType unknown() {
    return FluxType(TypeKind::Unknown, "<unknown>");
}
inline FluxType void_type() {
    return FluxType(TypeKind::Void, "Void");
}
inline FluxType never_type() {
    return FluxType(TypeKind::Never, "Never");
}

} // namespace flux::semantic

#endif // FLUX_TYPE_H
