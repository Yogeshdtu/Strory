# 39 — ORDER BOOK (PHASE 27)

## Prerequisites
`38-MARKET-DATA`, `19-STL`, `32-CACHE-MEMORY-PERFORMANCE`

## Yeh folder kyun
**Yeh HFT ka sabse classic interview question aur sabse important data structure hai.**

Hum ise **teen baar** banayenge — naive, better, aur optimized — aur har baar
**measure** karenge. Spec ka rule: pehle correct, phir fast.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-order-book-requirements.md` | Kya chahiye — add, cancel, modify, execute, query top |
| 02 | `02-design-space.md` | **Trade-off analysis** — map vs sorted vector vs flat array vs hybrid |
| 03 | `03-naive-map-implementation.md` | **Version 1: `std::map`** — simple aur correct |
| 04 | `04-measuring-v1.md` | **Benchmark v1** — latency distribution, profiling, bottlenecks |
| 05 | `05-sorted-vector-implementation.md` | **Version 2: sorted vector** — cache-friendly |
| 06 | `06-measuring-v2.md` | **Benchmark v2** — kya improve hua, kya nahi |
| 07 | `07-flat-array-book.md` | **Version 3: array-of-price-levels** — O(1) access |
| 08 | `08-intrusive-order-lists.md` | Intrusive linked lists for FIFO queues at each level |
| 09 | `09-order-id-lookup.md` | Order ID → order — hash map vs slab/arena indexing |
| 10 | `10-price-time-priority-impl.md` | FIFO queue per level, correct ordering |
| 11 | `11-top-of-book-fast-path.md` | **Best bid/ask ko O(1) mein** — cached, hot |
| 12 | `12-add-cancel-modify-execute.md` | Chaaron operations ki full implementation |
| 13 | `13-book-snapshots.md` | Snapshot generation, consistency |
| 14 | `14-measuring-v3.md` | **Final benchmark** — v1 vs v2 vs v3, cache miss analysis |
| 15 | `15-what-changed-and-why.md` | **Explain karo har optimization ne kya kiya** (spec requirement) |
| 16 | `16-testing-order-book.md` | Unit tests, invariant checks, fuzzing |
| 17 | `17-exercises.md` | Extensions + challenges |

## Examples

| File | Kya |
|---|---|
| `examples/01_orderbook_v1_map.cpp` | **Version 1: std::map (correct, simple)** |
| `examples/02_orderbook_v1_bench.cpp` | Version 1 benchmark |
| `examples/03_orderbook_v2_vector.cpp` | **Version 2: sorted vector** |
| `examples/04_orderbook_v2_bench.cpp` | Version 2 benchmark |
| `examples/05_orderbook_v3_flat.cpp` | **Version 3: flat array + intrusive lists** |
| `examples/06_orderbook_v3_bench.cpp` | Version 3 benchmark |
| `examples/07_comparison_suite.cpp` | **Teenon ka side-by-side comparison** |
| `examples/08_orderbook_tests.cpp` | Test suite |
| `examples/09_orderbook_fuzz.cpp` | Fuzzing harness |

## Time
4 hafte

## Status
⏳ **Yeh folder abhi syllabus stage pe hai.** Upar ki file list poori plan hai —
content agle batch mein aayega.

## Next
→ [`../40-MATCHING-ENGINE/00-README.md`](../40-MATCHING-ENGINE/00-README.md)
