# HFT Completeness Audit

HFT-relevant topics ka coverage tracker.

**Legend:** `[✓]` fully covered · `[~]` partially · `[ ]` missing

Last updated: **Batch 1**

---

## A. HFT PREREQUISITES (C++ side)

Yeh cheezein HFT se pehle aani hi chahiye. Inke bina HFT track ka koi matlab nahi.

| Status | Topic | Folder |
|---|---|---|
| [ ] | Move semantics aur zero-copy thinking | 18 |
| [ ] | RAII aur deterministic destruction | 17 |
| [ ] | Templates aur zero-cost abstraction | 21 |
| [ ] | STL container performance characteristics | 19 |
| [ ] | Custom allocators aur PMR | 19, 36 |
| [ ] | Object layout, padding, alignment | 11, 25 |
| [ ] | Undefined behaviour awareness | 25 |
| [ ] | Exceptions aur `-fno-exceptions` trade-off | 23 |
| [ ] | Threads, mutexes, condition variables | 26 |
| [ ] | Atomics aur memory ordering | 27 |
| [ ] | Lock-free data structures | 28 |

---

## B. SYSTEMS PREREQUISITES

| Status | Topic | Folder |
|---|---|---|
| [ ] | Linux syscalls aur unki cost | 29 |
| [ ] | CPU affinity, isolation (`isolcpus`, `nohz_full`) | 29 |
| [ ] | Real-time scheduling (`SCHED_FIFO`) | 29 |
| [ ] | Huge pages, `mlockall`, page fault avoidance | 29 |
| [ ] | NUMA aur memory locality | 29, 31 |
| [ ] | `mmap` aur shared memory | 29 |
| [ ] | Clocks: TSC, `clock_gettime`, PTP | 29, 35 |
| [ ] | Sockets, UDP, TCP | 30 |
| [ ] | Multicast | 30, 42 |
| [ ] | `epoll` aur event loops | 30 |
| [ ] | Kernel bypass (Onload, ef_vi, DPDK) | 30, 42 |
| [ ] | NIC tuning, IRQ affinity | 42 |
| [ ] | Hardware timestamping | 42 |

---

## C. HARDWARE / PERFORMANCE PREREQUISITES

| Status | Topic | Folder |
|---|---|---|
| [ ] | Pipelines, OoO, speculative execution | 31 |
| [ ] | Branch prediction aur misprediction cost | 31 |
| [ ] | SIMD | 31 |
| [ ] | Hyperthreading (aur HFT mein kyun disable) | 31 |
| [ ] | Frequency scaling, C-states, turbo | 31 |
| [ ] | Cache hierarchy aur latency numbers | 32 |
| [ ] | Cache lines aur false sharing | 32 |
| [ ] | Prefetching | 32 |
| [ ] | TLB | 32 |
| [ ] | Data-oriented design, AoS vs SoA | 32 |
| [ ] | Compiler optimizations aur inlining | 33 |
| [ ] | Reading generated assembly | 34 |
| [ ] | `perf` profiling | 35 |
| [ ] | Percentiles, jitter, tail latency | 35, 36 |
| [ ] | Correct benchmarking methodology | 35 |

---

## D. LOW-LATENCY C++ TECHNIQUES

| Status | Topic | Folder |
|---|---|---|
| [ ] | Latency vs throughput vs jitter | 36 |
| [ ] | p50 / p99 / p99.9 / p99.99 | 35, 36 |
| [ ] | Allocation avoidance aur preallocation | 36 |
| [ ] | Memory pools (fixed-size) | 36, 44 |
| [ ] | Object pools aur object reuse | 36, 44 |
| [ ] | Arena / bump allocators | 36 |
| [ ] | Ring buffers | 36, 41 |
| [ ] | Cache locality tuning | 32, 36 |
| [ ] | False sharing elimination | 28, 36 |
| [ ] | Branch-free / branchless code | 36 |
| [ ] | Virtual dispatch elimination (CRTP, variant, tables) | 16, 36 |
| [ ] | `std::function` cost aur alternatives | 36 |
| [ ] | Lock contention avoidance | 36, 41 |
| [ ] | Busy-spin vs blocking | 36, 41 |
| [ ] | CPU pinning strategy | 29, 41 |
| [ ] | NUMA-aware allocation | 29, 36 |
| [ ] | Syscall avoidance aur batching | 36 |
| [ ] | Page fault avoidance | 29, 36 |
| [ ] | Instruction cache locality, hot/cold splitting | 36, 43 |
| [ ] | Zero-copy patterns | 36, 42 |
| [ ] | Cache warming | 36 |
| [ ] | Trade-off discussion (kab NOT to optimize) | 36 |

---

## E. HFT DOMAIN KNOWLEDGE

| Status | Topic | Folder |
|---|---|---|
| [ ] | HFT kya hai, business model | 37 |
| [ ] | Exchange architecture | 37 |
| [ ] | Market microstructure | 37 |
| [ ] | Order types (market/limit/IOC/FOK/stop) | 37 |
| [ ] | Bid/ask/spread/depth | 37 |
| [ ] | Price-time priority | 37, 39 |
| [ ] | Market making vs taking | 37 |
| [ ] | Latency arbitrage | 37 |
| [ ] | Co-location aur proximity hosting | 37 |
| [ ] | Tick size, lot size, fees | 37 |
| [ ] | HFT system architecture (end-to-end) | 37 |
| [ ] | Risk systems | 37, 44 |
| [ ] | Strategy basics | 37, 44 |
| [ ] | Regulatory basics | 37 |

