# Examples — Folder 35 (profiling & benchmarking)

> Portable `.cpp` (Linux nahi chahiye — sab MinGW/Windows x86-64 pe chalte).
> **`-O2` mandatory** — `-O0` benchmarks bekaar (lesson 09). `./build.ps1
> folder 35-PROFILING-BENCHMARKING` → **5/5 OK** (the `.cpp` files) under
> strict flags. `05_google_benchmark/` uses `.cxx`/`.hpp` (multi-file,
> skipped by the `*.cpp` checker — run its `build.ps1`/`build.sh`).
> `06_perf_workflow.sh` + `07_flamegraph.sh` are **Linux-only** shell
> scripts (perf/FlameGraph — not compiled, `folder`/`checkall` skip `.sh`).

## The box these were measured on

**AMD Ryzen 7 4700U** (Zen 2), ~2.0 GHz effective (mobile, throttled),
**Windows x64 + MinGW-w64 GCC 15.1.0**. Plain `-O2` = **SSE2 baseline**.
TSC ~2.0 GHz (`ticks_per_ns ≈ 1.996`). **Quote the RATIOS / shapes.**
Run-to-run tail numbers (p99.9, max) vary a lot — an **unpinned Windows
box** with a live desktop; a pinned Linux isolated core would show far
tighter tails (that contrast is itself the lesson — 03, 06).

## Compile / run

```bash
./build.ps1 fast 35-PROFILING-BENCHMARKING/examples/01_timing_methods.cpp
# ... 02, 03, 04, 08 likewise
cd 35-PROFILING-BENCHMARKING/examples/05_google_benchmark && ./build.ps1   # (or ./build.sh)
bash 35-PROFILING-BENCHMARKING/examples/06_perf_workflow.sh                # Linux
bash 35-PROFILING-BENCHMARKING/examples/07_flamegraph.sh                   # Linux
```

## Examples

| File | Lesson(s) | Kya dikhata / measured (this box, ~2 GHz) |
|---|---|---|
| `01_timing_methods.cpp` | 02, 03 | `steady_clock` / `high_resolution_clock` / `system_clock` / `clock()` / `__rdtsc` — **resolution** (chrono all **100 ns**; `clock()` **1 ms** — MinGW `CLOCKS_PER_SEC=1000`; rdtsc 1 tick), **calibration** (~**1.996 ticks/ns**), **self-cost** (`steady_clock::now()` **~38 ns**, plain rdtsc **~9 ns**, fenced **~18 ns**), and a **94 µs workload** timed both ways (**93800 vs 93818 ns** — agree at 0.02% over a long interval; diverge on short ones). |
| `02_percentiles.cpp` | 04, 05, 07 | 200k samples, ~1% inject a cold-buffer touch. **min 401, mean ~700, median ~695, p90 ~925, p99 ~1000, p99.9 ~1700, p99.99 ~5000–28000, max 40k–120k ns.** mean ≈ median (bulk ~symmetric) yet **max/median 60–175×** — "mean fine ≠ tail fine". Linear histogram + `>p99.5` overflow → one linear width can't hold bulk+tail (→ log buckets, `08`). Tail numbers **vary run-to-run**, bulk stable. |
| `03_jitter_measure.cpp` | 06 | Identical 128-element hash per iteration, 300k iters, **CLEAN** (nothing else) vs **NOISY** (every 200th iter: `new`/`delete` + `yield`). **min 200.4 ns both** (real cost). CLEAN: p99 ~260–450, spikes(>2×) **~2800/300k**, max ~46–55 µs. NOISY: p99 ~460, spikes **~6300/300k**, max ~50–130 µs. Robust jitter signal = **spike count + p99.9**, not the single `max` (itself noisy — CLEAN's max can exceed NOISY's in a given run). Unpinned box → even CLEAN has ~1% >2× (timer interrupt). |
| `04_benchmark_mistakes.cpp` | 09 | 6 BUG/FIX pairs, `-O2`. **1. DCE:** BUG **0.000** / FIX 0.848 ns/op. **2. const-fold** (`mix(42)`): BUG **0.000** / FIX 0.867. **3. hoist** (`mix(x)`, x fixed): BUG **0.000** / FIX 1.010. **4. cold start:** run[0] **242000** / min-of-20 **204500** ns (~1.2×). **5. timer > op:** BUG **36.8** (≈ `now()` self-cost) / FIX (batch of 20M) **2.99** ns/op. **6. one run:** 0.961 / min-med-max 0.822/0.842/1.108 (**spread 35%**). Runtime `volatile` sink + `volatile` source defeat both DCE and const-fold. |
| `05_google_benchmark/` (`.cxx`/`.hpp`) | 08, 09 | **`minibench.hpp`** — ~150-line Google-Benchmark **shim** (`for (auto _ : state)`, `DoNotOptimize`/`ClobberMemory`, `state.range`/`SetItemsProcessed`/`PauseTiming`, `BENCHMARK`/`BENCHMARK_F`/`BENCHMARK_MAIN`, `Arg`/`Range`). `bench.cxx` switches to the real library by changing **one `#include`** (+ `-lbenchmark -lpthread`). Measured: `BM_reduce_NO_barrier` **0.49 ns/iter (loop DELETED)** vs `_WITH_barrier` **1030 ns**; `BM_memcpy/4096` **32 ns (128 GB/s)** vs `BM_manual_copy/4096` **1003 ns (4 GB/s)** ~30×; `BM_StringCopy/8` **3 ns (SSO)** → `/64` **49 ns** (heap). |
| `06_perf_workflow.sh` | 10, 11 | **Linux only.** Self-contained: builds a demo with 2 planted bottlenecks (`sum_mod` = runtime `%` → divider unit; `chase` = random-stride walk → cache misses), then runs `perf stat` (IPC, branch-miss), `perf stat -d -d` (cache), `perf stat -M TopdownL1`, `perf record -g` + `perf report`, `perf annotate` on both hot functions. Plus a `perf` cheat-sheet + gotchas (`perf_event_paranoid`, skid → `:pp`, symbols, VM). |
| `07_flamegraph.sh` | 12 | **Linux only.** Builds a 3-function demo (`heavy`/`light`/`mid`), shows the `perf record -g` → `stackcollapse-perf.pl` → `flamegraph.pl` pipeline, runs it for real if `perf` + Brendan Gregg's `FlameGraph` scripts are present (`_flame/cpu.svg` + top folded stacks). Reading rules (width=time, X=alphabetical, top plateau), variants (icicle, differential, off-CPU, event), alternatives (`hotspot`, Firefox Profiler, Speedscope). |
| `08_latency_recorder.cpp` | 07, 16 | Production-style **log-linear histogram** (`LatencyHistogram`, HdrHistogram-lite): `SUB_BITS=6` → 64 sub-buckets/octave → **~1.5% relative error**, 58 octaves → **3776 buckets / 29.5 KB fixed**. `record()` = bit-scan + increment, **~2 ns/event**; `vector.push_back()` ~2 ns *but* +15 MB RAM + `O(n log n)` sort + realloc-spike risk. Percentiles **vs exact: p50 +0.4%, p90 +1.0%, p99 +1.3%, p99.9 +0.0%, p99.99 +0.1%**. Log-spaced display shows the simulated stream is **bimodal** (bulk ~276 ns + spike hump ~10–22 µs). |

