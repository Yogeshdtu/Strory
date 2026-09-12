# 40 — MATCHING ENGINE (PHASE 28)

## Prerequisites
`39-ORDER-BOOK`

## Yeh folder kyun
Order book ke upar matching engine. Yeh exchange ka dil hai — aur ek excellent
project hai jo state machines, determinism, aur testing sikhaata hai.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-what-is-matching.md` | Matching engine ka kaam, exchange mein uski jagah |
| 02 | `02-matching-algorithm.md` | Price-time priority matching, crossing logic |
| 03 | `03-limit-orders.md` | Limit order handling, resting vs aggressing |
| 04 | `04-market-orders.md` | Market orders, sweeping levels, partial fills |
| 05 | `05-partial-fills.md` | Partial execution, remaining quantity |
| 06 | `06-ioc-and-fok.md` | IOC, FOK semantics aur implementation |
| 07 | `07-trade-events.md` | Trade event generation, execution reports |
| 08 | `08-self-trade-prevention.md` | STP rules, cancel-newest/oldest/both |
| 09 | `09-determinism.md` | **Deterministic execution** — same input, same output, kyun zaroori |
| 10 | `10-event-sourcing.md` | Event log, replay, state reconstruction |
| 11 | `11-single-threaded-design.md` | **Single-threaded kyun** — determinism vs parallelism trade-off |
| 12 | `12-state-machine-design.md` | Order state machine, valid transitions |
| 13 | `13-building-the-engine.md` | **BUILD: full matching engine** — step by step |
| 14 | `14-testing-matching-engine.md` | Unit tests, scenario tests, property-based tests |
| 15 | `15-fuzzing.md` | Fuzzing for invariant violations |
| 16 | `16-benchmarking.md` | Throughput aur latency benchmarks |
| 17 | `17-exercises.md` | Extensions — new order types, auctions |

## Examples

| File | Kya |
|---|---|
| `examples/01_matching_engine.cpp` | **Poora matching engine** |
| `examples/02_order_types.cpp` | Limit/market/IOC/FOK |
| `examples/03_trade_events.cpp` | Event generation |
| `examples/04_self_trade_prevention.cpp` | STP |
| `examples/05_event_sourcing.cpp` | Log + replay |
| `examples/06_engine_tests.cpp` | Test suite |
| `examples/07_engine_fuzz.cpp` | Fuzzer |
| `examples/08_engine_bench.cpp` | Throughput/latency benchmark |

## Time
3–4 hafte

## Status
✅ **COMPLETE** — 17 lessons (`01`–`17`) + 8 examples + 2 shared headers
(`matching_engine.hpp`, `engine_workload.hpp`). `./build.ps1 folder
40-MATCHING-ENGINE` → **8/8 OK**.

Poora `MatchingEngine`: Limit/Market/IOC/FOK order types, price-time
priority matching, self-trade prevention (3 modes), deterministic event
sequencing, event-sourcing replay. Central correctness finding: a naive
FOK precheck combined with self-trade prevention can silently violate
FOK's all-or-nothing contract — fixed with an STP-mode-aware precheck,
verified via a hand-crafted test (`06_engine_tests.cpp`) AND a 30000-cmd
fuzz run against an independent O(n) reference engine using a completely
different (dry-run) precheck strategy (`07_engine_fuzz.cpp`, zero
disagreements, zero FOK violations across 3774 FOK orders). Determinism
proven (not claimed) via byte-identical dual-engine replay
(`05_event_sourcing.cpp`). 43/43 scripted unit tests pass. Benchmarked
at `-O2` (`08_engine_bench.cpp`) — real numbers, mechanism explained for
each order type's latency profile.

## Next
→ [`../41-HFT-CONCURRENCY/00-README.md`](../41-HFT-CONCURRENCY/00-README.md)
