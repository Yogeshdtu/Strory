// 01_matching_engine.cpp
// ============================================================
// Poora matching engine, sabse basic demo -- ek resting ask, ek
// crossing buy limit, price-time priority se match, Trade event
// generate hota. (01-what-is-matching.md, 02-matching-algorithm.md)
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 01_matching_engine.cpp -o me1 && ./me1
// ============================================================

#include "matching_engine.hpp"

#include <cstdio>

static void print_result(const char* tag, const SubmitResult& r) {
    std::printf("%s -> status=%s, %zu trade(s)\n", tag, to_string(r.status), r.trades.size());
    for (const auto& t : r.trades) {
        std::printf("    Trade#%llu: aggressor=%llu resting=%llu price=%lld qty=%u\n",
                    static_cast<unsigned long long>(t.id),
                    static_cast<unsigned long long>(t.aggressor_id),
                    static_cast<unsigned long long>(t.resting_id),
                    static_cast<long long>(t.price),
                    t.qty);
    }
}

int main() {
    MatchingEngine engine;

    // ============================================================
    //  1. RESTING ORDERS -- book banate hain (koi cross nahi, sab rest hote)
    // ============================================================
    print_result("Add resting ASK 101 x50 (id=1)", engine.submit(make_limit(1, /*p=*/10, false, 101, 50)));
    print_result("Add resting ASK 102 x30 (id=2)", engine.submit(make_limit(2, /*p=*/10, false, 102, 30)));
    print_result("Add resting BID  99 x40 (id=3)", engine.submit(make_limit(3, /*p=*/11, true,  99, 40)));

    std::printf("\nbest_bid=%lld best_ask=%lld (spread se pehle koi cross nahi)\n\n",
                static_cast<long long>(engine.best_bid()), static_cast<long long>(engine.best_ask()));

    // ============================================================
    //  2. CROSSING LIMIT BUY -- best ask (101) se cross karta, match hota
    // ============================================================
    // Incoming BUY 101 x60 (participant alag, id=4): best ask 101 x50 poora
    // fill, phir 102 cross NAHI karta (101 < 102), baaki 10 REST hota --
    // aur yeh leftover apni HI side (BID, is_buy=true) pe rest hota, ask
    // side pe NAHI. Ek limit order jo partially match hota, uska remainder
    // hamesha apni original side pe rehta -- woh "opposite" nahi ban jaata.
    print_result("BUY limit 101 x60 (id=4)", engine.submit(make_limit(4, /*p=*/20, true, 101, 60)));

    std::printf("\nresting_count=%zu (order 1 gone; order 4's leftover 10 ab ek NAYI\n"
                "  bid level bana deta -- bids {101x10 (id=4), 99x40 (id=3)})\n\n",
                engine.resting_count());

    const Order* leftover = engine.find_resting(4);
    if (leftover != nullptr) {
        std::printf("order 4 leftover qty=%u (expected 10), best_bid ab=%lld (expected 101)\n",
                    leftover->qty, static_cast<long long>(engine.best_bid()));
    }

    // ============================================================
    //  3. MARKET ORDER -- price-limit-less, jitna book de utna sweep
    // ============================================================
    // Book ab: bids {101x10 (id=4), 99x40 (id=3)}, asks {102x30 (id=2)}
    print_result("SELL market x50 (id=5)", engine.submit(make_market(5, /*p=*/12, false, 50)));
    // SELL -> BID side se cross karta, best-price-first: 101x10 (id=4) poora
    // fill, phir 99x40 (id=3) poora fill -- total 50, incoming bhi poora
    // fill (dono bids khatam ho gaye, sirf itni hi liquidity thi).

    std::printf("\nhas_bid=%d (expect 0 -- dono bid levels poori tarah consume ho gaye)\n",
                engine.has_bid() ? 1 : 0);

    return 0;
}
