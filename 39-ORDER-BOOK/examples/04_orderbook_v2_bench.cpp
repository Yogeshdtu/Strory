// 04_orderbook_v2_bench.cpp
// ============================================================
// V2 (sorted vector) ki per-operation LATENCY DISTRIBUTION --
// (06-measuring-v2.md). SAME workload as 02 -- direct comparison.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 04_orderbook_v2_bench.cpp -o v2b && ./v2b
// ============================================================

#include "order_workload.hpp"
#include "orderbook_v2_vector.hpp"

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
    std::printf("  %-22s p50 %6.1f   p99 %8.1f   p99.9 %9.1f   max %10.1f  ns  (n=%zu)\n",
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

    constexpr std::size_t N = 200000;
    const auto ops = generate_workload(N, /*seed=*/999);   // SAME seed as 02

    BookV2 book;
    std::vector<double> ns_all, ns_add, ns_reduce;
    ns_all.reserve(N);

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
        const double dt = static_cast<double>(t1 - t0) / g_tpns;
        ns_all.push_back(dt);
        if (op.kind == OpKind::Add) ns_add.push_back(dt);
        else if (op.kind == OpKind::Execute || op.kind == OpKind::Cancel) ns_reduce.push_back(dt);
    }

    std::printf("=== BookV2 (sorted vector + deque + unordered_map) ===\n");
    report("ALL ops", ns_all);
    report("Add only", ns_add);
    report("Execute/Cancel only", ns_reduce);
    std::printf("\nfinal order_count = %zu\n", book.order_count());

    return 0;
}
