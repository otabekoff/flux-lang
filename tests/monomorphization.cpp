#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/resolver.h"
#include <cassert>
#include <iostream>

using namespace flux::semantic;

void test_transitive_monomorphization() {
    std::cout << "Testing transitive monomorphization..." << std::endl;
    std::string code = R"(
        func bar<T>(x: T) { }
        func foo<U>(y: U) { bar<U>(y); }
        func main() { foo<Int32>(42); }
    )";

    flux::Lexer lexer(code);
    flux::Parser parser(lexer.tokenize());
    auto module = parser.parse_module();

    Resolver resolver;
    resolver.resolve(module);

    const auto& insts = resolver.function_instantiations();

    bool found_foo = false;
    bool found_bar = false;
    for (const auto& inst : insts) {
        if (inst.name == "foo" && inst.args.size() == 1 && inst.args[0].name == "Int32")
            found_foo = true;
        if (inst.name == "bar" && inst.args.size() == 1 && inst.args[0].name == "Int32")
            found_bar = true;
    }

    assert(found_foo && "foo<Int32> should be recorded");
    assert(found_bar && "bar<Int32> should be recorded (transitive)");
    std::cout << "  Passed!" << std::endl;
}

void test_method_monomorphization() {
    std::cout << "Testing method monomorphization..." << std::endl;
    std::string code = R"(
        struct Wrapper<T> { val: T }
        impl<T> Wrapper<T> {
            func get(self) -> T { return self.val; }
            func call_other<U>(self, x: U) { self.get(); }
        }
        func main() {
            let w: Wrapper<Float32> = Wrapper<Float32>{ val: 1.0 };
            w.call_other<Int32>(42);
        }
    )";

    flux::Lexer lexer(code);
    flux::Parser parser(lexer.tokenize());
    auto module = parser.parse_module();

    Resolver resolver;
    resolver.resolve(module);

    const auto& insts = resolver.function_instantiations();

    bool found_call_other = false;
    bool found_get = false;
    for (const auto& inst : insts) {

        if (inst.name == "Wrapper::call_other") {
            // Expecting T=Float32, U=Int32
            if (inst.args.size() == 2 && inst.args[0].name == "Float32" &&
                inst.args[1].name == "Int32")
                found_call_other = true;
        }
        if (inst.name == "Wrapper::get") {
            // Expecting T=Float32
            if (inst.args.size() == 1 && inst.args[0].name == "Float32")
                found_get = true;
        }
    }

    assert(found_call_other && "Wrapper::call_other<Float32, Int32> should be recorded");
    assert(found_get && "Wrapper::get<Float32> should be recorded (recursive from call_other)");
    std::cout << "  Passed!" << std::endl;
}

int main() {
    try {
        test_transitive_monomorphization();
        test_method_monomorphization();
        std::cout << "All monomorphization tests passed!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
