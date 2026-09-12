// 01_baseline_pipeline.cpp
// ============================================================
// BASELINE (v0) -- jaan-boojh kar naive tick-to-order pipeline.
// Yeh folder 43 ka "measure FIRST" step hai: pehle ek honest,
// reproducible baseline number, PHIR profile (02), PHIR optimize (03).
//
// Pipeline (per market-data message):
//   parse   : std::string substr + std::stod/std::stoi
//   book    : std::map<double> bids/asks + std::list per level, std::map id-index
//   signal  : mid = (bb+ba)/2 ; SMA over last 64 mids (deque, RE-SUM every tick)
//   encode  : signal fire -> std::string order via snprintf + push_back
//
// DO measurements:
//   (A) "true" per-tick average  -- ONE steady_clock around a NO-rdtsc loop
//       (measurement khud latency add nahi karta)
//   (B) per-stage attribution    -- 4 rdtsc checkpoints/tick. rdtsc ki apni
//       cost absolutes ko INFLATE karti -> yeh sirf RATIO ke liye padho.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 01_baseline_pipeline.cpp -o base && ./base
//   (benchmarks: -O2 mandatory. -O0 pe yeh number bekaar hai.)
// ============================================================

#include "pipeline.hpp"

#include <chrono>
#include <cstdio>
#include <vector>

namespace ch = std::chrono;
using pipe::tsc;

int main() {
    const double tpns = pipe::calibrate_tpns();
    std::printf("ticks_per_ns = %.4f\n", tpns);

    const pipe::Feed feed = pipe::make_feed(120000);
    std::printf("feed: %zu msgs  (%zu add, %zu cancel)  blob=%zu bytes  max_id=%u\n\n",
                feed.off.size() - 1, feed.n_add, feed.n_cancel, feed.blob.size(), feed.max_id);

    // ---- (A) TRUE per-tick average: no rdtsc inside the loop ----
    {
        pipe::PipelineV0 p(feed);
        const std::size_t n = feed.off.size() - 1;
        const auto t0 = ch::steady_clock::now();
        for (std::size_t i = 0; i < n; ++i) {
            p.set_index(i);
            p.process_one(i, /*measure=*/false);
        }
        const double sec = ch::duration<double>(ch::steady_clock::now() - t0).count();
        std::printf("(A) TRUE per-tick average (clean loop, 1 clock) : %8.1f ns/tick\n",
                    sec * 1e9 / static_cast<double>(n));
        std::printf("    total wall = %.3f ms   orders fired = %zu\n\n",
                    sec * 1e3, p.orders());
    }

    // ---- (B) per-stage attribution + per-tick distribution ----
    {
        pipe::PipelineV0 p(feed);
        const std::size_t n = feed.off.size() - 1;
        std::vector<double> per_tick;   // not collected here -- ns_ accumulators used
        for (std::size_t i = 0; i < n; ++i) {
            p.set_index(i);
            p.process_one(i, /*measure=*/true);
        }
        const pipe::StageNs& s = p.stage_ticks();
        const double parse_ns  = s.parse  / tpns / static_cast<double>(s.ticks);
        const double book_ns   = s.book   / tpns / static_cast<double>(s.ticks);
        const double sig_ns    = s.signal / tpns / static_cast<double>(s.ticks);
        const double tot_ns    = s.total  / tpns / static_cast<double>(s.ticks);

        std::printf("(B) per-stage attribution (rdtsc -- ABSOLUTES inflated, read RATIOS)\n");
        std::printf("    parse   : %8.1f ns/tick   (%.0f%%)\n", parse_ns, 100.0 * parse_ns / tot_ns);
        std::printf("    book    : %8.1f ns/tick   (%.0f%%)\n", book_ns,  100.0 * book_ns  / tot_ns);
        std::printf("    signal  : %8.1f ns/tick   (%.0f%%)\n", sig_ns,   100.0 * sig_ns   / tot_ns);
        std::printf("    -------------------------------------\n");
        std::printf("    sum     : %8.1f ns/tick   (rdtsc-inflated total)\n\n", tot_ns);
    }

    std::puts("Kya seekha:");
    std::puts(" - (A) is the number you COMMIT as the baseline. Reproduce it before");
    std::puts("   every change (02-establishing-baseline.md).");
    std::puts(" - (B) says WHERE the time goes -- parse dominates (std::string +");
    std::puts("   std::stod), book is next (std::map node chase). Signal is cheap.");
    std::puts(" - Ab: 02_profile_analysis.sh se confirm karo (perf), phir");
    std::puts("   03_optimized_pipeline.cpp -- SAME feed, same measurement.");
    return 0;
}
