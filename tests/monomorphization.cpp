#include "ast/ast.h"
#include "semantic/resolver.h"
#include <cassert>
#include <exception> // Added for std::exception
#include <iostream>

using namespace flux::ast;
using namespace flux::semantic;

void test_function_monomorphization() {
    std::cerr << "--- Running test_function_monomorphization ---" << std::endl;
    // func foo<T>(x: T) {}
    FunctionDecl foo;
    foo.name = "foo";
    foo.type_params = {"T"};
    foo.return_type = "Void";
    foo.params.push_back({"x", "T"});

    Module mod;
    mod.functions.push_back(std::move(foo));

    // Call: foo<Int32>(1)
    std::vector<std::unique_ptr<Expr>> args;
    args.push_back(std::make_unique<NumberExpr>("1"));
    auto call =
        std::make_unique<CallExpr>(std::make_unique<IdentifierExpr>("foo<Int32>"), std::move(args));

    Resolver resolver;
    resolver.resolve(mod);
    (void)resolver.type_of(*call);

    const auto& f_insts = resolver.function_instantiations();
    std::cerr << "Function instantiations: " << f_insts.size() << std::endl;
    for (const auto& inst : f_insts) {
        std::cerr << "  " << inst.name << "<";
        bool first = true;
        for (const auto& arg : inst.args) {
            if (!first)
                std::cerr << ", ";
            std::cerr << arg.name;
            first = false;
        }
        std::cerr << ">" << std::endl;
    }

    if (f_insts.size() != 1) {
        throw std::runtime_error("FAIL: expected 1 function instantiation, got " +
                                 std::to_string(f_insts.size()));
    }
    if (f_insts[0].name != "foo") {
        throw std::runtime_error("FAIL: expected function name 'foo', got '" + f_insts[0].name +
                                 "'");
    }
    if (f_insts[0].args.size() != 1) {
        throw std::runtime_error("FAIL: expected 1 generic arg, got " +
                                 std::to_string(f_insts[0].args.size()));
    }
    if (f_insts[0].args[0].name != "Int32") {
        throw std::runtime_error("FAIL: expected generic arg 'Int32', got '" +
                                 f_insts[0].args[0].name + "'");
    }
    std::cerr << "PASSED: test_function_monomorphization" << std::endl;
}

void test_struct_monomorphization() {
    std::cerr << "--- Running test_struct_monomorphization ---" << std::endl;
    // struct Box<T> { val: T }
    StructDecl box_decl("Box", {"T"}, {{"val", "T"}});

    Module mod;
    mod.structs.push_back(std::move(box_decl));

    // Literal: Box<Float64> { val: 1.0 }
    std::vector<FieldInit> inits;
    inits.push_back({"val", std::make_unique<NumberExpr>("1.0")});
    auto lit = std::make_unique<StructLiteralExpr>("Box<Float64>", std::move(inits));

    Resolver resolver;
    resolver.resolve(mod);
    (void)resolver.type_of(*lit);

    const auto& t_insts = resolver.type_instantiations();
    std::cerr << "Type instantiations: " << t_insts.size() << std::endl;
    for (const auto& inst : t_insts) {
        std::cerr << "  " << inst.name << "<";
        bool first = true;
        for (const auto& arg : inst.args) {
            if (!first)
                std::cerr << ", ";
            std::cerr << arg.name;
            first = false;
        }
        std::cerr << ">" << std::endl;
    }

    // One instantiation: Box<Float64>
    bool found = false;
    for (const auto& inst : t_insts) {
        if (inst.name == "Box" && inst.args.size() == 1 && inst.args[0].name == "Float64") {
            found = true;
            break;
        }
    }
    if (!found) {
        throw std::runtime_error("FAIL: Box<Float64> instantiation not found");
    }
    std::cerr << "PASSED: test_struct_monomorphization" << std::endl;
}

void test_nested_monomorphization() {
    std::cerr << "--- Running test_nested_monomorphization ---" << std::endl;
    // struct Wrapper<T> { val: T }
    StructDecl wrap_decl("Wrapper", {"T"}, {{"val", "T"}});

    Module mod;
    mod.structs.push_back(std::move(wrap_decl));

    // Type resolution: Wrapper<Wrapper<Int32>>
    Resolver resolver;
    resolver.resolve(mod);
    Type nested = resolver.type_from_name("Wrapper<Wrapper<Int32>>");

    const auto& t_insts = resolver.type_instantiations();
    std::cerr << "Type instantiations: " << t_insts.size() << std::endl;
    for (const auto& inst : t_insts) {
        std::cerr << "  " << inst.name << "<";
        bool first = true;
        for (const auto& arg : inst.args) {
            if (!first)
                std::cerr << ", ";
            std::cerr << arg.name;
            first = false;
        }
        std::cerr << ">" << std::endl;
    }

    if (t_insts.size() < 2) {
        throw std::runtime_error(
            "FAIL: expected at least 2 type instantiations for nested generic");
    }

    bool found_inner = false;
    bool found_outer = false;
    for (const auto& inst : t_insts) {
        if (inst.name == "Wrapper" && inst.args.size() == 1) {
            if (inst.args[0].name == "Int32")
                found_inner = true;
            if (inst.args[0].name == "Wrapper<Int32>")
                found_outer = true;
        }
    }
    if (!found_inner)
        throw std::runtime_error("FAIL: Wrapper<Int32> not found");
    if (!found_outer)
        throw std::runtime_error("FAIL: Wrapper<Wrapper<Int32>> not found");

    std::cerr << "PASSED: test_nested_monomorphization" << std::endl;
}

int main() {
    try {
        test_function_monomorphization();
        test_struct_monomorphization();
        test_nested_monomorphization();
        std::cerr << "ALL MONOMORPHIZATION TESTS PASSED." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "CRASH: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
