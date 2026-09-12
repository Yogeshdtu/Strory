# 44 — HFT CAPSTONE PROJECTS (PHASE 32)

## Prerequisites
Sab kuch — folders 01 se 43 tak

## Yeh folder kyun
**Yeh course ka climax hai.**

Yahan aap sab kuch jodkar ek **mini HFT engine** banaoge. Har project ke liye spec
ka process follow hoga:

1. Correct/simple version banao
2. Measure karo
3. Profile karo
4. Bottlenecks identify karo
5. Optimize karo
6. Dobara benchmark karo
7. **Explain karo ki kya badla**

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-project-overview.md` | Sab projects ka map, dependencies, order |
| 02 | `02-project-market-data-simulator.md` | **Project 1:** messages, timestamps, sequence numbers, snapshots |
| 03 | `03-project-feed-parser.md` | **Project 2:** binary parser — simple → zero-copy |
| 04 | `04-project-order-book.md` | **Project 3:** limit order book — v1 → v3 (folder 39 se) |
| 05 | `05-project-matching-engine.md` | **Project 4:** matching engine (folder 40 se) |
| 06 | `06-project-memory-pool.md` | **Project 5:** fixed-size pool + **allocation benchmark** |
| 07 | `07-project-object-pool.md` | **Project 6:** object pool with reuse |
| 08 | `08-project-spsc-queue.md` | **Project 7:** SPSC ring buffer + benchmark |
| 09 | `09-project-strategy-simulator.md` | **Project 8:** strategy framework + backtester (no alpha) |
| 10 | `10-project-risk-engine.md` | **Project 9:** pre-trade risk checks, limits, kill switch |
| 11 | `11-project-order-manager.md` | **Project 10:** OMS + execution simulator |
| 12 | `12-project-mini-hft-engine.md` | **Project 11: MINI HFT ENGINE** — poora pipeline jodo |
| 13 | `13-integration-and-testing.md` | End-to-end testing, replay, determinism verification |
| 14 | `14-performance-report.md` | **Final performance report** — har component ka latency budget |
| 15 | `15-extensions.md` | Aage kya kar sakte ho — ideas |

## Examples

| File | Kya |
|---|---|
| `examples/01_market_data_simulator/` | **Full simulator** |
| `examples/02_feed_parser/` | **Parser: v1 + optimized** |
| `examples/03_order_book/` | **Order book: v1 + v2 + v3** |
| `examples/04_matching_engine/` | **Matching engine** |
| `examples/05_memory_pool/` | **Memory pool + benchmarks** |
| `examples/06_object_pool/` | **Object pool** |
| `examples/07_spsc_queue/` | **SPSC queue + benchmarks** |
| `examples/08_strategy_sim/` | **Strategy simulator** |
| `examples/09_risk_engine/` | **Risk engine** |
| `examples/10_order_manager/` | **OMS + execution sim** |
| `examples/11_mini_hft_engine/` | **🏆 COMPLETE PIPELINE:** MarketData → Parser → Book → Strategy → Risk → OMS → Execution |

## Time
8–12 hafte

## Status
✅ **COMPLETE (PHASE 32) — course ka climax.** 15 lessons (`01`–`15`) +
12 example drivers + 11 shared `mh_*.hpp` headers. `./build.ps1 folder
44-HFT-PROJECTS` → **12/12 OK** under strict warnings.

Ek `MiniHftEngine` (`mh_engine.hpp`) jo sab jodta: **MarketData → Parser
→ L2Book → Strategy → Risk → OMS → Venue → fills → PnL**, single-threaded,
fully deterministic (no wall clock — sab event-timestamp driven).

**Reuse over rewrite (capstone ka point):** `mh_types.hpp` folder-40 ka
`matching_engine.hpp` `#include` karta (Price/Qty/OrderId types + the
`MatchingEngine` itself as the "venue"); `07_spsc_queue.cpp` folder-41 ka
`spsc_queue.hpp` use karta; book/pools/strategy folder 39/14/36/43 ke
patterns pe.

**The capstone optimization (43 methodology):** `MiniHftEngine` is
`template <class Venue>`. `NaiveEngine` = `ExecutionSimulator` (folder-40
`std::map` MatchingEngine — one tree insert + one `std::list`-node
`malloc` per market message). Profiling showed this venue mirror was the
`book` stage's bulk (~150 ns/msg). `OptimizedEngine` = `FastVenue`
(`mh_fast_venue.hpp`) — flat-array aggregate book + per-level FIFO + IOC
sweep. **Correctness gate first:** `12_integration_tests.cpp` proves
naive == optimized byte-for-byte (fills, qty, P&L, position) across 5
seed/config combos, both deterministic. **Then** the speedup: book stage
~150 → ~67 ns/msg, end-to-end mean/msg ~225 → ~145 ns (**~1.5–1.6×** on
this unpinned Zen 2 box; p50/p99 the reliable comparison — `max` is
scheduler jitter, 41/13).

**Per-component measured (this box, ratios):** L2Book apply ~17 ns/msg
(BBO matches a `std::map` reference exactly); FixedPool alloc+free ~2.0×
(p50) / ~5.0× (p99.9) vs `new`/`delete`; ObjectPool stale-handle → nullptr
even post-recycle; SPSC hand-off ~6 M msg/s, every message in order; risk
14/14 checks (rate-limit ≠ kill switch); OMS accounting always settles
(no leaked orders, no phantom fills). Feed parser v1 vs v3: `-O2` pe
**~1.0×** (honest Rule-2 null — compiler already optimal at this scale,
cf. 43/08).

**No alpha.** `SpreadCrossStrategy` is a mechanical rule to exercise the
pipeline; its backtest P&L has zero predictive meaning (37 SPECIALIZED
list). Mojibake sweep: 16 → **0**.

## Next
→ [`../45-DEBUGGING/00-README.md`](../45-DEBUGGING/00-README.md)
