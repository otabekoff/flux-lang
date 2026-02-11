#ifndef FLUX_TYPE_H
#define FLUX_TYPE_H

#include <string>

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
};

struct Type {
    // For Option/Result generics
    std::vector<Type> generic_args;

    Type(TypeKind k = TypeKind::Unknown, std::string n = "<unknown>", bool is_mut = false,
         std::vector<Type> params = {}, std::unique_ptr<Type> ret = nullptr,
         std::vector<Type> generics = {})
        : kind(k), name(std::move(n)), is_mut_ref(is_mut), param_types(std::move(params)),
          return_type(std::move(ret)), generic_args(std::move(generics)) {}
    Type(const Type& other)
        : kind(other.kind), name(other.name), is_mut_ref(other.is_mut_ref),
          param_types(other.param_types),
          return_type(other.return_type ? std::make_unique<Type>(*other.return_type) : nullptr),
          generic_args(other.generic_args) {}

    Type& operator=(const Type& other) {
        if (this != &other) {
            kind = other.kind;
            name = other.name;
            is_mut_ref = other.is_mut_ref;
            param_types = other.param_types;
            return_type = other.return_type ? std::make_unique<Type>(*other.return_type) : nullptr;
            generic_args = other.generic_args;
        }
        return *this;
    }
    TypeKind kind;
    std::string name; // e.g. "Int32", "Float64", "Bool", "Color", etc.
    // For TypeKind::Ref, indicates if this is a mutable reference (&mut T)
    bool is_mut_ref = false;

    // For TypeKind::Function
    std::vector<Type> param_types;
    std::unique_ptr<Type> return_type;

    bool operator==(const Type& other) const {
        if (kind != other.kind || name != other.name || is_mut_ref != other.is_mut_ref)
            return false;
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

    bool operator!=(const Type& other) const {
        return !(*this == other);
    }
};

inline Type unknown() {
    return Type(TypeKind::Unknown, "<unknown>");
}
inline Type void_type() {
    return Type(TypeKind::Void, "Void");
}
inline Type never_type() {
    return Type(TypeKind::Never, "Never");
}

} // namespace flux::semantic

#endif // FLUX_TYPE_H
