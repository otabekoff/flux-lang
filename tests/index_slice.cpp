#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "lexer/diagnostic.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/resolver.h"

using namespace flux;
using namespace flux::semantic;

void test_array_indexing() {
    std::cout << "--- Testing array indexing ---" << std::endl;
    const std::string source = R"(
        func main() -> Void {
            let arr: [Int32; 3] = [1, 2, 3];
            let x: Int32 = arr[0];
        }
    )";

    try {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto mod = parser.parse_module();

        Resolver resolver;
        resolver.resolve(mod);
        std::cout << "ArrayIndexing: PASS" << std::endl;
    } catch (const DiagnosticError& e) {
        std::cerr << "ArrayIndexing FAILED: " << e.what() << " at " << e.line() << ":" << e.column()
                  << std::endl;
        exit(1);
    }
}

void test_array_slicing() {
    std::cout << "--- Testing array slicing ---" << std::endl;
    const std::string source = R"(
        func main() -> Void {
            let arr: [Int32; 3] = [1, 2, 3];
            let s: [Int32] = arr[0:2];
        }
    )";

    try {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto mod = parser.parse_module();

        Resolver resolver;
        resolver.resolve(mod);
        std::cout << "ArraySlicing: PASS" << std::endl;
    } catch (const DiagnosticError& e) {
        std::cerr << "ArraySlicing FAILED: " << e.what() << " at " << e.line() << ":" << e.column()
                  << std::endl;
        exit(1);
    }
}

int main() {
    test_array_indexing();
    test_array_slicing();
    return 0;
}
