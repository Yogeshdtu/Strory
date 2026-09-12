// 05_zero_copy_parser.cpp
// ============================================================
// STEP 3: OPTIMIZED parser. Same feed, same "useful work" (update a
// per-symbol last-price/qty table + count messages by type) -- par
// koi owned ParsedEvent struct nahi banta, koi vector push_back nahi,
// koi heap allocation nahi. Overlay-cast seedha buffer pe, sirf zaroori
// fields swap karo, seedha pre-allocated fixed state mein likho.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 05_zero_copy_parser.cpp -o zc && ./zc
// ============================================================

#include "wire_protocol.hpp"

#include <algorithm>
#include <array>
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
    std::printf("  %-28s p50 %6.1f   p99 %8.1f   p99.9 %9.1f   max %10.1f  ns\n",
                tag, pc(50), pc(99), pc(99.9), ns.back());
}

// Pre-allocated, fixed-size sink -- "the book" this parser updates.
// Zero allocation after startup (04-allocation-avoidance, folder 36).
struct SymbolState {
    std::int64_t  last_price = 0;
    std::uint32_t last_qty   = 0;
};
constexpr std::uint32_t NUM_SYMBOLS = 32;

int main() {
    // ---- calibrate ticks/ns ----
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
    const auto feed = generate_feed(N, /*start_seq=*/1, /*seed=*/999);   // SAME seed as 04 -- same feed

    std::array<SymbolState, NUM_SYMBOLS> book{};   // pre-allocated once, before timing starts
    std::size_t counts[5] = {};                    // Add/Execute/Cancel/Delete/Replace
    std::vector<double> ns;
    ns.reserve(N);

    std::size_t offset = 0;
    while (offset < feed.size()) {
        MsgHeader hdr{};
        if (!peek_header(feed.data() + offset, feed.size() - offset, hdr)) break;
        if (feed.size() - offset < hdr.length) break;

        const auto t0 = tsc();

        // ============================================================
        //  ZERO-COPY: overlay a typed pointer DIRECTLY on the buffer --
        //  no memcpy of the whole struct, no owned copy. Read + swap
        //  ONLY the 2-3 fields this "useful work" actually needs.
        //  (packed struct => alignof 1 => this cast doesn't trip
        //   -Wcast-align — verified in 01_message_structs.cpp)
        // ============================================================
        switch (hdr.msg_type) {
            case MSG_ADD_ORDER: {
                const auto* m = reinterpret_cast<const AddOrderMsg*>(feed.data() + offset);
                const std::uint32_t sym = net_to_host32(m->symbol_id) % NUM_SYMBOLS;
                book[sym].last_price = net_to_host_i64(m->price_ticks);
                book[sym].last_qty   = net_to_host32(m->qty);
                ++counts[0];
                break;
            }
            case MSG_EXECUTE: {
                const auto* m = reinterpret_cast<const ExecuteMsg*>(feed.data() + offset);
                (void)m;  // is demo mein order_id-> symbol lookup nahi (39-ORDER-BOOK ka kaam) -- sirf tally
                ++counts[1];
                break;
            }
            case MSG_CANCEL: {
                ++counts[2];
                break;
            }
            case MSG_DELETE: {
                ++counts[3];
                break;
            }
            case MSG_REPLACE: {
                const auto* m = reinterpret_cast<const ReplaceMsg*>(feed.data() + offset);
                (void)m;
                ++counts[4];
                break;
            }
            default: break;
        }

        const auto t1 = tsc();
        ns.push_back(static_cast<double>(t1 - t0) / g_tpns);

        offset += hdr.length;
    }

    std::printf("=== Zero-copy parser (overlay-read, fixed-size sink, 0 allocation) ===\n");
    report("zero_copy_parser (05)", ns);
    std::printf("total messages: %zu  (Add=%zu Exec=%zu Cancel=%zu Delete=%zu Replace=%zu)\n",
                counts[0] + counts[1] + counts[2] + counts[3] + counts[4],
                counts[0], counts[1], counts[2], counts[3], counts[4]);

    std::printf("\nsample book state (symbol 0): last_price=%lld last_qty=%u\n",
                static_cast<long long>(book[0].last_price), book[0].last_qty);
    std::printf("\n(06_parser_comparison.cpp dono ko SAME run mein, side-by-side compare karta)\n");

    return 0;
}
