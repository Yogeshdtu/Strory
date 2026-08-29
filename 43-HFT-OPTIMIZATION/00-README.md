# 43 — HFT OPTIMIZATION (end-to-end) (PHASE 31)

## Prerequisites
`42-HFT-NETWORKING`, `36-LOW-LATENCY-CPP`

## Yeh folder kyun
Ab sab kuch jodkar **systematically optimize** karenge.

Spec ka rule: measure → profile → hypothesize → change → **re-measure**. Har baar.
Aur har change ka **explanation** ki kya badla aur kyun.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-optimization-methodology.md` | **Poora process** — measure first, never guess |
| 02 | `02-establishing-baseline.md` | Baseline measurement, reproducible benchmarks |
| 03 | `03-finding-bottlenecks.md` | Profiling the full pipeline, where time actually goes |
| 04 | `04-hot-cold-path-separation.md` | Hot path identification, cold code ko alag karna |
| 05 | `05-attribute-hot-cold.md` | `__attribute__((hot/cold))`, section placement |
| 06 | `06-instruction-cache-layout.md` | I-cache optimization, **BOLT**, PGO for layout |
| 07 | `07-struct-layout-tuning.md` | Field reordering, hot fields together, cache line packing |
| 08 | `08-eliminating-false-sharing.md` | Systematic false sharing hunt |
| 09 | `09-fixed-point-arithmetic.md` | **Prices as integers** — fixed point, avoiding float entirely |
| 10 | `10-avoiding-division.md` | Division ki cost, reciprocal multiply, shifts, lookup tables |
| 11 | `11-lookup-tables.md` | Precomputed tables, `constexpr` generation, cache trade-off |
| 12 | `12-compile-time-strategy-dispatch.md` | Template dispatch, no runtime branches |
| 13 | `13-case-study-feed-handler.md` | **Case study: feed handler** — before/after with real numbers |
| 14 | `14-case-study-order-book.md` | **Case study: order book** — v1 → v3 full analysis |
| 15 | `15-case-study-full-pipeline.md` | **Case study: end-to-end** — tick-to-trade optimization |
| 16 | `16-when-to-stop.md` | **Diminishing returns** — kab optimize karna band karein |
| 17 | `17-exercises.md` | Optimization challenges with target numbers |

## Examples

| File | Kya |
|---|---|
| `examples/01_baseline_pipeline.cpp` | Unoptimized full pipeline |
| `examples/02_profile_analysis.sh` | perf-based bottleneck hunt |
| `examples/03_optimized_pipeline.cpp` | Optimized version |
| `examples/04_before_after.cpp` | Side-by-side measurement |
| `examples/05_fixed_point.cpp` | Fixed-point price arithmetic |
| `examples/06_division_elimination.cpp` | Division alternatives — measured |
| `examples/07_struct_tuning.cpp` | Layout optimization |
| `examples/08_hot_cold.cpp` | Hot/cold splitting |

## Time
3 hafte

## Status
⏳ **Yeh folder abhi syllabus stage pe hai.** Upar ki file list poori plan hai —
content agle batch mein aayega.

## Next
→ [`../44-HFT-PROJECTS/00-README.md`](../44-HFT-PROJECTS/00-README.md)
