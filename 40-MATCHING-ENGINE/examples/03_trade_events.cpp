// 03_trade_events.cpp
// ============================================================
// Trade event ke saare fields, aur "maker sets the price" convention --
// incoming order apni limit price se BEHTAR price pe bhi fill ho sakta,
// trade hamesha RESTING (maker) ki price pe hota. (07-trade-events.md)
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 03_trade_events.cpp -o events && ./events
// ============================================================

#include "matching_engine.hpp"

#include <cstdio>

static void dump_trade(const Trade& t) {
    std::printf("  Trade{ id=%llu  aggressor=%llu(p%u, %s)  resting=%llu(p%u)  "
                "price=%lld  qty=%u  seq=%llu }\n",
                static_cast<unsigned long long>(t.id),
                static_cast<unsigned long long>(t.aggressor_id), t.aggressor_participant,
                t.aggressor_is_buy ? "BUY" : "SELL",
                static_cast<unsigned long long>(t.resting_id), t.resting_participant,
                static_cast<long long>(t.price), t.qty,
                static_cast<unsigned long long>(t.seq));
}

int main() {
    MatchingEngine engine;

    // ============================================================
    //  1. "MAKER SETS THE PRICE" -- resting order ki price pe trade hota,
    //     incoming order ki price pe NAHI, chahe incoming behtar offer kare
    // ============================================================
    engine.submit(make_limit(1, 10, false, 100, 50));   // resting ASK @100

    // Incoming BUY apni limit 103 pe hai (100 se BEHTAR/zyaada offer kar
    // raha) -- par trade phir bhi 100 pe hoga, 103 pe NAHI. Yeh convention
    // hai: maker (jo pehle se resting tha, price commit kar chuka) ki price
    // "wins" -- aggressor ko apni limit se behtar hi milta hai, badtar kabhi
    // nahi.
    auto r1 = engine.submit(make_limit(2, 20, true, 103, 50));
    std::printf("Incoming BUY limit=103, resting ASK=100 -> trade price:\n");
    for (const auto& t : r1.trades) dump_trade(t);
    std::printf("  (trade.price == 100, incoming ki 103 nahi -- 'price improvement'\n"
                "   aggressor ko milta, exchange ko nahi)\n\n");

    // ============================================================
    //  2. MULTI-LEVEL SWEEP -- ek incoming order, MULTIPLE trades, har
    //     level apni HI price pe (level jump karte hi price badalti)
    // ============================================================
    engine.submit(make_limit(3, 10, false, 101, 20));
    engine.submit(make_limit(4, 10, false, 102, 20));
    engine.submit(make_limit(5, 10, false, 103, 20));

    auto r2 = engine.submit(make_limit(6, 20, true, 103, 55));
    std::printf("Incoming BUY limit=103 x55 sweeps 3 levels:\n");
    for (const auto& t : r2.trades) dump_trade(t);
    std::printf("  (teen alag trades, teen alag prices -- 101, 102, 103; total "
                "filled=20+20+15=55)\n\n");

    // ============================================================
    //  3. seq -- ek SHARED global event-timeline (orders AUR trades dono),
    //     replay/audit ke liye total order deta (10-event-sourcing preview)
    // ============================================================
    std::printf("Trade seq numbers strictly increasing across the whole run:\n");
    std::uint64_t last_seq = 0;
    bool monotonic = true;
    for (const auto& t : r1.trades) { if (t.seq <= last_seq) monotonic = false; last_seq = t.seq; }
    for (const auto& t : r2.trades) { if (t.seq <= last_seq) monotonic = false; last_seq = t.seq; }
    std::printf("  monotonic across both submits? %s\n", monotonic ? "haan" : "NAHI (bug!)");

    return 0;
}
