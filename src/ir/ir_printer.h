#ifndef FLUX_IR_PRINTER_H
#define FLUX_IR_PRINTER_H

#include "ir/ir.h"

#include <ostream>
#include <string>

namespace flux::ir {

/// Human-readable text format printer for Flux IR.
/// Produces output similar to LLVM IR for debugging.
class IRPrinter {
  public:
    /// Print an entire IR module.
    void print(const IRModule& module, std::ostream& os) const;

    /// Print a single function.
    void print_function(const IRFunction& fn, std::ostream& os) const;

    /// Print a single basic block.
    void print_block(const BasicBlock& bb, std::ostream& os) const;

    /// Print a single instruction.
    void print_instruction(const Instruction& inst, std::ostream& os) const;

    /// Convert an IR type to string representation.
    static std::string type_to_string(const IRType& type);

    /// Convert a value to its printed name.
    static std::string value_to_string(const Value& val);

  private:
    static std::string opcode_to_string(Opcode op);
};

} // namespace flux::ir

#endif // FLUX_IR_PRINTER_H
