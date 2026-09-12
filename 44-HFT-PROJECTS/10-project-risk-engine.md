# 10 — Project 9: Pre-trade risk engine

## Prerequisites
- `09-project-strategy-simulator.md`
- `37-HFT-FUNDAMENTALS/13-risk-systems.md`

## Yeh topic abhi kyun

Strategy aur exchange ke **beech** ka non-negotiable gate. Ek bhi outbound
order bina risk check ke exchange tak nahi jaana chahiye. Yeh woh cheez hai
jo "fat finger" $460M loss (Knight Capital, 2012) ko rok deti.

`09` (strategy) ne dikhaya ki bina risk ke position runaway ho jaata
(~-15000 shares). Risk engine use ±400 pe cap karta.

## `RiskEngine` (`mh_risk_engine.hpp`)

`check(const OrderRequest&, mid2, now) -> RiskVerdict`. Saare checks O(1),
hot path pe:

| # | Check | Rule | Verdict |
|---|---|---|---|
| 1 | **Fat finger** | `qty != 0 && qty <= max_order_qty && qty*px <= max_order_notional` | `RejFatFinger` |
| 2 | **Price collar** | `\|px*2 − mid2\| * 10000 <= band_bps * mid2` (integer; px far from mid → reject) | `RejPriceCollar` |
| 3 | **Position limit** | projected `\|position + (buy? +qty : −qty)\| <= max_position` (worst case = full fill) | `RejPositionLimit` |
| 4 | **Message rate** | rolling window: `<= max_msgs` per `window_ns` | `RejRateLimit` |
| 5 | **Kill switch** | after `kKillAfter` (50) *real* breaches → everything rejected | `RejKilled` |

`on_fill(Fill)` updates the real (post-trade) position + traded notional.
Deterministic — `now` comes from the event stream, not a wall clock, so
the rate-limit window is replay-stable.

### Design decision: rate-limit ≠ kill
A busy strategy is not a *broken* one. `RejRateLimit` **drops** the order
(backpressure) but does **not** count toward the kill switch. Only "real"
breaches (fat-finger, collar, position) — which indicate a bug or a
runaway — accumulate toward `kill()`. `09_risk_engine.cpp` asserts this
explicitly.

## Measure (`09_risk_engine.cpp`)

Every path triggered on purpose:
```
  [ok]   clean order -> Ok
  [ok]   qty 999 > max 100 -> RejFatFinger
  [ok]   qty 0 -> RejFatFinger
  [ok]   px 6% from mid -> RejPriceCollar
  [ok]   px 0.4% from mid -> Ok (within 0.5% band)
  [ok]   +390 then buy 20 -> +410 > 400 -> RejPositionLimit
  [ok]   +390 then sell 20 -> +370 -> Ok
  [ok]   260 msgs in one window -> >=50 RejRateLimit
  [ok]   rate-limit breaches do NOT trip the kill switch
  [ok]   next window -> Ok again
  [ok]   50+ real breaches -> killed
  [ok]   after kill -> even a clean order is RejKilled
  [ok]   manual kill()
ALL RISK CHECKS PASS (0 failures)
```

In `11_mini_hft_engine`, over 200k messages: `orders_ok=240`,
`rate_drops=79`, `risk_rejects=4396` — the collar/position checks reject
the vast majority of the mechanical strategy's over-eager quotes, and
`position` sits at −398 (the ±400 limit binding). The risk engine is
visibly doing its job.

## HFT relevance

- Risk is a **hard gate in the order path**, not a monitoring dashboard.
  It runs synchronously, before every order, in nanoseconds.
- Real risk engines add: per-symbol limits, gross vs net exposure,
  short-sale locate checks, self-match prevention, restricted lists,
  credit/margin, and exchange-mandated limits.
- The **kill switch** is sacred: one flag, checked first, and once set,
  nothing gets out. Often wired to a physical button and to automated
  triggers (P&L drawdown, message-rate anomaly, position drift).
- It must be **fast** (it's on the hot path) and **simple** (you must be
  able to reason about it under pressure).

## ⚠️ Traps

### Trap 1 — risk as async / advisory
"We'll check risk on a background thread." No. By the time the background
thread flags it, the order is filled and the position is real. Synchronous,
in-path, or it's not risk.

### Trap 2 — checking post-fill position only
Check the **projected** position assuming a full fill *before* sending. A
partial-fill assumption lets a burst of orders each individually pass while
collectively blowing the limit.

### Trap 3 — wall clock in the rate window
`now - window_start >= window_ns` with `now = steady_clock::now()` →
non-deterministic, un-testable. Use the event timestamp.

### Trap 4 — kill switch that can be un-set by normal flow
Once killed, only an explicit operator action (or process restart) should
clear it. A code path that quietly resets `killed_` defeats the purpose.

### Trap 5 — collar against a stale mid
The collar compares `px` to `mid2`. If the book's cached BBO is stale
(project 3 trap), the collar rejects good orders or passes bad ones.
Risk is only as good as the book it references.

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Risk is monitoring | Risk is a synchronous gate in the order path |
| Check position after the fill | Check *projected* position (full-fill assumption) before sending |
| Rate-limit = something is broken | Rate-limit = backpressure; only real breaches → kill |
| Kill switch can auto-recover | Kill is sticky; needs explicit operator clear |

## Exercises

1. `09_risk_engine.cpp` mein position limit check ko "post-fill" bana do
   (projected ki jagah current position). Kaunsa scenario ab pass ho jaata
   jo nahi hona chahiye?
   <details><summary>Answer</summary>
   Current position +390, `max` 400. 5 buy-20 orders back to back: har ek
   *current* position +390 dekh ke pass (each thinks "390 + 20 = 410... "
   wait, current is 390, so still rejects). Actually the bug bites with
   *partial* fills: if fills lag, `on_fill` hasn't updated yet, so current
   stays +390 while 3 orders each pass → +450 real. Projected (full-fill)
   assumption prevents it.
   </details>

2. `band_bps` ko 5 (0.05%) kar do. `11_mini_hft_engine` ke `orders_ok` pe
   asar?
   <details><summary>Answer</summary>
   Strategy IOC-quotes at best_bid/best_ask, ~1 tick from mid ≈ 0.005% —
   still inside 0.05%. So most still pass. Tighten to `band_bps = 0` and
   nearly everything rejects (any order not exactly at mid). The collar is
   about catching *fat fingers*, not normal quoting.
   </details>

3. Kill switch ke baad `on_fill` aata hai (an in-flight order filled after
   kill). `position` update hona chahiye?
   <details><summary>Answer</summary>
   Haan — `on_fill` real fills track karta regardless of `killed_`. Kill
   stops *new* orders; it doesn't un-happen fills already in flight. The
   position must stay accurate so an operator can flatten it.
   </details>

## Interview questions

1. Risk engine synchronous kyun hona chahiye, async kyun nahi?
2. "Projected position" check vs "current position" check — farak, aur
   partial fills ke saath kyaun projected zaroori?
3. Kill switch ke design rules (sticky, checked-first, explicit clear)?
4. Risk check ki latency kyun matter karti (yeh order path pe hai)?

## Next
→ [`11-project-order-manager.md`](11-project-order-manager.md)
