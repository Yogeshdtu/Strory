// 06_engine_tests.cpp
// ============================================================
// Scripted unit tests -- core matching, order types, STP, aur EK
// khaas FOK+STP interaction test jo precheck ki STP-awareness ko
// directly prove karta (06-ioc-and-fok.md ka central example).
// (14-testing-matching-engine.md)
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 06_engine_tests.cpp -o etests && ./etests
// ============================================================

#include "matching_engine.hpp"

#include <cstdio>
#include <string>

static int g_run = 0, g_pass = 0;

static void check(bool cond, const std::string& name) {
    ++g_run;
    if (cond) { ++g_pass; std::printf("  PASS  %s\n", name.c_str()); }
    else      { std::printf("  FAIL  %s\n", name.c_str()); }
}

static Qty sum_qty(const std::vector<Trade>& trades) {
    Qty s = 0;
    for (const auto& t : trades) s += t.qty;
    return s;
}

// ============================================================
//  1. Basic matching + price-time priority
// ============================================================
static void test_basic_match_and_fifo() {
    std::printf("-- basic match + FIFO --\n");
    MatchingEngine e;
    e.submit(make_limit(1, 1, false, 100, 30));
    e.submit(make_limit(2, 1, false, 100, 20));   // SAME price, arrives SECOND
    auto r = e.submit(make_limit(3, 2, true, 100, 40));
    check(r.status == OrderStatus::Filled, "incoming BUY 40 fully fills");
    check(r.trades.size() == 2, "2 trades (crosses both same-price orders)");
    check(!r.trades.empty() && r.trades[0].resting_id == 1, "FIFO: id=1 (first arrived) fills FIRST");
    check(r.trades.size() > 1 && r.trades[1].resting_id == 2, "FIFO: id=2 fills SECOND, for the remainder");
    check(r.trades.size() > 1 && r.trades[0].qty == 30 && r.trades[1].qty == 10,
          "qty split correctly: 30 from id1 (all of it), 10 from id2 (partial)");
}

// ============================================================
//  2. Limit leftover rests on ITS OWN side (01_matching_engine's finding)
// ============================================================
static void test_limit_leftover_rests_own_side() {
    std::printf("-- limit leftover rests on own side --\n");
    MatchingEngine e;
    e.submit(make_limit(1, 1, false, 100, 30));
    auto r = e.submit(make_limit(2, 2, true, 100, 50));   // 30 fills, 20 leftover
    check(r.status == OrderStatus::PartiallyFilled, "partial fill status");
    check(e.has_bid(), "leftover 20 now rests as a BID (not ask!)");
    check(e.has_bid() && e.best_bid() == 100, "resting bid price == incoming's limit price");
    const Order* o = e.find_resting(2);
    check(o != nullptr && o->qty == 20, "resting order's remaining qty == 20");
}

// ============================================================
//  3. Market order -- sweeps, leftover VOID (never rests)
// ============================================================
static void test_market_sweeps_and_voids_remainder() {
    std::printf("-- market order sweep + void remainder --\n");
    MatchingEngine e;
    e.submit(make_limit(1, 1, false, 100, 20));
    e.submit(make_limit(2, 1, false, 105, 10));
    auto r = e.submit(make_market(3, 2, true, 100));   // book has only 30
    check(sum_qty(r.trades) == 30, "market fills all available (30)");
    check(r.status == OrderStatus::Cancelled, "unfilled remainder -> Cancelled (voided, not resting)");
    check(!e.has_ask(), "book fully swept, no asks left");
    check(e.find_resting(3) == nullptr, "market order id NEVER appears as resting");
}

// ============================================================
//  4. IOC -- partial fill allowed, remainder VOID
// ============================================================
static void test_ioc_partial_then_void() {
    std::printf("-- IOC partial fill, remainder void --\n");
    MatchingEngine e;
    e.submit(make_limit(1, 1, false, 100, 15));
    auto r = e.submit(make_ioc(2, 2, true, 100, 50));
    check(sum_qty(r.trades) == 15, "IOC fills what crosses (15)");
    check(r.status == OrderStatus::Cancelled, "remainder voided");
    check(e.find_resting(2) == nullptr, "IOC order never rests");
}

// ============================================================
//  5. FOK -- succeed when enough, reject cleanly when not (book UNTOUCHED)
// ============================================================
static void test_fok_all_or_nothing() {
    std::printf("-- FOK all-or-nothing --\n");
    MatchingEngine e;
    e.submit(make_limit(1, 1, false, 100, 20));
    e.submit(make_limit(2, 1, false, 101, 20));

    auto r_fail = e.submit(make_fok(3, 2, true, 101, 100));   // only 40 available
    check(r_fail.status == OrderStatus::Rejected, "FOK too big -> Rejected");
    check(r_fail.trades.empty(), "Rejected FOK produces ZERO trades");
    check(e.resting_count() == 2, "book completely untouched by a rejected FOK");

    auto r_ok = e.submit(make_fok(4, 2, true, 101, 40));      // exactly 40
    check(r_ok.status == OrderStatus::Filled, "FOK exactly-enough -> Filled");
    check(sum_qty(r_ok.trades) == 40, "FOK fills its FULL qty, nothing less");
}

