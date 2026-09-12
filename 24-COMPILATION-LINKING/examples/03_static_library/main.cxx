#include "calc.hpp"
#include <cstdio>

int main() {
    std::printf("build_id  = %s\n", calc::build_id());
    std::printf("add(2,3)  = %lld\n", static_cast<long long>(calc::add(2, 3)));
    std::printf("mul(4,5)  = %lld\n", static_cast<long long>(calc::mul(4, 5)));

    std::int64_t xs[] = {1, 2, 3, 4};
    std::int64_t ys[] = {10, 20, 30, 40};
    std::printf("dot       = %lld\n",
                static_cast<long long>(calc::dot(xs, ys, 4)));
    // calc::huge_unused ko call NAHI kar rahe -> uska .o link nahi hoga
    return 0;
}
