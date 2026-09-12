// 06_object_pool.cpp
// ============================================================
// PROJECT 6 -- ObjectPool<T> with generation-checked handles.
//   1. reuse: release -> acquire reuses the slot
//   2. STALE HANDLE: a released handle's get() returns nullptr even after
//      the slot is recycled (the HFT use-after-free guard -- late ack/fill
//      on a recycled order slot must not land on the wrong order)
//   3. double-release is rejected
//   4. latency: acquire + release cycle
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 06_object_pool.cpp -o op && ./op
// ============================================================

#include "mh_engine_common.hpp"
#include "mh_object_pool.hpp"

#include <cstdio>

using namespace mhft;

struct Rec { std::uint64_t id = 0; std::int64_t px = 0; std::uint32_t qty = 0; };

int main() {
    std::printf("ticks_per_ns = %.4f\n\n", g_tpns());

    ObjectPool<Rec> pool(64);

    // 1. acquire / get / release
    auto h1 = pool.acquire();
    pool.get(h1)->id = 111;
    if (pool.get(h1)->id != 111) { std::puts("FAIL: get after acquire"); return 1; }
    if (pool.live() != 1)        { std::puts("FAIL: live count"); return 1; }

    const auto stale = h1;               // keep the old handle
    if (!pool.release(h1))       { std::puts("FAIL: release"); return 1; }
    if (pool.get(stale) != nullptr) { std::puts("FAIL: stale handle still resolves"); return 1; }
    if (pool.release(stale))     { std::puts("FAIL: double-release accepted"); return 1; }

    // 2. slot reuse + stale handle STILL rejected after recycle
    auto h2 = pool.acquire();            // should reuse h1's slot
    if (h2.idx != stale.idx)    { std::puts("FAIL: slot not reused"); return 1; }
    if (h2.gen == stale.gen)    { std::puts("FAIL: generation not bumped"); return 1; }
    pool.get(h2)->id = 222;
    if (pool.get(stale) != nullptr) { std::puts("FAIL: stale handle resolves after recycle"); return 1; }
    if (pool.get(h2)->id != 222)    { std::puts("FAIL: new handle broken"); return 1; }
    pool.release(h2);

    std::puts("correctness: reuse, stale-handle rejection (even post-recycle), no double-release   PASS\n");

    // 3. latency: acquire + release cycle
    constexpr int kIters = 200000;
    std::vector<double> ns; ns.reserve(kIters);
    for (int i = 0; i < kIters; ++i) {
        const std::uint64_t a = tsc();
        auto h = pool.acquire();
        keep(h);
        pool.release(h);
        const std::uint64_t b = tsc();
        ns.push_back(static_cast<double>(b - a) / g_tpns());
    }
    std::sort(ns.begin(), ns.end());
    std::printf("acquire + release cycle: p50 %.1f   p99 %.1f   p99.9 %.1f   max %.1f  ns\n",
                pct(ns, 50), pct(ns, 99), pct(ns, 99.9), ns.back());
    std::puts("(free-list index pop/push + one generation bump; slot storage preallocated.)");
    return 0;
}
