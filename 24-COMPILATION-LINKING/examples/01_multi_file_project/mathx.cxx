// mathx.cxx — DEFINITIONS. Yeh apni ek translation unit hai.
// Alag se compile hoti hai:  g++ -c mathx.cxx -o mathx.o
// ============================================================
#include "mathx.hpp"

namespace mathx {

std::int64_t gcd(std::int64_t a, std::int64_t b) {
    a = a < 0 ? -a : a;
    b = b < 0 ? -b : b;
    while (b) { std::int64_t t = b; b = a % b; a = t; }
    return a;
}

std::int64_t lcm(std::int64_t a, std::int64_t b) {
    if (a == 0 || b == 0) return 0;
    std::int64_t g = gcd(a, b);
    return (a / g) * b;
}

bool is_prime(std::int64_t n) {
    if (n < 2) return false;
    if (n % 2 == 0) return n == 2;
    for (std::int64_t d = 3; d * d <= n; d += 2)
        if (n % d == 0) return false;
    return true;
}

// extern const variable ki EK definition (declaration header mein thi)
const char* const kVersion = "mathx 1.0";

}  // namespace mathx