## Notes / jaan-boojh kar cheezein

- **`keep()` / `DoNotOptimize` / `volatile` sink in every timing example** —
  without them `-O2` deletes the loop (`04` case 1 = `0.000 ns/op`,
  `05`'s `BM_reduce_NO_barrier` = `0.49 ns/iter`). `04` deliberately uses a
  **`volatile` global sink + `volatile` global source** instead of the asm
  `keep()` — because `keep()` with `"+r,m"` on a value the compiler
  constant-folds hits "impossible constraint in 'asm'" at `-O2` (the same
  gotcha noted in folder 34), and `04`'s whole point is defeating
  const-folding.
- **`04` case 4 "cold start" is only ~1.2×** here (not the 2–100× you'd see
  disk-backed) — Windows commits pages fairly eagerly and only ~16k pages
  are touched. Kept as-is: an honest small number with the lesson that the
  effect scales with working-set size and backing store (Rule 2).
- **`02` / `03` tail numbers are run-to-run unstable** (p99.9, p99.99, max)
  — deliberately measured and taught: this is an **unpinned Windows desktop**.
  The bulk (min, p50, p90) is stable. Lessons 03/06 use this contrast to
  motivate pinning / `isolcpus` / a quiet core.
- **`05_google_benchmark/` is a shim, not the real library.** It exists so
  the example compiles + runs with zero dependencies and teaches the real
  API. It does **not** provide repeats/stddev/CV, CPU-vs-wall time, JSON, or
  multi-threaded benchmarks — that's exactly the value of installing real
  Google Benchmark (`build.ps1` / `build.sh` print the switch).
- **`06`/`07` are `.sh` and Linux-only** — `perf` and the FlameGraph scripts
  don't exist on Windows/MinGW. They're self-contained (embed their own demo
  `.cpp` via heredoc, both verified to compile) and `folder`/`checkall` skip
  `.sh`. Run under Linux/WSL.
- **Numbers are this-box (~2 GHz throttled Zen 2, SSE2 baseline, Windows,
  unpinned).** Ratios and distribution shapes port; absolutes and tail
  magnitudes don't. Re-measure on the production CPU + a pinned isolated
  core.
- **No `broken_on_purpose` file.** `04` and `05` contain BUG variants but
  they compile and run — the "bug" is a misleading *number*, not a compile
  error, and each sits next to its FIX.
- The 5 `.cpp` + the `05_google_benchmark` `.cxx` compile clean under
  `-std=c++20 -Wall -Wextra -Wpedantic -Wshadow -Wconversion
  -Wsign-conversion -Wcast-align -Wnull-dereference -Wdouble-promotion`.
