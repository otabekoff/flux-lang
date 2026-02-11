#include "semantic/resolver.h"
#include <cassert>
#include <iostream>

int main() {
    using namespace flux::semantic;
    Resolver r;

    // Signed/unsigned detection and width for all Int8..Int128, UInt8..UInt128
    for (int w = 8; w <= 128; w *= 2) {
        std::string intn = "Int" + std::to_string(w);
        std::string uintn = "UInt" + std::to_string(w);
        assert(r.is_signed_int_name(intn));
        assert(!r.is_unsigned_int_name(intn));
        assert(r.numeric_width(intn) == w);
        assert(!r.is_signed_int_name(uintn));
        assert(r.is_unsigned_int_name(uintn));
        assert(r.numeric_width(uintn) == w);
    }
    // Float detection and width
    assert(r.is_float_name("Float32"));
    assert(r.is_float_name("Float64"));
    assert(r.numeric_width("Float32") == 32);
    assert(r.numeric_width("Float64") == 64);
    // Float128 (if supported)
    assert(r.is_float_name("Float128"));
    assert(r.numeric_width("Float128") == 128);

    // IntPtr/UIntPtr
    assert(!r.is_signed_int_name("IntPtr"));
    assert(!r.is_unsigned_int_name("UIntPtr"));
    assert(r.numeric_width("IntPtr") == (int)(sizeof(void*) * 8));
    assert(r.numeric_width("UIntPtr") == (int)(sizeof(void*) * 8));

    // Promotion: all pairs of Int/UInt
    for (int wa = 8; wa <= 128; wa *= 2) {
        for (int wb = 8; wb <= 128; wb *= 2) {
            std::string a = "Int" + std::to_string(wa);
            std::string b = "Int" + std::to_string(wb);
            std::string res = r.promote_integer_name(a, b);
            int expect = std::max(wa, wb);
            assert(res == "Int" + std::to_string(expect));

            a = "UInt" + std::to_string(wa);
            b = "UInt" + std::to_string(wb);
            res = r.promote_integer_name(a, b);
            expect = std::max(wa, wb);
            assert(res == "UInt" + std::to_string(expect));

            // Mixed signed/unsigned: should be signed
            a = "Int" + std::to_string(wa);
            b = "UInt" + std::to_string(wb);
            res = r.promote_integer_name(a, b);
            expect = std::max(wa, wb);
            assert(res == "Int" + std::to_string(expect));
        }
    }

    std::cout << "numeric_promotion: OK\n";
    return 0;
}
