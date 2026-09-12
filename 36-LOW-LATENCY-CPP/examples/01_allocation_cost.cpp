// 01_allocation_cost.cpp
// ============================================================
// "Hot path pe `new`/`delete` = disaster" — yeh MEASURED proof hai.
// Throughput nahi, LATENCY DISTRIBUTION dekho: p50 theek lag sakta,
// par p99.9 aur max wahan hai jahan `malloc` free-list walk / `mmap` /
// lock / coalesce karta.
//
//   3 workloads:
//     A. fixed 64 B new/delete, tang loop
//     B. fixed 64 KB new/delete
//     C. mixed-size churn (8..8192 B, random) — realistic aur sabse bura
//
// Trade-off yaad rakho: general-purpose allocator throughput ke liye
// tuned hai (tcmalloc/jemalloc/glibc), tail ke liye nahi. HFT hot path
// pe pool/arena (examples 02–04).
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 01_allocation_cost.cpp -o a && ./a
// ============================================================

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>
#include <x86intrin.h>

namespace ch = std::chrono;

template <class T>
static inline void keep(T& v) { asm volatile("" : "+r,m"(v) : : "memory"); }
static inline std::uint64_t tsc() { _mm_lfence(); auto t = __rdtsc(); _mm_lfence(); return t; }

static double g_tpns = 1.0;

static void report(const char* tag, std::vector<double>& ns) {
    std::sort(ns.begin(), ns.end());
    auto pc = [&](double p) {
        std::size_t i = static_cast<std::size_t>(p / 100.0 * static_cast<double>(ns.size()));
        return ns[std::min(i, ns.size() - 1)];
    };
    std::printf("  %-22s p50 %6.1f   p99 %7.1f   p99.9 %8.1f   max %9.1f  ns\n",
                tag, pc(50), pc(99), pc(99.9), ns.back());
}

int main() {
    // ---- calibrate ticks->ns -------------------------------------
    {
        auto c0 = ch::steady_clock::now();
        std::uint64_t r0 = tsc();
        volatile std::uint64_t s = 0;
        while (ch::duration_cast<ch::milliseconds>(ch::steady_clock::now() - c0).count() < 120)
            s = s + 1;
        std::uint64_t r1 = tsc();
        auto c1 = ch::steady_clock::now();
        g_tpns = static_cast<double>(r1 - r0)
               / static_cast<double>(ch::duration_cast<ch::nanoseconds>(c1 - c0).count());
    }
    std::printf("ticks_per_ns = %.4f\n\n", g_tpns);

    constexpr int N = 200000;
    std::mt19937 rng(12345);

    // ---- A. fixed 64 B ----------------------------------------
    {
        std::vector<double> lat;
        lat.reserve(N);
        // warm the allocator
        for (int i = 0; i < 1000; ++i) { auto* p = new std::byte[64]; keep(p); delete[] p; }
        for (int i = 0; i < N; ++i) {
            std::uint64_t t0 = tsc();
            auto* p = new std::byte[64];
            std::uint64_t t1 = tsc();
            keep(p);
            delete[] p;
            lat.push_back(static_cast<double>(t1 - t0) / g_tpns);
        }
        report("new[64] + delete", lat);            // delete not timed; alloc is the spiky one
    }

    // ---- B. fixed 64 KB ---------------------------------------
    {
        std::vector<double> lat;
        lat.reserve(N);
        for (int i = 0; i < 200; ++i) { auto* p = new std::byte[65536]; keep(p); delete[] p; }
        for (int i = 0; i < N; ++i) {
            std::uint64_t t0 = tsc();
            auto* p = new std::byte[65536];
            std::uint64_t t1 = tsc();
            keep(p);
            delete[] p;
            lat.push_back(static_cast<double>(t1 - t0) / g_tpns);
        }
        report("new[64 KB]", lat);
    }

    // ---- C. mixed-size churn (keep some live) ----------------
    {
        std::vector<double> lat;
        lat.reserve(N);
        std::vector<std::byte*> live;
        constexpr std::size_t TARGET = 2048;
        live.reserve(TARGET + 1);
        std::uniform_int_distribution<int> sz(8, 8192);
        for (int i = 0; i < N; ++i) {
            if (live.size() >= TARGET) {
                // steady state: free one (random position), then alloc one
                std::size_t k = rng() % live.size();
                delete[] live[k];
                live[k] = live.back();
                live.pop_back();
            }
            std::size_t n = static_cast<std::size_t>(sz(rng));
            std::uint64_t t0 = tsc();
            auto* p = new std::byte[n];
            std::uint64_t t1 = tsc();
            p[0] = std::byte{1};
            keep(p);
            live.push_back(p);
            lat.push_back(static_cast<double>(t1 - t0) / g_tpns);
        }
        for (auto* p : live) delete[] p;
        report("mixed 8..8192 B churn", lat);
    }

    std::puts("\nKya seekha:");
    std::puts(" - p50 chhota (~tens of ns) — free-list fast path.");
    std::puts(" - p99.9 / max bahut bade — mmap/munmap, free-list walk, coalesce,");
    std::puts("   arena lock (multi-thread mein aur bura). Yeh tail HFT mein ek");
    std::puts("   missed tick hai.");
    std::puts(" - Mixed-size churn sabse jittery — fragmentation + size-class hops.");
    std::puts(" - Fix: hot path pe zero allocation — pre-allocate + pool/arena/ring");
    std::puts("   (examples 02–04, 09). Allocation ko startup pe / cold thread pe daalo.");
    return 0;
}
