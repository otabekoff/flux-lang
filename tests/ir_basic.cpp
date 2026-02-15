/// @file tests/ir_basic.cpp
/// End-to-end tests for IR construction, lowering, printing, and passes.

#include <iostream>
#include <sstream>

#include "ir/ir.h"
#include "ir/ir_builder.h"
#include "ir/ir_pass.h"
#include "ir/ir_printer.h"
#include "ir/passes/constant_folding.h"
#include "ir/passes/dead_code_elimination.h"
#include "ir/passes/inliner.h"
#include "ir/passes/ir_verifier.h"

using namespace flux::ir;

// ── Helpers ─────────────────────────────────────────────────

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT(cond, msg)                                                                          \
    do {                                                                                           \
        ++tests_run;                                                                               \
        if (!(cond)) {                                                                             \
            std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n";          \
            return false;                                                                          \
        }                                                                                          \
        ++tests_passed;                                                                            \
    } while (0)

// ── Test: Type creation ─────────────────────────────────────

bool test_ir_types() {
    auto i32 = make_i32();
    ASSERT(i32->kind == IRTypeKind::I32, "i32 kind");
    ASSERT(i32->name == "Int32", "i32 name");
    ASSERT(i32->is_integer(), "i32 is integer");
    ASSERT(!i32->is_float(), "i32 is not float");

    auto f64 = make_f64();
    ASSERT(f64->kind == IRTypeKind::F64, "f64 kind");
    ASSERT(f64->is_float(), "f64 is float");

    auto v = make_void();
    ASSERT(v->kind == IRTypeKind::Void, "void kind");

    auto b = make_bool();
    ASSERT(b->kind == IRTypeKind::Bool, "bool kind");

    auto p = make_ptr(i32);
    ASSERT(p->kind == IRTypeKind::Ptr, "ptr kind");
    ASSERT(p->pointee->kind == IRTypeKind::I32, "ptr -> i32");

    auto arr = make_array(i32, 10);
    ASSERT(arr->kind == IRTypeKind::Array, "array kind");
    ASSERT(arr->array_size == 10, "array size");

    std::cout << "  [PASS] test_ir_types\n";
    return true;
}

// ── Test: Constants ─────────────────────────────────────────

bool test_constants() {
    auto c1 = make_const_i32(42);
    ASSERT(c1->is_constant, "const is_constant");
    ASSERT(std::get<int64_t>(c1->constant_value) == 42, "const i32 value");
    ASSERT(c1->type->kind == IRTypeKind::I32, "const i32 type");

    auto c2 = make_const_f64(3.14);
    ASSERT(c2->is_constant, "const f64 is_constant");
    ASSERT(std::get<double>(c2->constant_value) - 3.14 < 0.001, "const f64 value");

    auto c3 = make_const_bool(true);
    ASSERT(c3->is_constant, "const bool is_constant");
    ASSERT(std::get<bool>(c3->constant_value) == true, "const bool value");

    auto c4 = make_const_string("hello");
    ASSERT(c4->is_constant, "const string is_constant");
    ASSERT(std::get<std::string>(c4->constant_value) == "hello", "const string value");

    std::cout << "  [PASS] test_constants\n";
    return true;
}

// ── Test: Builder arithmetic ────────────────────────────────

bool test_builder_arithmetic() {
    IRBuilder builder;
    auto i32 = make_i32();

    builder.create_function("add_fn",
                            {builder.create_value(i32, "x"), builder.create_value(i32, "y")}, i32);

    auto fn = builder.module().functions.back().get();
    auto x = fn->params[0];
    auto y = fn->params[1];

    auto sum = builder.emit_add(x, y);
    ASSERT(sum != nullptr, "emit_add returns value");
    ASSERT(sum->type->kind == IRTypeKind::I32, "add result type");

    auto diff = builder.emit_sub(x, y);
    ASSERT(diff != nullptr, "emit_sub returns value");

    auto prod = builder.emit_mul(x, y);
    ASSERT(prod != nullptr, "emit_mul returns value");

    builder.emit_ret(sum);

    ASSERT(fn->blocks.size() == 1, "one basic block");
    ASSERT(fn->blocks[0]->is_terminated(), "block terminated");

    std::cout << "  [PASS] test_builder_arithmetic\n";
    return true;
}

// ── Test: Control flow ──────────────────────────────────────

bool test_builder_control_flow() {
    IRBuilder builder;
    auto i32 = make_i32();
    auto bool_ty = make_bool();

    builder.create_function(
        "cf_fn", {builder.create_value(bool_ty, "cond"), builder.create_value(i32, "val")}, i32);

    auto fn = builder.module().functions.back().get();
    auto cond = fn->params[0];

    auto* then_bb = builder.create_block("then");
    auto* else_bb = builder.create_block("else");
    auto* merge_bb = builder.create_block("merge");

    builder.emit_cond_br(cond, then_bb, else_bb);

    builder.set_insert_point(then_bb);
    auto v1 = make_const_i32(1);
    builder.emit_br(merge_bb);

    builder.set_insert_point(else_bb);
    auto v2 = make_const_i32(2);
    builder.emit_br(merge_bb);

    builder.set_insert_point(merge_bb);
    auto phi = builder.emit_phi(i32, {{v1, then_bb}, {v2, else_bb}});
    builder.emit_ret(phi);

    ASSERT(fn->blocks.size() == 4, "4 blocks (entry + then + else + merge)");
    ASSERT(merge_bb->is_terminated(), "merge block terminated");

    // Check CFG edges
    ASSERT(then_bb->successors.size() == 1, "then has 1 successor");
    ASSERT(then_bb->successors[0] == merge_bb, "then -> merge");
    ASSERT(merge_bb->predecessors.size() == 2, "merge has 2 predecessors");

    std::cout << "  [PASS] test_builder_control_flow\n";
    return true;
}

// ── Test: IR Printer ────────────────────────────────────────

bool test_ir_printer() {
    IRBuilder builder;
    auto i32 = make_i32();

    builder.create_function("print_test", {builder.create_value(i32, "a")}, i32);

    auto fn = builder.module().functions.back().get();
    auto a = fn->params[0];

    auto c10 = make_const_i32(10);
    auto sum = builder.emit_add(a, c10);
    builder.emit_ret(sum);

    std::ostringstream oss;
    IRPrinter printer;
    printer.print(builder.module(), oss);

    std::string output = oss.str();

    ASSERT(output.find("func @print_test") != std::string::npos, "printer shows function name");
    ASSERT(output.find("add") != std::string::npos, "printer shows add");
    ASSERT(output.find("ret") != std::string::npos, "printer shows ret");
    ASSERT(output.find("entry:") != std::string::npos, "printer shows entry block");

    std::cout << "  [PASS] test_ir_printer\n";
    return true;
}

// ── Test: Constant folding ──────────────────────────────────

bool test_constant_folding() {
    IRBuilder builder;
    auto i32 = make_i32();

    builder.create_function("fold_test", {}, i32);

    auto c3 = make_const_i32(3);
    auto c7 = make_const_i32(7);

    auto sum = builder.emit_add(c3, c7);
    builder.emit_ret(sum);

    ConstantFoldingPass pass;
    bool modified = pass.run(builder.module());

    ASSERT(modified, "constant folding modified IR");
    ASSERT(sum->is_constant, "sum is constant after folding");
    ASSERT(std::get<int64_t>(sum->constant_value) == 10, "3 + 7 = 10");

    std::cout << "  [PASS] test_constant_folding\n";
    return true;
}

// ── Test: DCE ───────────────────────────────────────────────

bool test_dead_code_elimination() {
    IRBuilder builder;
    auto i32 = make_i32();

    // Give the function a parameter to shift builder's value ID counter
    builder.create_function("dce_test", {builder.create_value(i32, "param")}, i32);

    auto fn = builder.module().functions.back().get();
    auto param = fn->params[0];

    // This add result is never used (dead code)
    auto c1 = make_const_i32(1);
    auto dead = builder.emit_add(param, c1);
    (void)dead;

    // Return param directly
    builder.emit_ret(param);

    size_t original_instr_count = fn->blocks[0]->instructions.size();

    DeadCodeEliminationPass pass;
    bool modified = pass.run(builder.module());

    ASSERT(modified, "DCE modified IR");
    size_t new_instr_count = fn->blocks[0]->instructions.size();
    ASSERT(new_instr_count < original_instr_count, "DCE removed dead instruction");

    std::cout << "  [PASS] test_dead_code_elimination\n";
    return true;
}

// ── Test: BasicBlock terminator detection ───────────────────

bool test_terminator_detection() {
    auto bb = std::make_unique<BasicBlock>();
    bb->label = "test";

    ASSERT(!bb->is_terminated(), "empty block not terminated");

    auto ret = std::make_unique<Instruction>();
    ret->opcode = Opcode::Ret;
    bb->instructions.push_back(std::move(ret));

    ASSERT(bb->is_terminated(), "block with ret is terminated");

    std::cout << "  [PASS] test_terminator_detection\n";
    return true;
}

// ── Test: Memory operations ─────────────────────────────────

bool test_memory_ops() {
    IRBuilder builder;
    auto i32 = make_i32();

    builder.create_function("mem_test", {}, make_void());

    auto alloca = builder.emit_alloca(i32, "x");
    ASSERT(alloca->type->kind == IRTypeKind::Ptr, "alloca returns ptr");
    ASSERT(alloca->type->pointee->kind == IRTypeKind::I32, "alloca pointee is i32");

    auto v = make_const_i32(42);
    builder.emit_store(v, alloca);

    auto loaded = builder.emit_load(alloca);
    ASSERT(loaded->type->kind == IRTypeKind::I32, "load produces i32");

    builder.emit_ret();

    std::cout << "  [PASS] test_memory_ops\n";
    return true;
}

// ── Test: Call instruction ──────────────────────────────────

bool test_call_instruction() {
    IRBuilder builder;
    auto i32 = make_i32();

    builder.create_function("caller", {}, i32);

    auto arg1 = make_const_i32(1);
    auto arg2 = make_const_i32(2);
    auto result = builder.emit_call("add", {arg1, arg2}, i32);

    ASSERT(result != nullptr, "call returns value");
    ASSERT(result->type->kind == IRTypeKind::I32, "call result type");

    builder.emit_ret(result);

    std::cout << "  [PASS] test_call_instruction\n";
    return true;
}

// ── Test: Comparison operations ─────────────────────────────

bool test_comparison_ops() {
    IRBuilder builder;
    auto i32 = make_i32();

    builder.create_function(
        "cmp_test", {builder.create_value(i32, "a"), builder.create_value(i32, "b")}, make_bool());

    auto fn = builder.module().functions.back().get();
    auto a = fn->params[0];
    auto b = fn->params[1];

    auto eq = builder.emit_eq(a, b);
    ASSERT(eq->type->kind == IRTypeKind::Bool, "eq returns bool");

    auto lt = builder.emit_lt(a, b);
    ASSERT(lt->type->kind == IRTypeKind::Bool, "lt returns bool");

    builder.emit_ret(eq);

    std::cout << "  [PASS] test_comparison_ops\n";
    return true;
}

// ── Test: Struct layout ─────────────────────────────────────

bool test_struct_layout() {
    IRModule module;
    module.name = "struct_test";

    StructLayout layout;
    layout.name = "Point";
    layout.field_names = {"x", "y"};
    layout.field_types = {make_f64(), make_f64()};
    module.struct_layouts.push_back(std::move(layout));

    ASSERT(module.struct_layouts.size() == 1, "one struct layout");
    ASSERT(module.struct_layouts[0].name == "Point", "struct name");
    ASSERT(module.struct_layouts[0].field_names.size() == 2, "2 fields");

    std::cout << "  [PASS] test_struct_layout\n";
    return true;
}

// ── Test: Constant folding float ────────────────────────────

bool test_constant_folding_float() {
    IRBuilder builder;
    auto f64 = make_f64();

    builder.create_function("fold_float", {}, f64);

    auto c3 = make_const_f64(3.5);
    auto c2 = make_const_f64(2.0);

    auto product = builder.emit_mul(c3, c2);
    builder.emit_ret(product);

    ConstantFoldingPass pass;
    bool modified = pass.run(builder.module());

    ASSERT(modified, "float constant folding modified IR");
    ASSERT(product->is_constant, "product is constant after folding");
    ASSERT(std::get<double>(product->constant_value) == 7.0, "3.5 * 2.0 = 7.0");

    std::cout << "  [PASS] test_constant_folding_float\n";
    return true;
}

// ── Test: Pass pipeline ─────────────────────────────────────

bool test_pass_pipeline() {
    IRBuilder builder;
    auto i32 = make_i32();

    builder.create_function("pipeline_test", {}, i32);

    // Dead: 3 + 7 = 10 (never used)
    auto c3 = make_const_i32(3);
    auto c7 = make_const_i32(7);
    builder.emit_add(c3, c7);

    // Live: return 42
    auto c42 = make_const_i32(42);
    builder.emit_ret(c42);

    std::vector<std::unique_ptr<IRPass>> passes;
    passes.push_back(std::make_unique<ConstantFoldingPass>());
    passes.push_back(std::make_unique<DeadCodeEliminationPass>());

    int modified = run_passes(builder.module(), passes);
    ASSERT(modified >= 1, "at least one pass modified IR");

    std::cout << "  [PASS] test_pass_pipeline\n";
    return true;
}

// ── Test: Verifier ──────────────────────────────────────────

bool test_ir_verifier() {
    IRBuilder builder;
    auto i32 = make_i32();
    builder.create_function("bad_func", {}, i32);
    // Empty block (unterminated)

    IRVerifierPass verifier;
    try {
        verifier.run(builder.module());
        std::cout << "  [FAIL] Verifier failed to catch empty block\n";
        return false;
    } catch (const std::runtime_error&) {
        // Expected
    }

    // Add terminator
    builder.emit_ret(make_const_i32(0));
    try {
        verifier.run(builder.module());
    } catch (const std::runtime_error& e) {
        std::cout << "  [FAIL] Verifier rejected valid function: " << e.what() << "\n";
        return false;
    }

    std::cout << "  [PASS] test_ir_verifier\n";
    return true;
}

// ── Test: Inliner ───────────────────────────────────────────

bool test_inliner() {
    IRBuilder builder;
    auto i32 = make_i32();

    // Callee: func inc(x) { return x + 1; }
    builder.create_function("inc", {builder.create_value(i32, "x")}, i32);
    auto p = builder.current_function()->params[0];
    auto one = make_const_i32(1);
    auto res = builder.emit_add(p, one);
    builder.emit_ret(res);

    // Caller: func main() { return inc(10); }
    builder.create_function("main", {}, i32);
    auto ten = make_const_i32(10);
    auto call_res = builder.emit_call("inc", {ten}, i32);
    builder.emit_ret(call_res);

    InlinerPass inliner;
    // We expect modification
    if (!inliner.run(builder.module())) {
        std::cout << "  [FAIL] Inliner reported no modification\n";
        return false;
    }

    auto main_fn = builder.module().functions.back().get();
    bool has_call = false;
    bool has_add = false;
    for (const auto& inst : main_fn->blocks[0]->instructions) {
        if (inst->opcode == Opcode::Call)
            has_call = true;
        if (inst->opcode == Opcode::Add)
            has_add = true;
    }

    if (has_call) {
        std::cout << "  [FAIL] Call instruction remains after inlining\n";
        return false;
    }
    if (!has_add) {
        std::cout << "  [FAIL] Inlined instruction missing\n";
        return false;
    }

    std::cout << "  [PASS] test_inliner\n";
    return true;
}

// ── Main ────────────────────────────────────────────────────

int main() {
    std::cout << "=== Flux IR Basic Tests ===\n\n";

    bool all_passed = true;

    all_passed &= test_ir_types();
    all_passed &= test_constants();
    all_passed &= test_builder_arithmetic();
    all_passed &= test_builder_control_flow();
    all_passed &= test_ir_printer();
    all_passed &= test_constant_folding();
    all_passed &= test_dead_code_elimination();
    all_passed &= test_terminator_detection();
    all_passed &= test_memory_ops();
    all_passed &= test_call_instruction();
    all_passed &= test_comparison_ops();
    all_passed &= test_struct_layout();
    all_passed &= test_constant_folding_float();
    all_passed &= test_pass_pipeline();
    all_passed &= test_ir_verifier();
    all_passed &= test_inliner();

    std::cout << "\n" << tests_passed << "/" << tests_run << " assertions passed.\n";

    if (all_passed) {
        std::cout << "\n=== ALL TESTS PASSED ===\n";
        return 0;
    } else {
        std::cout << "\n=== SOME TESTS FAILED ===\n";
        return 1;
    }
}
