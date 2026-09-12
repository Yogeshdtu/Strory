// 08_strategy_sim.cpp
// ============================================================
// PROJECT 8 -- Strategy simulator / backtester.
//
// >>> YEH ALPHA NAHI HAI. <<<  SpreadCrossStrategy ek MECHANICAL rule hai
// (SMA crossover + book imbalance -> IOC quote). Iska "P&L" ka koi
// predictive matlab nahi -- yeh sirf pipeline exercise karne aur
// deterministic backtest infra dikhane ke liye hai (37 SPECIALIZED list).
//
//   1. run over the sim feed via FastVenue -> signal count, fills, P&L
//   2. mark-to-market P&L = realized cash flow + position * final mid
//   3. determinism: same seed -> identical result
//   4. parameter sweep: cooldown vs signals/fills (infra demo, not tuning advice)
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 08_strategy_sim.cpp -o ss && ./ss
// ============================================================

#include "mh_engine_common.hpp"
#include "mh_fast_venue.hpp"

#include <cstdio>

using namespace mhft;

struct BtResult {
    std::uint64_t signals = 0, fills = 0, filled_qty = 0;
    std::int64_t  position = 0;
    std::int64_t  cash = 0;          // scaled
    std::int64_t  final_mid2 = 0;
};

static BtResult backtest(std::uint64_t n, std::uint64_t seed, std::size_t cooldown) {
    MarketDataSimulator sim(seed);
    sim.set_limit(n);
    L2Book book(3 * n + 16);
    FastVenue venue;
    SpreadCrossStrategy::Config sc; sc.cooldown = cooldown;
    SpreadCrossStrategy strat(sc);

    BtResult r;
    MdMessage m;
    while (sim.next(m)) {
        book.apply(m);
        venue.on_market_event(m);
        const StrategyDecision d = strat.on_book(book);
        if (!d.act) continue;
        ++r.signals;
        OrderRequest req; req.side = d.side; req.type = d.type;
        req.px = d.px; req.qty = d.qty; req.ts = m.ts;
        Qty filled = 0;
        auto fills = venue.send(req, r.signals, filled);
        for (const Fill& f : fills) {
            ++r.fills; r.filled_qty += f.qty;
            const std::int64_t sq = is_buy(f.side) ? static_cast<std::int64_t>(f.qty)
                                                   : -static_cast<std::int64_t>(f.qty);
            r.position += sq;
            r.cash     -= sq * f.px;
        }
        strat.note_fired();
        if (book.has_bid() && book.has_ask()) r.final_mid2 = book.mid2();
    }
    return r;
}

int main() {
    constexpr std::uint64_t N = 200000;
    std::puts(">>> SpreadCrossStrategy is MECHANICAL, NOT alpha. P&L below is meaningless\n"
              ">>> as a predictor -- this is backtest INFRASTRUCTURE, not a signal.\n");

    const BtResult a = backtest(N, 44, 40);
    const BtResult b = backtest(N, 44, 40);   // replay

    // mark-to-market: realized cash + position marked at final mid
    const std::int64_t mtm = a.cash + a.position * (a.final_mid2 / 2);
    std::printf("backtest (N=%llu, seed=44, cooldown=40):\n", (unsigned long long)N);
    std::printf("  signals=%llu  fills=%llu  filled_qty=%llu  position=%lld\n",
                (unsigned long long)a.signals, (unsigned long long)a.fills,
                (unsigned long long)a.filled_qty, (long long)a.position);
    std::printf("  realized cash (scaled) = %lld\n", (long long)a.cash);
    std::printf("  mark-to-market P&L (scaled, /100 for $) = %lld  (~$%.2f)\n",
                (long long)mtm, static_cast<double>(mtm) / 10000.0);

    const bool det = a.signals == b.signals && a.fills == b.fills &&
                     a.filled_qty == b.filled_qty && a.position == b.position && a.cash == b.cash;
    std::printf("\ndeterminism (same seed x2): %s\n", det ? "IDENTICAL" : "*** MISMATCH ***");

    std::puts("\nparameter sweep (cooldown -> signals / fills)  [infra demo, NOT tuning advice]:");
    for (std::size_t cd : {10u, 20u, 40u, 80u, 160u}) {
        const BtResult r = backtest(N, 44, cd);
        std::printf("  cooldown=%3zu  signals=%5llu  fills=%5llu  |position|=%lld\n",
                    cd, (unsigned long long)r.signals, (unsigned long long)r.fills,
                    (long long)(r.position < 0 ? -r.position : r.position));
    }
    return det ? 0 : 1;
}
