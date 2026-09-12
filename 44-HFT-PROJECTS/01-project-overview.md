# 01 — Project overview: the mini HFT engine

## Prerequisites
- Folders 01–43 (sab kuch)
- Khaas: 38 (market data), 39 (order book), 40 (matching engine),
  41 (HFT concurrency), 43 (optimization methodology)

## Yeh folder kyun

**Yeh course ka climax hai.** Ab tak har cheez alag-alag seekhi. Yahan
sab ko **ek connected system** mein jodenge — ek *mini HFT engine* jo
market data leta hai aur orders bhejta hai, end to end.

Har component ke liye spec ka process (CLAUDE.md §2.12):

```
1. correct/simple version banao
2. measure karo
3. profile karo
4. bottleneck identify karo
5. optimize karo
6. dobara benchmark karo
7. EXPLAIN karo ki kya badla aur kyun
```

## The pipeline

```
  MarketDataSimulator ──wire bytes──▶ FeedParser ──▶ L2Book ──▶ Strategy
   (project 1)                        (project 2)     (proj 3)   (proj 8)
                                                                     │
                                                              OrderRequest
                                                                     ▼
                                                                RiskEngine ──▶ OrderManager
                                                                (project 9)    (project 10)
                                                                                    │
                                          ┌─────────────────────────────────────────┘
                                          ▼
                                        Venue  ──fills──▶  OMS + Risk + PnL
                                   (proj 4 / proj 11)
```

Har box ek project hai. Lessons `02`–`12` ek-ek project. `13` integration
+ testing. `14` final performance report (latency budget). `15` extensions.

## Dependency order (kis order mein banaya jaaye)

```
mh_types.hpp   (folder 40 ka matching_engine.hpp include karta -- Price/Qty/OrderId reuse)
   │
   ├── mh_market_data.hpp   (proj 1)   ──┐
   ├── mh_feed_parser.hpp   (proj 2)   ──┤
   ├── mh_order_book.hpp    (proj 3)   ──┤
   ├── mh_mem_pool.hpp      (proj 5)   ──┤
   ├── mh_object_pool.hpp   (proj 6)   ──┼──▶ mh_engine_common.hpp
   ├── mh_strategy.hpp      (proj 8)   ──┤
   ├── mh_risk_engine.hpp   (proj 9)   ──┤
   └── mh_order_manager.hpp (proj 10)  ──┘        │
       (+ spsc_queue.hpp from folder 41, proj 7)  │
       (+ matching_engine.hpp from folder 40, proj 4)
                                                  ▼
                          mh_fast_venue.hpp (proj 11 -- the OPTIMIZED venue)
                                                  ▼
                                           mh_engine.hpp
                                    template<class Venue> MiniHftEngine
                                    NaiveEngine  = MatchingEngine venue
                                    OptimizedEngine = FastVenue venue
```

## Reuse — the point of a capstone

| Piece | Reused from | Kaise |
|---|---|---|
| `Price/Qty/OrderId` types | 40 | `mh_types.hpp` `#include`s `matching_engine.hpp` |
| `MatchingEngine` (the "venue") | 40 | `ExecutionSimulator` wraps it |
| `SpscQueue<T,N>` | 41 | `07_spsc_queue.cpp` `#include`s it |
| flat-array book design | 39 V3 / 43 pipeline | `L2Book`, `FastVenue` |
| fixed-point prices, no division | 43/09, 43/10 | prices ints; SMA cross via cross-multiply |
| pools | 14 / 36 | `FixedPool`, `ObjectPool` polished as reusable templates |
| measure→change→explain | 43 | every project's benchmark has a before/after + reason |

**Copy-paste nahi. `#include`.** Agar ek component 3 jagah chahiye to woh
ek header hona chahiye, teen copies nahi.

## The two engines — capstone ka before/after

`mh_engine.hpp` template hai `Venue` pe:

- **`NaiveEngine`** — venue = `ExecutionSimulator` (folder 40 ka
  `std::map` + `std::list` `MatchingEngine`). Correct, simple, well-tested.
  Yeh "step 1".
- **`OptimizedEngine`** — venue = `FastVenue` (flat-array aggregate book +
  per-level FIFO + IOC sweep). Yeh "step 5" — profiling ke baad, jab pata
  chala ki venue mirror sabse mehnga stage hai.

`11_mini_hft_engine.cpp` dono chalata, **correctness gate pehle** (dono ka
trading output — fills, qty, P&L, position — byte-identical?), phir
speedup (~1.5–1.6× is box pe).

## What each project delivers

| # | Project | Deliverable | Measured result |
|---|---|---|---|
| 1 | Market data simulator | deterministic non-crossing L3 feed + wire encode | seq/ts/non-cross invariants + replay identical |
| 2 | Feed parser | v1 (safe) + v3 (fast), same output | agreement 200k/0; at `-O2` v1≈v3 (Rule-2 null) |
| 3 | Order book | flat-array L2 + cached BBO | BBO matches a `std::map` ref exactly; ~17 ns/apply |
| 4 | Matching engine | folder 40's engine, integrated as the venue | IOC fills at maker price, deterministic |
| 5 | Memory pool | `FixedPool<T,N>` | alloc+free ~2× (p50) / ~5× (p99.9) vs new/delete |
| 6 | Object pool | `ObjectPool<T>` + generation handles | stale handle → nullptr even after recycle |
| 7 | SPSC queue | 41's queue as the wire→engine hand-off | ~6 M msg/s, every message in order |
| 8 | Strategy simulator | mechanical rule + backtest infra | deterministic; **not alpha** |
| 9 | Risk engine | 5 pre-trade checks + kill switch | 14/14 checks; rate-limit ≠ kill |
| 10 | Order manager | OMS state machine + exec sim | every order settles; no phantom fills |
| 11 | **MINI HFT ENGINE** | the whole pipeline, naive vs optimized | ~1.5–1.6× end-to-end, output identical |
| 13 | Integration & testing | 5-config determinism + invariant suite | 0 failures |
| 14 | Performance report | per-stage latency budget | see `14-performance-report.md` |

## ⚠️ Ground rules for this folder

1. **No alpha.** The strategy is mechanical. Its P&L means nothing.
   (37 SPECIALIZED list — real signals are proprietary.)
2. **Single-threaded** by design (except the SPSC example). 41/13: on an
   unpinned box, threading jitter buries the optimization signal we're
   trying to measure.
3. **Deterministic.** No wall clock anywhere in the engine — every
   timestamp comes from the event stream. Replay must be byte-identical.
4. **Correct before fast.** Every optimization has a correctness gate that
   runs *first* (43/01).
5. **Real numbers.** Every "faster" is measured on the box in
   `examples/README.md` and quoted as a ratio.

## Next
→ [`02-project-market-data-simulator.md`](02-project-market-data-simulator.md)
