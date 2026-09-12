# 36 — ULTRA-LOW-LATENCY C++ (PHASE 24)

## Prerequisites
`35-PROFILING-BENCHMARKING` (poora performance track)

## Yeh folder kyun
**Yeh mandatory major section hai.**

Ab tak aapne C++, systems, CPU, cache, aur profiling seekh li. Ab woh sab jodkar
**ultra-low-latency** code likhna seekhenge.

⚠️ **Spec ka rule:** koi micro-optimization trick bina context ke nahi. Har technique
ke saath **trade-off** bhi padhenge — kab use karo aur kab NAHI.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-latency-throughput-jitter.md` | **Teenon ka fark**, kis situation mein kya optimize karein |
| 02 | `02-tail-latency.md` | **p99/p99.9/p99.99** — kyun average bekaar hai, budget thinking |
| 03 | `03-sources-of-jitter.md` | GC(nahi), allocation, page faults, syscalls, context switches, interrupts, TLB, frequency |
| 04 | `04-allocation-avoidance.md` | **Hot path mein allocation = disaster** — measured proof |
| 05 | `05-preallocation.md` | Startup pe sab allocate karo, steady state mein kuch nahi |
| 06 | `06-memory-pools.md` | **Fixed-size memory pool — build it, benchmark it** |
| 07 | `07-object-pools.md` | **Object pool** — reuse, free list, construction cost |
| 08 | `08-arena-allocators.md` | Bump/arena allocator, monotonic buffer, PMR ke saath |
| 09 | `09-custom-allocators.md` | STL containers ke saath custom allocators |
| 10 | `10-cache-locality-tuning.md` | Hot/cold splitting, struct packing, alignment (recap + practice) |
| 11 | `11-false-sharing-elimination.md` | Padding, per-thread data, measured |
| 12 | `12-branch-free-programming.md` | **Branchless techniques**, `cmov`, lookup tables, trade-offs |
| 13 | `13-virtual-dispatch-elimination.md` | **CRTP, `std::variant`, function tables, tag dispatch** — measured comparison |
| 14 | `14-std-function-cost.md` | `std::function` ki cost, alternatives (`function_ref`, templates) |
| 15 | `15-ring-buffers.md` | **Ring buffer** — design, power-of-2, cache-friendly indices |
| 16 | `16-batching.md` | Batching aur amortization, trade-off with latency |
| 17 | `17-syscall-avoidance.md` | Syscall counting, busy-poll vs blocking, `io_uring` intro |
| 18 | `18-page-fault-avoidance.md` | `mlockall`, pre-faulting, huge pages, warm-up |
| 19 | `19-cpu-pinning-strategy.md` | Which thread on which core, isolation, NUMA placement |
| 20 | `20-cache-warming.md` | Warm-up strategies, keeping code and data hot |
| 21 | `21-instruction-cache.md` | I-cache locality, hot/cold function splitting, `__attribute__((hot))` |
| 22 | `22-zero-copy-patterns.md` | Zero-copy thinking — views, spans, in-place parsing |
| 23 | `23-compile-time-dispatch.md` | Templates se runtime branches hatana |
| 24 | `24-tradeoffs-and-when-not-to.md` | **HONEST TRADE-OFF DISCUSSION** — kab yeh sab galat hai |
| 25 | `25-exercises.md` | Practice + optimization challenges |

## Examples

| File | Kya |
|---|---|
| `examples/01_allocation_cost.cpp` | Allocation latency distribution — p50/p99/p99.9 |
| `examples/02_memory_pool.cpp` | **Fixed-size pool — build + benchmark vs new/delete** |
| `examples/03_object_pool.cpp` | Object pool with reuse |
| `examples/04_arena_allocator.cpp` | Bump allocator |
| `examples/05_pmr_containers.cpp` | PMR se allocation-free vectors |
| `examples/06_branchless.cpp` | Branch vs branchless — measured |
| `examples/07_dispatch_comparison.cpp` | virtual vs CRTP vs variant vs table — measured |
| `examples/08_std_function_cost.cpp` | function vs lambda vs fn-pointer |
| `examples/09_ring_buffer.cpp` | Ring buffer implementation |
| `examples/10_batching.cpp` | Batch vs per-item — throughput/latency curve |
| `examples/11_page_fault_warmup.cpp` | mlockall + prefault ka asar |
| `examples/12_hot_cold_split.cpp` | I-cache locality demo |

## Time
4 hafte

## Status
✅ **COMPLETE (Batch 10 — PHASE 24).** 24 lessons (`01`–`24`) + `25-exercises.md`
+ 12 examples. `./build.ps1 folder 36-LOW-LATENCY-CPP` → **12/12 OK** under
strict flags. Benchmarks measured at `-O2` on an AMD Zen 2 ~2 GHz box (SSE2,
Windows/MinGW, unpinned — ratios/shapes port, tail absolutes don't).

- **Lessons** — `01` latency/throughput/jitter (three axes, budget thinking)
  · `02` tail latency (p99.9 is the scorecard; C++ has no GC) · `03` jitter
  sources (a hot-path audit checklist; eliminate/bound/make-rare) · `04`
  allocation avoidance (measured tail; the hidden allocations) · `05`
  pre-allocation (warm-up vs steady state; prove zero-alloc) · `06` memory
  pools (build + benchmark a `FixedPool`) · `07` object pools (construct vs
  recycle) · `08` arenas / monotonic buffer / PMR · `09` custom allocators
  (PMR vs classic `Allocator<T>`) · `10` cache locality (hot/cold, AoS/SoA,
  flat) · `11` false sharing (measured 3.5–44×; a jitter source) · `12`
  branch-free (when it wins, when it *loses*; `-O2` if-conversion) · `13`
  virtual dispatch elimination (CRTP/variant/switch/table, homo vs hetero,
  measured) · `14` `std::function` cost (`function_ref`, the SBO cliff) ·
  `15` ring buffers (pow-2, monotonic counters, release/acquire, cached
  index) · `16` batching (throughput vs head-of-line; opportunistic) · `17`
  syscall avoidance (busy-poll cost, `io_uring`+`SQPOLL`, kernel bypass) ·
  `18` page-fault avoidance (`MAP_POPULATE`/`mlockall`/touch/stack; huge
  pages & THP jitter) · `19` CPU pinning (`isolcpus`/`nohz_full`/`rcu_nocbs`,
  SMT, NUMA, `SCHED_FIFO` hazard) · `20` cache warming (cold start vs decay;
  dry-run) · `21` I-cache (Frontend Bound; hot/cold split; PGO/LTO/BOLT) ·
  `22` zero-copy (views/spans/overlays; `from_chars`; the 4 overlay caveats)
  · `23` compile-time dispatch (`if constexpr`, non-type params, startup
  fn-pointer pick; instantiation bloat) · `24` **HONEST trade-offs** (every
  technique's hidden cost; six "when NOT to"; the measure→profile→one
  change→re-measure→explain process).
- **Examples** — `01_allocation_cost` (latency distribution — mixed churn
  p99.9 ~2.6 µs) · `02_memory_pool` (`FixedPool` p99.9 ~30 ns flat vs `new`
  ~180) · `03_object_pool` (construct 30 vs recycle 20 ns) · `04_arena_allocator`
  (~16× vs new/delete) · `05_pmr_containers` (`pmr::vector` on stack, 0
  global `new`) · `06_branchless` (mask ~0.26 vs branchy ~0.32; switch vs
  table ~12×; Rule 2 — `-O2` if-converted the branchy version) ·
  `07_dispatch_comparison` (virtual 7 ns vs CRTP 0.6 ns hetero) ·
  `08_std_function_cost` (`std::function` ~2×; 64-B capture → heap in ctor;
  Rule 2 — latency-bound loop ties the fast three) · `09_ring_buffer` (p50
  20 ns) · `10_batching` (the throughput/HoL curve) · `11_page_fault_warmup`
  (COLD vs WARM ~13×/~95×) · `12_hot_cold_split` (Rule 2 — no measurable
  difference in a micro-bench; kept honest).

## Next
→ [`../37-HFT-FUNDAMENTALS/00-README.md`](../37-HFT-FUNDAMENTALS/00-README.md)
