#ifndef FLUX_TYPE_CONVERTER_H
#define FLUX_TYPE_CONVERTER_H

#include "ir/ir.h"
#include <llvm-c/Core.h>

namespace flux::codegen {

class TypeConverter {
  public:
    explicit TypeConverter(LLVMContextRef context);

    LLVMTypeRef convert(const ir::IRType& type);

  private:
    LLVMContextRef context;

    LLVMTypeRef convert_kind(ir::IRTypeKind kind);
};

} // namespace flux::codegen

#endif // FLUX_TYPE_CONVERTER_H
