#include <iostream>
#include <string>
#include <vector>

#include "lexer/diagnostic.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/resolver.h"

using namespace flux;
using namespace flux::semantic;

int main() {
    const std::string source = R"(
        trait Display {
            func to_string(self) -> String;
        }

        struct Point {
            x: Int32,
            y: Int32
        }

        impl Display for Point {
            func to_string(self) -> String {
                return "Point";
            }
        }

        func print_it<T: Display>(x: T) -> String {
            return x.to_string();
        }

        func main() -> Void {
            let p: Point = Point { x: 1, y: 2 };
            print_it(p);
        }
    )";

    try {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto mod = parser.parse_module();

        Resolver resolver;
        resolver.resolve(mod);
        std::cout << "GenericMethodTest: PASS" << std::endl;
    } catch (const DiagnosticError& e) {
        std::cerr << "GenericMethodTest FAILED: " << e.what() << " at " << e.line() << ":"
                  << e.column() << std::endl;
        return 1;
    }
    return 0;
}
