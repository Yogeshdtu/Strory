// unused.cxx — jaan-boojh ke: is function ko main call NAHI karta.
// Static library se link karte waqt linker sirf ZAROORI members (.o) kheenchta
// hai. `nm app` mein `huge_unused_table` nahi dikhega (agar --gc-sections /
// member selection kaam kare).
#include "calc.hpp"
#include <cstdint>

namespace calc {
std::int64_t huge_unused(std::int64_t x) {
    volatile std::int64_t acc = 0;
    for (int i = 0; i < 1000; ++i) acc += x + i;
    return acc;
}
}  // namespace calc
