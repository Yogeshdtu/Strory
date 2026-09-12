# 35 — PROFILING & BENCHMARKING (PHASE 23)

## Prerequisites
`34-ASSEMBLY`, `32-CACHE-MEMORY-PERFORMANCE`

## Yeh folder kyun
**Measure karo, guess mat karo.**

Aapki intuition performance ke baare mein aksar galat hoti hai. Yeh folder aapko
sahi measure karna sikhaata hai — jo har optimization ka pehla step hai.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-why-measure.md` | Intuition kyun fail hoti hai, premature optimization, Amdahl's law |
| 02 | `02-timing-correctly.md` | `std::chrono`, `steady_clock` vs `system_clock`, resolution |
| 03 | `03-rdtsc-timing.md` | TSC, invariant TSC, serialization, overhead subtraction |
| 04 | `04-statistics.md` | **Mean vs median**, variance, why average lies |
| 05 | `05-percentiles.md` | **p50, p90, p99, p99.9, p99.99** — kyun tail matter karta hai |
| 06 | `06-jitter-and-tail-latency.md` | **Jitter** — HFT ka asli dushman, sources of jitter |
| 07 | `07-histograms.md` | HdrHistogram, latency distributions, plotting |
| 08 | `08-microbenchmarking.md` | **Google Benchmark** — setup, fixtures, common mistakes |
| 09 | `09-benchmark-pitfalls.md` | Dead code elimination, warm-up, alignment noise, frequency scaling |
| 10 | `10-perf-basics.md` | **`perf stat`, `perf record`, `perf report`** — practical workflow |
| 11 | `11-perf-advanced.md` | `perf annotate`, hardware counters, sampling vs counting |
| 12 | `12-flame-graphs.md` | Flame graphs banana aur padhna |
| 13 | `13-cachegrind-callgrind.md` | Valgrind tools for profiling |
| 14 | `14-vtune-intro.md` | Intel VTune — top-down analysis |
| 15 | `15-sanitizers.md` | ASan, UBSan, TSan, MSan — practical usage |
| 16 | `16-production-measurement.md` | **Live systems mein latency measure karna** — low-overhead approaches |
| 17 | `17-exercises.md` | Practice + profiling challenges |

## Examples

| File | Kya |
|---|---|
| `examples/01_timing_methods.cpp` | chrono vs rdtsc vs clock_gettime |
| `examples/02_percentiles.cpp` | Latency histogram + percentiles |
| `examples/03_jitter_measure.cpp` | Jitter measurement |
| `examples/04_benchmark_mistakes.cpp` | Common benchmark bugs |
| `examples/05_google_benchmark/` | Google Benchmark setup |
| `examples/06_perf_workflow.sh` | perf ka poora workflow |
| `examples/07_flamegraph.sh` | Flame graph generation |
| `examples/08_latency_recorder.cpp` | Production-style low-overhead recorder |

## Time
3 hafte

## Status
✅ **COMPLETE (Batch 9 — PHASE 23).** 16 lessons (`01`–`16`) + `17-exercises.md`
+ 8 examples. `./build.ps1 folder 35-PROFILING-BENCHMARKING` → **5/5 OK**
(the `.cpp` files) under strict flags.

- **Lessons** — `01` why measure (Amdahl, premature opt, throughput vs latency)
  · `02` `<chrono>` (steady vs system vs hires, resolution, self-cost)
  · `03` `rdtsc` (fencing, calibration, core-hop — builds on 34/11)
  · `04` statistics (mean lies, median/min/σ, bimodal) · `05` percentiles
  (nearest-rank, nines, fan-out amplification, coordinated omission)
  · `06` jitter (SW + HW/firmware sources, isolcpus/nohz_full, "quiet core")
  · `07` histograms (log-linear, HdrHistogram, CDF plots) · `08` Google
  Benchmark (State loop, DoNotOptimize, fixtures) · `09` benchmark pitfalls
  (DCE / const-fold / hoist / cold start / alignment / frequency / denormals)
  · `10` `perf` basics (stat/record/report, IPC, top-down) · `11` `perf`
  advanced (annotate, PEBS/IBS `:pp`, `perf mem`/`c2c`, LBR) · `12` flame
  graphs · `13` Valgrind (cachegrind `Ir`, callgrind, massif, DHAT) · `14`
  VTune / top-down (bandwidth vs latency bound, `toplev`) · `15` sanitizers
  (ASan/UBSan/TSan/MSan) · `16` production measurement (inline rdtsc + per-
  thread histogram, SPSC ring, sampling, white vs black box).
- **Examples** — `01_timing_methods` · `02_percentiles` · `03_jitter_measure`
  · `04_benchmark_mistakes` (6 BUG/FIX pairs) · `05_google_benchmark/`
  (`minibench.hpp` shim + `bench.cxx` + build scripts — one-line switch to the
  real library) · `06_perf_workflow.sh` + `07_flamegraph.sh` (Linux-only,
  self-contained) · `08_latency_recorder` (HdrHistogram-lite: ~2 ns/record,
  29.5 KB, ≤1.3% percentile error). Measured this box (~2 GHz Zen 2, SSE2,
  unpinned Windows) — ratios/shapes port, tail absolutes don't.

## Next
→ [`../36-LOW-LATENCY-CPP/00-README.md`](../36-LOW-LATENCY-CPP/00-README.md)
