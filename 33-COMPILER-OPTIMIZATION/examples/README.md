# Examples — Folder 33 (compiler optimization)

> Portable `.cpp` (Linux nahi chahiye — sab MinGW/Windows x86-64 pe chalte).
> **`-O2` mandatory** — `-O0` benchmarks bekaar (lesson 14). `./build.ps1
> folder 33-COMPILER-OPTIMIZATION` → **6/6 OK** (the `.cpp` files) under strict
> flags. `06_lto_demo/` uses `.cxx` (multi-file, skipped by the `*.cpp`
> checker — run its `build.ps1`/`build.sh`). `07_pgo_workflow.sh` is a shell
> script (not compiled).

## The box these were measured on

**AMD Ryzen 7 4700U** (Zen 2), ~2.0 GHz effective (mobile, throttled). Plain
`-O2` here = **SSE2 baseline** (4 floats/vector); `-march=native` → AVX2
(8-wide). **Quote the RATIOS.** Run-to-run ±~30%.

## Compile / run

```bash
./build.ps1 fast 33-COMPILER-OPTIMIZATION/examples/03_vectorization.cpp
# 01: run manually at 5 -O levels (see file header)
# 06_lto_demo: cd there, ./build.ps1  (or ./build.sh)
# 07_pgo_workflow.sh <example.cpp>: full 3-step PGO pipeline
```

## Examples

| File | Lesson(s) | Kya dikhata / measured (this box, ~2 GHz) |
|---|---|---|
| `01_optimization_levels.cpp` | 01 | same 16.7M-iter reduction at `-O0/-O1/-O2/-O3/-Os`. **-O0 80 ms → -O1 13.4 ms (~6× cliff); -O1/-O2/-O3/-Os all ~13.3–13.5 ms** — for this reduction shape, only the `-O0→-O1` jump matters. (A vectorizable map shape would keep gaining — see `03`.) |
| `02_inlining_demo.cpp` | 03 | tiny body `x*k + (x>>3)` in a 400M carried loop. **noinline 1.31 ns/iter vs normal/always_inline 0.97–1.0 → ~1.3×**; `normal == always_inline` (compiler already inlined it). Real value of inlining is cross-boundary folding (see `04`/`06`), not the call overhead. |
| `03_vectorization.cpp` | 05, 04, 13 | 64k floats, L2-resident. **MAP** `out[i]=f(a[i])+rep`: scalar 0.52 → **vectorized 0.15 ns/elem (~3.5×)** (SSE2, no reassociation needed). **REDUCE** `s+=a[i]`: **0.72 ns/elem = scalar speed at plain `-O2`** (reassociation not allowed) → **~4× with `-ffast-math`** (verified). **PREFIX**: 0.75 ns/elem, never vectorizes (loop-carried dep). |
| `04_aliasing_restrict.cpp` | 09, 05 | `out[i] = in[i]*(*scale) + (*offset)`, functions `[[gnu::noinline]]`. **may-alias 0.43 vs `__restrict` 0.12 ns/elem → ~3.5×** (`*scale`/`*offset` reloaded every iter + scalar, vs hoisted + vectorized). ⚠️ Inlined (no `noinline`) the compiler proves non-aliasing from the call site itself → gap vanishes — that's the lesson. `blur3` (real serial dep): 2.27 ns/elem, `__restrict` can't help. |
| `05_branch_hints.cpp` | 08, 07 (branch prediction) | hot loop, ~1/1000 rare slow path. **no hint 0.56 vs `[[likely/unlikely]]` 0.42 vs `__builtin_expect` 0.41 ns/elem → ~1.3×** — small, the HW predictor already nails a 1/1000 branch. The hint changes **layout** (slow_path moves out-of-line); matters in a big function with many rare checks. PGO does this + more, measured. |
| `06_lto_demo/` (`.cxx`) | 10, 03 | `hot_transform` (5-op hash) defined in `mathx.cxx`, called per element in `main.cxx`'s **throughput** loop. **NO LTO 1.75 ns/elem vs `-flto` 0.76 → ~2.3×** (cross-TU call inlined away). ⚠️ Rule 2: an earlier *carried* version showed **no** LTO benefit (latency-bound on the hash critical path — the ~2-cyc call overlaps). LTO helps on the throughput path, not behind a dependency chain. |
| `07_pgo_workflow.sh` | 11 | full PGO pipeline: baseline → `-fprofile-generate` → run workload → `-fprofile-use` (+ `time`, `size`). On the tight `03` kernel the delta is in the noise — **that's the lesson**: PGO's value scales with code branchiness/size (parsers, engines), not tight numeric kernels. |
| `08_benchmark_barriers.cpp` | 14 | `sum_squares(200M)` × 5 reps. **no barrier 0.00 ms (loop DELETED)** vs **DoNotOptimize(result) 48.8 ms** vs **DoNotOptimize(in+out) 48.4 ms**; **vector fill + ClobberMemory 0.18 ms/rep** (stores retained). This is the `keep()` helper from folders 31/32, fully explained. |

## Notes / jaan-boojh kar cheezein

- **`keep()` / `DoNotOptimize` / `ClobberMemory`** in every timing example —
  without them `-O2` deletes the loop (`08` case 1 = `0.00 ms`). Zero
  instructions emitted; it's pure info to the optimizer.
- **`03` CASE 2 (float reduce)** stays scalar at plain `-O2` **on purpose** —
  partial-sum vectorization = reassociation = different rounding, which the
  compiler won't do without `-ffast-math` / `-fassociative-math` / `#pragma
  omp simd reduction`. Measured ~4× with `-ffast-math` (verified in-lesson).
  This is a Rule-2 "the compiler correctly refused" result.
- **`04` functions are `[[gnu::noinline]]` deliberately** — inlined, the
  compiler proves `out`/`scale`/`offset` don't alias from `main`'s context
  and the aliasing pessimism disappears (no `__restrict` needed). The example
  forces the standalone-function view to make `__restrict`'s value visible.
  LTO/inlining solving aliasing for you *is* the lesson.
- **`06_lto_demo` uses a THROUGHPUT loop** (independent per element). The
  first draft used a carried loop and LTO showed no gain — latency-bound on
  the hash critical path, the call overhead hidden by OoO. Documented as a
  Rule-2 nuance in lesson 10.
- **`01` -O1..-O3 are equal here** because the workload is a scalar reduction
  with a data-dependent hash — nothing to vectorize. A map-shaped loop keeps
  gaining (`03`). Don't generalize "-O3 == -O2" from one workload.
- **No `broken_on_purpose` file.** Every example is a correct program
  demonstrating a compiler behaviour.
- **Numbers are this-box, ~2 GHz throttled Zen 2, SSE2 baseline.** Ratios
  port; add `-march=native` for AVX2 width; re-benchmark on the production
  CPU + toolchain (lessons 11, 12).
- All 6 `.cpp` + the `06_lto_demo` `.cxx` files compile clean under
  `-std=c++20 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
  -Wcast-align -Wnull-dereference -Wdouble-promotion`.
