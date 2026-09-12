# 13 — Integration and testing: replay, determinism, invariants

## Prerequisites
- `12-project-mini-hft-engine.md`
- `40-MATCHING-ENGINE/16-testing.md` (reference-model + fuzz),
  `39-ORDER-BOOK/16-testing-order-book.md` (invariants)

## Yeh topic abhi kyun

Ek pipeline jo compile hoti hai aur "chalti hai" — woh **kaafi nahi**. HFT
mein ek silent bug = paisa. Yeh lesson batata ki poore system ko kaise
verify karte: replay determinism, cross-implementation equivalence, aur
executable invariants.

## The three test types

### 1. Determinism (replay = byte-identical)
Run the engine twice with the same seed/config; every `Stats` field must
match exactly.

```cpp
const auto a = run_engine<OptimizedEngine>(N);
const auto b = run_engine<OptimizedEngine>(N);
assert(a.fills == b.fills && a.filled_qty == b.filled_qty &&
       a.position == b.position && a.realized_pnl == b.realized_pnl && ...);
```

Why it's the foundation: if replay isn't deterministic, **no other test
means anything** — a failure could be the bug or could be luck. Determinism
requires: no wall clock, no un-seeded RNG, no `unordered_map` iteration
order dependence, no uninitialized reads, no data races.

### 2. Cross-implementation equivalence (the correctness gate)
The "correct/simple" version and the "optimized" version must produce
**identical output**.

```cpp
NaiveEngine     n(seed, cfg);   // std::map MatchingEngine venue
OptimizedEngine o(seed, cfg);   // FastVenue
assert(same_trading(n.run(N), o.run(N)));   // fills, qty, pnl, position, signals
```

`12_integration_tests.cpp` does this across **5 (seed, cooldown, threshold)
combos** — `{44,40,1002}`, `{7,40,1002}`, `{123,20,1002}`, `{44,80,1001}`,
`{999,60,1003}` — and asserts:
```
INTEGRATION: ALL PASS  (0 failures across 5 configs)
```

This is exactly folder 40's reference-model technique (RefEngine vs the
real engine, 30000-command fuzz, 0 disagreements) and folder 43's
before/after agreement gate — applied to the whole pipeline.

### 3. Executable invariants
Properties that must hold *no matter what*, checked at runtime:

| Invariant | Where | Meaning of a violation |
|---|---|---|
| `seq` strictly `+1` | sim, engine `gap_count` | feed gap / bug in sequence handling |
| book never crossed | `01`, `03` (vs `std::map` ref) | stale BBO, bad apply logic |
| BBO matches a brute-force reference | `03` | cached-BBO tracking bug |
| `\|position\| <= risk max` | `11`, `12` | risk gate bypassed or wrong |
| OMS `live_ == 0` at end | `10` | leaked order (missing terminal event) |
| `filled_qty <= submitted_qty` | `10` | double-counted fills |
| stale handle → `nullptr` | `06` | use-after-recycle |

## The test pyramid for this pipeline

```
   12_integration_tests.cpp   ── whole pipeline, 5 configs, determinism + equivalence + invariants
   11_mini_hft_engine.cpp     ── whole pipeline, 1 config, + latency budget
   01..10 (per-project)       ── each component: correctness + a benchmark
   (folders 39, 40)           ── the reused components' own deep tests + fuzz
```

Run order: component tests first (fast, localized failures), then
integration (slow, catches interaction bugs).

## HFT relevance

- **Determinism is a hard requirement**, not a nice-to-have. It's how you
  reproduce a production incident, how CI catches regressions, and how a
  backtest is trustworthy.
- **Replay from recorded market data** is the real version of
  `01_market_data_sim` — a captured trading day fed through the engine,
  output diffed against a known-good run.
- **Reference models**: a slow, obviously-correct implementation run in
  parallel with the fast one, output compared every step. Folder 40's
  fuzz harness is the template.
- **Property-based / fuzz testing**: generate random-but-valid input
  sequences, check invariants hold. Folder 40 caught the FOK+STP bug this
  way.
- **Shadow trading**: run the new engine alongside the live one on real
  data, don't send its orders, diff the decisions.

## ⚠️ Traps

### Trap 1 — "it passed once"
A non-deterministic bug passes 9 times and fails the 10th. Run determinism
checks many times, ideally with different seeds, and in CI on every commit.

### Trap 2 — comparing means instead of exact output
"Naive P&L ≈ optimized P&L" hides a bug. Compare **exact** integer fields.
If they can't be exactly equal (e.g. float involved), that's a design
smell — make them exact (43/09).

### Trap 3 — `unordered_map` iteration order
Iterating an `unordered_map` and acting on the order → non-deterministic
across runs/compilers/rehashes. Sort keys, or use an ordered container, or
don't depend on order.

### Trap 4 — invariants only in debug builds
`assert` compiles out with `-DNDEBUG`. Critical invariants (risk limits,
position) should be real `if`-checks that log/kill, not `assert`s.

### Trap 5 — testing the sim, not the system
The sim is a test *fixture*. Don't spend all your effort making the sim
realistic — spend it on the invariant checks around the code under test.

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Compiles + runs = tested | Determinism + equivalence + invariants, all checked |
| One passing run is enough | Non-deterministic bugs need many runs / seeds |
| Reference model is wasted effort | It's how folder 40 found a real correctness bug |
| `assert` is enough for risk limits | `assert` compiles out; risk checks are real `if`s |

## Exercises

1. `mh_market_data.hpp` mein `Rng` ko `static` bana do (shared across
   sims). `01_market_data_sim.cpp`'s determinism check kya karega?
   <details><summary>Answer</summary>
   Do sims ek shared RNG se pull karenge → second sim ki sequence pehle
   se drained state se shuru → streams diverge → "NON-DETERMINISTIC".
   Per-object RNG + `reset()` discipline is what makes replay work.
   </details>

2. `12_integration_tests.cpp` mein ek 6th config add karo jaha strategy
   NEVER fires (`thr_num` huge). Kaunsa check ab fail ho sakta?
   <details><summary>Answer</summary>
   "engine did some trading (fills > 0)" fail hoga (0 fills). Determinism
   + equivalence + invariants abhi bhi pass (0 == 0 trivially). The
   `fills > 0` check exists to catch a config where the pipeline silently
   does nothing.
   </details>

3. `L2Book` mein ek deliberate bug daalo: `best_bid_` update only on the
   FIRST add. `03_order_book.cpp` kya dikhaayega?
   <details><summary>Answer</summary>
   `best_bid_` frozen at the first bid level → diverges from the
   `std::map` reference as soon as a higher bid arrives → "BBO DIVERGED",
   mismatch count > 0. The brute-force reference catches exactly this
   class of cached-state bug.
   </details>

## Interview questions

1. Determinism kyun har doosre test ki precondition hai?
2. Reference-model testing — kaise, aur folder 40 mein isne kya pakda?
3. `assert` vs a real `if`-check for a risk invariant — kaunsa kab?
4. `unordered_map` non-determinism kaise ghusti hai, aur fix?

## Next
→ [`14-performance-report.md`](14-performance-report.md)
