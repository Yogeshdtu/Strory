// 04_parser_benchmark.cpp
// ============================================================
// STEP 2 of the process: MEASURE the simple parser (03) before touching
// it. Per-message latency DISTRIBUTION (p50/p99/p99.9/max), not just an
// average -- folder 35's lesson applied here.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 04_parser_benchmark.cpp -o bench && ./bench
//   (-O2 MANDATORY for benchmark numbers -- -O0 numbers are meaningless)
// ============================================================

#include "wire_protocol.hpp"

#include <algorithm>
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

// ============================================================
//  Same ParsedEvent + parse_one() as 03_simple_parser.cpp -- ek OWNED,
//  swapped copy of the message, pushed into a std::vector.
// ============================================================
struct ParsedEvent {
    std::uint8_t  type;
    std::uint32_t seq;
    std::uint64_t order_id;
    std::uint32_t symbol_id;
    std::uint32_t qty;
    std::int64_t  price_ticks;
    char          side;
};

static ParsedEvent parse_one(const std::byte* buf, const MsgHeader& hdr) {
    ParsedEvent ev{};
    ev.type = hdr.msg_type;
    ev.seq  = hdr.seq_num;
    switch (hdr.msg_type) {
        case MSG_ADD_ORDER: {
            AddOrderMsg m{};
            std::memcpy(&m, buf, sizeof m);
            ev.order_id = net_to_host64(m.order_id);
            ev.symbol_id = net_to_host32(m.symbol_id);
            ev.qty = net_to_host32(m.qty);
            ev.price_ticks = net_to_host_i64(m.price_ticks);
            ev.side = static_cast<char>(m.side);
            break;
        }
        case MSG_EXECUTE: {
            ExecuteMsg m{};
            std::memcpy(&m, buf, sizeof m);
            ev.order_id = net_to_host64(m.order_id);
            ev.qty = net_to_host32(m.exec_qty);
            break;
        }
        case MSG_CANCEL: {
            CancelMsg m{};
            std::memcpy(&m, buf, sizeof m);
            ev.order_id = net_to_host64(m.order_id);
            ev.qty = net_to_host32(m.cancel_qty);
            break;
        }
        case MSG_DELETE: {
            DeleteMsg m{};
            std::memcpy(&m, buf, sizeof m);
            ev.order_id = net_to_host64(m.order_id);
            break;
        }
        case MSG_REPLACE: {
            ReplaceMsg m{};
            std::memcpy(&m, buf, sizeof m);
            ev.order_id = net_to_host64(m.new_order_id);
            ev.qty = net_to_host32(m.qty);
            ev.price_ticks = net_to_host_i64(m.price_ticks);
            break;
        }
        default: break;
    }
    return ev;
}

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
    const auto feed = generate_feed(N, /*start_seq=*/1, /*seed=*/999);

    // ============================================================
    //  Naive parser: HAR message ek owned ParsedEvent ban ke ek
    //  std::vector mein push hota -- KOI reserve() nahi (03 jaisa
    //  naive first-draft). Per-message latency time karo.
    // ============================================================
    std::vector<ParsedEvent> events;   // no reserve
    std::vector<double> ns;
    ns.reserve(N);   // measurement vector reserved -- iski cost measure NAHI karni

    std::size_t offset = 0;
    while (offset < feed.size()) {
        MsgHeader hdr{};
        if (!peek_header(feed.data() + offset, feed.size() - offset, hdr)) break;
        if (feed.size() - offset < hdr.length) break;

        const auto t0 = tsc();
        events.push_back(parse_one(feed.data() + offset, hdr));
        const auto t1 = tsc();
        ns.push_back(static_cast<double>(t1 - t0) / g_tpns);

        offset += hdr.length;
    }

    std::printf("=== Naive parser (owned ParsedEvent per message, growing vector) ===\n");
    report("simple_parser (03)", ns);
    std::printf("messages parsed: %zu / %zu\n", events.size(), N);
    std::printf("\n(05_zero_copy_parser.cpp isi feed ko ALLOCATION-FREE parse karta;\n"
                " 06_parser_comparison.cpp dono ko side-by-side chalata)\n");

    return 0;
}
