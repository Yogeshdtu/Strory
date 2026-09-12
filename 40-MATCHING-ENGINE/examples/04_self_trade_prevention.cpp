// 04_self_trade_prevention.cpp
// ============================================================
// STP ke teeno modes -- CancelNewest, CancelOldest, CancelBoth -- SAME
// starting scenario pe, side-by-side. (08-self-trade-prevention.md)
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 04_self_trade_prevention.cpp -o stp && ./stp
// ============================================================

#include "matching_engine.hpp"

#include <cstdio>

constexpr ParticipantId SELF   = 42;   // dono resting aur incoming isi participant ke
constexpr ParticipantId OTHER  = 43;

static void show_trades(const SubmitResult& r) {
    Qty filled = 0;
    for (const auto& t : r.trades) filled += t.qty;
    std::printf("  status=%-16s filled=%-3u trades=%zu\n", to_string(r.status), filled, r.trades.size());
}

int main() {
    // ============================================================
    //  1. STP off (baseline) -- self-trade normally allow ho jaata
    // ============================================================
    {
        MatchingEngine e;
        e.submit(make_limit(1, SELF, false, 100, 50));    // apni hi resting ASK
        std::printf("[No STP] incoming BUY 100 x50, SAME participant as resting ask:\n");
        show_trades(e.submit(make_limit(2, SELF, true, 100, 50)));
        std::printf("  -> self-trade HO GAYA (STP off tha) -- filled=50 expected\n\n");
    }

    // ============================================================
    //  2. CancelNewest -- incoming (hamesha 'newest') cancel, resting UNTOUCHED
    // ============================================================
    {
        MatchingEngine e;
        e.submit(make_limit(1, OTHER, false, 100, 20));   // OTHER ka ask -- normal match
        e.submit(make_limit(2, SELF,  false, 100, 30));   // SELF ka ask -- SAME level, baad mein
        std::printf("[CancelNewest] book: ASK 100 {id1=OTHER x20, id2=SELF x30}\n"
                    "  incoming BUY 100 x100, participant=SELF, stp=CancelNewest:\n");
        auto r = e.submit(make_limit(3, SELF, true, 100, 100, StpMode::CancelNewest));
        show_trades(r);
        std::printf("  -> id1(OTHER)x20 match hota (FIFO first), phir id2(SELF) touch\n"
                    "     hote hi INCOMING poora cancel ho jaata (id2 resting untouched)\n"
                    "     filled=20 expected (sirf pehla, self-touch pe hi ABORT)\n\n");
    }

    // ============================================================
    //  3. CancelOldest -- resting (older, SELF ka) hatao, incoming AAGE try karta
    // ============================================================
    {
        MatchingEngine e;
        e.submit(make_limit(1, SELF,  false, 100, 20));   // SELF ka ask -- yeh hatega
        e.submit(make_limit(2, OTHER, false, 100, 30));   // OTHER ka ask -- isse match hoga
        std::printf("[CancelOldest] book: ASK 100 {id1=SELF x20, id2=OTHER x30}\n"
                    "  incoming BUY 100 x100, participant=SELF, stp=CancelOldest:\n");
        auto r = e.submit(make_limit(3, SELF, true, 100, 100, StpMode::CancelOldest));
        show_trades(r);
        std::printf("  -> id1(SELF) TOUCH hote hi CANCEL/REMOVE (match nahi), phir\n"
                    "     id2(OTHER)x30 se match, phir book khaali -> baaki 70 rest\n"
                    "     filled=30 expected, id1 poori tarah gone (self-trade AVOID hua,\n"
                    "     par incoming apna matching jaari rakh saka)\n\n");
        std::printf("  find_resting(1) == nullptr? %s (id1 STP se cancel ho gaya)\n\n",
                    e.find_resting(1) == nullptr ? "haan" : "NAHI (bug!)");
    }

    // ============================================================
    //  4. CancelBoth -- resting hatao AUR incoming ka remainder bhi VOID
    // ============================================================
    {
        MatchingEngine e;
        e.submit(make_limit(1, OTHER, false, 100, 20));
        e.submit(make_limit(2, SELF,  false, 100, 30));
        std::printf("[CancelBoth] book: ASK 100 {id1=OTHER x20, id2=SELF x30}\n"
                    "  incoming BUY 100 x100, participant=SELF, stp=CancelBoth:\n");
        auto r = e.submit(make_limit(3, SELF, true, 100, 100, StpMode::CancelBoth));
        show_trades(r);
        std::printf("  -> id1(OTHER)x20 match, phir id2(SELF) touch: id2 REMOVE hota\n"
                    "     AUR incoming ka baaki 80 bhi VOID (dono cancel) -- filled=20\n\n");
    }

    return 0;
}
