// 03_optimized_pipeline.cpp
// ============================================================
// OPTIMIZED (v3) -- SAME feed, SAME measurement harness as 01.
// Sirf ISKO chalane se optimization "sahi" nahi hoti -- 04_before_after.cpp
// dono ko back-to-back chala kar (a) speedup aur (b) OUTPUT AGREEMENT
// dono verify karta. Yeh file wahi (A)+(B) format deta jo 01 deta hai,
// taaki number-to-number compare seedha ho.
//
// v0 -> v3 mein kya badla:
//   parse : std::string substr + std::stod   ->  const char* hand-parse,
//           price ko FIXED-POINT int64 (x100) -- koi float nahi
//   book  : std::map<double> + std::list      ->  flat array[level] of qty
//           + cached best-bid/best-ask index (top-of-book O(1))
//           + id -> location: std::map        ->  flat std::vector (ids dense)
//   signal: std::deque + re-sum + /2, /W      ->  ring buffer + running sum,
//           mid = (bb+ba)>>1 equiv, threshold check ALL-INTEGER (no divide)
//   encode: std::string + push_back (alloc)   ->  fixed-layout POD, no alloc
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 03_optimized_pipeline.cpp -o opt && ./opt
// ============================================================

#include "pipeline.hpp"

#include <chrono>
#include <cstdio>

namespace ch = std::chrono;

int main() {
    const double tpns = pipe::calibrate_tpns();
    std::printf("ticks_per_ns = %.4f\n", tpns);

    const pipe::Feed feed = pipe::make_feed(120000);
    std::printf("feed: %zu msgs  (%zu add, %zu cancel)  blob=%zu bytes\n\n",
                feed.off.size() - 1, feed.n_add, feed.n_cancel, feed.blob.size());

    // ---- (A) TRUE per-tick average ----
    {
        pipe::PipelineV3 p(feed);
        const std::size_t n = feed.off.size() - 1;
        const auto t0 = ch::steady_clock::now();
        for (std::size_t i = 0; i < n; ++i) {
            p.set_index(i);
            p.process_one(i, false);
        }
        const double sec = ch::duration<double>(ch::steady_clock::now() - t0).count();
        std::printf("(A) TRUE per-tick average (clean loop, 1 clock) : %8.1f ns/tick\n",
                    sec * 1e9 / static_cast<double>(n));
        std::printf("    total wall = %.3f ms   orders fired = %zu\n\n", sec * 1e3, p.orders());
    }

    // ---- (B) per-stage attribution ----
    {
        pipe::PipelineV3 p(feed);
        const std::size_t n = feed.off.size() - 1;
        for (std::size_t i = 0; i < n; ++i) {
            p.set_index(i);
            p.process_one(i, true);
        }
        const pipe::StageNs& s = p.stage_ticks();
        const double parse_ns = s.parse  / tpns / static_cast<double>(s.ticks);
        const double book_ns  = s.book   / tpns / static_cast<double>(s.ticks);
        const double sig_ns   = s.signal / tpns / static_cast<double>(s.ticks);
        const double tot_ns   = s.total  / tpns / static_cast<double>(s.ticks);
        std::printf("(B) per-stage attribution (rdtsc -- ABSOLUTES inflated, read RATIOS)\n");
        std::printf("    parse   : %8.1f ns/tick   (%.0f%%)\n", parse_ns, 100.0 * parse_ns / tot_ns);
        std::printf("    book    : %8.1f ns/tick   (%.0f%%)\n", book_ns,  100.0 * book_ns  / tot_ns);
        std::printf("    signal  : %8.1f ns/tick   (%.0f%%)\n", sig_ns,   100.0 * sig_ns   / tot_ns);
        std::printf("    -------------------------------------\n");
        std::printf("    sum     : %8.1f ns/tick   (rdtsc-inflated total)\n\n", tot_ns);
    }

    std::puts("Kya seekha:");
    std::puts(" - Compare THIS (A) number to 01's (A) number -- SAME feed, same clock.");
    std::puts("   Woh speedup hai. 04_before_after.cpp isko ek run mein karta +");
    std::puts("   yeh bhi check karta ki dono ne SAME ticks pe order fire kiya");
    std::puts("   (agreement) -- warna 'fast' ka koi matlab nahi agar answer galat hai.");
    std::puts(" - (B) mein ab parse/book dono gir gaye; jo pehle 5% tha (signal)");
    std::puts("   ab bada dikh sakta -- Amdahl: sabse bada slice hata do to");
    std::puts("   agla slice naya bottleneck ban jaata (03-finding-bottlenecks.md).");
    return 0;
}
