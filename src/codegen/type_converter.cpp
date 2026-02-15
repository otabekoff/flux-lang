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
        return LLVMPointerTypeInContext(context, 0);
    }

    case ir::IRTypeKind::Array: {
        LLVMTypeRef elem = convert(*type.element_type);
        return LLVMArrayType(elem, static_cast<unsigned>(type.array_size));
    }

    case ir::IRTypeKind::Struct:
    case ir::IRTypeKind::Tuple: {
        std::vector<LLVMTypeRef> fields;
        for (const auto& f : type.field_types) {
            fields.push_back(convert(*f));
        }
        return LLVMStructTypeInContext(context, fields.data(), static_cast<unsigned>(fields.size()),
                                       false);
    }

    case ir::IRTypeKind::Slice: {
        // Slice is { ptr, len }
        LLVMTypeRef fields[] = {LLVMPointerTypeInContext(context, 0),
                                LLVMInt64TypeInContext(context)};
        return LLVMStructTypeInContext(context, fields, 2, false);
    }

    case ir::IRTypeKind::Function: {
        std::vector<LLVMTypeRef> params;
        for (const auto& p : type.param_types) {
            params.push_back(convert(*p));
        }
        LLVMTypeRef ret = convert(*type.return_type);
        return LLVMFunctionType(ret, params.data(), static_cast<unsigned>(params.size()), false);
    }

    default:
        throw std::runtime_error("TypeConverter: Unsupported IR type kind");
    }
}

} // namespace flux::codegen
