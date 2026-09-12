# 41 — HFT CONCURRENCY (PHASE 29)

## Prerequisites
`40-MATCHING-ENGINE`, `28-LOCK-FREE`

## Yeh folder kyun
HFT threading normal multithreading se **bilkul alag** hai. Yahan hum threads
share nahi karte — hum unhe isolate karte hain.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-hft-threading-model.md` | Pipeline model, thread per stage, shared-nothing |
| 02 | `02-single-writer-principle.md` | **Single writer** — ek data ka ek hi malik, no locks needed |
| 03 | `03-shared-nothing-design.md` | Data partitioning, message passing over sharing |
| 04 | `04-spsc-queue-for-pipeline.md` | **SPSC queue between stages — build + benchmark** |
| 05 | `05-lmax-disruptor.md` | **Disruptor pattern** — ring buffer, sequences, batching |
| 06 | `06-seqlock-for-snapshots.md` | **Seqlock** — market data snapshot bina lock ke |
| 07 | `07-busy-spin-vs-blocking.md` | **Busy-spin trade-off** — latency vs CPU, measured |
| 08 | `08-core-pinning-strategy.md` | Which thread where, isolated cores, NUMA nodes |
| 09 | `09-lock-free-logging.md` | Async logging — hot path sirf enqueue karta hai |
| 10 | `10-wait-free-reads.md` | Wait-free read paths for strategy threads |
| 11 | `11-avoiding-priority-inversion.md` | Priority inversion, RT scheduling ke saath |
| 12 | `12-numa-thread-placement.md` | NUMA-aware placement, memory locality |
| 13 | `13-timing-and-sequencing.md` | Event ordering across threads, timestamps |
| 14 | `14-exercises.md` | Practice + pipeline building |

## Examples

| File | Kya |
|---|---|
| `examples/01_spsc_hft.cpp` | **Production-grade SPSC queue** |
| `examples/02_spsc_benchmark.cpp` | Latency + throughput measurement |
| `examples/03_disruptor.cpp` | Disruptor-style ring |
| `examples/04_seqlock_snapshot.cpp` | Seqlock market data |
| `examples/05_busy_spin_vs_cv.cpp` | Busy-spin vs condvar — measured |
| `examples/06_core_pinning.cpp` | Pinning + isolation |
| `examples/07_async_logger.cpp` | Lock-free logger |
| `examples/08_pipeline_demo.cpp` | Multi-stage pipeline |

## Time
3 hafte

## Status
✅ **COMPLETE** — 14 lessons (`01`–`14`) + 8 examples + 2 shared headers
(`spsc_queue.hpp`, `seqlock.hpp`). `./build.ps1 folder 41-HFT-CONCURRENCY`
→ **8/8 OK**.

Real thread-per-stage pipeline built end to end (`08_pipeline_demo.cpp`
runs 40-MATCHING-ENGINE's ACTUAL `MatchingEngine`, not a stand-in),
production-grade SPSC queue + a simplified LMAX Disruptor (fan-out,
gating, batching, correctness-verified with 2 independent consumers over
4M events) + seqlock snapshots. Central measured findings: seqlock beats
`shared_mutex` by ~2500-3500x under an adversarial max-rate-writer stress
test (vs 28's original ~80-100x under gentler conditions); busy-spin vs
blocking measured on BOTH latency AND real CPU-time cost (via a
`GetThreadTimes`+`DuplicateHandle` fix for a MinGW `native_handle()`
quirk); core pinning (`SetThreadAffinityMask`) reduced hiccup frequency
but produced a WORSE single max-delay outlier — an honest demonstration
that affinity ≠ isolation; async logging cut hot-path cost ~14x. A
counter-intuitive pacing/tail-latency finding (slower feed pace made
p99 WORSE, not better, because longer test duration meant more exposure
to OS scheduler jitter, not more backlog) is documented as a genuine
Rule-2 case study in lesson 13.

## Next
→ [`../42-HFT-NETWORKING/00-README.md`](../42-HFT-NETWORKING/00-README.md)
