// 12_integration_tests.cpp
// ============================================================
// PROJECT 13 (lesson) -- end-to-end integration + determinism + invariants.
//
// Across several (seed, config) combinations, assert:
//   A. NaiveEngine (std::map MatchingEngine venue) and OptimizedEngine
//      (FastVenue) produce IDENTICAL trading output  (correctness gate)
//   B. each engine is DETERMINISTIC (run x2 -> byte-identical Stats)
//   C. INVARIANTS: |position| <= risk max, zero sequence gaps, and the
//      end-to-end pipeline never produced a fill it shouldn't (filled_qty
//      consistent between the two venues)
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 12_integration_tests.cpp -o it && ./it
// ============================================================

#include "mh_engine.hpp"

#include <cstdio>

using namespace mhft;

static int fails = 0;
#define CHECK(cond, name) do { \
    if (cond) std::printf("  [ok]   %s\n", name); \
    else    { std::printf("  [FAIL] %s\n", name); ++fails; } } while (0)

template <class A, class B>
static bool same_trading(const A& a, const B& b) {
    return a.signals == b.signals && a.orders_ok == b.orders_ok &&
           a.fills == b.fills && a.filled_qty == b.filled_qty &&
           a.position == b.position && a.realized_pnl == b.realized_pnl &&
           a.gap_count == b.gap_count;
}

int main() {
    constexpr std::uint64_t N = 150000;
    const std::int64_t kPosMax = 400;

    struct Cfg { std::uint64_t seed; std::size_t cooldown; std::int64_t thr_num; };
    const Cfg cfgs[] = {
        {44, 40, 1002}, {7, 40, 1002}, {123, 20, 1002}, {44, 80, 1001}, {999, 60, 1003},
    };

    for (const Cfg& c : cfgs) {
        SpreadCrossStrategy::Config sc;
        sc.cooldown = c.cooldown; sc.thr_num = c.thr_num;

        NaiveEngine     n1(c.seed, sc), n2(c.seed, sc);
        OptimizedEngine o1(c.seed, sc), o2(c.seed, sc);
        const auto sn1 = n1.run(N, false);
        const auto sn2 = n2.run(N, false);
        const auto so1 = o1.run(N, false);
        const auto so2 = o2.run(N, false);

        char tag[96];

        std::snprintf(tag, sizeof tag, "seed=%llu cd=%zu thr=%lld : naive deterministic",
                      (unsigned long long)c.seed, c.cooldown, (long long)c.thr_num);
        CHECK(same_trading(sn1, sn2), tag);

        std::snprintf(tag, sizeof tag, "seed=%llu cd=%zu thr=%lld : optimized deterministic",
                      (unsigned long long)c.seed, c.cooldown, (long long)c.thr_num);
        CHECK(same_trading(so1, so2), tag);

        std::snprintf(tag, sizeof tag, "seed=%llu cd=%zu thr=%lld : naive == optimized (correctness gate)",
                      (unsigned long long)c.seed, c.cooldown, (long long)c.thr_num);
        CHECK(same_trading(sn1, so1), tag);

        std::snprintf(tag, sizeof tag, "seed=%llu : |position| <= %lld", (unsigned long long)c.seed, (long long)kPosMax);
        CHECK(so1.position <= kPosMax && so1.position >= -kPosMax, tag);

        std::snprintf(tag, sizeof tag, "seed=%llu : zero sequence gaps", (unsigned long long)c.seed);
        CHECK(so1.gap_count == 0, tag);

        std::snprintf(tag, sizeof tag, "seed=%llu : engine did some trading (fills > 0)", (unsigned long long)c.seed);
        CHECK(so1.fills > 0, tag);
    }

    std::printf("\n%s  (%d failures across %zu configs)\n",
                fails == 0 ? "INTEGRATION: ALL PASS" : "INTEGRATION: SOME FAILED",
                fails, sizeof(cfgs) / sizeof(cfgs[0]));
    return fails == 0 ? 0 : 1;
}
