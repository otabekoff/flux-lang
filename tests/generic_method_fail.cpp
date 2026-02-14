#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "lexer/diagnostic.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/resolver.h"

int main() {
    std::string code = R"(
        trait Display {
            func to_string(self) -> String;
        }

        func print_it<T: Display>(x: T) -> Void {
            x.invalid_method(); // SHOULD FAIL
        }
    )";

    try {
        flux::Lexer lexer(code);
        auto tokens = lexer.tokenize();
        flux::Parser parser(tokens);
        auto module = parser.parse_module();

        flux::semantic::Resolver resolver;
        resolver.resolve(module);

        std::cout << "GenericMethodFailTest: FAIL (expected error but passed)" << std::endl;
        return 1;
    } catch (const flux::DiagnosticError& e) {
        std::string msg = e.what();
        if (msg.find("has no field or method 'invalid_method'") != std::string::npos) {
            std::cout << "GenericMethodFailTest: PASS" << std::endl;
            return 0;
        } else {
            std::cout << "GenericMethodFailTest: FAIL (wrong error: " << msg << ")" << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cout << "GenericMethodFailTest: ERROR (" << e.what() << ")" << std::endl;
        return 1;
    }

    return 0;
}
