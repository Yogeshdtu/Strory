# 33 — COMPILER OPTIMIZATION (PHASE 22)

## Prerequisites
`32-CACHE-MEMORY-PERFORMANCE`, `21-TEMPLATES`

## Yeh folder kyun
Compiler aapka sabse bada partner hai. Uske saath kaam karna seekho — uske khilaaf nahi.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-optimization-levels.md` | `-O0` `-O1` `-O2` `-O3` `-Os` `-Ofast` — kya karte hain, kya trade-off |
| 02 | `02-godbolt-workflow.md` | **Compiler Explorer** — roz ka workflow, assembly padhna |
| 03 | `03-inlining.md` | Inlining heuristics, `inline` ka asli matlab, `always_inline`, `noinline` |
| 04 | `04-loop-optimizations.md` | Unrolling, fusion, interchange, invariant hoisting, strength reduction |
| 05 | `05-vectorization.md` | Auto-vectorization, kya rokta hai, `-fopt-info-vec`, pragmas |
| 06 | `06-constant-folding.md` | Constant folding, propagation, dead code elimination |
| 07 | `07-devirtualization.md` | Compiler virtual calls kab resolve kar leta hai, `final` ka faayda |
| 08 | `08-branch-hints.md` | `[[likely]]`/`[[unlikely]]`, `__builtin_expect`, PGO se behtar |
| 09 | `09-aliasing-and-restrict.md` | **Aliasing optimization ko rokta hai**, `__restrict__`, examples |
| 10 | `10-lto.md` | Link-Time Optimization — cross-TU inlining |
| 11 | `11-pgo.md` | **Profile-Guided Optimization** — workflow, real gains |
| 12 | `12-march-and-mtune.md` | `-march=native`, ISA extensions, portability trade-off |
| 13 | `13-fast-math-dangers.md` | `-ffast-math` — kya todta hai, kab safe hai |
| 14 | `14-preventing-optimization.md` | Benchmarks mein `volatile`, `DoNotOptimize`, compiler barriers |
| 15 | `15-reading-optimized-output.md` | Optimized assembly padhna, verify karna ki aapka intent poora hua |
| 16 | `16-exercises.md` | Practice + assembly reading |

## Examples

| File | Kya |
|---|---|
| `examples/01_optimization_levels.cpp` | Same code, alag -O levels |
| `examples/02_inlining_demo.cpp` | Inlining ka asar |
| `examples/03_vectorization.cpp` | Vectorized vs scalar |
| `examples/04_aliasing_restrict.cpp` | restrict ka faayda — assembly diff |
| `examples/05_branch_hints.cpp` | likely/unlikely |
| `examples/06_lto_demo/` | LTO multi-file project |
| `examples/07_pgo_workflow.sh` | PGO ka poora workflow |
| `examples/08_benchmark_barriers.cpp` | DoNotOptimize patterns |

## Time
2–3 hafte

## Status
✅ **COMPLETE (Batch 9 — PHASE 22).** 15 lessons + `16-exercises.md` +
8 examples (6 portable `.cpp` + `06_lto_demo/` multi-file `.cxx` +
`07_pgo_workflow.sh`).

- `./build.ps1 folder 33-COMPILER-OPTIMIZATION` → **6/6 OK** under strict flags.
- Benchmarks measured at `-O2` (AMD Zen 2, ~2 GHz — **ratios port, absolutes
  don't**; plain `-O2` = SSE2 baseline here):
  - `01` -O levels: **-O0 80 ms → -O1 13.4 ms (~6× cliff)**; -O1/-O2/-O3/-Os
    equal for this reduction shape (nothing to vectorize).
  - `02` inlining: noinline **1.31 ns/iter** vs inlined **~1.0** (~1.3×);
    `normal == always_inline`.
  - `03` vectorization: MAP scalar→vectorized **~3.5×**; float REDUCE
    **scalar-speed at `-O2`, ~4× with `-ffast-math`** (reassociation — Rule 2);
    PREFIX never vectorizes.
  - `04` aliasing: may-alias vs `__restrict` **~3.5×** (functions
    `[[gnu::noinline]]`; inlined/LTO the compiler solves it itself).
  - `05` branch hints: **~1.3×** (HW predictor already nails a 1/1000 branch;
    the hint is code layout).
  - `06` LTO: throughput loop **NO LTO 1.75 → `-flto` 0.76 ns/elem (~2.3×)**;
    ⚠️ a carried loop showed **no** LTO gain (latency-bound — Rule 2 nuance).
  - `08` barriers: **no barrier 0.00 ms (loop deleted)** vs DoNotOptimize ~48 ms.

## Next
→ [`../34-ASSEMBLY/00-README.md`](../34-ASSEMBLY/00-README.md)
