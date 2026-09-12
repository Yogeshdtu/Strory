# Examples — Folder 44 (HFT capstone projects)

> Portable `.cpp` — sab MinGW/Windows x86-64 pe chalte. **`-O2` mandatory**
> for the benchmarks. `./build.ps1 folder 44-HFT-PROJECTS` → **12/12 OK**
> under strict warnings. The `mh_*.hpp` headers are the shared library
> (glob `*.cpp` unhe alag se compile nahi karta).
>
> `07_spsc_queue.cpp` reuses `41-HFT-CONCURRENCY/examples/spsc_queue.hpp`;
> the venue reuses `40-MATCHING-ENGINE/examples/matching_engine.hpp`.
> **Capstone ka poora point yehi hai — jodo, dobara mat likho.**

## The box these were measured on

**AMD Ryzen 7 4700U** (Zen 2), ~2.0 GHz, **Windows x64 + MinGW-w64 GCC
15.1.0**, unpinned. TSC ~2.0 GHz (`ticks_per_ns` 1.9962). **Ratios/shapes
quote karo** — absolutes ~20% run-to-run (folder 35/06, carried forward),
tail `max` especially jittery.

## The headers (shared "library")

| Header | Project | Kya |
|---|---|---|
| `mh_types.hpp` | — | `#include`s folder-40's `matching_engine.hpp`; adds `Side`, `MdMessage`, `OrderRequest`, `Fill`, fixed-point `px_parse`/`px_format` |
| `mh_market_data.hpp` | 1 | `MarketDataSimulator` — deterministic L3 feed, monotonic seq + ts, **guaranteed non-crossing** book (stale-quote pulling), 38-byte BE wire `encode` |
| `mh_feed_parser.hpp` | 2 | `parse_v1` (portable shift-and-or, bounds-checked) + `parse_v3` (memcpy + bswap, frame-validated once) + `record_wire` |
| `mh_order_book.hpp` | 3 | `L2Book` — flat-array aggregate (39/43 V3) + cached BBO + dense-id index + `resolve_cross` (39/16 invariant made executable) |
| `mh_mem_pool.hpp` | 5 | `FixedPool<T,N>` — preallocated slab + intrusive free list |
| `mh_object_pool.hpp` | 6 | `ObjectPool<T>` — typed, **generation-checked handles** (stale-handle = use-after-free guard) |
| `mh_strategy.hpp` | 8 | `SpreadCrossStrategy` — **mechanical, NOT alpha**; SMA-cross + imbalance → IOC quote; division-free (43/10) |
| `mh_risk_engine.hpp` | 9 | `RiskEngine` — fat-finger / price-collar / position / rate-limit / kill-switch, all O(1) |
| `mh_order_manager.hpp` | 10 | `OrderManager` (OMS state machine on a gen-checked pool) + `ExecutionSimulator` (wraps folder-40 `MatchingEngine` as "the venue") |
| `mh_fast_venue.hpp` | 11 | `FastVenue` — the OPTIMIZED venue: flat-array aggregate book + per-level FIFO + IOC sweep. Same fills as `MatchingEngine`, no per-message tree/alloc |
| `mh_engine_common.hpp` | — | pulls the headers together + rdtsc harness + `g_tpns()` + `pct()` |
| `mh_engine.hpp` | 11 | `template <class Venue> MiniHftEngine` — the whole pipeline. `NaiveEngine` = `MatchingEngine` venue, `OptimizedEngine` = `FastVenue` |

## The drivers

| File | Project | Kya check karta |
|---|---|---|
| `01_market_data_sim.cpp` | 1 | seq strictly +1, ts monotonic, **book never crosses**, deterministic replay |
| `02_feed_parser.cpp` | 2 | v1 vs v3 **agreement** (200k frames, 0 mismatch) + throughput |
| `03_order_book.cpp` | 3 | BBO vs a brute-force `std::map` reference (0 mismatch / 118k) + `apply()` ns/msg |
| `04_matching_engine.cpp` | 4 | aggressive IOC probes → fills at maker price, far orders void, deterministic |
| `05_memory_pool.cpp` | 5 | distinct ptrs / exhaustion→nullptr / free→reuse + alloc-free latency vs `new`/`delete` |
| `06_object_pool.cpp` | 6 | reuse + **stale-handle rejection (even post-recycle)** + no double-release + cycle latency |
| `07_spsc_queue.cpp` | 7 | MdMessage hand-off: throughput + paced latency + **consumer saw every msg in order** |
| `08_strategy_sim.cpp` | 8 | backtest infra: signals / fills / mark-to-market P&L, deterministic, cooldown sweep. **NOT alpha.** |
| `09_risk_engine.cpp` | 9 | every verdict triggered explicitly + kill switch + rate-limit-≠-kill |
| `10_order_manager.cpp` | 10 | state machine transitions + full-run **accounting** (every order settles, no phantom fills) |
| `11_mini_hft_engine.cpp` | 11 | 🏆 the whole pipeline: per-stage budget, end-to-end p50/p99/p99.9, **naive vs optimized correctness gate**, determinism, invariants |
| `12_integration_tests.cpp` | 13 | naive==optimized + determinism + invariants across **5 seed/config combos** |

## Key numbers (this box, ratios)

```
01  sim              seq +1, ts monotonic, book never crossed, replay IDENTICAL
02  parser           v1 == v3 output (200k frames, 0 mismatch)
                     parse_v1 ~1.76 ns/frame ; parse_v3 ~1.76 ns/frame (1.0x --
                     at -O2 the compiler makes the "naive" one just as fast;
                     honest Rule-2 null, cf. 43/08)
03  L2Book.apply()   ~17 ns/msg ; BBO matches a std::map reference exactly
04  matching venue   fills at maker price, IOC remainder void, deterministic
05  FixedPool        alloc+free p50 ~2.0x, p99.9 ~5.0x faster than new/delete
06  ObjectPool       stale handle -> nullptr even after the slot is recycled
07  SPSC hand-off    ~6 M msg/s blast ; paced p50 ~350 ns ; every msg in order
09  RiskEngine       14/14 checks pass ; rate-limit does NOT trip the kill switch
10  OMS              every submitted order settles ; filled_qty <= submitted_qty
11  MINI ENGINE      end-to-end mean/msg naive ~225 -> optimized ~145 ns (~1.5-1.6x)
                     lever = the venue mirror: book stage ~150 -> ~67 ns/msg
                     (std::map MatchingEngine -> flat-array + per-level FIFO sweep)
                     correctness gate: naive == optimized (fills, qty, pnl, pos) IDENTICAL
                     determinism: run x2 byte-identical ; |position| <= risk max held
                     NOTE: optimized `max` tail is jittery (unpinned box, OS scheduler --
                     41/13, 43/16); p50/p99 are the reliable comparison
12  integration      naive==optimized + determinism + invariants: 5 configs, 0 failures
```

## Scope / honesty notes

- **No alpha.** `SpreadCrossStrategy` is a mechanical rule to exercise the
  pipeline. Its backtest P&L has zero predictive meaning (37 SPECIALIZED list).
- **Single-threaded** by design — deterministic measurement (41/13: threading
  jitter buries the optimization signal). `07` is the one threaded example
  (the SPSC hand-off is inherently 2-thread).
- The `MarketDataSimulator` is a *model*, not a real venue feed; it produces
  a coherent non-crossing book so the strategy/venue have something sane to
  trade against.
- Every "faster" claim is measured on the box above and quoted as a ratio.
