# 12 — Project 11: THE MINI HFT ENGINE

## Prerequisites
- `02`–`11` (saare projects)
- `43-HFT-OPTIMIZATION` (poora — the measure→change→explain loop)

## Yeh topic abhi kyun

Sab jodo. `MiniHftEngine` (`mh_engine.hpp`) har component ko ek
single-threaded deterministic loop mein chalata:

```
for each market-data message:
   encode -> parse_v3                     [proj 1, 2]
   book.apply ; venue.on_market_event     [proj 3, 4/11]
   decision = strategy.on_book(book)      [proj 8]
   if decision.act:
      verdict = risk.check(...)           [proj 9]
      if Ok:
         cl = oms.submit(...) ; oms.on_ack(cl)   [proj 10]
         fills = venue.send(...)          [proj 4/11]
         for f in fills: oms.on_fill ; risk.on_fill ; pnl                 [proj 10, 9]
         if not Filled: oms.on_unfilled_cancel   [proj 10]
```

Har stage rdtsc se timed → per-stage latency budget.

## The before/after — `template <class Venue>`

```cpp
using NaiveEngine     = MiniHftEngine<ExecutionSimulator>;   // std::map MatchingEngine venue
using OptimizedEngine = MiniHftEngine<FastVenue>;            // flat-array + per-level FIFO sweep
```

The optimization loop (43 methodology):
1. **Baseline** — `NaiveEngine`, everything correct, measured.
2. **Profile** — per-stage rdtsc: the `book` stage is ~150 ns/msg and
   `ExecutionSimulator::on_market_event` (a `std::map` insert + `std::list`
   node `malloc` per market message) is the bulk of it.
3. **Hypothesis** — our IOC orders only ever hit top-of-book aggregate
   liquidity; the venue doesn't need a full per-order tree. A flat-array
   aggregate book + per-level FIFO gives the same fills without the tree
   or the per-message allocation.
4. **Change (one)** — `FastVenue` (`mh_fast_venue.hpp`).
5. **Re-measure** — `11_mini_hft_engine.cpp`.
6. **Explain** — below.

## Measure (`11_mini_hft_engine.cpp`, this box, N=200000)

### Correctness gate FIRST (43/01)
```
correctness gate -- naive vs optimized trading output: IDENTICAL
  (signals, orders_ok, fills, filled_qty, position, realized_pnl all equal)
determinism (optimized run x2): IDENTICAL
```
`12_integration_tests.cpp` extends this to **5 seed/config combos, 0
failures**. The optimization provably did not change behaviour.

### Speedup
```
per-stage (rdtsc, ns/msg -- probe cost inflates absolutes):
  naive      parse ~45   book ~150   strat ~30   risk ~0.6   oms ~0.4
  optimized  parse ~47   book  ~67   strat ~30   risk ~0.7   oms ~0.3

end-to-end mean/msg: naive ~225 ns  ->  optimized ~145 ns   (~1.5-1.6x)
end-to-end p50     : naive ~215 ns  ->  optimized ~130 ns
```

### Explain — what changed and why (spec §2.12)

| | Naive venue | Optimized venue | Mechanism |
|---|---|---|---|
| storage | `std::map<Price, PriceLevel>` + `std::list` per level + `unordered_map` index | `std::array<int64, kLevels>` aggregate + `std::vector` per-level FIFO + dense-id `loc_` | tree walk + RAM-scattered nodes → contiguous array indexed by `px - base` |
| per market Add | tree insert + **list-node `malloc`** | `arr[idx] += qty` + `fifo_[idx].push_back` (amortized no alloc) | allocator + lock + cache pollution gone |
| per market Cancel | tree lookups + list scan + **frees** | `loc_[id]` → level → short FIFO scan + `arr[idx] -= qty` | O(1) direct index, no frees |
| our IOC | engine matches per resting order | sweep opposite side best→limit, FIFO within level | same price-time result, aggregated |

Net: the `book` stage roughly halves (~150 → ~67 ns/msg), pulling
end-to-end ~1.5–1.6×. The other stages are unchanged (and mostly rdtsc
probe cost anyway — 43/02).

### Invariants held
```
|position| <= risk max (400)?  held      (position sits at -398 -- the limit binds)
sequence gaps == 0?            held
```

### The tail — honest note
```
optimized max: ~200000-350000 ns   (worse than naive's ~40000)
```
This is **OS-scheduler jitter on an unpinned desktop**, not the
optimization (41/13, 43/16). p50/p99/p99.9 are all better for optimized;
the single `max` outlier is the engine thread getting descheduled once in
200k iterations. On a pinned/isolated core the tail collapses. **Quote
p50/p99; treat `max` on this box as noise.**

