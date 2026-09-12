// 10_feed_handler.cpp
// ============================================================
// POORA FEED HANDLER -- 03-09 ke sab pieces ek pipeline mein: framing +
// gap detection + zero-copy parse + per-symbol book state update.
// End-to-end per-message latency measured (16-building-feed-handler.md).
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 10_feed_handler.cpp -o handler && ./handler
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

static std::vector<std::byte> simulate_loss(const std::vector<std::byte>& feed,
                                             double keep_prob, std::uint64_t seed) {
    std::vector<std::byte> received;
    received.reserve(feed.size());
    SimpleRng rng(seed);
    std::size_t offset = 0;
    while (offset < feed.size()) {
        MsgHeader hdr{};
        if (!peek_header(feed.data() + offset, feed.size() - offset, hdr)) break;
        if (feed.size() - offset < hdr.length) break;
        if (rng.next_unit() < keep_prob) {
            const auto* p = feed.data() + offset;
            received.insert(received.end(), p, p + hdr.length);
        }
        offset += hdr.length;
    }
    return received;
}

// ============================================================
//  FeedHandler -- fixed-size state, allocation-free hot path.
// ============================================================
struct SymbolState {
    std::int64_t  last_price = 0;
    std::uint32_t last_qty   = 0;
    std::uint64_t last_ts_ns = 0;
    std::uint32_t updates    = 0;
};

class FeedHandler {
public:
    explicit FeedHandler(std::uint32_t num_symbols) : book_(num_symbols) {}

    // Ek message process karo. Return: kitne bytes consume hue (0 = partial,
    // caller ko aur bytes chahiye -- 11-message-framing).
    std::size_t on_bytes(const std::byte* buf, std::size_t remaining) {
        MsgHeader hdr{};
        if (!peek_header(buf, remaining, hdr)) return 0;   // partial header
        if (remaining < hdr.length) return 0;               // partial body

        // ---- gap detection (04-sequence-numbers) ----
        if (!first_) {
            if (hdr.seq_num > expected_seq_) {
                gaps_ += (hdr.seq_num - expected_seq_);
                ++gap_events_;
            } else if (hdr.seq_num < expected_seq_) {
                ++duplicates_;
            }
        }
        expected_seq_ = hdr.seq_num + 1;
        first_ = false;
        ++messages_;

        // ---- zero-copy dispatch + book update (09-zero-copy-parsing) ----
        if (hdr.msg_type == MSG_ADD_ORDER) {
            const auto* m = reinterpret_cast<const AddOrderMsg*>(buf);
            const std::uint32_t sym = net_to_host32(m->symbol_id) % static_cast<std::uint32_t>(book_.size());
            auto& s = book_[sym];
            s.last_price = net_to_host_i64(m->price_ticks);
            s.last_qty   = net_to_host32(m->qty);
            s.last_ts_ns = hdr.exch_ts_ns;
            ++s.updates;
        }
        // (Execute/Cancel/Delete/Replace: real book would need order_id ->
        //  symbol lookup, 39-ORDER-BOOK ka kaam -- yahan sirf tally.)

        return hdr.length;
    }

    std::uint64_t messages()   const { return messages_; }
    std::uint64_t gap_events() const { return gap_events_; }
    std::uint64_t gaps()       const { return gaps_; }
    std::uint64_t duplicates() const { return duplicates_; }
    const SymbolState& symbol(std::uint32_t id) const { return book_[id % book_.size()]; }

private:
    std::vector<SymbolState> book_;
    std::uint32_t expected_seq_ = 0;
    bool first_ = true;
    std::uint64_t messages_ = 0, gap_events_ = 0, gaps_ = 0, duplicates_ = 0;
};

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
    constexpr std::uint32_t NUM_SYMBOLS = 32;
    const auto upstream = generate_feed(N, 1, 4242, NUM_SYMBOLS);
    const auto feed = simulate_loss(upstream, /*keep_prob=*/0.999, /*seed=*/7);  // realistic ~0.1% loss

    FeedHandler handler(NUM_SYMBOLS);
    std::vector<double> ns;
    ns.reserve(N);

    std::size_t offset = 0;
    while (offset < feed.size()) {
        const auto t0 = tsc();
        const std::size_t consumed = handler.on_bytes(feed.data() + offset, feed.size() - offset);
        const auto t1 = tsc();
        if (consumed == 0) break;   // partial message at buffer end
        ns.push_back(static_cast<double>(t1 - t0) / g_tpns);
        offset += consumed;
    }

    std::printf("=== End-to-end feed handler (framing + gap-detect + zero-copy + book update) ===\n");
    report("on_bytes()", ns);
    std::printf("\nmessages processed: %llu\n", static_cast<unsigned long long>(handler.messages()));
    std::printf("gap events: %llu  (total %llu sequence numbers missing)\n",
                static_cast<unsigned long long>(handler.gap_events()),
                static_cast<unsigned long long>(handler.gaps()));
    std::printf("duplicates/out-of-order: %llu\n", static_cast<unsigned long long>(handler.duplicates()));

    std::printf("\nsample book state:\n");
    for (std::uint32_t sym = 0; sym < 4; ++sym) {
        const auto& s = handler.symbol(sym);
        std::printf("  symbol %u: last_price=%lld last_qty=%u updates=%u\n",
                    sym, static_cast<long long>(s.last_price), s.last_qty, s.updates);
    }

    return 0;
}
