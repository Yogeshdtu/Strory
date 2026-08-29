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
⏳ **Yeh folder abhi syllabus stage pe hai.** Upar ki file list poori plan hai —
content agle batch mein aayega.

## Next
→ [`../45-DEBUGGING/00-README.md`](../45-DEBUGGING/00-README.md)
