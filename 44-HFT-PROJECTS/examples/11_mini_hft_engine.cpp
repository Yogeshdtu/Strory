// 11_mini_hft_engine.cpp
// ============================================================
// PROJECT 11 -- THE MINI HFT ENGINE.  MarketData -> Parser -> Book ->
// Strategy -> Risk -> OMS -> Venue -> fills -> PnL.
//
//   NaiveEngine     : venue = ExecutionSimulator (folder 40 ka std::map
//                     MatchingEngine -- correct/simple, "step 1")
//   OptimizedEngine : venue = FastVenue (flat-array aggregate book + IOC
//                     sweep -- "step 5", after profiling found the venue
//                     mirror was the bottleneck)
//
// Yeh file:
//   1. dono chalata -> per-stage latency budget + end-to-end p50/p99/p99.9
//   2. speedup + explanation (43/15)
//   3. CORRECTNESS GATE: dono ne SAME trading output diya? (fills, qty, pnl,
//      position) -- speedup se PEHLE (43/01)
//   4. DETERMINISM: optimized ko dobara chala, byte-compare
//   5. invariants: |position| <= risk max, zero sequence gaps
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 11_mini_hft_engine.cpp -o mini && ./mini
// ============================================================

#include "mh_engine.hpp"

#include <cstdio>

using namespace mhft;

template <class E>
static typename E::Stats run_engine(std::uint64_t n) {
    E e;
    return e.run(n);
}

template <class S>
static void print_budget(const char* tag, const S& s) {
    const double n = static_cast<double>(s.messages);
    const double tp = g_tpns();
    std::printf("  %-10s parse %6.1f  book %7.1f  strat %6.1f  risk %6.1f  oms %6.1f   (ns/msg)\n",
                tag, s.t_parse / tp / n, s.t_book / tp / n, s.t_strat / tp / n,
                s.t_risk / tp / n, s.t_oms / tp / n);
}

static void print_dist(const char* tag, std::vector<double> d) {
    std::sort(d.begin(), d.end());
    std::printf("  %-10s p50 %6.1f   p99 %7.1f   p99.9 %8.1f   max %9.1f   (ns, n=%zu)\n",
                tag, pct(d, 50), pct(d, 99), pct(d, 99.9), d.empty() ? 0.0 : d.back(), d.size());
}

template <class S>
static double mean_ns(const S& s) {
    double m = 0;
    for (double x : s.tick_ns) m += x;
    return m / static_cast<double>(s.tick_ns.size());
}

int main() {
    std::printf("ticks_per_ns = %.4f\n\n", g_tpns());
    constexpr std::uint64_t N = 200000;

    const auto sn = run_engine<NaiveEngine>(N);
    const auto so = run_engine<OptimizedEngine>(N);

    std::printf("run summary (N=%llu market-data messages, optimized):\n", (unsigned long long)N);
    std::printf("  signals=%llu  orders_ok=%llu  rate_drops=%llu  risk_rejects=%llu\n",
                (unsigned long long)so.signals, (unsigned long long)so.orders_ok,
                (unsigned long long)so.rate_drops, (unsigned long long)so.risk_rejects);
    std::printf("  fills=%llu  filled_qty=%llu  position=%lld  realized_pnl(scaled)=%lld  gaps=%lld\n\n",
                (unsigned long long)so.fills, (unsigned long long)so.filled_qty,
                (long long)so.position, (long long)so.realized_pnl, (long long)so.gap_count);

    std::puts("per-stage latency budget (rdtsc attribution -- ns/msg; probe cost inflates absolutes):");
    print_budget("naive", sn);
    print_budget("optimized", so);
    std::puts("");

    std::puts("end-to-end per-message latency:");
    print_dist("naive", sn.tick_ns);
    print_dist("optimized", so.tick_ns);

    const double mn = mean_ns(sn), mo = mean_ns(so);
    std::printf("\n  mean/msg: naive %.1f ns  ->  optimized %.1f ns   (%.1fx)\n", mn, mo, mn / mo);
    std::puts("  what changed: the venue mirror. naive = std::map MatchingEngine (a node");
    std::puts("  alloc + tree walk per market message); optimized = flat-array aggregate");
    std::puts("  book + O(levels-swept) IOC sweep. Same fills, no per-message allocation.");

    // ---- CORRECTNESS GATE (before speedup matters) ----
    const bool agree =
        sn.signals == so.signals && sn.orders_ok == so.orders_ok &&
        sn.fills   == so.fills   && sn.filled_qty == so.filled_qty &&
        sn.position == so.position && sn.realized_pnl == so.realized_pnl;
    std::printf("\n  correctness gate -- naive vs optimized trading output: %s\n",
                agree ? "IDENTICAL" : "*** DIVERGED ***");
    if (!agree)
        std::printf("    naive : fills=%llu qty=%llu pos=%lld pnl=%lld\n"
                    "    opt   : fills=%llu qty=%llu pos=%lld pnl=%lld\n",
                    (unsigned long long)sn.fills, (unsigned long long)sn.filled_qty,
                    (long long)sn.position, (long long)sn.realized_pnl,
                    (unsigned long long)so.fills, (unsigned long long)so.filled_qty,
                    (long long)so.position, (long long)so.realized_pnl);

    // ---- determinism ----
    const auto so2 = run_engine<OptimizedEngine>(N);
    const bool det =
        so.messages == so2.messages && so.signals == so2.signals &&
        so.orders_ok == so2.orders_ok && so.fills == so2.fills &&
        so.filled_qty == so2.filled_qty && so.position == so2.position &&
        so.realized_pnl == so2.realized_pnl && so.gap_count == so2.gap_count;
    std::printf("  determinism (optimized run x2): %s\n", det ? "IDENTICAL" : "*** MISMATCH ***");

    // ---- invariants ----
    const bool pos_ok = so.position <= 400 && so.position >= -400;
    std::printf("  invariant: |position| <= risk max (400)?  %s\n", pos_ok ? "held" : "VIOLATED");
    std::printf("  invariant: sequence gaps == 0?            %s\n",
                (so.gap_count == 0) ? "held" : "VIOLATED");

    const bool ok = agree && det && pos_ok && so.gap_count == 0;
    std::printf("\n%s\n", ok ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED");
    return ok ? 0 : 1;
}