// ============================================================
//  6. FOK + STP interaction -- THE central correctness test.
//     Naive precheck (sum ALL total_qty at crossable levels, self-blind)
//     would say "70 available >= 50 target -> PASS." Par CancelOldest
//     STP mode us 40 (SELF ka) ko match hote hi SKIP kar deta (fill nahi
//     hota usse) -- ACTUAL fillable sirf 30 (OTHER ka) hai, jo target se
//     KAM hai. Ek naive precheck yahan ek FOK CONTRACT VIOLATION create
//     karta (partial fill on a supposedly all-or-nothing order). Hamara
//     STP-aware precheck isko REJECT karta hai, sahi se. (06-ioc-and-fok.md)
// ============================================================
static void test_fok_stp_interaction() {
    std::printf("-- FOK + STP interaction (the real edge case) --\n");
    constexpr ParticipantId SELF = 9, OTHER = 10;
    MatchingEngine e;
    e.submit(make_limit(1, SELF,  false, 100, 40));   // touched FIRST (FIFO), but SELF
    e.submit(make_limit(2, OTHER, false, 100, 30));   // touched SECOND, OTHER

    // Naive sum at level 100 = 40+30 = 70 >= 50 -- naakami se PASS ho jaata
    // agar precheck STP-unaware hota. STP-aware sahi answer: 30 (sirf OTHER
    // ka, kyunki CancelOldest SELF ka 40 skip karta, use count NAHI karta).
    auto r = e.submit(make_fok(3, SELF, true, 100, 50, StpMode::CancelOldest));
    check(r.status == OrderStatus::Rejected,
          "STP-aware precheck correctly REJECTS (naive sum 70 would have wrongly PASSED)");
    check(r.trades.empty(), "rejected FOK -> zero trades (no partial-fill contract violation)");
    check(e.resting_count() == 2, "book untouched -- id1(SELF) NOT removed (precheck rejected before matching started)");

    // Sanity: same scenario par TARGET chhota (30, exactly what's fillable
    // after skip) -- ab yeh PASS hona chahiye.
    auto r2 = e.submit(make_fok(4, SELF, true, 100, 30, StpMode::CancelOldest));
    check(r2.status == OrderStatus::Filled, "target==truly-available (30) -> Filled");
    check(sum_qty(r2.trades) == 30, "exactly 30 filled, from OTHER's order only");
    check(e.find_resting(1) == nullptr, "id1(SELF) WAS removed this time (STP fired during actual match)");
}

// ============================================================
//  7. STP CancelNewest / CancelBoth -- resting-side effects
// ============================================================
static void test_stp_modes() {
    std::printf("-- STP modes: resting-side effects --\n");
    constexpr ParticipantId SELF = 5, OTHER = 6;
    {
        MatchingEngine e;
        e.submit(make_limit(1, SELF, false, 100, 20));
        auto r = e.submit(make_limit(2, SELF, true, 100, 20, StpMode::CancelNewest));
        check(r.trades.empty(), "CancelNewest: zero trades (incoming aborts on first self-touch)");
        check(e.find_resting(1) != nullptr, "CancelNewest: resting order UNTOUCHED");
        check(e.find_resting(2) == nullptr, "CancelNewest: incoming did NOT rest either (Limit+stp_aborted -> void)");
    }
    {
        MatchingEngine e;
        e.submit(make_limit(1, SELF, false, 100, 20));
        auto r = e.submit(make_limit(2, SELF, true, 100, 20, StpMode::CancelBoth));
        check(r.trades.empty(), "CancelBoth: zero trades");
        check(e.find_resting(1) == nullptr, "CancelBoth: resting order REMOVED");
        check(e.find_resting(2) == nullptr, "CancelBoth: incoming also did not rest");
        check(!e.has_ask(), "CancelBoth: book fully empty (only order was the STP-cancelled one)");
    }
    {
        // OTHER participant -- STP must NOT fire (different participant)
        MatchingEngine e;
        e.submit(make_limit(1, OTHER, false, 100, 20));
        auto r = e.submit(make_limit(2, SELF, true, 100, 20, StpMode::CancelBoth));
        check(r.status == OrderStatus::Filled, "different participant -> STP does not fire, normal fill");
    }
}

// ============================================================
//  8. Duplicate id, cancel-nonexistent, replace
// ============================================================
static void test_edge_ops() {
    std::printf("-- duplicate id / cancel-nonexistent / replace --\n");
    MatchingEngine e;
    e.submit(make_limit(1, 1, true, 99, 10));
    auto dup = e.submit(make_limit(1, 1, true, 98, 5));   // SAME id, still resting
    check(dup.status == OrderStatus::Rejected, "duplicate id (still resting) -> Rejected");

    check(!e.cancel(999), "cancel on nonexistent id returns false (no crash)");
    check(e.cancel(1), "cancel on existing resting id succeeds");
    check(e.find_resting(1) == nullptr, "cancelled order gone from book");

    e.submit(make_limit(2, 1, true, 99, 10));
    auto rep = e.replace(2, make_limit(3, 1, true, 99, 25));
    check(rep.status == OrderStatus::New || rep.status == OrderStatus::PartiallyFilled || rep.status == OrderStatus::Filled,
          "replace succeeds (old gone, new submitted)");
    check(e.find_resting(2) == nullptr, "old id (2) gone after replace");
    check(e.find_resting(3) != nullptr, "new id (3) present after replace");

    auto rep_gone = e.replace(9999, make_limit(4, 1, false, 200, 5));   // old_id already gone
    check(rep_gone.status != OrderStatus::Rejected, "replace where old_id already gone still submits the new order");
}

int main() {
    test_basic_match_and_fifo();
    test_limit_leftover_rests_own_side();
    test_market_sweeps_and_voids_remainder();
    test_ioc_partial_then_void();
    test_fok_all_or_nothing();
    test_fok_stp_interaction();
    test_stp_modes();
    test_edge_ops();

    std::printf("\n%d/%d tests passed\n", g_pass, g_run);
    return (g_pass == g_run) ? 0 : 1;
}
