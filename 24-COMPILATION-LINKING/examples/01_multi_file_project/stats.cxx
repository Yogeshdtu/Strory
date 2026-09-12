// stats.cxx — DEFINITIONS. Teesri translation unit.
// mathx.hpp include karta hai -> mathx.o ke saath link karna zaroori.
// ============================================================
#include "stats.hpp"
#include "mathx.hpp"
#include <cmath>

namespace stats {

double mean(std::span<const double> xs) {
    if (xs.empty()) return 0.0;
    double s = 0.0;
    for (double x : xs) s += x;
    return s / static_cast<double>(xs.size());
}

double stddev(std::span<const double> xs) {
    if (xs.empty()) return 0.0;
    double m = mean(xs);
    double acc = 0.0;
    for (double x : xs) acc += (x - m) * (x - m);
    return std::sqrt(acc / static_cast<double>(xs.size()));
}

std::int64_t count_primes_upto(std::int64_t n) {
    std::int64_t c = 0;
    for (std::int64_t k = 2; k <= n; ++k)
        if (mathx::is_prime(k)) ++c;      // <-- cross-TU call (mathx.o se resolve hoga)
    return c;
}

}  // namespace stats
