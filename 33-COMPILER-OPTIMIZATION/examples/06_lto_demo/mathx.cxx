// mathx.cxx -- the definition, in its own translation unit
#include "mathx.hpp"

// Tiny body. If `main` could SEE this, -O2 would inline it and then
// fold/vectorize the caller's loop. Across a TU boundary (no LTO) it
// stays a real function with a real `call`.
std::uint32_t hot_transform(std::uint32_t x) noexcept {
    x ^= x >> 15;
    x *= 0x2c1b3c6dU;
    x ^= x >> 12;
    x *= 0x297a2d39U;
    x ^= x >> 15;
    return x;
}
