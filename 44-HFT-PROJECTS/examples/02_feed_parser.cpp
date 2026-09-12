// 02_feed_parser.cpp
// ============================================================
// PROJECT 2 -- Binary feed parser: v1 (simple, portable, safe) vs
// v3 (memcpy + bswap, frame-validated once).
//
//   1. AGREEMENT gate: dono N frames pe EXACT same MdMessage dete (43/01)
//   2. throughput: dono ka ns/frame -O2 pe measured
//
// (Yeh capstone context mein 38/06 ka polished version hai.)
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 02_feed_parser.cpp -o fp && ./fp
// ============================================================

#include "mh_engine_common.hpp"

#include <cstdio>

using namespace mhft;

template <class F>
static double bench(F f, int reps = 25) {
    double best = 1e300;
    for (int r = 0; r < reps; ++r) {
        const std::uint64_t a = tsc();
        f();
        const std::uint64_t b = tsc();
        best = std::min(best, static_cast<double>(b - a));
    }
    return best;
}

int main() {
    std::printf("ticks_per_ns = %.4f\n\n", g_tpns());
    constexpr std::uint64_t N = 300000;

    MarketDataSimulator sim(44);
    std::vector<std::uint8_t> wire = record_wire(sim, N);
    const std::uint64_t frames = wire.size() / kWireSize;
    std::printf("recorded %llu frames (%zu bytes)\n", (unsigned long long)frames, wire.size());

    // ---- agreement ----
    std::uint64_t mism = 0;
    for (std::uint64_t i = 0; i < frames; ++i) {
        const std::uint8_t* p = wire.data() + i * kWireSize;
        MdMessage a{}, b{};
        parse_v1(p, kWireSize, a);
        parse_v3(p, b);
        if (a.seq != b.seq || a.ts != b.ts || a.type != b.type || a.side != b.side ||
            a.order_id != b.order_id || a.px != b.px || a.qty != b.qty) ++mism;
    }
    std::printf("agreement : %llu frames, %llu mismatches -> %s\n\n",
                (unsigned long long)frames, (unsigned long long)mism,
                mism == 0 ? "IDENTICAL" : "*** DIVERGED ***");

    // ---- throughput ----
    volatile std::uint64_t sink = 0;
    const double t1 = bench([&] {
        MdMessage m{};
        for (std::uint64_t i = 0; i < frames; ++i) {
            parse_v1(wire.data() + i * kWireSize, kWireSize, m);
            sink += m.qty;
        }
    });
    const double t3 = bench([&] {
        MdMessage m{};
        for (std::uint64_t i = 0; i < frames; ++i) {
            parse_v3(wire.data() + i * kWireSize, m);
            sink += m.qty;
        }
    });
    keep(sink);
    const double n = static_cast<double>(frames);
    std::printf("parse_v1 (shift-and-or, bounds-checked) : %5.2f ns/frame\n", t1 / g_tpns() / n);
    std::printf("parse_v3 (memcpy + bswap, frame-checked): %5.2f ns/frame   (%.2fx)\n",
                t3 / g_tpns() / n, t1 / t3);

    std::puts("\nKya seekha: dono BE-correct aur same output. v3 tez kyunki per-field");
    std::puts("shift-loop ki jagah ek memcpy+bswap (compiler aksar `movbe`/`bswap` mein");
    std::puts("fold kar deta) aur length-check hot loop se bahar. Real feed pe farak");
    std::puts("chhota jab tak parse poore pipeline ka bada hissa na ho (Amdahl -- 43/03).");

    return mism == 0 ? 0 : 1;
}
