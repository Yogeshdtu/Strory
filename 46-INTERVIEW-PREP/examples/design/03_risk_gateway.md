# Design problem — Pre-trade risk gateway

> Full reference: course folder 37/13 (risk systems), 44 `mh_risk_engine.hpp`.

---

## Prompt

"Every order the strategy wants to send must pass a pre-trade risk check
before it leaves the building. Design that check. It's on the hot path."

---

## 1. Clarify

- **Which checks?** Fat-finger, position limit, price collar, message
  rate, kill switch — confirm the set.
- **Latency budget?** On the hot path, per order → target tens of ns.
- **Position:** tracked here (from fills) or fed in from a position
  service?
- **Action on breach:** reject the order, or reject + trip the kill
  switch? Which breaches count toward the kill switch?
- **Reference price** for the collar: mid from the book — how stale can
  it be?

## 2. Data model

A handful of integers — fits in one cache line, no allocation, no locks
(single-threaded per instrument):

```
struct RiskState {
    int64  position;          // signed, in lots/shares
    int64  max_position;
    uint32 max_order_qty;
    int64  max_order_notional;
    int64  band_bps;          // price collar half-width
    uint32 msg_count;         // rolling window
    uint32 max_msgs;
    uint64 window_start_ns;
    uint64 window_ns;
    uint32 breach_count;      // real breaches
    uint32 kill_after;        // trip threshold
    bool   killed;
};
```

## 3. The checks (all O(1), in order, before the order is sent)

| # | Check | Logic | On fail |
|---|---|---|---|
| 0 | Kill switch | `if (killed) return RejKilled;` | reject (no counter) |
| 1 | Fat-finger qty | `qty <= max_order_qty` | reject + `breach()` |
| 2 | Fat-finger notional | `qty * px_ticks <= max_order_notional` (watch i64 overflow) | reject + `breach()` |
| 3 | Price collar | `abs(px*2 − mid2) * 10000 <= band_bps * mid2` (mid2 = bid+ask, avoids a `/2`) | reject + `breach()` |
| 4 | Position limit | `abs(position ± qty) <= max_position` (projected **full** fill) | reject + `breach()` |
| 5 | Message rate | roll the window; `msg_count < max_msgs` | **drop** (`rate_drops++`), **not** a `breach()` |

`breach()` increments `breach_count`; at `kill_after` real breaches →
`killed = true` (block everything until a human resets).

`on_fill(fill)` updates the real `position`.

## 4. Why rate-limit ≠ kill switch

A burst of orders (a fast market, a strategy loop) should be *throttled*,
not treated as N fat-finger errors that trip the kill switch. Route the
rate-limit path through its own counter (`rate_drops`), returning
`RejRateLimit` directly. (Real folder-44 bug: rate-limit breaches were
tripping the kill switch after 10 → tuned to a separate counter +
`kill_after` 50.)

## 5. Failure modes

| Failure | Handling |
|---|---|
| Stale `mid` (feed gap) | collar check uses a bad reference → widen the band on stale data, or suspend quoting until the book is fresh |
| i64 overflow in `qty * px` | use `__int128` or bound the inputs; a notional check that overflows is worse than no check |
| Kill switch tripped | loud, persistent alert; require a manual, logged reset — never auto-clear |
| Position drift (missed fill) | reconcile against the OMS / exchange position periodically; alert on mismatch |
| Clock jump (window math) | monotonic clock only; a backward jump must not reset the window to "empty" |

## 6. Trade-offs

- **Strict collars** → fewer bad orders, but more *missed* quotes near
  fast moves. Tune per instrument / volatility.
- **Projected full fill** for the position check (conservative — assumes
  the whole order fills) vs **expected fill** (tighter, riskier).
- **Check here (per instrument)** vs **a central risk service**
  (portfolio-level limits, but adds a hop / a dependency on the hot
  path). Common: fast local checks + async portfolio risk that can flip
  the local kill switch.

## 7. Measure / verify

- Unit-test **every verdict path** explicitly (each rejection reason
  triggered; kill switch trips at exactly `kill_after`; rate-limit does
  **not** trip it) — folder 44's `09_risk_engine.cpp` does 14/14.
- `rdtsc` the check on the hot path → it should be a few ns (a handful
  of integer comparisons).
- In a full-engine replay, assert the invariant `|position| ≤
  max_position` holds for the entire session.
