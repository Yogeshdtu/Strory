// 09_risk_engine.cpp
// ============================================================
// PROJECT 9 -- Pre-trade risk engine. Har check ko explicitly trigger
// karke verify karo (37/13):
//   1. OK path
//   2. fat-finger  (qty / notional)
//   3. price collar (order too far from mid)
//   4. position limit (post-trade projection)
//   5. rate limit  (rolling window) -- NOTE: does NOT count toward kill
//   6. kill switch  (after kKillAfter real breaches -> everything rejected)
//   7. on_fill() position tracking
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 09_risk_engine.cpp -o re && ./re
// ============================================================

#include "mh_risk_engine.hpp"

#include <cstdio>

using namespace mhft;

static int fails = 0;
#define CHECK(cond, name) do { \
    if (cond) std::printf("  [ok]   %s\n", name); \
    else    { std::printf("  [FAIL] %s\n", name); ++fails; } } while (0)

static OrderRequest mk(Side s, Price px, Qty q, Ts ts = 0) {
    OrderRequest r; r.side = s; r.type = OrderType::IOC; r.px = px; r.qty = q; r.ts = ts;
    return r;
}

int main() {
    const std::int64_t mid2 = 20000;   // mid = 100.00

    // 1. OK
    {
        RiskEngine re;
        CHECK(re.check(mk(Side::Buy, 10005, 20), mid2, 0) == RiskVerdict::Ok, "clean order -> Ok");
    }
    // 2. fat finger
    {
        RiskEngine re;
        CHECK(re.check(mk(Side::Buy, 10005, 999), mid2, 0) == RiskVerdict::RejFatFinger,
              "qty 999 > max 100 -> RejFatFinger");
        CHECK(re.check(mk(Side::Buy, 10005, 0), mid2, 0) == RiskVerdict::RejFatFinger,
              "qty 0 -> RejFatFinger");
    }
    // 3. price collar (band 0.50%; order at 106.00 vs mid 100.00 = 6% away)
    {
        RiskEngine re;
        CHECK(re.check(mk(Side::Buy, 10600, 20), mid2, 0) == RiskVerdict::RejPriceCollar,
              "px 6% from mid -> RejPriceCollar");
        CHECK(re.check(mk(Side::Buy, 10040, 20), mid2, 0) == RiskVerdict::Ok,
              "px 0.4% from mid -> Ok (within 0.5% band)");
    }
    // 4. position limit (max_position 400)
    {
        RiskEngine re;
        Fill f; f.side = Side::Buy; f.qty = 390; f.px = 10000;
        re.on_fill(f);                                  // position now +390
        CHECK(re.position() == 390, "on_fill moved position to +390");
        CHECK(re.check(mk(Side::Buy, 10005, 20), mid2, 0) == RiskVerdict::RejPositionLimit,
              "+390 then buy 20 -> +410 > 400 -> RejPositionLimit");
        CHECK(re.check(mk(Side::Sell, 9995, 20), mid2, 0) == RiskVerdict::Ok,
              "+390 then sell 20 -> +370 -> Ok");
    }
    // 5. rate limit (max 200 / 1ms window) -- and NOT a kill trigger
    {
        RiskEngine re;
        int rate_rej = 0;
        for (int i = 0; i < 260; ++i)
            if (re.check(mk(Side::Buy, 10005, 5), mid2, 100 /*same window*/) == RiskVerdict::RejRateLimit)
                ++rate_rej;
        CHECK(rate_rej >= 50, "260 msgs in one window -> >=50 RejRateLimit");
        CHECK(!re.is_killed(), "rate-limit breaches do NOT trip the kill switch");
        // new window -> allowed again
        CHECK(re.check(mk(Side::Buy, 10005, 5), mid2, 2'000'000) == RiskVerdict::Ok,
              "next window -> Ok again");
    }
    // 6. kill switch after kKillAfter (50) REAL breaches
    {
        RiskEngine re;
        for (int i = 0; i < 60; ++i) re.check(mk(Side::Buy, 10600, 20), mid2, 0);  // collar breaches
        CHECK(re.is_killed(), "50+ real breaches -> killed");
        CHECK(re.check(mk(Side::Buy, 10005, 20), mid2, 0) == RiskVerdict::RejKilled,
              "after kill -> even a clean order is RejKilled");
    }
    // 7. explicit kill
    {
        RiskEngine re;
        re.kill();
        CHECK(re.check(mk(Side::Buy, 10005, 20), mid2, 0) == RiskVerdict::RejKilled, "manual kill()");
    }

    std::printf("\n%s  (%d failures)\n", fails == 0 ? "ALL RISK CHECKS PASS" : "SOME FAILED", fails);
    return fails == 0 ? 0 : 1;
}
