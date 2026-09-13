// 06_allocation_benchmark.cpp
// ============================================================
// Allocation LATENCY DISTRIBUTION -- p50 / p99 / p99.9 / max  (HFT style)
// ============================================================
//   BENCHMARK -- -O2 ZAROORI:
//     g++ -std=c++20 -O2 -march=native 06_allocation_benchmark.cpp -o ab && ./ab
//     ya:  .\build.ps1 fast 14-MEMORY/examples/06_allocation_benchmark.cpp
// ============================================================
//   Average latency jhoot bolti hai. HFT mein TAIL matter karta hai:
//   p50 (median) 30 ns ho sakta hai, par p99.9 5000 ns -- aur wahi ek
//   allocation aapko poore tick pe pichhe kar deti hai.
//
//   Yeh 3 cheezein measure karta hai:
//     A) new + delete (block turant free)      -- allocator ka "hot" path
//     B) new, blocks retain (kabhi free nahi)  -- allocator ko grow karna padta
//     C) ek fixed buffer se reuse (pool jaisa) -- baseline: koi allocator nahi
//
//   rdtsc se timing (100 ns OS clock allocation fast-path ke liye mota hai).
// ============================================================

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <x86intrin.h>

static inline std::uint64_t rdtsc() {
    _mm_lfence();
    std::uint64_t t = __rdtsc();
    _mm_lfence();
    return t;
}

// TSC ko ns mein badalne ke liye calibrate karo
static double ns_per_cycle() {
    using Clock = std::chrono::steady_clock;
    auto c0 = rdtsc();
    auto w0 = Clock::now();
    // ~150 ms busy wait
    while (std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - w0).count() < 150) { }
    auto c1 = rdtsc();
    auto w1 = Clock::now();
    double ns = static_cast<double>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(w1 - w0).count());
    return ns / static_cast<double>(c1 - c0);
}

static void report(const char* name, std::vector<double>& ns) {
    std::sort(ns.begin(), ns.end());
    auto pct = [&](double p) {
        std::size_t idx = static_cast<std::size_t>(p / 100.0 * static_cast<double>(ns.size() - 1));
        return ns[idx];
    };
    double sum = 0;
    for (double v : ns) sum += v;
    std::printf("  %-26s  n=%zu\n", name, ns.size());
    std::printf("     mean %8.1f | p50 %8.1f | p90 %8.1f | p99 %8.1f | p99.9 %9.1f | max %10.1f  (ns)\n",
                sum / static_cast<double>(ns.size()),
                pct(50), pct(90), pct(99), pct(99.9), ns.back());
}

int main() {
    const double npc = ns_per_cycle();
    const std::size_t SZ   = 64;        // block size (bytes)
    const int         REPS = 200000;

    // rdtsc pair ka apna overhead
    std::uint64_t ovh = ~std::uint64_t{0};
    for (int i = 0; i < 5000; ++i) {
        auto a = rdtsc(); auto b = rdtsc();
        ovh = std::min(ovh, b - a);
    }

    auto cyc_to_ns = [&](std::uint64_t c) {
        double x = (c > ovh) ? static_cast<double>(c - ovh) : 0.0;
        return x * npc;
    };

    std::printf("calibration: %.4f ns/cycle   rdtsc overhead: %llu cycles   block: %zu B\n\n",
                npc, static_cast<unsigned long long>(ovh), SZ);

    // ---- A) new + immediate delete ----
    {
        std::vector<double> lat;
        lat.reserve(static_cast<std::size_t>(REPS));
        volatile std::uint8_t sinkv = 0;
        for (int i = 0; i < REPS; ++i) {
            auto t0 = rdtsc();
            auto* p = new std::uint8_t[SZ];
            auto t1 = rdtsc();
            p[0] = static_cast<std::uint8_t>(i);
            sinkv = static_cast<std::uint8_t>(sinkv + p[0]);
            delete[] p;
            lat.push_back(cyc_to_ns(t1 - t0));
        }
        (void)sinkv;
        report("A) new+delete (reuse)", lat);
    }

    // ---- B) new, retain all (allocator must grow) ----
    {
        std::vector<double> lat;
        lat.reserve(static_cast<std::size_t>(REPS));
        std::vector<std::uint8_t*> keep;
        keep.reserve(static_cast<std::size_t>(REPS));
        for (int i = 0; i < REPS; ++i) {
            auto t0 = rdtsc();
            auto* p = new std::uint8_t[SZ];
            auto t1 = rdtsc();
            p[0] = static_cast<std::uint8_t>(i);
            keep.push_back(p);
            lat.push_back(cyc_to_ns(t1 - t0));
        }
        for (auto* p : keep) delete[] p;
        report("B) new, retained (grow)", lat);
    }

    // ---- C) fixed buffer reuse (no allocator at all) ----
    {
        std::vector<double> lat;
        lat.reserve(static_cast<std::size_t>(REPS));
        alignas(16) std::uint8_t buffer[SZ];
        volatile std::uint8_t sinkv = 0;
        for (int i = 0; i < REPS; ++i) {
            auto t0 = rdtsc();
            std::uint8_t* p = buffer;             // "allocation" = ek pointer
            auto t1 = rdtsc();
            p[0] = static_cast<std::uint8_t>(i);
            sinkv = static_cast<std::uint8_t>(sinkv + p[0]);
            lat.push_back(cyc_to_ns(t1 - t0));
        }
        (void)sinkv;
        report("C) fixed buffer (pool-ish)", lat);
    }

    std::printf(
        "\n  Padhne ka tareeka:\n"
        "  - C (kuch nahi karta) ka p50/p99.9 ~flat (10-20 ns = do lfence+rdtsc ka apna cost).\n"
        "  - A / B ka MEDIAN bhi theek (tens of ns, C ka floor samet) -- par p99.9 aur max bahut upar\n"
        "    (max: tens of us se milliseconds tak, run-variable -- OS se memory\n"
        "     maangi / heap lock / page fault).\n"
        "  - Wahi ek tail spike HFT hot path pe allowed nahi -> pre-alloc / pool / arena\n"
        "    (file 08, 10, aur 07_simple_pool.cpp).\n"
        "  (C ka lone 'max' outlier bhi dikh sakta hai -- woh allocation nahi, measuring\n"
        "   thread ko OS ne preempt kiya. Measurement ki bhi apni tail hoti hai.)\n");
    return 0;
}