---

## F. MARKET DATA

| Status | Topic | Folder |
|---|---|---|
| [ ] | L1 / L2 / L3 data | 38 |
| [ ] | Snapshots vs incremental updates | 38 |
| [ ] | Sequence numbers aur gap detection | 38 |
| [ ] | ITCH protocol | 38 |
| [ ] | FIX / FAST | 38 |
| [ ] | SBE | 38 |
| [ ] | Binary / zero-copy parsing | 38 |
| [ ] | Endianness aur wire formats | 38 |
| [ ] | A/B feed arbitration | 38 |
| [ ] | Recovery aur retransmission | 38 |
| [ ] | Timestamping aur clock sync | 38, 42 |
| [ ] | Conflation | 38 |
| [ ] | **BUILD:** market data simulator | 38, 44 |
| [ ] | **BUILD:** feed handler + parser | 38, 44 |

---

## G. ORDER BOOK

| Status | Topic | Folder |
|---|---|---|
| [ ] | Order book data structure design space | 39 |
| [ ] | Naive `std::map` implementation | 39 |
| [ ] | Sorted vector implementation | 39 |
| [ ] | Flat array-of-price-levels | 39 |
| [ ] | Intrusive lists for order queues | 39 |
| [ ] | Order ID lookup (hash vs slab) | 39 |
| [ ] | Add / cancel / modify / execute | 39 |
| [ ] | Price-time priority implementation | 39 |
| [ ] | Top-of-book fast path | 39 |
| [ ] | **Benchmark: naive vs optimized** | 39, 43 |

---

## H. MATCHING ENGINE

| Status | Topic | Folder |
|---|---|---|
| [ ] | Matching algorithm | 40 |
| [ ] | Limit orders | 40 |
| [ ] | Market orders | 40 |
| [ ] | Partial fills | 40 |
| [ ] | IOC / FOK | 40 |
| [ ] | Trade events | 40 |
| [ ] | Self-trade prevention | 40 |
| [ ] | Determinism aur sequencing | 40 |
| [ ] | Event sourcing aur replay | 40 |
| [ ] | Testing + fuzzing | 40 |

---

## I. HFT CONCURRENCY

| Status | Topic | Folder |
|---|---|---|
| [ ] | Single-writer principle | 41 |
| [ ] | Shared-nothing design | 41 |
| [ ] | **BUILD:** SPSC queue | 41, 44 |
| [ ] | LMAX Disruptor pattern | 41 |
| [ ] | Seqlock for snapshots | 41 |
| [ ] | Busy-spin vs condvar trade-off | 41 |
| [ ] | Core pinning strategy | 41 |
| [ ] | Lock-free logging | 41 |
| [ ] | NUMA-aware thread placement | 41 |

---

## J. HFT NETWORKING

| Status | Topic | Folder |
|---|---|---|
| [ ] | Multicast receive path | 42 |
| [ ] | Kernel bypass deep dive | 42 |
| [ ] | Busy-poll sockets | 42 |
| [ ] | Hardware timestamping | 42 |
| [ ] | NIC + IRQ tuning | 42 |
| [ ] | TCP tuning for gateways | 42 |
| [ ] | FPGA offload (intro) | 42 |
| [ ] | Wire-to-wire latency measurement | 42 |
| [ ] | **BUILD:** low-latency UDP receiver | 42, 44 |

---

## K. HFT PROJECTS (code deliverables)

| Status | Project | Folder |
|---|---|---|
| [ ] | Market Data Simulator | 44 |
| [ ] | Binary Feed Parser | 44 |
| [ ] | Limit Order Book | 44 |
| [ ] | Matching Engine | 44 |
| [ ] | Memory Pool + benchmarks | 44 |
| [ ] | Object Pool | 44 |
| [ ] | SPSC Ring Buffer + benchmarks | 44 |
| [ ] | Strategy Simulator | 44 |
| [ ] | Risk Engine | 44 |
| [ ] | Order Manager + Execution Simulator | 44 |
| [ ] | **MINI HFT ENGINE (full pipeline)** | 44 |

---

## L. INTERVIEW READINESS

| Status | Area | Folder |
|---|---|---|
| [ ] | C++ fundamentals questions | 46 |
| [ ] | Pointers/memory questions | 46 |
| [ ] | OOP/vtable questions | 46 |
| [ ] | STL questions | 46 |
| [ ] | Templates questions | 46 |
| [ ] | Move semantics questions | 46 |
| [ ] | Concurrency questions | 46 |
| [ ] | Memory model questions | 46 |
| [ ] | Linux/systems questions | 46 |
| [ ] | Networking questions | 46 |
| [ ] | CPU/cache questions | 46 |
| [ ] | Performance/optimization questions | 46 |
| [ ] | HFT architecture design rounds | 46 |
| [ ] | DSA problems | 20, 47 |
| [ ] | Probability/brainteasers | 46 |

---

## Current HFT readiness

**0%** — HFT track abhi shuru nahi hua.

Aur yeh **bilkul theek hai**. HFT track shuru karne se pehle sections A, B, aur C
(prerequisites) complete hone chahiye. Woh folders 12–35 mein hain.

Agar aap HFT pe seedha jump karoge to woh cargo-cult programming hogi — code copy
karoge, samjhoge kuch nahi, aur interview mein pehle follow-up question pe atak jaoge.

**Rasta:** 01 → 35 pehle. Phir 36 → 44.
