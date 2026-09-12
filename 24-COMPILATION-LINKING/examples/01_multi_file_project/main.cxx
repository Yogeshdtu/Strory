// main.cxx — chauthi translation unit. Sirf HEADERS include karti hai,
// definitions link time pe milti hain (mathx.o + stats.o se).
// ============================================================
#include "mathx.hpp"
#include "stats.hpp"
#include <array>
#include <cstdio>

int main() {
    std::printf("%s\n\n", mathx::kVersion);

    std::printf("gcd(48, 18)        = %lld\n", static_cast<long long>(mathx::gcd(48, 18)));
    std::printf("lcm(4, 6)          = %lld\n", static_cast<long long>(mathx::lcm(4, 6)));
    std::printf("square(9)          = %lld   (constexpr, header mein)\n",
                static_cast<long long>(mathx::square(9)));
    std::printf("is_prime(97)       = %s\n", mathx::is_prime(97) ? "true" : "false");
    std::printf("clamp_nonneg(-5)   = %lld   (inline, header mein)\n",
                static_cast<long long>(mathx::clamp_nonneg(-5)));

    std::array<double, 5> xs{2.0, 4.0, 4.0, 4.0, 6.0};
    std::printf("\nmean(xs)           = %.3f\n", stats::mean(xs));
    std::printf("stddev(xs)         = %.3f\n", stats::stddev(xs));
    std::printf("count_primes<=100  = %lld   (stats -> mathx cross-TU call)\n",
                static_cast<long long>(stats::count_primes_upto(100)));

    return 0;
}
