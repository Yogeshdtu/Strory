# 09 — Project 8: Strategy framework + backtester (no alpha)

## Prerequisites
- `08-project-spsc-queue.md`
- `37-HFT-FUNDAMENTALS/10-hft-strategies-overview.md` + the SPECIALIZED /
  DOMAIN-SPECIFIC list (alpha is out of scope)
- `43-HFT-OPTIMIZATION/09-fixed-point.md`, `10-avoiding-division.md`

## Yeh topic abhi kyun

Pipeline ka "brain." Book state aata, ek order decision jaata. **Lekin —
yeh alpha nahi hai.**

> Real trading signals (alpha) proprietary IP hain. Koi public nahi karta.
> 37 ki SPECIALIZED list mein yeh explicitly out of scope hai. Jo yahan hai
> woh ek **mechanical rule** hai jiska ekmatra kaam pipeline ko exercise
> karna aur ek deterministic backtest infrastructure dikhana hai. Iske
> backtest P&L ka koi predictive matlab **nahi** hai.

## `SpreadCrossStrategy` (`mh_strategy.hpp`)

Rule: mid ki 256-message SMA se dislocation + book imbalance:

```
mid2      = best_bid + best_ask                     (== 2 * mid, no /2 -- 43/09)
ring_sum += mid2 - ring[pos] ; ring[pos] = mid2     (running sum, no re-scan -- 43/10)
fire up   <=>  mid2 * window * thr_den  >  ring_sum * thr_num     (all integer, no division)
              AND  imbalance > -imb_block           (soft filter: don't buy a collapsing bid)
-> IOC buy @ best_ask, qty = order_qty
(symmetric for the down case -> IOC sell @ best_bid)
cooldown: after firing, wait `cooldown` messages
```

Everything is a **pure function of book state** — no wall clock, no RNG,
no hidden state beyond the ring + cooldown counter. Fully deterministic,
replay-safe.

Design carried from folder 43:
- prices are integer ticks (43/09)
- SMA via ring + running sum, no per-tick re-scan (43/10)
- threshold check by cross-multiply, **zero division** in the hot path
  (43/10)

## Measure (`08_strategy_sim.cpp`)

```
>>> SpreadCrossStrategy is MECHANICAL, NOT alpha. P&L below is meaningless. <<<

backtest (N=200000, seed=44, cooldown=40):
  signals=~4700  fills=~4700  filled_qty=~90000  position=~-15000
  mark-to-market P&L (scaled) = negative  (~ -$500)

determinism (same seed x2): IDENTICAL

parameter sweep (cooldown -> signals / fills)  [infra demo, NOT tuning advice]:
  cooldown= 10  signals=17290  ...
  cooldown= 40  signals= 4715  ...
  cooldown=160  signals= 1213  ...
```

- **Determinism**: same seed → byte-identical result. This is what makes a
  backtest trustworthy — a change in output means a change in *code*, not
  in luck.
- **P&L is negative.** A mechanical spread-crossing rule pays the spread
  every time it fires and has no edge — losing money is the *expected*
  outcome. That's fine; it's not a signal.
- **Position runs away** (~-15000 shares at cooldown 40). `08` deliberately
  omits the risk engine, so there's nothing capping position. This is
  exactly why project 9 (risk) exists — in `11_mini_hft_engine` the risk
  position limit binds and caps it at ±400.
- The **parameter sweep** is *backtest infrastructure* (vary a knob,
  re-run, compare), not tuning advice — there's no alpha to tune.

## HFT relevance

- A strategy framework is a plug-in shape: `on_book(book) -> decision`.
  Real systems have many strategies behind one interface, dispatched at
  config time (43/12), not per-tick.
- Backtesting infrastructure — deterministic replay, parameter sweeps,
  P&L attribution, transaction-cost modelling — is real, valuable
  engineering. The *signals* that go in it are the part nobody shares.
- The decision must be cheap (`on_book` is on the hot path, every message).
  Integer math, no allocation, no division — same discipline as the rest
  of the pipeline.

## ⚠️ Traps

### Trap 1 — thinking this is a real strategy
It loses money by construction. Don't "improve" it — that's not the
exercise. The exercise is the *plumbing* around it.

### Trap 2 — float in the signal
`double` mid / SMA → non-deterministic-feeling results at the threshold
boundary (43/09), and `/2` + `/window` divisions on the hot path (43/10).
Integer + cross-multiply.

### Trap 3 — hidden state / wall clock in the strategy
`std::chrono::now()`, a `static` counter, an RNG → replay no longer
byte-identical → backtest untrustworthy. Everything a pure function of the
passed-in book.

### Trap 4 — no cooldown / no position awareness
Fires every message it can → floods risk → rate-limited (project 9) →
position runs away if risk is off. Cooldown + (in the real engine) a
position-aware risk gate.

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| This strategy just needs tuning to be profitable | It has no edge by construction; tuning noise |
| Backtest P&L number = expected live P&L | Even a real signal's backtest overstates (costs, slippage, overfit) |
| Strategy can be slow, it fires rarely | `on_book` runs *every* message; must be cheap |
| Float is fine for a signal | Non-deterministic boundary + hot-path division |

## Exercises

1. `08_strategy_sim.cpp` mein risk engine add karo (position limit 400).
   Position aur P&L pe kya asar?
   <details><summary>Answer</summary>
   Position ±400 pe cap ho jaayega (further same-side orders RejPositionLimit).
   Fills kam, filled_qty kam. P&L still negative but bounded. Yeh exactly
   `11_mini_hft_engine` ka behaviour — risk gate hai.
   </details>

2. Strategy ko "only trade when spread == 1 tick" filter add karo. Signal
   count pe asar? Kya yeh alpha hai?
   <details><summary>Answer</summary>
   Signals kam (narrow spread rare-ish). Nahi, yeh alpha nahi — bas ek
   execution filter (tight spread = cheaper to cross). Alpha = *predicting*
   which direction mid moves. That's the part we don't build.
   </details>

3. `thr_num/thr_den` ko `1000/1000` (0% threshold) kar do. Kya hota?
   <details><summary>Answer</summary>
   Fire whenever `mid2 * window > ring_sum` i.e. mid above its own SMA at
   all → fires ~half the eligible messages → flood → rate-limited, position
   pinned. Threshold ka kaam noise filter karna — 0% = no filter.
   </details>

## Interview questions

1. Strategy `on_book` deterministic kyun hona chahiye (backtest terms)?
2. Signal ki division-free implementation — SMA cross ko cross-multiply se
   kaise?
3. Backtest P&L live P&L se kyun overstate karta (3 reasons)?
4. Strategy framework mein dispatch (kaunsi strategy) — per-tick ya
   config-time? Kyun (43/12)?

## Next
→ [`10-project-risk-engine.md`](10-project-risk-engine.md)
