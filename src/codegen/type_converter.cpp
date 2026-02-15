#include "codegen/type_converter.h"
#include <stdexcept>

namespace flux::codegen {

TypeConverter::TypeConverter(LLVMContextRef context) : context(context) {}

LLVMTypeRef TypeConverter::convert(const ir::IRType& type) {
    switch (type.kind) {
    case ir::IRTypeKind::Void:
        return LLVMVoidTypeInContext(context);
    case ir::IRTypeKind::Bool:
        return LLVMInt1TypeInContext(context);
    case ir::IRTypeKind::I8:
        return LLVMInt8TypeInContext(context);
    case ir::IRTypeKind::I16:
        return LLVMInt16TypeInContext(context);
    case ir::IRTypeKind::I32:
        return LLVMInt32TypeInContext(context);
    case ir::IRTypeKind::I64:
        return LLVMInt64TypeInContext(context);
    case ir::IRTypeKind::F32:
        return LLVMFloatTypeInContext(context);
    case ir::IRTypeKind::F64:
        return LLVMDoubleTypeInContext(context);

    case ir::IRTypeKind::Ptr: {
        // LLVM 15+ uses opaque pointers
        return LLVMPointerTypeInContext(context, 0);
    }

    case ir::IRTypeKind::Array: {
        LLVMTypeRef elem = convert(*type.element_type);
        return LLVMArrayType(elem, static_cast<unsigned>(type.array_size));
    }

    default:
        throw std::runtime_error("TypeConverter: Unsupported IR type kind");
    }
}

} // namespace flux::codegen
