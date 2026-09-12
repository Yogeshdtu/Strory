# 32 — CACHE & MEMORY PERFORMANCE (PHASE 21)

## Prerequisites
`31-CPU-ARCHITECTURE`

## Yeh folder kyun
**Yeh performance engineering ka sabse important folder hai.**

Folder 01 lesson 11 mein aapne 7x slowdown dekha tha. Ab uska poora science.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-memory-hierarchy.md` | Registers → L1 → L2 → L3 → RAM → disk, latency numbers |
| 02 | `02-cache-lines.md` | **64-byte cache line**, why it exists, alignment |
| 03 | `03-cache-organization.md` | Direct-mapped, set-associative, ways, index/tag/offset |
| 04 | `04-cache-misses.md` | **Compulsory, capacity, conflict** misses — teenon ke examples |
| 05 | `05-locality.md` | **Spatial aur temporal locality** — code examples |
| 06 | `06-prefetching.md` | Hardware prefetcher, `__builtin_prefetch`, patterns jo detect hote hain |
| 07 | `07-false-sharing.md` | **False sharing deep** — measured, padding se fix |
| 08 | `08-cache-friendly-structures.md` | Flat arrays vs linked structures, B-trees vs BSTs |
| 09 | `09-aos-vs-soa-deep.md` | **AoS vs SoA** — poora analysis, SIMD ke saath |
| 10 | `10-data-oriented-design.md` | **DOD** — data pehle, code baad mein; game/HFT industry ka approach |
| 11 | `11-tlb-and-huge-pages.md` | TLB structure, TLB misses, huge pages ka faayda (measured) |
| 12 | `12-store-buffers.md` | Store buffer, write combining, non-temporal stores |
| 13 | `13-memory-bandwidth.md` | Bandwidth vs latency bound, STREAM benchmark |
| 14 | `14-measuring-cache.md` | **`perf stat`** — cache-misses, cache-references, LLC-load-misses |
| 15 | `15-cache-optimization-recipes.md` | Practical recipes — blocking/tiling, padding, hot/cold splitting |
| 16 | `16-exercises.md` | Practice + optimization challenges |

## Examples

| File | Kya |
|---|---|
| `examples/01_cache_line_size.cpp` | Cache line size detect karo empirically |
| `examples/02_stride_access.cpp` | Stride ka latency pe asar |
| `examples/03_matrix_traversal.cpp` | Row vs column — full analysis |
| `examples/04_false_sharing.cpp` | False sharing + padding fix — measured |
| `examples/05_aos_vs_soa.cpp` | Full AoS/SoA benchmark |
| `examples/06_prefetch.cpp` | Manual prefetching ka faayda |
| `examples/07_matrix_blocking.cpp` | Naive vs blocked matrix multiply |
| `examples/08_tlb_hugepages.cpp` | Huge pages ka asar |
| `examples/09_perf_analysis.sh` | perf stat workflow |

## Time
3 hafte

## Status
✅ **COMPLETE (Batch 9 — PHASE 21).** 15 lessons + `16-exercises.md` +
8 portable `.cpp` examples + `09_perf_analysis.sh` (Linux perf workflow).

- Examples verified: `./build.ps1 folder 32-CACHE-MEMORY-PERFORMANCE` → **8/8 OK**
  (strict flags: `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
  -Wcast-align -Wnull-dereference -Wdouble-promotion`).
- Benchmarks measured at `-O2` on this box (AMD Zen 2, ~2 GHz throttled —
  **ratios port, absolutes don't**):
  - `03` row-major vs column-major traversal: **~10×** at `-O2`;
    **~1.0× at `-O3 -march=native`** (GCC `-ftree-loop-interchange` fixes it — Rule 2).
  - `02` sequential vs random line access: **~7×** (14 GB/s vs 2 GB/s).
  - `04` false sharing: **~6× to ~44×, run-to-run** (scheduling-dependent → jittery).
  - `05` AoS/SoA: scan-few-fields **SoA ~2.0×**; scan-most-fields **SoA ~2.4×**
    (gap doesn't shrink — SoA vectorizes); random whole-record **AoS ~3×**.
  - `06` SW prefetch: **~1.1× (marginal) or ~0.33× (3× SLOWER)** — cautionary, Rule 2.
  - `07` matmul: loop-order `ikj` **~4×**; naive 64×64 blocking **~10-15% slower
    than `ikj`** (needs tuned microkernel — Rule 2).
  - `08` TLB/cache latency cliff at ~8 MiB working set (**~1.2 ns → ~95 ns**).
- Example filename `08_tlb_hugepages.cpp` kept from the plan but implemented as a
  **portable** TLB-pressure benchmark (huge-page *allocation* is OS-specific —
  covered in lesson 11: Linux `MAP_HUGETLB`/THP, Windows large pages).

## Next
→ [`../33-COMPILER-OPTIMIZATION/00-README.md`](../33-COMPILER-OPTIMIZATION/00-README.md)
