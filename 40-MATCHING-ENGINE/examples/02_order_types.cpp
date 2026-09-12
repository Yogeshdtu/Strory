// 02_order_types.cpp
// ============================================================
// Limit / Market / IOC / FOK -- charon ka exact behavior, side-by-side,
// SAME starting book pe (03/04/05/06 mein detail).
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 02_order_types.cpp -o types && ./types
// ============================================================

#include "matching_engine.hpp"

#include <cstdio>

// Har scenario ke liye FRESH book -- taaki types independently compare ho
// sakein, ek doosre ke leftover state se contaminate hue bina.
static MatchingEngine fresh_book() {
    MatchingEngine e;
    e.submit(make_limit(100, 1, false, 100, 30));   // ASK 100 x30
    e.submit(make_limit(101, 1, false, 105, 400));  // ASK 105 x400
    return e;
}

static void show(const char* tag, const SubmitResult& r) {
    Qty filled = 0;
    for (const auto& t : r.trades) filled += t.qty;
    std::printf("%-28s status=%-16s filled=%-4u trades=%zu\n",
                tag, to_string(r.status), filled, r.trades.size());
}

int main() {
    std::printf("Starting book (same for all 4): ASK 100 x30, ASK 105 x400\n\n");

    // ============================================================
    //  1. LIMIT -- jitna cross kare fill, baaki REST karta
    // ============================================================
    {
        auto e = fresh_book();
        show("LIMIT BUY 105 x100", e.submit(make_limit(1, 9, true, 105, 100)));
        // 100x30 poora fill, phir 105x400 se 70 fill (total 100) -- SAB
        // fill ho gaya, kuch rest nahi hua is case mein.
        std::printf("  -> resting_count=%zu (poora fill ho gaya, kuch resting nahi)\n\n",
                    e.resting_count());
    }

    // ============================================================
    //  2. MARKET -- price-limit-less, book jitna de utna, baaki VOID
    // ============================================================
    {
        auto e = fresh_book();
        show("MARKET BUY x1000", e.submit(make_market(2, 9, true, 1000)));
        // Book mein sirf 430 total hai (30+400) -- 430 fill hoga, baaki
        // 570 VOID (market order kabhi rest NAHI hota).
        std::printf("  -> book ke paas sirf 430 tha, baaki 570 VOID ho gaya (Cancelled status\n"
                    "     ke saath bhi 430 ke trades already hue -- status != 'kuch nahi hua')\n\n");
    }

    // ============================================================
    //  3. IOC -- limit price ke saath, jitna TURANT mile, baaki VOID
    // ============================================================
    {
        auto e = fresh_book();
        show("IOC BUY 100 x100", e.submit(make_ioc(3, 9, true, 100, 100)));
        // Sirf 100x30 cross karta (105 > 100, cross nahi karta) -- 30 fill,
        // baaki 70 VOID (IOC bhi kabhi rest nahi karta, chahe price-eligible
        // liquidity book mein bilkul na ho).
        std::printf("  -> sirf 100x30 cross karti thi, 30 fill, baaki 70 VOID (rest NAHI)\n\n");
    }

    // ============================================================
    //  4. FOK -- POORA turant fill ya BILKUL nahi (precheck)
    // ============================================================
    {
        auto e = fresh_book();
        show("FOK BUY 105 x100 (fits)", e.submit(make_fok(4, 9, true, 105, 100)));
        // 430 available hai jo cross karta (100+400 range) -- 100 <= 430,
        // precheck PASS, poora 100 fill hota.

        auto e2 = fresh_book();
        show("FOK BUY 105 x1000 (too big)", e2.submit(make_fok(5, 9, true, 105, 1000)));
        // Sirf 430 available -- 1000 > 430, precheck FAIL -- REJECTED,
        // ZERO trades (book bilkul touch nahi hua, resting orders as-is).
        std::printf("  -> dusra FOK precheck mein hi fail ho gaya -- 0 trades, book untouched "
                    "(resting_count=%zu, still 2)\n", e2.resting_count());
    }

    return 0;
}
