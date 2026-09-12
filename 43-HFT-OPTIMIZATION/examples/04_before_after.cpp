// 04_before_after.cpp
// ============================================================
// v0 aur v3 -- EK process mein, back-to-back, SAME feed.
//
// Do cheezein saath check karta:
//   1. SPEEDUP  -- true per-tick average (clean loop) + per-stage ratios
//   2. AGREEMENT -- dono ne EXACT same ticks pe order fire kiya? "Fast" ka
//      koi matlab nahi agar output badal gaya. Yeh optimization ka
//      NON-NEGOTIABLE gate hai (01-optimization-methodology.md).
//
// Agreement 100% nahi bhi ho sakta: v0 float threshold-math karta, v3
// integer. Boundary pe kuch ticks flip ho sakte -- yeh 09-fixed-point.md
// ka point hai (float ka rounding non-deterministic-feeling hota). Jo bhi
// mismatch ho, HIDE mat karo -- report karo.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 04_before_after.cpp -o ba && ./ba
// ============================================================

#include "pipeline.hpp"

#include <chrono>
#include <cstdio>
#include <vector>

namespace ch = std::chrono;

template <class P>
static double true_avg_ns(const pipe::Feed& feed, std::size_t& orders_out) {
    P p(feed);
    const std::size_t n = feed.off.size() - 1;
    const auto t0 = ch::steady_clock::now();
    for (std::size_t i = 0; i < n; ++i) { p.set_index(i); p.process_one(i, false); }
    const double sec = ch::duration<double>(ch::steady_clock::now() - t0).count();
    orders_out = p.orders();
    return sec * 1e9 / static_cast<double>(n);
}

template <class P>
static pipe::StageNs stage_ns(const pipe::Feed& feed, double tpns) {
    P p(feed);
    const std::size_t n = feed.off.size() - 1;
    for (std::size_t i = 0; i < n; ++i) { p.set_index(i); p.process_one(i, true); }
    pipe::StageNs s = p.stage_ticks();
    s.parse  = s.parse  / tpns / static_cast<double>(s.ticks);
    s.book   = s.book   / tpns / static_cast<double>(s.ticks);
    s.signal = s.signal / tpns / static_cast<double>(s.ticks);
    s.total  = s.total  / tpns / static_cast<double>(s.ticks);
    return s;
}

int main() {
    const double tpns = pipe::calibrate_tpns();
    const pipe::Feed feed = pipe::make_feed(120000);
    const std::size_t n = feed.off.size() - 1;
    std::printf("feed: %zu msgs  |  ticks_per_ns = %.4f\n\n", n, tpns);

    // ---- 1. SPEEDUP (true per-tick average, clean loop) ----
    std::size_t o0 = 0, o3 = 0;
    const double a0 = true_avg_ns<pipe::PipelineV0>(feed, o0);
    const double a3 = true_avg_ns<pipe::PipelineV3>(feed, o3);

    std::puts("=== per-tick latency (true average, no in-loop rdtsc) ===");
    std::printf("  v0 (naive)     : %9.1f ns/tick\n", a0);
    std::printf("  v3 (optimized) : %9.1f ns/tick\n", a3);
    std::printf("  speedup        : %9.1fx\n\n", a0 / a3);

    // ---- per-stage ratios (rdtsc -- absolutes inflated, ratios OK) ----
    const pipe::StageNs s0 = stage_ns<pipe::PipelineV0>(feed, tpns);
    const pipe::StageNs s3 = stage_ns<pipe::PipelineV3>(feed, tpns);
    std::puts("=== per-stage (rdtsc attribution -- read the SHAPE, not absolutes) ===");
    std::printf("  %-8s  %12s  %12s  %10s\n", "stage", "v0 ns/tick", "v3 ns/tick", "ratio");
    std::printf("  %-8s  %12.1f  %12.1f  %9.1fx\n", "parse",  s0.parse,  s3.parse,  s0.parse  / s3.parse);
    std::printf("  %-8s  %12.1f  %12.1f  %9.1fx\n", "book",   s0.book,   s3.book,   s0.book   / s3.book);
    std::printf("  %-8s  %12.1f  %12.1f  %9.1fx\n", "signal", s0.signal, s3.signal, s0.signal / s3.signal);
    std::puts("  (v3 per-stage is dominated by the 4 rdtsc probes themselves --");
    std::puts("   ~80 ns of lfence+rdtsc bracketing > the actual work. Trust (A).)\n");

    // ---- 2. AGREEMENT: same order-fire ticks? ----
    pipe::PipelineV0 p0(feed);
    pipe::PipelineV3 p3(feed);
    for (std::size_t i = 0; i < n; ++i) {
        p0.set_index(i); p0.process_one(i, false);
        p3.set_index(i); p3.process_one(i, false);
    }
    const std::vector<std::size_t>& f0 = p0.fire_idx();
    const std::vector<std::size_t>& f3 = p3.fire_idx();

    std::puts("=== output agreement (order-fire ticks) ===");
    std::printf("  v0 fired %zu orders,  v3 fired %zu orders\n", f0.size(), f3.size());
    std::size_t match = 0;
    const std::size_t lim = std::min(f0.size(), f3.size());
    for (std::size_t i = 0; i < lim; ++i) if (f0[i] == f3[i]) ++match;
    std::printf("  positional matches: %zu / %zu\n", match, std::max(f0.size(), f3.size()));
    if (f0 == f3) {
        std::puts("  -> IDENTICAL. Fixed-point signal math reproduced float exactly here.");
    } else {
        std::puts("  -> DIVERGENCE. First few differing indices:");
        std::size_t shown = 0;
        for (std::size_t i = 0; i < lim && shown < 5; ++i) {
            if (f0[i] != f3[i]) { std::printf("     v0 @ %zu  vs  v3 @ %zu\n", f0[i], f3[i]); ++shown; }
        }
        std::puts("     (boundary float-rounding -- 09-fixed-point.md. Report, don't hide.)");
    }

    std::puts("\nKya seekha:");
    std::puts(" - Gate 1 (agreement) PEHLE. Fail -> optimization reject, chahe kitna");
    std::puts("   fast ho. Gate 2 (speedup) uske BAAD.");
    std::puts(" - ~80x end-to-end -- lekin yeh EK number hai. Lessons 13/14/15 isko");
    std::puts("   stage-by-stage todte: parse (hand-parse + fixed-point), book (flat");
    std::puts("   array + cached top), signal (ring + integer). Har ek ka apna delta.");
    return 0;
}
