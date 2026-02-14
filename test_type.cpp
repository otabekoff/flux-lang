#include "src/semantic/type.h"
#include <vector>

namespace flux::semantic {
struct Test {
    std::vector<FluxType> args;
};
} // namespace flux::semantic

int main() {
    flux::semantic::FluxType t;
    return 0;
}
