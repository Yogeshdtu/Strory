#include "calc.hpp"
namespace calc {
std::int64_t mul(std::int64_t a, std::int64_t b) { return a * b; }

std::int64_t dot(const std::int64_t* xs, const std::int64_t* ys, std::size_t n) {
    std::int64_t s = 0;
    for (std::size_t i = 0; i < n; ++i) s += mul(xs[i], ys[i]);
    return s;
}
}  // namespace calc
