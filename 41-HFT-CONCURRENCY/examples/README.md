# Examples — Folder 41 (HFT concurrency)

> Portable `.cpp` (Windows-native APIs used deliberately in 05/06 — see
> below). **`-O2` mandatory** for every benchmark here. `./build.ps1
> folder 41-HFT-CONCURRENCY` → **8/8 OK**. 2 shared `.hpp` headers
> (`spsc_queue.hpp`, `seqlock.hpp`) — glob `*.cpp` doesn't compile them
> standalone. `08_pipeline_demo.cpp` also includes
> `../../40-MATCHING-ENGINE/examples/matching_engine.hpp` directly —
> the capstone runs the REAL matching engine, not a stand-in.

## The box these were measured on

**AMD Ryzen 7 4700U** (Zen 2), ~2.0 GHz, **Windows x64 + MinGW-w64 GCC
15.1.0**, 8 logical processors, **unpinned desktop** (no `isolcpus`/
`nohz_full`-style OS isolation). This matters more in this folder than
almost any other: **every benchmark's tail (p99+) is dominated by OS
scheduler jitter**, not by the mechanism being tested — quote the
medians and the shapes, not the absolute tails (35/06's lesson, sharper
here than usual).

## Windows-specific content (documented, not hidden)

`05_busy_spin_vs_cv.cpp` and `06_core_pinning.cpp` use `<windows.h>`
(`GetThreadTimes` for real per-thread CPU time via a duplicated thread
handle — `native_handle()` on this MinGW posix-threading build is a
`pthread_t`, not a Win32 `HANDLE`, so a `DuplicateHandle(GetCurrentThread())`
call from inside the thread is needed to get one usable from another
thread — and `SetThreadAffinityMask` for pinning). The Linux equivalents
(`getrusage(RUSAGE_THREAD,...)`, `pthread_setaffinity_np`/
`sched_setaffinity`, `isolcpus`/`nohz_full`) are already built and
verified in `29-LINUX-SYSTEMS/examples/06_cpu_affinity.linux.cpp` and
`10_realtime_thread.linux.cpp` — this folder demonstrates the SAME
concepts with the API the dev box actually has, and (unlike a
`.linux.cpp` stub) actually runs and measures a real effect.

## Examples

| File | Lesson(s) | Kya |
|---|---|---|
| `spsc_queue.hpp` | 04 | Reusable padded + cached-index SPSC ring (28's design, packaged) |
| `seqlock.hpp` | 06 | Reusable seqlock snapshot (28's design, packaged) |
| `01_spsc_hft.cpp` | 04 | API + correctness demo — order and content preserved exactly across 2M messages |
| `02_spsc_benchmark.cpp` | 04 | Rigorous rdtsc p50/p99/p99.9 hand-off latency + throughput |
| `03_disruptor.cpp` | 05 | Single-producer/N-independent-consumer gated ring — fan-out + batching, correctness-verified |
| `04_seqlock_snapshot.cpp` | 06 | Seqlock vs `shared_mutex` under max-rate writer contention |
| `05_busy_spin_vs_cv.cpp` | 07 | Spin vs block vs hybrid — latency AND real CPU-time cost, both sides of the trade-off |
| `06_core_pinning.cpp` | 08 | `SetThreadAffinityMask` + measured scheduling-jitter A/B (pinned vs unpinned) |
| `07_async_logger.cpp` | 09 | Lock-free async logger — hot path only enqueues, ~14x faster than sync format+write |
| `08_pipeline_demo.cpp` | 01, 02, 03, 13 | **Capstone**: real 3-stage thread-per-stage pipeline running 40's actual `MatchingEngine` |

## Key numbers (this run)

```
SPSC hand-off (02, N=20M paced):        p50  400.8 ns   p99  171856.7 ns (OS jitter)
Disruptor fan-out (03, N=4M, 2 consumers): both consumers process ALL 4M events, 0 mismatches
Seqlock vs shared_mutex (04, unthrottled writer + 4 readers):
  seqlock       ~103-129 M reads/s   torn=0
  shared_mutex  ~0.036-0.041 M reads/s   torn=0   <- ~2500-3500x slower under this stress load
Spin vs block vs hybrid (05, N=100K paced ~5us apart):
  spin    p50  210.4 ns   CPU ~97-100% of one core
  block   p50 8997.1 ns   CPU ~15-31%
  hybrid  p50 7354.0 ns   CPU ~19-53%    <- median between spin and block, CPU noisy
Core pinning (06, N=20M iterations):
  unpinned  p99  140 ticks   max     829,657 ticks
  pinned    p99   80 ticks   max  15,600,420 ticks   <- fewer hiccups, but ONE huge outlier
Async logger (07, N=500K):
  naive (sync format+write)  p50 1082.1 ns
  async (enqueue only)       p50   70.1 ns   <- ~13.9x mean improvement
Pipeline capstone (08, N=500K orders, 394547 trades, real MatchingEngine):
  end-to-end (Stage1 arrival -> Stage3 receipt): p50 971.8 ns   (tail OS-jitter-dominated)
```

## The recurring finding across this folder

**On an unpinned desktop, every mechanism's p50 tells a clean, expected
story (spin < hybrid < block; seqlock >> shared_mutex under contention;
async logging >> sync logging) — but the p99+ tail is swamped by OS
scheduler jitter regardless of which mechanism is used underneath.**
`06`'s core-pinning experiment shows *why*: `SetThreadAffinityMask` only
guarantees a thread stays ON one core, not that it has that core to
itself — genuine tail-latency control needs OS-level isolation
(`isolcpus`/`nohz_full`, already built in 29), not just affinity. This
is the folder's central, honestly-reported limitation, not swept under
the rug (06-seqlock, 07-busy-spin, 08-core-pinning, 09-determinism-
adjacent lessons all reference it).
