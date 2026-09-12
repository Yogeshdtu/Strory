// 03_order_book.cpp
// ============================================================
// PROJECT 3 -- L2 order book. Yeh driver correctness + speed dono checkta:
//
//   1. INVARIANT check vs a brute-force reference: har message ke baad,
//      L2Book ka best_bid/best_ask ek O(n) linear scan se match kare
//      (39/16 "book invariants" executable).
//   2. throughput: apply() ka ns/message -O2 pe.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 03_order_book.cpp -o ob && ./ob
// ============================================================

#include "mh_engine_common.hpp"

#include <cstdio>
#include <map>

using namespace mhft;

// brute-force reference: std::map per side, id -> (side, px, qty)
struct RefBook {
    std::map<Price, std::int64_t, std::greater<Price>> bids;
    std::map<Price, std::int64_t>                      asks;
    std::unordered_map<OrderId, std::pair<Side, Price>> loc;
    std::unordered_map<OrderId, Qty>                    qty;

    void apply(const MdMessage& m) {
        if (m.type == MdType::Add) {
            (is_buy(m.side) ? bids[m.px] : asks[m.px]) += m.qty;
            loc[m.order_id] = {m.side, m.px};
            qty[m.order_id] = m.qty;
        } else {
            auto it = loc.find(m.order_id);
            if (it == loc.end()) return;
            const auto [sd, px] = it->second;
            std::int32_t take = static_cast<std::int32_t>(qty[m.order_id]);
            if (m.type == MdType::Trade && static_cast<std::int32_t>(m.qty) < take)
                take = static_cast<std::int32_t>(m.qty);
            if (is_buy(sd)) { bids[px] -= take; if (bids[px] <= 0) bids.erase(px); }
            else            { asks[px] -= take; if (asks[px] <= 0) asks.erase(px); }
            qty[m.order_id] = static_cast<Qty>(static_cast<std::int32_t>(qty[m.order_id]) - take);
            if (qty[m.order_id] == 0) { loc.erase(m.order_id); qty.erase(m.order_id); }
        }
    }
    bool  has_bid() const { return !bids.empty(); }
    bool  has_ask() const { return !asks.empty(); }
    Price best_bid() const { return bids.begin()->first; }
    Price best_ask() const { return asks.begin()->first; }
};

int main() {
    std::printf("ticks_per_ns = %.4f\n\n", g_tpns());
    constexpr std::uint64_t N = 120000;

    MarketDataSimulator sim(44);
    sim.set_limit(N);
    L2Book  fast(3 * N + 16);
    RefBook ref;

    std::uint64_t mismatches = 0, checked = 0;
    std::vector<MdMessage> tape;
    tape.reserve(N);

    MdMessage m;
    while (sim.next(m)) {
        tape.push_back(m);
        fast.apply(m);
        ref.apply(m);
        // reference may still transiently cross (no resolve); only compare
        // when the reference itself is clean.
        if (ref.has_bid() && ref.has_ask() && ref.best_bid() < ref.best_ask()) {
            ++checked;
            if (!fast.has_bid() || !fast.has_ask() ||
                fast.best_bid() != ref.best_bid() || fast.best_ask() != ref.best_ask())
                ++mismatches;
        }
    }
    std::printf("invariant vs brute-force reference:\n");
    std::printf("  %llu comparisons, %llu mismatches -> %s\n\n",
                (unsigned long long)checked, (unsigned long long)mismatches,
                mismatches == 0 ? "BBO always agrees" : "*** BBO DIVERGED ***");

    // ---- throughput: replay the tape through a fresh flat book ----
    double best = 1e300;
    for (int r = 0; r < 20; ++r) {
        L2Book b(3 * N + 16);
        const std::uint64_t a = tsc();
        for (const MdMessage& x : tape) b.apply(x);
        const std::uint64_t c = tsc();
        best = std::min(best, static_cast<double>(c - a));
        keep(b);
    }
    std::printf("L2Book.apply() : %.1f ns/message  (flat array + cached BBO, %llu msgs)\n",
                best / g_tpns() / static_cast<double>(tape.size()),
                (unsigned long long)tape.size());
    std::puts("(std::map version measured in folder 39 -- ~10-25x slower per op.)");

    return mismatches == 0 ? 0 : 1;
}
