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
⏳ **Yeh folder abhi syllabus stage pe hai.** Upar ki file list poori plan hai —
content agle batch mein aayega.

## Next
→ [`../37-HFT-FUNDAMENTALS/00-README.md`](../37-HFT-FUNDAMENTALS/00-README.md)
