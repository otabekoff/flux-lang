#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "lexer/diagnostic.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/resolver.h"

int main() {
    const std::string source = R"(
        trait Display {
            func to_string(self) -> String;
        }

        trait Wrapper<T: Display> {
            func wrap(self, x: T) -> String {
                // Should be able to call x.to_string() because T: Display
                return x.to_string();
            }
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

        struct MyWrapper {}
        impl Wrapper<Point> for MyWrapper {}

        func main() -> Void {
            let w: MyWrapper = MyWrapper {};
            let p: Point = Point { x: 1, y: 2 };
            w.wrap(p);
        }
    )";

    try {
        flux::Lexer lexer(source);
        auto tokens = lexer.tokenize();
        flux::Parser parser(tokens);
        auto mod = parser.parse_module();

        flux::semantic::Resolver resolver;
        resolver.resolve(mod);
        std::cout << "TraitDefaultGenericTest: PASS" << std::endl;
    } catch (const flux::DiagnosticError& e) {
        std::cerr << "TraitDefaultGenericTest FAILED: " << e.what() << " at " << e.line() << ":"
                  << e.column() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "TraitDefaultGenericTest ERROR: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
