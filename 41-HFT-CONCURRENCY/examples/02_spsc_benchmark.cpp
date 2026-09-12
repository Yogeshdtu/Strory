// 02_spsc_benchmark.cpp
// ============================================================
// SpscQueue<T,N> -- rigorous measurement: throughput (blast pass) AND
// per-message hand-off LATENCY distribution (paced pass, so the queue
// stays shallow and we're measuring hand-off latency, not queue-depth
// latency). rdtsc harness -- SAME idiom as 35/36/38/39/40.
// (04-spsc-queue-for-pipeline.md)
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 02_spsc_benchmark.cpp -o spscbench && ./spscbench
// ============================================================

#include "spsc_queue.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>
#include <x86intrin.h>

namespace ch = std::chrono;
static inline std::uint64_t tsc() { _mm_lfence(); auto t = __rdtsc(); _mm_lfence(); return t; }
static double g_tpns = 1.0;

struct Msg { std::uint64_t seq; std::uint64_t t_tsc; };

constexpr std::size_t   kCap    = 1u << 12;
constexpr std::uint64_t kThru   = 20'000'000;   // throughput pass -- blast
constexpr std::uint64_t kLat    = 500'000;      // latency pass -- paced
constexpr std::uint64_t kPaceTicks = 4000;      // ~2us @ ~2GHz -- keeps queue shallow

static void report(std::vector<double>& ns) {
    std::sort(ns.begin(), ns.end());
    auto pc = [&](double p) {
        std::size_t i = static_cast<std::size_t>(p / 100.0 * static_cast<double>(ns.size()));
        return ns[std::min(i, ns.size() - 1)];
    };
    std::printf("  hand-off latency: p50 %6.1f   p99 %8.1f   p99.9 %9.1f   max %10.1f  ns  (n=%zu)\n",
                pc(50), pc(99), pc(99.9), ns.back(), ns.size());
}

int main() {
    {
        auto c0 = ch::steady_clock::now();
        std::uint64_t r0 = tsc();
        volatile std::uint64_t s = 0;
        while (ch::duration_cast<ch::milliseconds>(ch::steady_clock::now() - c0).count() < 120) s = s + 1;
        std::uint64_t r1 = tsc();
        auto c1 = ch::steady_clock::now();
        g_tpns = static_cast<double>(r1 - r0)
               / static_cast<double>(ch::duration_cast<ch::nanoseconds>(c1 - c0).count());
    }
    std::printf("ticks_per_ns = %.4f\n\n", g_tpns);

    static SpscQueue<Msg, kCap> q;
    std::atomic<bool> go{false}, done{false};
    std::vector<double> lat_ns; lat_ns.reserve(kLat);

    std::thread consumer([&] {
        while (!go.load(std::memory_order_acquire)) {}
        Msg m;
        while (!done.load(std::memory_order_relaxed)) {
            if (q.try_pop(m)) {
                const double dt = static_cast<double>(tsc() - m.t_tsc) / g_tpns;
                lat_ns.push_back(dt);
            }
        }
        while (q.try_pop(m)) {
            const double dt = static_cast<double>(tsc() - m.t_tsc) / g_tpns;
            lat_ns.push_back(dt);
        }
    });

    // ---- throughput pass: blast kThru messages as fast as possible ----
    auto t0 = ch::steady_clock::now();
    go.store(true, std::memory_order_release);
    for (std::uint64_t i = 0; i < kThru; ++i) {
        const Msg m{i, tsc()};
        while (!q.try_push(m)) {}
    }
    const double thru_sec = ch::duration<double>(ch::steady_clock::now() - t0).count();
    lat_ns.clear();   // throughput-pass hand-off times aren't representative -- drop them

    // ---- latency pass: paced, queue stays ~empty, measures pure hand-off ----
    std::uint64_t next = tsc();
    for (std::uint64_t i = 0; i < kLat; ++i) {
        next += kPaceTicks;
        while (tsc() < next) {}
        const Msg m{kThru + i, tsc()};
        while (!q.try_push(m)) {}
    }
    while (lat_ns.size() < kLat) {}   // let consumer drain the paced pass
    done.store(true, std::memory_order_relaxed);
    consumer.join();

    std::printf("Throughput (blast, %llu msgs): %.1f M msg/s\n",
                static_cast<unsigned long long>(kThru),
                static_cast<double>(kThru) / thru_sec / 1e6);
    report(lat_ns);

    return 0;
}
