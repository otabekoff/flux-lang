#ifndef FLUX_IR_H
#define FLUX_IR_H

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace flux::ir {

// ============================================================
//  IR Types — simplified type system for lowered representation
// ============================================================

enum class IRTypeKind {
    Void,
    Bool,
    I8,
    I16,
    I32,
    I64,
    I128,
    U8,
    U16,
    U32,
    U64,
    U128,
    F32,
    F64,
    F128,
    Ptr,      // pointer to another type
    Struct,   // aggregate with named fields
    Enum,     // tagged union
    Array,    // fixed-size [T; N]
    Slice,    // fat pointer (ptr + len)
    Function, // function pointer type
    Tuple,    // anonymous product type
    Never,    // diverging (no value)
};

struct IRType {
    IRTypeKind kind = IRTypeKind::Void;
    std::string name;

    // For Ptr — the pointee type
    std::shared_ptr<IRType> pointee;

    // For Struct/Tuple — field types
    std::vector<std::shared_ptr<IRType>> field_types;
    std::vector<std::string> field_names; // empty for tuples

    // For Array — element type and size
    std::shared_ptr<IRType> element_type;
    uint64_t array_size = 0;

    // For Function — param types and return type
    std::vector<std::shared_ptr<IRType>> param_types;
    std::shared_ptr<IRType> return_type;

    // For Enum — variant info
    struct EnumVariant {
        std::string name;
        std::vector<std::shared_ptr<IRType>> payload_types;
    };
    std::vector<EnumVariant> variants;

    bool operator==(const IRType& other) const {
        return kind == other.kind && name == other.name;
    }
    bool operator!=(const IRType& other) const {
        return !(*this == other);
    }

    bool is_integer() const {
        return kind >= IRTypeKind::I8 && kind <= IRTypeKind::U128;
    }
    bool is_signed_integer() const {
        return kind >= IRTypeKind::I8 && kind <= IRTypeKind::I128;
    }
    bool is_unsigned_integer() const {
        return kind >= IRTypeKind::U8 && kind <= IRTypeKind::U128;
    }
    bool is_float() const {
        return kind >= IRTypeKind::F32 && kind <= IRTypeKind::F128;
    }
    bool is_numeric() const {
        return is_integer() || is_float();
    }
};

// Convenience constructors
inline std::shared_ptr<IRType> make_void() {
    return std::make_shared<IRType>(IRTypeKind::Void, "Void");
}
inline std::shared_ptr<IRType> make_bool() {
    return std::make_shared<IRType>(IRTypeKind::Bool, "Bool");
}
inline std::shared_ptr<IRType> make_i8() {
    return std::make_shared<IRType>(IRTypeKind::I8, "Int8");
}
inline std::shared_ptr<IRType> make_i16() {
    return std::make_shared<IRType>(IRTypeKind::I16, "Int16");
}
inline std::shared_ptr<IRType> make_i32() {
    return std::make_shared<IRType>(IRTypeKind::I32, "Int32");
}
inline std::shared_ptr<IRType> make_i64() {
    return std::make_shared<IRType>(IRTypeKind::I64, "Int64");
}
inline std::shared_ptr<IRType> make_i128() {
    return std::make_shared<IRType>(IRTypeKind::I128, "Int128");
}
inline std::shared_ptr<IRType> make_u8() {
    return std::make_shared<IRType>(IRTypeKind::U8, "UInt8");
}
inline std::shared_ptr<IRType> make_u16() {
    return std::make_shared<IRType>(IRTypeKind::U16, "UInt16");
}
inline std::shared_ptr<IRType> make_u32() {
    return std::make_shared<IRType>(IRTypeKind::U32, "UInt32");
}
inline std::shared_ptr<IRType> make_u64() {
    return std::make_shared<IRType>(IRTypeKind::U64, "UInt64");
}
inline std::shared_ptr<IRType> make_u128() {
    return std::make_shared<IRType>(IRTypeKind::U128, "UInt128");
}
inline std::shared_ptr<IRType> make_f32() {
    return std::make_shared<IRType>(IRTypeKind::F32, "Float32");
}
inline std::shared_ptr<IRType> make_f64() {
    return std::make_shared<IRType>(IRTypeKind::F64, "Float64");
}
inline std::shared_ptr<IRType> make_f128() {
    return std::make_shared<IRType>(IRTypeKind::F128, "Float128");
}
inline std::shared_ptr<IRType> make_never() {
    return std::make_shared<IRType>(IRTypeKind::Never, "Never");
}

inline std::shared_ptr<IRType> make_ptr(std::shared_ptr<IRType> pointee) {
    auto t = std::make_shared<IRType>();
    t->kind = IRTypeKind::Ptr;
    t->name = "&" + pointee->name;
    t->pointee = std::move(pointee);
    return t;
}

inline std::shared_ptr<IRType> make_array(std::shared_ptr<IRType> elem, uint64_t size) {
    auto t = std::make_shared<IRType>();
    t->kind = IRTypeKind::Array;
    t->name = "[" + elem->name + "; " + std::to_string(size) + "]";
    t->element_type = std::move(elem);
    t->array_size = size;
    return t;
}

// ============================================================
//  SSA Values
// ============================================================

using ValueID = uint32_t;

struct Value {
    ValueID id = 0;
    std::shared_ptr<IRType> type;
    std::string name; // optional debug name (e.g. "x", "tmp")

    bool is_constant = false;

    // Constant storage (union-like via variant)
    std::variant<std::monostate, int64_t, uint64_t, double, bool, std::string> constant_value;
};

using ValuePtr = std::shared_ptr<Value>;

inline ValuePtr make_const_i32(int32_t v) {
    auto val = std::make_shared<Value>();
    val->type = make_i32();
    val->is_constant = true;
    val->constant_value = static_cast<int64_t>(v);
    return val;
}

inline ValuePtr make_const_i64(int64_t v) {
    auto val = std::make_shared<Value>();
    val->type = make_i64();
    val->is_constant = true;
    val->constant_value = v;
    return val;
}

inline ValuePtr make_const_f64(double v) {
    auto val = std::make_shared<Value>();
    val->type = make_f64();
    val->is_constant = true;
    val->constant_value = v;
    return val;
}

inline ValuePtr make_const_bool(bool v) {
    auto val = std::make_shared<Value>();
    val->type = make_bool();
    val->is_constant = true;
    val->constant_value = v;
    return val;
}

inline ValuePtr make_const_string(std::string v) {
    auto val = std::make_shared<Value>();
    auto t = std::make_shared<IRType>();
    t->kind = IRTypeKind::Ptr;
    t->name = "&String";
    val->type = std::move(t);
    val->is_constant = true;
    val->constant_value = std::move(v);
    return val;
}

// ============================================================
//  Instructions (opcodes)
// ============================================================

enum class Opcode {
    // Arithmetic
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Neg,

    // Bitwise
    BitAnd,
    BitOr,
    BitXor,
    Shl,
    Shr,
    BitNot,

    // Comparison
    Eq,
    Ne,
    Lt,
    Le,
    Gt,
    Ge,

    // Logical
    LogicAnd,
    LogicOr,
    LogicNot,

    // Memory
    Alloca,        // allocate stack memory for a local variable
    Load,          // load from pointer
    Store,         // store to pointer
    GetElementPtr, // address of array/struct element
    GetField,      // address of struct field by index

    // Type conversion
    IntCast,    // integer widening/narrowing
    FloatCast,  // float widening/narrowing
    IntToFloat, // int → float
    FloatToInt, // float → int
    Bitcast,    // reinterpret bits

    // Control flow
    Br,          // unconditional branch
    CondBr,      // conditional branch (cond, true_bb, false_bb)
    Switch,      // multi-way branch
    Ret,         // return from function
    Unreachable, // control never reaches here

    // Calls
    Call,         // direct function call
    CallIndirect, // indirect call through function pointer

    // SSA
    Phi, // phi node for merging values from predecessors

    // Aggregates
    InsertValue,  // insert value into tuple/struct aggregate
    ExtractValue, // extract value from tuple/struct aggregate
    ArrayInit,    // initialize array with values
    StructInit,   // initialize struct with field values
};

// ============================================================
//  Instruction
// ============================================================

struct BasicBlock; // forward declaration

struct Instruction {
    Opcode opcode;
    ValuePtr result;                // SSA result (nullptr for void ops like Store, Br)
    std::vector<ValuePtr> operands; // input values
    std::shared_ptr<IRType> type;   // result type

    // For Br/CondBr/Switch
    BasicBlock* true_block = nullptr;
    BasicBlock* false_block = nullptr;
    std::vector<std::pair<ValuePtr, BasicBlock*>> switch_cases; // for Switch

    // For Call
    std::string callee_name;

    // For GetField / InsertValue / ExtractValue
    uint32_t field_index = 0;

    // For Phi
    std::vector<std::pair<ValuePtr, BasicBlock*>> phi_incoming;

    // Source location for debugging
    uint32_t line = 0;
    uint32_t column = 0;
};

using InstructionPtr = std::unique_ptr<Instruction>;

// ============================================================
//  Basic Block
// ============================================================

struct BasicBlock {
    std::string label;
    std::vector<InstructionPtr> instructions;

    // CFG edges
    std::vector<BasicBlock*> predecessors;
    std::vector<BasicBlock*> successors;

    bool is_terminated() const {
        if (instructions.empty())
            return false;
        auto op = instructions.back()->opcode;
        return op == Opcode::Br || op == Opcode::CondBr || op == Opcode::Switch ||
               op == Opcode::Ret || op == Opcode::Unreachable;
    }
};

using BasicBlockPtr = std::unique_ptr<BasicBlock>;

// ============================================================
//  IR Function
// ============================================================

struct IRFunction {
    std::string name;
    std::vector<ValuePtr> params;
    std::shared_ptr<IRType> return_type;
    std::vector<BasicBlockPtr> blocks;
    BasicBlock* entry = nullptr;

    bool is_async = false;
    bool is_external = false;

    // Source location
    uint32_t line = 0;
    uint32_t column = 0;

    BasicBlock* create_block(const std::string& label) {
        auto bb = std::make_unique<BasicBlock>();
        bb->label = label;
        auto* ptr = bb.get();
        blocks.push_back(std::move(bb));
        if (!entry)
            entry = ptr;
        return ptr;
    }
};

using IRFunctionPtr = std::unique_ptr<IRFunction>;

// ============================================================
//  Struct Layout (for codegen)
// ============================================================

struct StructLayout {
    std::string name;
    std::vector<std::string> field_names;
    std::vector<std::shared_ptr<IRType>> field_types;
};

// ============================================================
//  IR Module (top-level container)
// ============================================================

struct IRModule {
    std::string name;
    std::vector<IRFunctionPtr> functions;
    std::vector<StructLayout> struct_layouts;
    std::unordered_map<std::string, ValuePtr> global_constants;

    IRFunction* find_function(const std::string& fname) const {
        for (auto& fn : functions) {
            if (fn->name == fname)
                return fn.get();
        }
        return nullptr;
    }
};

} // namespace flux::ir

#endif // FLUX_IR_H
