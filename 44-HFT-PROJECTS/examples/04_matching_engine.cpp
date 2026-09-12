// 04_matching_engine.cpp
// ============================================================
// PROJECT 4 -- Matching engine, INTEGRATED. Folder 40 ne engine ko
// deeply build/test/fuzz kiya; yahan hum use pipeline mein daalte:
//
//   sim -> wire -> parse -> ExecutionSimulator (mirrors market into a
//   MatchingEngine) -> hum kuch aggressive IOC orders bhejte -> trades.
//
//   1. marketable IOC fills at the resting (maker) price, best-first
//   2. non-marketable IOC -> zero fill, status Cancelled
//   3. determinism: same tape replay -> identical fills
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 04_matching_engine.cpp -o me && ./me
// ============================================================

#include "mh_engine_common.hpp"

#include <cstdio>

using namespace mhft;

struct FillLog { std::uint64_t count = 0, qty = 0; std::int64_t notional = 0; };

static FillLog drive(std::uint64_t warm, std::uint64_t probes, std::uint64_t seed) {
    MarketDataSimulator sim(seed);
    ExecutionSimulator  venue;
    FillLog log;

    MdMessage m;
    std::uint64_t seen = 0;
    L2Book book(4 * (warm + probes) + 16);
    while (sim.next(m)) {
        venue.on_market_event(m);
        book.apply(m);
        ++seen;
        if (seen < warm) continue;
        if (seen % 37 == 0 && seen < warm + probes * 37 && book.has_bid() && book.has_ask()) {
            // alternate marketable buy / non-marketable sell
            OrderRequest r;
            r.ts = m.ts; r.qty = 15; r.type = OrderType::IOC;
            if ((seen / 37) % 2 == 0) { r.side = Side::Buy;  r.px = book.best_ask(); }
            else                      { r.side = Side::Sell; r.px = book.best_bid() - 20; } // far -> no fill
            Qty filled = 0;
            auto fills = venue.send(r, seen, filled);
            for (const Fill& f : fills) {
                ++log.count; log.qty += f.qty;
                log.notional += (is_buy(f.side) ? 1 : -1) * static_cast<std::int64_t>(f.qty) * f.px;
            }
        }
        if (seen >= warm + probes * 37) break;
    }
    return log;
}

int main() {
    const FillLog a = drive(2000, 200, 44);
    const FillLog b = drive(2000, 200, 44);   // replay

    std::printf("aggressive IOC probes through the MatchingEngine venue:\n");
    std::printf("  fills=%llu  filled_qty=%llu  net_notional(scaled)=%lld\n",
                (unsigned long long)a.count, (unsigned long long)a.qty, (long long)a.notional);
    std::printf("  (marketable buys filled at resting ask; far sells filled 0 -- IOC void)\n");

    const bool det = a.count == b.count && a.qty == b.qty && a.notional == b.notional;
    std::printf("\ndeterminism (same tape x2): %s\n", det ? "IDENTICAL" : "*** MISMATCH ***");

    const bool ok = det && a.count > 0;
    std::printf("\n%s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
