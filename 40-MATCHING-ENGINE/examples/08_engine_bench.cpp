// 08_engine_bench.cpp
// ============================================================
// Throughput + per-submit() latency distribution -- REAL numbers,
// -O2 pe measured (16-benchmarking.md). rdtsc harness -- SAME idiom
// jo 35/36/38/39 mein use hua.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 08_engine_bench.cpp -o ebench && ./ebench
// ============================================================

#include "engine_workload.hpp"
#include "matching_engine.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>
#include <x86intrin.h>

namespace ch = std::chrono;
static inline std::uint64_t tsc() { _mm_lfence(); auto t = __rdtsc(); _mm_lfence(); return t; }
static double g_tpns = 1.0;

template <class T> static inline void keep(T& v) { asm volatile("" : "+r,m"(v) : : "memory"); }

static void report(const char* tag, std::vector<double>& ns) {
    if (ns.empty()) { std::printf("  %-24s (no samples)\n", tag); return; }
    std::sort(ns.begin(), ns.end());
    auto pc = [&](double p) {
        std::size_t i = static_cast<std::size_t>(p / 100.0 * static_cast<double>(ns.size()));
        return ns[std::min(i, ns.size() - 1)];
    };
    std::printf("  %-24s p50 %7.1f   p99 %9.1f   p99.9 %10.1f   max %10.1f  ns  (n=%zu)\n",
                tag, pc(50), pc(99), pc(99.9), ns.back(), ns.size());
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

    constexpr std::size_t N = 150000;
    const auto cmds = generate_commands(N, /*seed=*/555);

    MatchingEngine engine;
    std::vector<double> ns_all, ns_submit, ns_cancel;
    std::vector<double> ns_limit, ns_market, ns_ioc, ns_fok;
    ns_all.reserve(N);

    std::size_t total_trades = 0;

    for (const auto& c : cmds) {
        const auto t0 = tsc();
        if (c.kind == CmdKind::Submit) {
            auto r = engine.submit(c.order);
            keep(r);
            total_trades += r.trades.size();
        } else {
            bool ok = engine.cancel(c.cancel_id);
            keep(ok);
        }
        const auto t1 = tsc();
        const double dt = static_cast<double>(t1 - t0) / g_tpns;
        ns_all.push_back(dt);

        if (c.kind == CmdKind::Cancel) {
            ns_cancel.push_back(dt);
        } else {
            ns_submit.push_back(dt);
            switch (c.order.type) {
                case OrderType::Limit:  ns_limit.push_back(dt);  break;
                case OrderType::Market: ns_market.push_back(dt); break;
                case OrderType::IOC:    ns_ioc.push_back(dt);    break;
                case OrderType::FOK:    ns_fok.push_back(dt);    break;
            }
        }
    }

    std::printf("=== MatchingEngine (std::map + std::list + unordered_map index) ===\n");
    report("ALL ops", ns_all);
    report("submit() only", ns_submit);
    report("  submit: Limit", ns_limit);
    report("  submit: Market", ns_market);
    report("  submit: IOC", ns_ioc);
    report("  submit: FOK", ns_fok);
    report("cancel() only", ns_cancel);

    std::printf("\ntotal commands = %zu, total trades generated = %zu\n", N, total_trades);
    std::printf("final: resting_count=%zu", engine.resting_count());
    if (engine.has_bid()) std::printf(", best_bid=%lld", static_cast<long long>(engine.best_bid()));
    if (engine.has_ask()) std::printf(", best_ask=%lld", static_cast<long long>(engine.best_ask()));
    std::printf("\n");

    // Throughput -- pure sum of all per-op times as a simple, honest
    // "single-threaded ops/sec" estimate (36-LOW-LATENCY-CPP's caution:
    // yeh microbenchmark hai, real feed burstiness/network shamil nahi).
    double total_ns = 0.0;
    for (double v : ns_all) total_ns += v;
    const double ops_per_sec = static_cast<double>(N) / (total_ns / 1e9);
    std::printf("throughput (single-threaded, this workload mix) ~= %.2f million ops/sec\n",
                ops_per_sec / 1e6);

    return 0;
}
