#include "ir/ir_printer.h"

#include <sstream>

namespace flux::ir {

// ── Type formatting ─────────────────────────────────────────

std::string IRPrinter::type_to_string(const IRType& type) {
    switch (type.kind) {
    case IRTypeKind::Void:
        return "void";
    case IRTypeKind::Bool:
        return "bool";
    case IRTypeKind::I8:
        return "i8";
    case IRTypeKind::I16:
        return "i16";
    case IRTypeKind::I32:
        return "i32";
    case IRTypeKind::I64:
        return "i64";
    case IRTypeKind::I128:
        return "i128";
    case IRTypeKind::U8:
        return "u8";
    case IRTypeKind::U16:
        return "u16";
    case IRTypeKind::U32:
        return "u32";
    case IRTypeKind::U64:
        return "u64";
    case IRTypeKind::U128:
        return "u128";
    case IRTypeKind::F32:
        return "f32";
    case IRTypeKind::F64:
        return "f64";
    case IRTypeKind::F128:
        return "f128";
    case IRTypeKind::Never:
        return "never";
    case IRTypeKind::Ptr:
        if (type.pointee)
            return "&" + type_to_string(*type.pointee);
        return "&unknown";
    case IRTypeKind::Array:
        if (type.element_type)
            return "[" + type_to_string(*type.element_type) + "; " +
                   std::to_string(type.array_size) + "]";
        return "[?]";
    case IRTypeKind::Struct:
        return type.name.empty() ? "struct" : type.name;
    case IRTypeKind::Enum:
        return type.name.empty() ? "enum" : type.name;
    case IRTypeKind::Slice:
        return "&[" + (type.element_type ? type_to_string(*type.element_type) : "?") + "]";
    case IRTypeKind::Function:
        return "fn " + type.name;
    case IRTypeKind::Tuple: {
        std::string s = "(";
        for (size_t i = 0; i < type.field_types.size(); ++i) {
            if (i > 0)
                s += ", ";
            s += type_to_string(*type.field_types[i]);
        }
        s += ")";
        return s;
    }
    default:
        return "?";
    }
}

// ── Value formatting ────────────────────────────────────────

std::string IRPrinter::value_to_string(const Value& val) {
    if (val.is_constant) {
        if (std::holds_alternative<int64_t>(val.constant_value))
            return std::to_string(std::get<int64_t>(val.constant_value));
        if (std::holds_alternative<uint64_t>(val.constant_value))
            return std::to_string(std::get<uint64_t>(val.constant_value));
        if (std::holds_alternative<double>(val.constant_value)) {
            std::ostringstream oss;
            oss << std::get<double>(val.constant_value);
            return oss.str();
        }
        if (std::holds_alternative<bool>(val.constant_value))
            return std::get<bool>(val.constant_value) ? "true" : "false";
        if (std::holds_alternative<std::string>(val.constant_value))
            return "\"" + std::get<std::string>(val.constant_value) + "\"";
    }
    return val.name;
}

// ── Opcode formatting ───────────────────────────────────────

std::string IRPrinter::opcode_to_string(Opcode op) {
    switch (op) {
    case Opcode::Add:
        return "add";
    case Opcode::Sub:
        return "sub";
    case Opcode::Mul:
        return "mul";
    case Opcode::Div:
        return "div";
    case Opcode::Mod:
        return "mod";
    case Opcode::Neg:
        return "neg";
    case Opcode::BitAnd:
        return "and";
    case Opcode::BitOr:
        return "or";
    case Opcode::BitXor:
        return "xor";
    case Opcode::Shl:
        return "shl";
    case Opcode::Shr:
        return "shr";
    case Opcode::BitNot:
        return "not";
    case Opcode::Eq:
        return "eq";
    case Opcode::Ne:
        return "ne";
    case Opcode::Lt:
        return "lt";
    case Opcode::Le:
        return "le";
    case Opcode::Gt:
        return "gt";
    case Opcode::Ge:
        return "ge";
    case Opcode::LogicAnd:
        return "logic_and";
    case Opcode::LogicOr:
        return "logic_or";
    case Opcode::LogicNot:
        return "logic_not";
    case Opcode::Alloca:
        return "alloca";
    case Opcode::Load:
        return "load";
    case Opcode::Store:
        return "store";
    case Opcode::GetElementPtr:
        return "getelementptr";
    case Opcode::GetField:
        return "getfield";
    case Opcode::IntCast:
        return "intcast";
    case Opcode::FloatCast:
        return "floatcast";
    case Opcode::IntToFloat:
        return "int2float";
    case Opcode::FloatToInt:
        return "float2int";
    case Opcode::Bitcast:
        return "bitcast";
    case Opcode::Br:
        return "br";
    case Opcode::CondBr:
        return "condbr";
    case Opcode::Switch:
        return "switch";
    case Opcode::Ret:
        return "ret";
    case Opcode::Unreachable:
        return "unreachable";
    case Opcode::Call:
        return "call";
    case Opcode::CallIndirect:
        return "call_indirect";
    case Opcode::Phi:
        return "phi";
    case Opcode::InsertValue:
        return "insertvalue";
    case Opcode::ExtractValue:
        return "extractvalue";
    case Opcode::ArrayInit:
        return "arrayinit";
    case Opcode::StructInit:
        return "structinit";
    default:
        return "???";
    }
}

// ── Instruction printing ────────────────────────────────────

void IRPrinter::print_instruction(const Instruction& inst, std::ostream& os) const {
    os << "    ";

    switch (inst.opcode) {
    // ── Terminators ─────────────────────────────────────
    case Opcode::Br:
        os << "br %" << (inst.true_block ? inst.true_block->label : "?") << "\n";
        return;

    case Opcode::CondBr:
        os << "condbr " << value_to_string(*inst.operands[0]) << ", %"
           << (inst.true_block ? inst.true_block->label : "?") << ", %"
           << (inst.false_block ? inst.false_block->label : "?") << "\n";
        return;

    case Opcode::Ret:
        if (inst.operands.empty()) {
            os << "ret void\n";
        } else {
            os << "ret " << type_to_string(*inst.operands[0]->type) << " "
               << value_to_string(*inst.operands[0]) << "\n";
        }
        return;

    case Opcode::Unreachable:
        os << "unreachable\n";
        return;

    // ── Store (no result) ───────────────────────────────
    case Opcode::Store:
        os << "store " << type_to_string(*inst.operands[0]->type) << " "
           << value_to_string(*inst.operands[0]) << ", " << type_to_string(*inst.operands[1]->type)
           << " " << value_to_string(*inst.operands[1]) << "\n";
        return;

    // ── Alloca ──────────────────────────────────────────
    case Opcode::Alloca:
        os << value_to_string(*inst.result) << " = alloca " << type_to_string(*inst.type) << "\n";
        return;

    // ── Call ────────────────────────────────────────────
    case Opcode::Call:
        if (inst.result) {
            os << value_to_string(*inst.result) << " = ";
        }
        os << "call @" << inst.callee_name << "(";
        for (size_t i = 0; i < inst.operands.size(); ++i) {
            if (i > 0)
                os << ", ";
            os << type_to_string(*inst.operands[i]->type) << " "
               << value_to_string(*inst.operands[i]);
        }
        os << ")";
        if (inst.type)
            os << " -> " << type_to_string(*inst.type);
        os << "\n";
        return;

    // ── Phi ─────────────────────────────────────────────
    case Opcode::Phi:
        os << value_to_string(*inst.result) << " = phi " << type_to_string(*inst.type) << " ";
        for (size_t i = 0; i < inst.phi_incoming.size(); ++i) {
            if (i > 0)
                os << ", ";
            os << "[" << value_to_string(*inst.phi_incoming[i].first) << ", %"
               << inst.phi_incoming[i].second->label << "]";
        }
        os << "\n";
        return;

    // ── StructInit ──────────────────────────────────────
    case Opcode::StructInit:
        os << value_to_string(*inst.result) << " = structinit @" << inst.callee_name << " {";
        for (size_t i = 0; i < inst.operands.size(); ++i) {
            if (i > 0)
                os << ", ";
            os << value_to_string(*inst.operands[i]);
        }
        os << "}\n";
        return;

    // ── InsertValue / ExtractValue ──────────────────────
    case Opcode::InsertValue:
        os << value_to_string(*inst.result) << " = insertvalue "
           << value_to_string(*inst.operands[0]) << ", " << value_to_string(*inst.operands[1])
           << ", " << inst.field_index << "\n";
        return;

    case Opcode::ExtractValue:
        os << value_to_string(*inst.result) << " = extractvalue "
           << value_to_string(*inst.operands[0]) << ", " << inst.field_index << "\n";
        return;

    // ── GetField ────────────────────────────────────────
    case Opcode::GetField:
        os << value_to_string(*inst.result) << " = getfield " << value_to_string(*inst.operands[0])
           << ", " << inst.field_index << "\n";
        return;

    default:
        break;
    }

    // ── Generic binary/unary/cast instructions ──────────
    if (inst.result) {
        os << value_to_string(*inst.result) << " = ";
    }

    os << opcode_to_string(inst.opcode);

    if (inst.type) {
        os << " " << type_to_string(*inst.type);
    }

    for (size_t i = 0; i < inst.operands.size(); ++i) {
        os << " " << value_to_string(*inst.operands[i]);
        if (i + 1 < inst.operands.size())
            os << ",";
    }

    os << "\n";
}

// ── Block printing ──────────────────────────────────────────

void IRPrinter::print_block(const BasicBlock& bb, std::ostream& os) const {
    os << bb.label << ":";
    if (!bb.predecessors.empty()) {
        os << "  ; preds: ";
        for (size_t i = 0; i < bb.predecessors.size(); ++i) {
            if (i > 0)
                os << ", ";
            os << "%" << bb.predecessors[i]->label;
        }
    }
    os << "\n";

    for (const auto& inst : bb.instructions) {
        print_instruction(*inst, os);
    }
}

// ── Function printing ───────────────────────────────────────

void IRPrinter::print_function(const IRFunction& fn, std::ostream& os) const {
    os << "func @" << fn.name << "(";
    for (size_t i = 0; i < fn.params.size(); ++i) {
        if (i > 0)
            os << ", ";
        os << type_to_string(*fn.params[i]->type) << " " << fn.params[i]->name;
    }
    os << ")";
    if (fn.return_type && fn.return_type->kind != IRTypeKind::Void) {
        os << " -> " << type_to_string(*fn.return_type);
    }
    os << " {\n";

    for (const auto& bb : fn.blocks) {
        print_block(*bb, os);
    }

    os << "}\n";
}

// ── Module printing ─────────────────────────────────────────

void IRPrinter::print(const IRModule& module, std::ostream& os) const {
    os << "; Flux IR Module: " << module.name << "\n";
    os << "; Struct layouts: " << module.struct_layouts.size() << "\n";
    os << "; Functions: " << module.functions.size() << "\n\n";

    // Print struct layouts
    for (const auto& layout : module.struct_layouts) {
        os << "struct @" << layout.name << " {\n";
        for (size_t i = 0; i < layout.field_names.size(); ++i) {
            os << "    " << layout.field_names[i] << ": " << type_to_string(*layout.field_types[i])
               << "\n";
        }
        os << "}\n\n";
    }

    // Print functions
    for (const auto& fn : module.functions) {
        print_function(*fn, os);
        os << "\n";
    }
}

} // namespace flux::ir
