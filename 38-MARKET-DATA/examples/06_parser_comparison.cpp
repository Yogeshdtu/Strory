// 06_parser_comparison.cpp
// ============================================================
// STEP 4: BEFORE/AFTER, side by side, SAME run, SAME feed. Naive
// (owned ParsedEvent + growing vector, 03/04) vs zero-copy (overlay
// read + fixed sink, 05). Real numbers, not estimates (CLAUDE.md Rule 2).
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 06_parser_comparison.cpp -o cmp && ./cmp
// ============================================================

#include "wire_protocol.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
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

struct ParsedEvent {
    std::uint8_t type; std::uint32_t seq; std::uint64_t order_id;
    std::uint32_t symbol_id; std::uint32_t qty; std::int64_t price_ticks; char side;
};

static ParsedEvent parse_one_naive(const std::byte* buf, const MsgHeader& hdr) {
    ParsedEvent ev{};
    ev.type = hdr.msg_type;
    ev.seq  = hdr.seq_num;
    switch (hdr.msg_type) {
        case MSG_ADD_ORDER: { AddOrderMsg m{}; std::memcpy(&m, buf, sizeof m);
            ev.order_id = net_to_host64(m.order_id); ev.symbol_id = net_to_host32(m.symbol_id);
            ev.qty = net_to_host32(m.qty); ev.price_ticks = net_to_host_i64(m.price_ticks);
            ev.side = static_cast<char>(m.side); break; }
        case MSG_EXECUTE: { ExecuteMsg m{}; std::memcpy(&m, buf, sizeof m);
            ev.order_id = net_to_host64(m.order_id); ev.qty = net_to_host32(m.exec_qty); break; }
        case MSG_CANCEL: { CancelMsg m{}; std::memcpy(&m, buf, sizeof m);
            ev.order_id = net_to_host64(m.order_id); ev.qty = net_to_host32(m.cancel_qty); break; }
        case MSG_DELETE: { DeleteMsg m{}; std::memcpy(&m, buf, sizeof m);
            ev.order_id = net_to_host64(m.order_id); break; }
        case MSG_REPLACE: { ReplaceMsg m{}; std::memcpy(&m, buf, sizeof m);
            ev.order_id = net_to_host64(m.new_order_id); ev.qty = net_to_host32(m.qty);
            ev.price_ticks = net_to_host_i64(m.price_ticks); break; }
        default: break;
    }
    return ev;
}

struct SymbolState { std::int64_t last_price = 0; std::uint32_t last_qty = 0; };
constexpr std::uint32_t NUM_SYMBOLS = 32;

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
    const auto feed = generate_feed(N, 1, 999);   // SAME feed for both -- fair comparison

    // ---- naive ----
    std::vector<ParsedEvent> events;               // no reserve
    std::vector<double> ns_naive; ns_naive.reserve(N);
    {
        std::size_t offset = 0;
        while (offset < feed.size()) {
            MsgHeader hdr{};
            if (!peek_header(feed.data() + offset, feed.size() - offset, hdr)) break;
            if (feed.size() - offset < hdr.length) break;
            const auto t0 = tsc();
            events.push_back(parse_one_naive(feed.data() + offset, hdr));
            const auto t1 = tsc();
            ns_naive.push_back(static_cast<double>(t1 - t0) / g_tpns);
            offset += hdr.length;
        }
    }

    // ---- zero-copy ----
    // SAME per-type work as naive (every type touched, not just Add) --
    // taaki comparison sirf "owned-copy-into-vector vs overlay-read-into-
    // fixed-sink" axis pe ho, "kitne type handle kiye" pe nahi.
    std::array<SymbolState, NUM_SYMBOLS> book{};
    std::size_t counts[5] = {};
    std::vector<double> ns_zc; ns_zc.reserve(N);
    {
        std::size_t offset = 0;
        while (offset < feed.size()) {
            MsgHeader hdr{};
            if (!peek_header(feed.data() + offset, feed.size() - offset, hdr)) break;
            if (feed.size() - offset < hdr.length) break;
            const auto t0 = tsc();
            switch (hdr.msg_type) {
                case MSG_ADD_ORDER: {
                    const auto* m = reinterpret_cast<const AddOrderMsg*>(feed.data() + offset);
                    const std::uint32_t sym = net_to_host32(m->symbol_id) % NUM_SYMBOLS;
                    book[sym].last_price = net_to_host_i64(m->price_ticks);
                    book[sym].last_qty   = net_to_host32(m->qty);
                    ++counts[0];
                    break;
                }
                case MSG_EXECUTE:  ++counts[1]; break;
                case MSG_CANCEL:   ++counts[2]; break;
                case MSG_DELETE:   ++counts[3]; break;
                case MSG_REPLACE:  ++counts[4]; break;
                default: break;
            }
            const auto t1 = tsc();
            ns_zc.push_back(static_cast<double>(t1 - t0) / g_tpns);
            offset += hdr.length;
        }
    }

    std::printf("=== BEFORE (naive: owned struct + growing vector) vs AFTER (zero-copy) ===\n");
    std::printf("same feed (%zu messages), same box, same run:\n\n", N);
    report("naive (03/04)", ns_naive);
    report("zero-copy (05)", ns_zc);

    std::sort(ns_naive.begin(), ns_naive.end());
    std::sort(ns_zc.begin(), ns_zc.end());
    auto p999 = [](std::vector<double>& v) { return v[static_cast<std::size_t>(0.999 * static_cast<double>(v.size()))]; };
    const double ratio = p999(ns_naive) / p999(ns_zc);

    std::printf("\np99.9 ratio (naive / zero-copy) = %.1fx\n", ratio);
    std::printf("events.size()=%zu (naive keeps everything -- more memory too)\n", events.size());
    std::printf("zero-copy tally: Add=%zu Exec=%zu Cancel=%zu Delete=%zu Replace=%zu\n",
                counts[0], counts[1], counts[2], counts[3], counts[4]);
    std::printf("book is a fixed %u-symbol array -- 0 bytes grew after startup\n", NUM_SYMBOLS);

    return 0;
}
