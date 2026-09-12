# Examples — Folder 40 (matching engine)

> Portable `.cpp` — sab MinGW/Windows x86-64 pe chalte. **`-O2` mandatory**
> for `08_engine_bench.cpp`. `./build.ps1 folder 40-MATCHING-ENGINE` →
> **8/8 OK** under strict flags. 2 shared `.hpp` headers
> (`matching_engine.hpp`, `engine_workload.hpp`) — glob `*.cpp` inhe alag
> se compile nahi karta.

## The box these were measured on

**AMD Ryzen 7 4700U** (Zen 2), ~2.0 GHz, **Windows x64 + MinGW-w64 GCC
15.1.0**. TSC ~2.0 GHz. **Quote the ratios/shapes** — tail absolutes and
`max` vary run-to-run on this unpinned box (folder 35/06's lesson, carried
forward).

## Scope

Yeh engine ek incoming ("aggressive") order ko resting orders se **khud
match/cross** karta aur khud Trade events **generate** karta — yeh
39-ORDER-BOOK se DIFFERENT hai (39 sirf feed events APPLY karke state
maintain karta, khud koi crossing-decision nahi leta). Detail:
`01-what-is-matching.md`.

Resting-order storage 39's V1 (`std::map` + `std::list` +
`unordered_map` index) jaisa hai — 39 mein storage-optimization ka
3-version story already ho chuka; is folder ka focus **algorithm/
semantics/determinism** hai.

## Compile / run

```bash
./build.ps1 fast 40-MATCHING-ENGINE/examples/08_engine_bench.cpp
```

## Examples

| File | Lesson(s) | Kya |
|---|---|---|
| `matching_engine.hpp` | 02-13 | Poora `MatchingEngine` — Limit/Market/IOC/FOK, STP (3 modes), deterministic sequencing |
| `engine_workload.hpp` | — | Shared deterministic command generator (Submit/Cancel, saare order types + STP modes mixed) |
| `01_matching_engine.cpp` | 01, 02 | Basic crossing demo — resting book, crossing limit, market sweep |
| `02_order_types.cpp` | 03-06 | Limit/Market/IOC/FOK, SAME starting book, side-by-side |
| `03_trade_events.cpp` | 07 | Trade struct fields, "maker sets the price" convention, multi-level sweep |
| `04_self_trade_prevention.cpp` | 08 | Saare 3 STP modes (CancelNewest/Oldest/Both), SAME scenario |
| `05_event_sourcing.cpp` | 09, 10, 11 | Command log REPLAY — do independent engines, byte-identical proof |
| `06_engine_tests.cpp` | 14 | 43 unit tests — including the FOK+STP precheck correctness test |
| `07_engine_fuzz.cpp` | 15 | 30000-cmd fuzz vs an independent O(n) reference engine (dry-run FOK strategy) |
| `08_engine_bench.cpp` | 16 | Per-`submit()` latency distribution, by order type |

## Key correctness finding — FOK + STP interaction

A naive FOK precheck ("sum total resting qty at crossable levels, check
`>= target`") is **wrong** once self-trade prevention is combined with
FOK: if STP is going to skip or abort on a same-participant resting order
during the real match, that quantity was never actually fillable, and the
naive sum overcounts it — silently violating FOK's all-or-nothing
contract (a *partial* fill on an order that promised "full or nothing").

`matching_engine.hpp`'s `available_qty()` precheck is **STP-mode-aware**
(walks the same priority order the real match would, applying the same
skip/abort rules) so it always agrees with what the real match will
achieve. `06_engine_tests.cpp`'s `test_fok_stp_interaction()` demonstrates
the exact scenario where naive vs. aware disagree (naive sum 70 ≥ target
50 → would incorrectly PASS; aware sum 30 < 50 → correctly REJECTS).
Full story: `06-ioc-and-fok.md`.

## Correctness verification

- **43/43** scripted unit tests (`06_engine_tests.cpp`) — matching, FIFO,
  all 4 order types, all 3 STP modes, the FOK+STP interaction, duplicate
  ids, cancel-nonexistent, replace.
- **30000/30000 agree, 0 disagree** against an independently-implemented
  O(n) reference engine (`07_engine_fuzz.cpp`) — plain `std::vector` +
  linear scan, and a **dry-run-copy** FOK precheck strategy (completely
  different from the real engine's closed-form STP-aware sum). 0
  self-trade leaks across every STP-tagged order, 0 FOK partial-fill
  violations across 3774 FOK orders hit.
- **Determinism proven, not claimed**: `05_event_sourcing.cpp` replays
  the identical 20000-command log into two independent fresh engines and
  diffs every trade field-by-field — byte-identical.

## Key numbers (this run, N=150000 commands, `-O2`)

```
                       p50     p99      p99.9      (ns)
submit(): Limit        160.3   721.4    6171.7     <- new price-level cost (map+list alloc, 39's V1 pattern)
submit(): Market       250.5  1082.1    2394.6     <- multi-level sweep, more trades per call
submit(): IOC           50.1   490.9     921.8     <- often no-cross, cheap
submit(): FOK           40.1   480.9     961.8     <- precheck-reject is O(1)-ish, cheap
cancel()                60.1   300.6     911.7

throughput (single-threaded, this workload mix) ~= 5.63 million ops/sec
```

Full breakdown + mechanism discussion: `16-benchmarking.md`.