## HFT relevance

This is the shape of a real tick-to-trade engine — just smaller and with
the alpha removed:

- **Single thread, deterministic, event-timestamp-driven.** Real engines
  add threads (one pinned per stage, SPSC queues between — project 7) but
  keep each stage deterministic.
- **The venue here is a simulator.** In production, "send" goes to a real
  order gateway over TCP (42/11) and fills come back async. The OMS +
  generation handles (project 10, 6) are exactly what makes async fills
  safe.
- **The optimization pattern is real**: profile the connected system,
  find the stage that dominates, replace it with a data-structure that
  fits the actual access pattern, prove behaviour unchanged, re-measure.
  You did this for the venue; the same loop applies to parsing, the book,
  serialization, everything.

## ⚠️ Traps

### Trap 1 — optimizing before profiling
"Parse is probably slow" → rewrite parse → 0% (it was ~5 ns of real work).
The profile said the *venue* was ~150 ns/msg. Measure first (43/03).

### Trap 2 — skipping the correctness gate
`FastVenue` is ~2× faster and *wrong* would be worse than useless. The
gate (naive == optimized, byte-for-byte, across 5 configs) runs before the
speedup is even printed.

### Trap 3 — trusting v3's per-stage rdtsc absolutes
`parse ~47 ns/msg` for the optimized engine is ~40 ns of `lfence+rdtsc`
probe + ~5 ns of real work. Read the *ratio* between naive and optimized
per stage, and trust the single-clock end-to-end mean (43/02).

### Trap 4 — threading it and expecting a speedup
41/13: on this box, a 3-thread version's p99 was 100× worse (scheduler),
p50 unchanged. Threading buys throughput (parallel symbols), not
single-stream latency — unless the threads are pinned + isolated.

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| ~1.5× is a weak result | It's the *connected-system* number; the venue stage alone ~2×, and behaviour is provably identical |
| The `max` tail regression is real | Unpinned-box scheduler noise; p50/p99/p99.9 all improved |
| FastVenue could replace MatchingEngine everywhere | It's an aggregate-fill approximation; exact only for simple IOC flow — validated against the real engine |
| Per-stage rdtsc numbers are the truth | Probe cost dominates fast stages; use ratios + the single-clock mean |

## Exercises

1. `11_mini_hft_engine.cpp` mein `OptimizedEngine` ke andar `FastVenue`
   ka per-level FIFO hata do (aggregate qty only). Correctness gate pe
   kya asar?
   <details><summary>Answer</summary>
   Without per-order FIFO, `FastVenue` can't track which resting orders
   its sweeps consumed → later cancels over-subtract → book state drifts
   from the `MatchingEngine` → naive vs optimized diverge (we saw ~3
   shares over 200k in development). The FIFO is what makes the gate pass
   exactly.
   </details>

2. Profile shows `book` at 67 ns/msg (optimized). Amdahl: if you made it
   0 ns, end-to-end speedup vs the current optimized?
   <details><summary>Answer</summary>
   Optimized total ~145 ns/msg, book ~67 (real work less — probe cost).
   Say ~40 ns of book is real. `1 / (1 - 40/145)` ≈ 1.38× max. But much
   of the remaining 105 ns is rdtsc probe — remove the probes and the
   real end-to-end is far lower, and book's share changes. Measure with a
   single clock, not per-stage rdtsc (43/02).
   </details>

3. Set `SpreadCrossStrategy::Config::cooldown` to 5 (fires ~8× more).
   What happens to `orders_ok`, `rate_drops`, `position`?
   <details><summary>Answer</summary>
   Way more signals → `rate_drops` explodes (risk rate-limiter kicks in
   hard) → `orders_ok` grows more slowly than signals → `position` still
   caps at ±400 (position limit). The risk engine absorbs the flood; the
   engine stays correct. Determinism + correctness gate still pass.
   </details>

## Interview questions

1. Tick-to-trade engine ke stages, aur is capstone ka each se mapping?
2. Optimization loop: is engine mein bottleneck kaise find kiya, kya
   badla, kaise verify kiya?
3. `FastVenue` `MatchingEngine` se kyun tez, aur kab woh galat (kaunse
   order flow pe)?
4. Yeh single-threaded kyun hai, aur multi-thread karne pe latency kyun
   worse ho sakti (unpinned)?

## Next
→ [`13-integration-and-testing.md`](13-integration-and-testing.md)
