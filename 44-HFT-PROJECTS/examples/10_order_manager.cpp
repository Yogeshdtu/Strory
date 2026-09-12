// 10_order_manager.cpp
// ============================================================
// PROJECT 10 -- OMS + Execution simulator.
//   1. state machine: New -> Acked -> PartiallyFilled -> Filled
//   2. reject path : New -> Rejected  (order leaves 'live')
//   3. IOC remainder: partial fill then unfilled-cancel
//   4. ACCOUNTING: after a full run, every submitted order has settled
//      (live() == 0) and total filled qty <= total submitted qty
//   5. generation safety: OMS records come from a pooled, gen-checked store
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 10_order_manager.cpp -o om && ./om
// ============================================================

#include "mh_engine_common.hpp"

#include <cstdio>

using namespace mhft;

static int fails = 0;
#define CHECK(cond, name) do { \
    if (cond) std::printf("  [ok]   %s\n", name); \
    else    { std::printf("  [FAIL] %s\n", name); ++fails; } } while (0)

static OrderRequest mk(Side s, Price px, Qty q) {
    OrderRequest r; r.side = s; r.type = OrderType::IOC; r.px = px; r.qty = q; return r;
}

int main() {
    // ---- 1/2/3: state machine ----
    {
        OrderManager oms;
        auto id = oms.submit(mk(Side::Buy, 10005, 50));
        CHECK(oms.get(id)->state == OmsState::New, "submit -> New");
        oms.on_ack(id);
        CHECK(oms.get(id)->state == OmsState::Acked, "on_ack -> Acked");
        oms.on_fill(id, 20);
        CHECK(oms.get(id)->state == OmsState::PartiallyFilled, "partial fill -> PartiallyFilled");
        oms.on_fill(id, 30);
        CHECK(oms.get(id) == nullptr, "full fill -> retired (get -> nullptr)");
        CHECK(oms.live() == 0, "no live orders after full fill");

        auto id2 = oms.submit(mk(Side::Sell, 9995, 10));
        oms.on_reject(id2);
        CHECK(oms.get(id2) == nullptr && oms.live() == 0, "reject -> retired");

        auto id3 = oms.submit(mk(Side::Buy, 10005, 40));
        oms.on_ack(id3);
        oms.on_fill(id3, 15);
        oms.on_unfilled_cancel(id3);           // IOC remainder voided
        CHECK(oms.get(id3) == nullptr && oms.live() == 0, "IOC partial + unfilled-cancel -> retired");
        std::puts("");
    }

    // ---- 4: full-run accounting through the execution simulator ----
    {
        constexpr std::uint64_t N = 120000;
        MarketDataSimulator sim(44);
        sim.set_limit(N);
        L2Book book(3 * N + 16);
        ExecutionSimulator venue;
        OrderManager oms;
        SpreadCrossStrategy strat;
        RiskEngine risk;

        std::uint64_t submitted_qty = 0, filled_qty = 0, submitted = 0;
        MdMessage m;
        while (sim.next(m)) {
            book.apply(m);
            venue.on_market_event(m);
            const auto d = strat.on_book(book);
            if (!d.act) continue;
            OrderRequest r; r.side = d.side; r.type = d.type; r.px = d.px; r.qty = d.qty; r.ts = m.ts;
            if (risk.check(r, book.mid2(), m.ts) != RiskVerdict::Ok) continue;
            const auto cl = oms.submit(r);
            oms.on_ack(cl);
            ++submitted; submitted_qty += r.qty;
            Qty f = 0;
            auto fills = venue.send(r, cl, f);
            for (const Fill& x : fills) { oms.on_fill(cl, x.qty); risk.on_fill(x); filled_qty += x.qty; }
            if (venue.last_status() != OrderStatus::Filled) oms.on_unfilled_cancel(cl);
            strat.note_fired();
        }
        std::printf("full run: submitted=%llu orders (%llu qty), filled %llu qty\n",
                    (unsigned long long)submitted, (unsigned long long)submitted_qty,
                    (unsigned long long)filled_qty);
        CHECK(oms.live() == 0 && oms.all_settled(), "every submitted order settled (live == 0)");
        CHECK(filled_qty <= submitted_qty, "filled qty <= submitted qty (no phantom fills)");
    }

    std::printf("\n%s  (%d failures)\n", fails == 0 ? "OMS: ALL PASS" : "OMS: SOME FAILED", fails);
    return fails == 0 ? 0 : 1;
}
