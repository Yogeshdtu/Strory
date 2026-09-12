// mh_engine_common.hpp
// ============================================================
// Shared plumbing for the mini engine + its example drivers:
//   - all component headers pulled together
//   - rdtsc harness (SAME idiom as folders 35..43)
//   - TSC calibration (g_tpns()) + a nearest-rank percentile helper
// ============================================================
#pragma once

#include "mh_feed_parser.hpp"
#include "mh_order_book.hpp"
#include "mh_order_manager.hpp"
#include "mh_risk_engine.hpp"
#include "mh_strategy.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

#if defined(__x86_64__) || defined(_M_X64)
#  include <x86intrin.h>
#endif

namespace mhft {

static inline std::uint64_t tsc() {
#if defined(__x86_64__) || defined(_M_X64)
    _mm_lfence();
    const std::uint64_t t = __rdtsc();
    _mm_lfence();
    return t;
#else
    return 0;
#endif
}

template <class T>
static inline void keep(T& v) { asm volatile("" : "+r,m"(v) : : "memory"); }

// calibrate once, cache. tsc ticks -> ns  =>  ticks / g_tpns()
inline double g_tpns() {
    static const double v = [] {
        namespace ch = std::chrono;
        const auto c0 = ch::steady_clock::now();
        const std::uint64_t r0 = tsc();
        volatile std::uint64_t spin = 0;
        while (ch::duration_cast<ch::milliseconds>(ch::steady_clock::now() - c0).count() < 100)
            spin = spin + 1;
        const std::uint64_t r1 = tsc();
        const auto c1 = ch::steady_clock::now();
        const double dns = static_cast<double>(ch::duration_cast<ch::nanoseconds>(c1 - c0).count());
        const double dt  = static_cast<double>(r1 - r0);
        return (dns > 0.0) ? dt / dns : 1.0;
    }();
    return v;
}

inline double pct(const std::vector<double>& sorted, double p) {
    if (sorted.empty()) return 0.0;
    std::size_t i = static_cast<std::size_t>(p / 100.0 * static_cast<double>(sorted.size()));
    if (i >= sorted.size()) i = sorted.size() - 1;
    return sorted[i];
}

}  // namespace mhft
