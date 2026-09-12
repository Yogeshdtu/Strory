// 05_memory_pool.cpp
// ============================================================
// PROJECT 5 -- FixedPool<T,N> vs new/delete.
//   1. correctness: distinct pointers, exhaustion -> nullptr, free -> reuse
//   2. latency: alloc+free cycle, p50/p99/p99.9/max vs global new/delete
// (36/02 ka concept; yahan capstone-integrated typed version.)
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 05_memory_pool.cpp -o mp && ./mp
// ============================================================

#include "mh_engine_common.hpp"
#include "mh_mem_pool.hpp"

#include <cstdio>
#include <unordered_set>

using namespace mhft;

struct Rec { std::uint64_t a, b, c, d; };   // 32 bytes

static void report(const char* tag, std::vector<double>& ns) {
    std::sort(ns.begin(), ns.end());
    std::printf("  %-18s p50 %5.1f   p99 %6.1f   p99.9 %7.1f   max %8.1f  ns\n",
                tag, pct(ns, 50), pct(ns, 99), pct(ns, 99.9), ns.empty() ? 0.0 : ns.back());
}

int main() {
    std::printf("ticks_per_ns = %.4f\n\n", g_tpns());

    // ---- correctness ----
    {
        FixedPool<Rec, 1024> pool;
        std::unordered_set<Rec*> seen;
        std::vector<Rec*> held;
        for (int i = 0; i < 1024; ++i) {
            Rec* p = pool.alloc();
            if (!p || !seen.insert(p).second) { std::puts("FAIL: null or duplicate"); return 1; }
            held.push_back(p);
        }
        if (pool.alloc() != nullptr)   { std::puts("FAIL: exhausted pool returned non-null"); return 1; }
        if (pool.in_use() != 1024)     { std::puts("FAIL: in_use wrong"); return 1; }
        for (Rec* p : held) pool.free(p);
        if (pool.in_use() != 0)        { std::puts("FAIL: in_use after free"); return 1; }
        Rec* again = pool.alloc();
        if (!again || seen.find(again) == seen.end()) { std::puts("FAIL: freed slot not reused"); return 1; }
        pool.free(again);
        std::puts("correctness: distinct ptrs, exhaustion -> nullptr, free -> reuse   PASS\n");
    }

    // ---- latency: alloc+free cycle ----
    constexpr int kIters = 200000;
    std::vector<double> pool_ns; pool_ns.reserve(kIters);
    std::vector<double> heap_ns; heap_ns.reserve(kIters);

    {
        FixedPool<Rec, 4096> pool;
        // pre-fill/free once to warm the free list
        Rec* w[64]; for (auto& x : w) x = pool.alloc();
        for (auto& x : w) pool.free(x);
        for (int i = 0; i < kIters; ++i) {
            const std::uint64_t a = tsc();
            Rec* p = pool.alloc();
            keep(p);
            pool.free(p);
            const std::uint64_t b = tsc();
            pool_ns.push_back(static_cast<double>(b - a) / g_tpns());
        }
    }
    for (int i = 0; i < kIters; ++i) {
        const std::uint64_t a = tsc();
        Rec* p = new Rec;
        keep(p);
        delete p;
        const std::uint64_t b = tsc();
        heap_ns.push_back(static_cast<double>(b - a) / g_tpns());
    }

    std::puts("alloc + free cycle latency (32-byte record):");
    report("FixedPool<Rec,N>", pool_ns);
    report("new/delete",       heap_ns);

    std::sort(pool_ns.begin(), pool_ns.end());
    std::sort(heap_ns.begin(), heap_ns.end());
    std::printf("\n  p50 speedup: %.1fx   p99.9 speedup: %.1fx\n",
                pct(heap_ns, 50) / pct(pool_ns, 50),
                pct(heap_ns, 99.9) / std::max(pct(pool_ns, 99.9), 0.1));
    std::puts("  pool: intrusive free-list pop/push, no syscall, no lock, no size classes.");
    std::puts("  trade-off: fixed capacity, one block size, per-thread, freed mem stays put.");
    return 0;
}
