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
⏳ **Yeh folder abhi syllabus stage pe hai.** Upar ki file list poori plan hai —
content agle batch mein aayega.

## Next
→ [`../33-COMPILER-OPTIMIZATION/00-README.md`](../33-COMPILER-OPTIMIZATION/00-README.md)
