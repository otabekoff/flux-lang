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
    Void,
    Never,
    Unknown
};

struct Type {
    TypeKind kind;
    std::string name; // e.g. "Int32", "Float64", "Bool", "Color", etc.

    bool operator==(const Type& other) const {
        return kind == other.kind && name == other.name;
    }

    bool operator!=(const Type& other) const {
        return !(*this == other);
    }
};

inline Type unknown() {
    return {TypeKind::Unknown, "<unknown>"};
}
inline Type void_type() {
    return {TypeKind::Void, "Void"};
}

} // namespace flux::semantic

#endif // FLUX_TYPE_H
