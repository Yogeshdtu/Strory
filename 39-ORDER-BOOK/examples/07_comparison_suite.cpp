// 07_comparison_suite.cpp
// ============================================================
// V1 vs V2 vs V3 -- SAME workload, SAME run, side by side. (14-measuring-
// v3.md, 15-what-changed-and-why.md ka source data.)
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 07_comparison_suite.cpp -o cmp && ./cmp
// ============================================================

#include "order_workload.hpp"
#include "orderbook_v1_map.hpp"
#include "orderbook_v2_vector.hpp"
#include "orderbook_v3_flat.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>
#include <x86intrin.h>

namespace ch = std::chrono;
static inline std::uint64_t tsc() { _mm_lfence(); auto t = __rdtsc(); _mm_lfence(); return t; }
static double g_tpns = 1.0;

static void report(const char* tag, std::vector<double>& ns) {
    std::sort(ns.begin(), ns.end());
    auto pc = [&](double p) {
        std::size_t i = static_cast<std::size_t>(p / 100.0 * static_cast<double>(ns.size()));
        return ns[std::min(i, ns.size() - 1)];
    };
    std::printf("  %-30s p50 %6.1f   p99 %8.1f   p99.9 %9.1f   max %10.1f  ns\n",
                tag, pc(50), pc(99), pc(99.9), ns.back());
}

template <class Book>
static std::vector<double> run(Book& book, const std::vector<Op>& ops) {
    std::vector<double> ns;
    ns.reserve(ops.size());
    for (const auto& op : ops) {
        const auto t0 = tsc();
        switch (op.kind) {
            case OpKind::Add:     book.add(op.order_id, op.is_buy, op.price_ticks, op.qty); break;
            case OpKind::Execute: book.reduce(op.order_id, op.qty); break;
            case OpKind::Cancel:  book.reduce(op.order_id, op.qty); break;
            case OpKind::Delete:  book.remove(op.order_id); break;
            case OpKind::Replace: book.replace(op.order_id, op.new_order_id, op.is_buy,
                                                 op.price_ticks, op.qty); break;
        }
        const auto t1 = tsc();
        ns.push_back(static_cast<double>(t1 - t0) / g_tpns);
    }
    return ns;
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

    constexpr std::size_t N = 200000;
    const auto ops = generate_workload(N, /*seed=*/999);   // SAME as 02/04/06

    BookV1 v1;
    BookV2 v2;
    BookV3 v3(/*center=*/10000, /*max_orders=*/150000);

    auto ns1 = run(v1, ops);
    auto ns2 = run(v2, ops);
    auto ns3 = run(v3, ops);

    std::printf("=== V1 vs V2 vs V3 -- %zu ops, same workload ===\n\n", N);
    report("V1 std::map+list+unordered_map", ns1);
    report("V2 sorted vector+deque",         ns2);
    report("V3 flat array+intrusive+hash",   ns3);

    // ---- correctness cross-check: same workload -> IDENTICAL final state? ----
    std::printf("\n=== Cross-version correctness ===\n");
    std::printf("order_count: V1=%zu V2=%zu V3=%zu  match? %s\n",
                v1.order_count(), v2.order_count(), v3.order_count(),
                (v1.order_count() == v2.order_count() && v2.order_count() == v3.order_count()) ? "haan" : "NAHI (BUG!)");
    std::printf("best_bid:    V1=%lld V2=%lld V3=%lld  match? %s\n",
                static_cast<long long>(v1.best_bid()), static_cast<long long>(v2.best_bid()),
                static_cast<long long>(v3.best_bid()),
                (v1.best_bid() == v2.best_bid() && v2.best_bid() == v3.best_bid()) ? "haan" : "NAHI (BUG!)");
    std::printf("best_ask:    V1=%lld V2=%lld V3=%lld  match? %s\n",
                static_cast<long long>(v1.best_ask()), static_cast<long long>(v2.best_ask()),
                static_cast<long long>(v3.best_ask()),
                (v1.best_ask() == v2.best_ask() && v2.best_ask() == v3.best_ask()) ? "haan" : "NAHI (BUG!)");
    std::printf("best_bid_qty: V1=%u V2=%u V3=%u  match? %s\n",
                v1.best_bid_qty(), v2.best_bid_qty(), v3.best_bid_qty(),
                (v1.best_bid_qty() == v2.best_bid_qty() && v2.best_bid_qty() == v3.best_bid_qty()) ? "haan" : "NAHI (BUG!)");

    // ---- ratios ----
    std::sort(ns1.begin(), ns1.end());
    std::sort(ns3.begin(), ns3.end());
    auto p999 = [](std::vector<double>& v) { return v[static_cast<std::size_t>(0.999 * static_cast<double>(v.size()))]; };
    std::printf("\np99.9 ratio V1/V3 = %.1fx\n", p999(ns1) / p999(ns3));

    return 0;
}
