# HFT Completeness Audit

HFT-relevant topics ka coverage tracker.

**Legend:** `[✓]` fully covered · `[~]` partially · `[ ]` missing

Last updated: **Batch 11 part 6 COMPLETE (PHASE 34: the FINAL GAP AUDIT) — 🏁 course structurally complete.** Sections A–L all COMPLETE; the one stale `[~]` in this file — `constexpr` / compile-time computation — is now `[✓]` (full treatment in folder 21: `07-if-constexpr.md`, `13-template-metaprogramming.md`). Cross-checked `CPP-COMPLETENESS-AUDIT.md` (8 stale markers fixed) and drove every `WHAT-I-STILL-NEED-TO-LEARN.md` section to **NONE**; genuinely-specialist topics (FPGA, DPDK app-dev, RDMA, exchange protocol specs, alpha, quant math, and now the C++23-only stdlib types that need `-std=c++23`) are in the explicit SPECIALIZED list. Prev: Batch 11 part 5 COMPLETE (PHASE 33: 49 PROJECTS) — every content folder (00–49) is now built. Connected, difficulty-ordered projects: 5 lesson files (17 project specs + a code-review-checklist doc + a folder-44 pointer) + **17 verified reference implementations** (`examples/{beginner,intermediate,advanced}/`). HFT-relevant threads: the advanced allocators (arena/pool/segregated — **measured ~20–28× vs `::operator new` at `-O2`**) are the general form of folder-44's `FixedPool`/`ObjectPool`; the lock-free **SPSC ring** and **MPSC Vyukov** queue are the wire→engine hand-off; the **epoll echo server** (`*.linux.cpp`) is the OS event loop a feed handler / order gateway sits on (folder 42); the KV store's **WAL + replay** is the determinism/crash-recovery model folder 43/44 use; `05-hft-projects-link.md` maps each advanced project onto its folder-44 counterpart. **No new HFT-capability rows** — this folder *applies* the prerequisites in connected programs. All 16 non-linux examples compile strict-clean + run + assert. Mojibake: 0. **Folders 00–49 all built — Sections A–L all COMPLETE — only the final gap audit remains** (a cross-repo review to drive every `WHAT-I-STILL-NEED-TO-LEARN.md` section to "NONE" / explicit "SPECIALIZED"). Prev: Batch 11 part 4 COMPLETE (PHASE 33: 48 CHEATSHEETS). The **quick-reference layer** — 13 markdown sheets distilling folders `01`–`47`, each cross-linking its deep source folder. HFT-relevant sheets: `08` HFT production tuning checklist (BIOS → kernel cmdline `isolcpus`/`nohz_full`/`rcu_nocbs` → IRQ affinity → `mlockall`+prefault+hugetlbfs → per-thread pinning → NIC/kernel-bypass → warm-up → continuous `perf` verification + a gotchas table — condenses folders 29/36/42), `09` memory ordering (the 6 orders, release/acquire handoff, `cmpxchg` weak/strong, ABA + fixes, lock-free vs wait-free — condenses 27/28), `10` latency numbers (the core ladder, consistent with 46/12), `12` HFT glossary (order book / order types / participants / microstructure / infra / protocols / regulation — condenses folder 37), `13` includes the HFT-architecture revision sheet + the make-a-market drill. **No `.cpp`, no new HFT-capability rows** — this folder *condenses* folders 01–47. Mojibake: 0. Prev: Batch 11 part 3 COMPLETE (PHASE 33: 47 CODING-PROBLEMS). A graded **practice bank** — 10 themed problem files, **250 problems** (basics 30 · arrays/strings 40 · pointers/memory 25 · OOP/design 20 · STL 35 · templates 20 · concurrency 25 · lock-free 15 · optimize-this 20 · HFT 20), each = statement + `Pattern:` hint + `<details>` (approach + complexity). Plus `11-solutions/` (README + 10 per-category worked-code writeups) and **10 verified runnable examples** (`./build.ps1 folder 47-CODING-PROBLEMS` → 10/10 OK strict, all run + assert; incl. `10_order_book_ops` with a `crossed()` invariant, `08_seqlock` 2M-write torn-reads=0, `07_blocking_queue` N-prod/M-cons checksum). File `09` "optimize-this" targets grounded in folders 32/43's measured numbers; `09_optimize_row_vs_col.cpp` measures **~45–70× at `-O2`** here (Rule 2: bigger than the textbook ~7×, cache + TLB + vectorization stack — reported, not rounded). Files `08`/`10` HFT problems map onto folders 39/40/41 + `46-INTERVIEW-PREP/examples`. **No new HFT-capability rows** — this folder *drills* folders 01–46; it flips the one remaining Section-L `[~]` (DSA/coding problems) to `[✓]`. Mojibake: 0. Prev: Batch 11 part 2 COMPLETE (PHASE 33: 46 INTERVIEW-PREP). A layered question bank (`02`–`14` = Layer 1 types → Layer 13 tick-to-trade architecture, every answer cross-refs the source course folder) + interview strategy (`01`), system design (`15` — arc + 4 worked designs + rubric), brainteasers/probability (`16` — EV, the make-a-market game, classic puzzles, mental math, Kelly), C++ trick questions (`17` — 30+ traps with the right answer + why), behavioural (`18` — STAR + the 6 stories + discussing this course's projects), mock scripts with rubrics (`19`), resume (`20`). 20 lessons + 8 **verified** coding examples (`examples/*.cpp` → `./build.ps1 folder 46-INTERVIEW-PREP` → 8/8 OK, strict) + 4 design docs + 4 mock transcripts. **No new HFT-capability rows** — this folder *tests* folders 01–45; every Section-L row moves to `[✓]`. Mojibake 6 → 0. Prev: Batch 11 part 1 COMPLETE (PHASE 33: 45 DEBUGGING — a skill folder). Sections A–K stay COMPLETE — folder 45 adds no new HFT-capability rows (the HFT-relevant pieces it touches, async lock-free logging and `rr`/`perf` for latency-bug hunting, are already tracked under folder 41 lines and folder 35). What it adds is the applied **debugging toolchain**: mindset (hypothesis→test, `git bisect`), GDB basics + advanced with **real transcripts on this box** (MinGW GCC 15.1.0, GDB 16.3), crashes + core dumps, `-O2` debugging (`<optimized out>`, constant-folded functions), sanitizers (ASan/UBSan/TSan/MSan internals + combine matrix), valgrind, multithreaded (`thread apply all bt`, deadlock signature), reverse debugging (`rr`), logging strategy (re-deepens the folder-41 **HFT async logger**), `perf` for bugs (off-CPU, syscall storms, false sharing), and a 40+-entry symptom→tool→fix bug catalog. 13 lessons + 5 standalone examples + 10 "find & fix" buggy programs; `./build.ps1 folder 45-DEBUGGING` → 5/5 OK under strict warnings; mojibake 34 → 0. **Wrap-up track: folder 45 done; only 46 (Section L, interview readiness) + 47–49 + the final gap audit remain.** Prev: Batch 10 part 9 COMPLETE (PHASE 32: 44 HFT-PROJECTS — the CAPSTONE). **Section K (HFT projects) is now COMPLETE — Sections A–K all done.** Folder 44 assembles everything from folders 01–43 into one `MiniHftEngine`: MarketData → Parser → L2Book → Strategy → Risk → OMS → Venue → fills → PnL, single-threaded, fully deterministic (no wall clock — event-timestamp driven). 15 lessons + 12 example drivers + 11 shared `mh_*.hpp` headers, `./build.ps1 folder 44-HFT-PROJECTS` → 12/12 OK under strict warnings. **Reuse over rewrite** (the capstone's whole point): folder 40's `MatchingEngine` is `ExecutionSimulator` ("the venue"), folder 41's `SpscQueue` is the wire→engine hand-off, folder 39/14/36/43 patterns for book/pools/strategy. **The capstone optimization** (43 methodology, applied end-to-end): `MiniHftEngine` is `template <class Venue>` — `NaiveEngine` (std::map `MatchingEngine` venue: one tree insert + one `std::list`-node `malloc` per market message) vs `OptimizedEngine` (`FastVenue`: flat-array aggregate book + per-level FIFO + IOC sweep). Profiling found the venue mirror was the `book` stage's bulk (~150 ns/msg); `FastVenue` roughly halves it (~67 ns/msg) → ~1.5–1.6× end-to-end. **Correctness gate first:** `12_integration_tests.cpp` proves naive == optimized byte-for-byte (fills, qty, P&L, position) across 5 seed/config combos, both deterministic, invariants (|position| ≤ risk max, zero sequence gaps, OMS always settles) held. Per-component measured: L2Book apply ~17 ns/msg (BBO matches a `std::map` reference exactly); FixedPool ~2.0×/~5.0× (p50/p99.9) vs `new`/`delete`; ObjectPool stale-handle → nullptr even post-recycle; SPSC ~6 M msg/s in-order; risk 14/14 checks; feed parser v1≈v3 at `-O2` (honest Rule-2 null). **No alpha** — `SpreadCrossStrategy` is mechanical (37 SPECIALIZED list). Mojibake 16 → 0. Prev: Batch 10 part 8 COMPLETE (PHASE 31: 43 HFT-OPTIMIZATION). Folder 43 is the end-to-end optimization *methodology* folder — measure → profile → hypothesize → change (one) → re-measure → **explain**, with a **correctness gate before the speedup gate**. Its spine `pipeline.hpp` carries two full tick-to-order pipelines on one deterministic feed: `PipelineV0` (naive: `substr`+`stod` parse, `std::map<double>`+`std::list` book, `std::deque` re-sum SMA, `std::string` encode) and `PipelineV3` (hand int-parse + fixed-point price, flat-array book + cached top-of-book + dense-id direct index, ring-buffer running-sum SMA with **zero division**, POD encode). V3's signal math is the exact integer equivalent of V0's float cross-condition; `04_before_after.cpp` proves the order-fire stream is byte-identical (110/110) **before** reporting speedup. Measured this box (Zen 2, unpinned — ratios): end-to-end **~60–80×** (v0 ~1.9–2.7 µs/tick → v3 ~25–45 ns/tick); division `div`→shift ~17× / →magic-mul ~13× / →reciprocal ~7× (only the divide timed — the CLAUDE.md "hidden `%` in the loop" warning explicitly heeded); struct fat→SoA ~8×; fixed-point `0.1×10 = 0.99999999999999988898 ≠ 1.0`; **hot/cold split ~1% — an honest Rule-2 null** (frontend not the bottleneck at this scale, carries 36/12; attribute kept anyway, cost 0). `02_profile_analysis.sh` (`perf` bottleneck-hunt + signal→lesson map) is Linux-only, `bash -n`-checked. `./build.ps1 folder 43-HFT-OPTIMIZATION` → 7/7 OK; mojibake sweep found 298 stray Devanagari/Cyrillic homoglyphs across drafts, fixed → 0. **Sections A–J complete; folder 43 deepens Sections C/D (applied) — the build-track deliverable is folder 44.** Prev: Batch 10 part 7 COMPLETE (PHASE 30: 42 HFT-NETWORKING). **Section J (HFT networking) is now COMPLETE.** Folder 42 covers the full wire-to-wire path: the kernel-bypass ladder (kernel-socket → `SO_BUSY_POLL` → Onload → ef_vi → DPDK) with an honest trade-off table, an `INetworkReceiver` abstraction making backend choice a config-time decision, and two genuine fixes to earlier limitations — TX+RX hardware timestamps now share ONE kernel clock domain (fixing 30/09's relative-jitter-only measurement), and a mid-development correction on a TCP writev claim (the ~40ms Nagle stall needs the SENDER's Nagle on, which `TCP_NODELAY` here prevents — corrected before shipping). A full wire-to-wire capstone chains kernel timestamps across 2 hops + processing. **This folder's code is majority Linux-only** (`.linux.cpp`, this dev box has no WSL) — written carefully, hand-reviewed (several sign-compare/unbounded-block/empty-vector bugs caught and fixed), every number marked as an explicit unmeasured estimate, never fabricated. **Sections A–J are now all complete.** Prev: Batch 10 part 6 (PHASE 29: 41 HFT-CONCURRENCY) — Section I complete: a real multi-threaded pipeline built on 40's matching engine, seqlock ~2500-3500× faster than shared_mutex under adversarial contention, and an honest affinity-≠-isolation finding from core-pinning. Before that: Batch 10 part 5 (PHASE 28: 40 MATCHING-ENGINE) — Section H complete: Limit/Market/IOC/FOK matching, self-trade prevention, and a genuine correctness finding (a naive FOK precheck silently violates FOK's all-or-nothing contract once combined with STP) fixed + fuzz-verified against an independent reference engine (0 disagreements across 30000 commands). Before that: Batch 10 part 4 (PHASE 27: 39 ORDER-BOOK) — Section G complete: the order book built 3 times, a genuine Rule-2 regression (V2 overall worse than V1) root-caused + fixed in V3. Before that: Batch 10 part 3 (PHASE 26: 38 MARKET-DATA) — Section F complete. Before that: Batch 10 part 2 (PHASE 25: 37 HFT-FUNDAMENTALS) — domain knowledge. Before that: Batch 10 part 1 (PHASE 24: 36 LOW-LATENCY-CPP) — Section D essentially complete.
Previously: Batch 8 DONE (PHASE 17–18: 27 ATOMICS-MEMORY-MODEL — data races + all 6 memory orders + CAS + happens-before + fences + x86-TSO/ARM + ABA + litmus; 28 LOCK-FREE — SPSC & MPMC rings + Treiber + Michael-Scott + hazard pointers + epochs/RCU + seqlock + "when NOT lock-free").

---

## A. HFT PREREQUISITES (C++ side)

Yeh cheezein HFT se pehle aani hi chahiye. Inke bina HFT track ka koi matlab nahi.

| Status | Topic | Folder |
|---|---|---|
| [✓] | Move semantics aur zero-copy thinking — value categories (2-question model), move ctor/assign (steal+null), `std::move` = cast, `return std::move` pessimization, perfect forwarding, **`noexcept` move + `std::vector` growth (measured ~3x)**, copy elision (RVO/NRVO) | 13, 18 |
| [✓] | RAII aur deterministic destruction — acquire-in-ctor/release-in-dtor, stack unwinding + the 5 dtor-skip gaps, move-only resource wrapper, `unique_ptr`/`shared_ptr`/`weak_ptr` (control block, atomic refcount, `make_shared` 1 vs 2 allocs), custom deleters + EBO, Rule of Zero | 17 |
| [✓] | Templates aur zero-cost abstraction — full folder 21: function/class templates, deduction, NTTP (→ inline storage + unrolled loops), specialization, variadic + folds, `if constexpr`, `<type_traits>` internals, **SFINAE**, **concepts**, **CRTP** + policy-based design, tag dispatch, TMP via `constexpr`, two-phase lookup, instantiation bloat + `extern template`, **templates-in-HFT** synthesis. Measured: CRTP ~0.56 ns/call vs virtual (real boundary) ~2.43 ns; virtual ~2.51 / template ~1.14 / `variant`+`visit` ~1.13 ns per call | 16, 19, 21 |
| [✓] | STL container performance characteristics — full per-container layout + complexity + **measured cache cost** (19 files 02–07, 25): iterate 1M `vector` 0.67 / `list` 18 / `set` 197 ms; `map` 1049 vs sorted-vec 357 vs `unordered_map` 113 ns/lookup; invalidation table; growth/`reserve`; **STL-in-HFT** synthesis (19 file 26) | 09, 10, 11, 13, 14, 19 |
| [✓] | Custom allocators aur PMR — Allocator concept (C++17 minimal surface, converting ctor, `operator==`), arena/bump + pool allocators (19 file 23, `09_custom_allocator`); **PMR** (`monotonic_buffer_resource` on a stack buffer → 0 global `new` measured, `null_memory_resource` upstream as a no-alloc assertion, `release()` reuse) (19 file 24, `10_pmr_demo`); HFT hot-path pool patterns full in 36 | 14, 19, 36 |
| [✓] | Object layout, padding, alignment — deep dive + measured reorder (30-45%) + packed wire structs + static_assert | 11, 25 |
| [✓] | Undefined behaviour awareness — UB vs unspecified vs impl-defined; how `-O2`/`-O3` exploit UB (null-check removal, signed-overflow loop assumptions); catalog by category; UBSan/ASan/TSan in CI, `_GLIBCXX_ASSERTIONS`, `static_assert` layout guards; `std::bit_cast` not pointer-cast (23 file 13); object-model UB depth in 25 | 23, 25 |
| [✓] | Exceptions aur `-fno-exceptions` trade-off — measured: happy-path try/catch vs return-code **~1.0×** (zero-cost), **~6000+ ns per throw+catch vs ~1.5 ns per return → ~4000×**; the 4 reasons HFT disables exceptions (latency determinism / hot `.text` size for I-cache / more aggressive inlining / compiler-enforced auditability); the exception-free toolkit (`[[nodiscard]] enum class`, `std::expected` with a cheap enum `E`, `error_code` at OS boundaries, `new(nothrow)` / pools, `abort` for corrupt invariants); RAII + `noexcept` moves unchanged | 23 |
| [✓] | Threads, mutexes, condition variables — `std::thread`/`jthread` (join-or-`terminate`, `native_handle` for pinning), **data race = UB** (measured ~70% lost updates), `std::mutex` (uncontended ~15 ns CAS vs contended futex + context switch = a P99 spike — why the hot path takes **no** locks), lock guards, **deadlock** (Coffman 4 + fixes), `shared_mutex` vs the snapshot pattern, **CV predicate pattern** + lost/spurious wakeup, futures/`packaged_task`, a **built thread pool** (measured ~5.7× / 8 cores), C++20 `latch`/`barrier`/`semaphore`/`stop_token`, `thread_local` (TLS cost, `Context&` alternative). Hot-path posture: single pinned thread + lock-free queues; locks/CVs on the control plane only | 26 |
| [✓] | Atomics aur memory ordering — the **formal data-race definition** (⇒ whole-program UB); `std::atomic<T>` + all ops (x86 `mov`/`xchg`/`lock xadd`/`lock cmpxchg`); **CAS** (`weak`/`strong`, spurious failure, `expected` overwrite, CAS loops, retry storms); **all 6 memory orders** + when-which (`relaxed` counter / `release`-`acquire` publish / `seq_cst` default + its store-side `mfence`); **happens-before / synchronizes-with / sequenced-before**; fences (`atomic_thread_fence` vs compiler-only `atomic_signal_fence`); **x86-TSO vs ARM/POWER** (store→load reorder, multi-copy atomicity, per-op cost on ARM); `std::atomic_ref`; **ABA** + tagged word; **litmus tests** (SB/MP/LB/IRIW). Measured: store `relaxed`/`release` ~0.7 ns vs `seq_cst` ~13 ns (~18×), load & RMW order-insensitive on x86; store buffering observed (rel/acq does NOT stop it, seq_cst does) | 27 |
| [✓] | Lock-free data structures — **SPSC ring** (release/acquire index hand-off, no CAS, wait-free in practice, no ABA — built + optimized: naive ~52 → cached-index ~16–20 ns/msg), **Vyukov bounded MPMC** (per-cell `seq` turnstile), **Michael-Scott queue** (dummy node, helping), **Treiber stack** (tagged head, no DWCAS); **memory reclamation** (fixed pool / hazard pointers / epochs / RCU / QSBR — the unbounded-memory failure mode); **seqlock** (1-writer/N-reader snapshot — measured ~80–100× faster reads than `shared_mutex`); progress guarantees (wait-free ⊂ lock-free ⊂ obstruction-free); **testing** (invariant stress → TSan → model checkers → forced interleaving); **when NOT lock-free** (measured: contended Treiber ~5× *slower* than `mutex+vector`; SPSC ~4–6× *faster* — shape decides; sharding as the usual better answer) | 28 |

---

## B. SYSTEMS PREREQUISITES

> **Section B is now COMPLETE** at the lesson level (folders 29–30). Examples are
> `*.linux.cpp` — real Linux code, `SKIP (linux-only)` on this MinGW box; verify
> on Linux/WSL. Numbers in `EXPECTED` blocks are typical, "not measured on your
> machine" labelled (Rule 2). Deeper HFT-specific NIC/IRQ/timestamping treatment
> still lands in folder 42.

| Status | Topic | Folder |
|---|---|---|
| [✓] | Linux syscalls aur unki cost — `syscall` instr → ring 3→0, ~300 ns fixed overhead (mode switch + KPTI/retpoline), **vDSO** (`clock_gettime` ~20 ns, no trap), count-reduction (buffering / `mmap` / shm / `recvmmsg` / `io_uring` / busy-poll), `strace -c` for profiling; ratio syscall:userspace ~100–300× | 29 |
| [✓] | CPU affinity, isolation (`isolcpus`, `nohz_full`) — affinity ≠ isolation (need both); `sched_setaffinity`/`cpu_set_t`; `isolcpus`/`nohz_full`/`rcu_nocbs`/`irqaffinity` boot params; `cpuset` cgroup; SMT sibling idling; topology-aware pinning by physical core; startup self-check; example `06` (pinning tightens the tail) | 29 |
| [✓] | Real-time scheduling (`SCHED_FIFO`) — RT class above CFS; no timeslice; hazards (starvation / priority inversion / `sched_rt_runtime_us` throttle); `RLIMIT_RTTIME`; **"isolated core + CFS busy-poll often matches/beats FIFO with less risk"**; example `10` (CFS vs FIFO jitter) | 29 |
| [✓] | Huge pages, `mlockall`, page fault avoidance — minor/major/COW fault cost; `mlockall(MCL_CURRENT\|MCL_FUTURE)` + pre-fault + dry-run ⇒ **zero page faults in steady state**; `getrusage` fault counts; TLB reach; THP vs hugetlbfs; `khugepaged` collapse stall; examples `07` (cold/warm/mlock), `08` (4K vs 2M pointer-chase) | 14, 29 |
| [✓] | NUMA aur memory locality — local vs remote latency (~1.5–2.2×); **first-touch** placement + the "init thread allocates everything" bug; `numactl`/`mbind`/`set_mempolicy`; `kernel.numa_balancing=0`; NIC's NUMA node; `numastat`/`numa_maps`; keep the whole hot pipeline on one node | 29, 31 |
| [✓] | `mmap` aur shared memory — file/anon × shared/private, lazy first-touch fault, `MAP_POPULATE`/`madvise`; `shm_open`+`mmap` cross-process; **folder 28's SPSC ring in `/dev/shm`/hugetlbfs** for feed→strategy→gateway, no shared locks; layout rules (offsets not pointers); TLB shootdown on `munmap`; example `05` (fork + shm hand-off) | 29 |
| [✓] | Clocks: TSC, `clock_gettime`, PTP — `CLOCK_MONOTONIC` (vDSO, ~20 ns) vs `_RAW` (trap, ~250 ns) vs `_COARSE` (~6 ns, 1 ms granular); `rdtsc`/`rdtscp` + calibration + invariant-TSC flags; `clocksource=tsc` mandatory; busy-spin to a deadline for sub-µs "act at T"; PTP + `ptp4l` + `phc2sys` for cross-host; example `09` (clock cost table) | 19, 29, 30, 35 |
| [✓] | Sockets, UDP, TCP — call sequences, `recv` 0 vs <0, **framing** (length prefix), `getaddrinfo` at startup; TCP deep (handshake / window / RTO ~200 ms vs 3-dup-ACK fast retransmit / SACK / cwnd / TIME_WAIT); **Nagle + delayed-ACK ~40 ms deadlock** → `TCP_NODELAY` + one `writev`/msg; UDP loss/reorder + A/B feeds + kernel drops + `recvmmsg`; examples `01`–`05` | 30 |
| [✓] | Multicast — `IP_ADD_MEMBERSHIP` = kernel filter + IGMP report; IGMP snooping failure modes; explicit `imr_ifindex` on multi-NIC; SSM (`232/8`); TTL/LOOP; **A/B feed arbitration by sequence number** (fills most single-path loss with zero round-trips); example `06` | 30, 42 |
| [✓] | `epoll` aur event loops — O(ready) vs `select`/`poll` O(N) + `FD_SETSIZE`; level vs edge-triggered (drain to `EAGAIN`); event-loop skeleton; `EPOLLOUT` add/remove discipline; `EPOLLONESHOT`/`EXCLUSIVE`/`RDHUP`; `timerfd`/`signalfd`/`eventfd` in one loop; hot path busy-polls/bypasses instead; example `07` (full ET server) | 30 |
| [✓] | Kernel bypass (Onload, ef_vi, DPDK, AF_XDP) — what's removed (syscall + copy + IP/UDP/TCP stack + softirq + wakeup, ~1–5 µs → ~100–300 ns); what you take on (your own protocol / TCP, a spinning core, hugepages, NIC seizure, `tcpdump` blind); DPDK vs Onload/VMA (`LD_PRELOAD`) vs ef_vi vs **AF_XDP** (mainline, pragmatic first step); the adoption ladder | 30, 42 |
| [✓] | NIC tuning, IRQ affinity — NIC RX rings (`ethtool -G`), interrupt coalescing (min/off for HFT), **GRO/LRO off**, RSS queues aligned to housekeeping cores + pinned IRQs, RPS off for hot queues, pause frames off, `pfifo_fast`/`mq` qdisc; hard IRQ → softirq path; `irqbalance` off; persist settings; verify by drop counters | 30, 42 |
| [✓] | Hardware timestamping — `SO_TIMESTAMPING` (HW RX/TX + SW), `recvmsg` cmsg parse (`scm_timestamping.ts[0]`/`ts[2]`), TX stamps on the error queue; **clock domains** (HW PHC vs app clock — discipline or jitter-only); PTP (`ptp4l` disciplines PHC, `phc2sys` disciplines system clock); boundary/transparent-clock switches; MiFID II; example `09` | 30, 42 |

---

## C. HARDWARE / PERFORMANCE PREREQUISITES

| Status | Topic | Folder |
|---|---|---|
| [✓] | Pipelines, OoO, speculative execution — **folder 31 files 04–08**: pipelining (latency vs throughput, 4 hazard types, ~15–20 cyc flush/refill), superscalar (execution ports, ILP, IPC, limiting-port analysis via `llvm-mca`), out-of-order (ROB / register renaming / reservation stations, speculation + rollback, the OoO window, why a *dependent* chain of cache misses can't be hidden → pointer-chasing is the enemy), speculative execution (Spectre/Meltdown, KPTI/retpoline cost, `mitigations=off` preconditions + when it barely helps a busy-poll loop). Examples 01/02 measured: serial `imul` chain vs 4 independent chains **~4×** (ILP) | 06, 31 |
| [✓] | Branch prediction aur misprediction cost — **folder 31 file 07 + examples 03/04**: gshare/TAGE, BTB/RAS, indirect/virtual dispatch, `[[likely]]`/`[[unlikely]]`, predictor warm-up. **Measured ~6–7×**: unpredictable branch (RANDOM ~4–8 ns/elem) vs predictable (SORTED ~0.5–1.4 ns/elem). Rule 2: default `-O2` if-converts the `if` to a branchless `cmovge` → no effect visible; the example disables if-conversion to expose the raw cost, and "the compiler already made it branchless" is the taught lesson | 06, 31 |
| [✓] | SIMD / auto-vectorization — **folder 31 files 10–11 + examples 05–06**: SSE2→AVX→AVX2+FMA→AVX-512, what SIMD is good/bad at, SoA prerequisite, alignment/tail-handling, `_mm256_*` naming + core patterns (load/store/arith/cmp→mask/blend/movemask/shuffle/hreduce), `target("avx2")` + CPUID dispatch, Highway/xsimd, AVX-512 downclock. **Measured: scalar → SSE 4.1× → AVX2 ~9.5×** (float sum, L2-resident; converges to ~1× when bandwidth-bound); **same-code auto-vectorized ~2.5–3×** | 07, 31 |
| [✓] | Hyperthreading (aur HFT mein kyun disable) — **folder 31 file 12**: SMT shares execution ports / L1 / µop cache / TLB / branch predictor, and halves the OoO window (ROB/RS/PRF) → a hot thread's per-iteration time depends on the uncontrolled sibling = jitter, fat p99. HFT disables SMT (BIOS / `nosmt` / `smt/control`) *or* keeps it on with every hot core's sibling isolated and idle; non-latency work (research/backtest) keeps SMT for the ~1.1–1.3× throughput. Size compute pools to *physical* cores. Also closes cross-sibling side channels (file 08) | 31 |
| [✓] | Frequency scaling, C-states, turbo — **folder 31 file 13**: P-states/turbo (opportunistic → non-deterministic → some HFT disable turbo for a *guaranteed* base frequency), C-states (deep-idle wake = +tens–100 µs latency + cold cache → busy-poll or `max_cstate=1` on latency cores), thermal throttling (silently defeats a locked frequency — monitor `PkgTmp`), AVX/AVX-512 frequency offset (can make a 4× SIMD loop a net loss on Skylake-X/CLX), uncore frequency (pin high). "Lock the frequency; ns/cycle must be a constant." Example 08 calibrates this box's ~2 GHz effective | 31 |
| [✓] | Cache hierarchy aur latency numbers — **folder 32 files 01–04**: registers→L1→L2→L3→DRAM→disk ladder + this box's geometry, the "1 second = L1" analogy, a `load`'s full path, set-associative organization (index/tag/offset, ways, **critical stride** = size/assoc, power-of-two-dims poison, VIPT), the 3 C's + coherence + a symptom→cause table, **MLP** (which misses OoO hides, which it can't). Measured: stride ramp (ex 01), latency cliff ~1.2 ns (L1) → ~15 ns (4 MiB) → ~95 ns (8 MiB+, DRAM) (ex 08) | 07, 32 |
| [✓] | Cache lines aur false sharing — 64B line, struct density, `alignas(64)` (07, 09, 11); **folder 32 files 02, 07**: line = unit of transfer/coherence, why 64, line straddle, `hardware_destructive_interference_size`, MESI ping-pong deep, `perf c2c`, constructive sharing. **Measured ~6× to ~44×, run-to-run** (32 `04`: padded ~0.2 ns/inc stable vs packed swinging with core placement — *false sharing is jittery, not just slow*), plus ~10× (26 `08`) and ~3.5–4× (28 `07`); **padding alone doesn't fix an SPSC ring** if the opposite index is reloaded every op → cache the opposite index (28 `02` V2); "accumulate locally + combine" beats perfect padding | 07, 09, 11, 26, 28, 32 |
| [✓] | Prefetching — **folder 32 file 06 + example 06**: HW prefetchers (next-line / adjacent / stride / region) + their limits (per-page, monotonic-only), `__builtin_prefetch(addr, rw, locality)` + distance tuning, `PREFETCHNTA`. ⚠️ **Measured Rule 2**: SW prefetch on an independent gather is **~1.1× (marginal — MLP already overlaps ~10 misses)** and on a memory-saturated heavy loop **~0.33× = 3× SLOWER** (prefetch requests contend for LFBs/bandwidth). "A scalpel, not free money" — wins on in-order/narrow cores or a verified MLP deficit | 32 |
| [✓] | TLB aur huge pages — **folder 32 file 11 + example 08**: 4-level page walk (up to 4 dependent mem accesses), L1 dTLB / L2 STLB reach math (4 KiB → ~256 KiB / ~6 MiB), 2 MiB huge pages → 512× reach + shorter walk, explicit hugetlbfs (`MAP_HUGETLB`) vs THP downsides (`khugepaged` jitter → HFT uses explicit + pre-fault + `mlockall`), Windows large pages, NUMA first-touch. Measured latency cliff at ~8 MiB (STLB + L3 coincide with 4 KiB pages — honest limitation stated in-file) | 07, 29, 32 |
| [✓] | Data-oriented design, AoS vs SoA — **folder 32 files 09–10 + example 05**: the three access patterns → three winners (scan-few-fields **SoA ~2.0×**, scan-most-fields **SoA ~2.4×** — gap doesn't shrink, SoA vectorizes; random whole-record **AoS ~3×**), AoSoA hybrid; DOD philosophy (`vector<Base*>`+`virtual` hot-loop cost breakdown, existence-based processing, handles + generation counters, "group by what you do, not what things are", where OOP still fits). Plus 09/11 measured (~1.6–4×) | 07, 09, 11, 32 |
| [✓] | Compiler optimizations aur inlining — **full folder 33** (15 lessons): `-O` levels (`-O0→-Ofast`, the `-O0→-O1` ~6× cliff measured, per-file override, `-g` on release), godbolt/`-fopt-info-*`/`objdump`/`llvm-mca`, **inlining** (`inline`=ODR not a directive; heuristics; `always_inline`/`noinline`/`flatten`; the cross-TU/no-LTO wall; inlining as the *enabler* of const-fold/CSE/vectorize/devirt; noinline **1.31 ns/iter** vs inlined ~1.0), **loop opts** (LICM/strength-reduction/unroll+"one accumulator" trap/fusion/fission/interchange-`-O3`/unswitch/rotation), **auto-vec** (conditions; **float reduction won't vectorize at `-O2`** — reassociation — **~4× only with `-ffast-math`**, plain map **~3.5×**; `-fopt-info-vec[-missed]`), **const-fold/prop/DCE** (whole functions vanish; `constexpr`/`consteval`/`if constexpr`/`[[assume]]`), **devirtualization** (exact-type/`final`/speculative-LTO/PGO; the honest "won't fire for a heterogeneous container"), **branch hints** (= *layout* not prediction; **~1.3×** measured; wrong hint = regression), **aliasing/`__restrict`** (blocks LICM/CSE/vectorize; TBAA UB; `bit_cast`; **~3.5×** ex 04 — pessimism *only* with `[[gnu::noinline]]`), **LTO** (cross-TU inline/devirt/DCE; ThinLTO; ODR-exposure; **~2.3×** on a throughput loop, *none* on a carried one), **PGO** (3-step + AutoFDO; HFT = replayed session), **`-march`/`-mtune`** (v1..v4; `native` wrong for shipping; AVX-512 downclock; multi-versioning), **`-ffast-math` dangers** (`-ffinite-math-only` deletes NaN guards; safe scoped alternatives; **never on priced/audited code**), **benchmark barriers** (`DoNotOptimize`/`ClobberMemory` = zero instructions), **reading optimized output** (verification checklist). Plus 08 (inline vs call ~6×), 24/15 (LTO+PGO release config). Rule 2 three times, taught | 07, 08, 24, 31, 32, 33 |
| [✓] | Reading generated assembly — **full folder 34** (13 lessons): why read (verification/optimization/debugging), x86-64 registers + sub-register zeroing + `xmm`/`ymm`/`zmm`, **AT&T vs Intel** (5 differences, which tool gives which), the ~20 common instructions (`lea` ≠ `mov [..]`; `cmp`/`test`; recognize `idiv`/`rep movs`/`lock`), addressing modes (recover `sizeof`/field offsets from a loop), **pattern recognition** (`if`/`cmov`/loop/`while`/`switch` dense & sparse/call/virtual `call [reg+off]`/constant-fold/reciprocal-multiply division), **SIMD asm** (scalar vs packed suffix, width from `add ptr,16/32`, horizontal-reduce cluster, `vzeroupper`, `vgather`), **`rdtsc` timing** (fencing, ticks≠cycles≠ns, core hopping — measured calibration ~2.0 ticks/ns, plain ~1 tick, fenced ~20), **inline asm** (4 sections, missing-clobber = `-O2` corruption, when to use vs intrinsics), **disassembly tools** (`-S` / `objdump -dS` / `perf annotate` / `gdb` / `addr2line` / `llvm-mca` — which for which question). 10 "which C++ made this asm?" puzzles. Plus 06/08/21/22 asm touches | 06, 08, 21, 22, 33, 34 |
| [✓] | Call stack / ABI — frames, prologue/epilogue ± frame pointer, spills = register pressure, tail call → `jmp`, **System V vs Windows x64** (arg registers, `this`, large-struct return, `extern "C"`, varargs `al`), Win64 shadow space, stack growth/overflow (folder 34 lessons 06–07; folder 08; folder 25 object-model side) | 08, 25, 34 |
| [✓] | `constexpr` / compile-time computation — dual-use functions, `consteval`, compile-time tables + UB detection (08); **full treatment in folder 21** (`07-if-constexpr.md`, `13-template-metaprogramming.md` — TMP via `constexpr`, compile-time `factorial`/`fib`, `constexpr` parsing) + `constinit` / static-init (24, 25) | 08, 21 |
| [✓] | `perf` profiling — **full folder 35** (lessons 10–14): `perf stat` (IPC, cache-miss rates, branch-miss, **top-down** Retiring/Frontend/Backend/Bad-Spec with fix directions, `perf list`) · `perf record`/`report` (`-g` frame-pointer vs dwarf vs lbr, self vs children) · **`perf annotate`** per-instruction % + **skid → `:pp` (PEBS/IBS)** · `cycle_activity.stalls_l3_miss` to quantify memory-bound · **`perf mem`** (per-access latency + served-from) · **`perf c2c`** (false sharing / HITM) · counter multiplexing · **flame graphs** (`stackcollapse`→`flamegraph.pl`, on- vs off-CPU, differential) · **Valgrind** (cachegrind `Ir` CI gate, callgrind + KCachegrind, massif, DHAT) · **VTune** recursive top-down + `toplev.py` + AMD uProf / Instruments. `06_perf_workflow.sh` + `07_flamegraph.sh` = self-contained Linux workflows (planted `%`-divider + cache-miss hotspots) | 32, 33, 35 |
| [✓] | Percentiles, jitter, tail latency — **full folder 35** (lessons 04–07, 16): mean-lies-on-skewed-latency / median / min / σ-useless + CV/MAD / **bimodal = two code paths** (35/04); **p50/p90/p99/p99.9/p99.99** nearest-rank + "nines" + **fan-out tail amplification** ("the tail at scale") + **coordinated omission** + HdrHistogram interval-correction + percentiles-don't-average (35/05); **jitter** — SW (timer/scheduler/IRQ/page-fault/`malloc`/syscall/lock) + HW/firmware (freq/C-states/**SMI**/SMT/NUMA/thermal), `cyclictest`/`rtla`/`hwlatdetect`, the **quiet-core recipe** (BIOS + `isolcpus`+`nohz_full`+`rcu_nocbs` + pin/`mlockall`/no-alloc) (35/06); **histograms** (linear buckets fail, log-linear / HdrHistogram, CDF plot, `08_latency_recorder.cpp` ~2 ns/record ≤1.3% error) (35/07); production always-on measurement (35/16); measured allocation tail (14/08). Measured: identical work **min 200 ns** but spike-count 2× under noise; mean ≈ median yet **max/median 60–175×** | 14, 35, 36 |
| [✓] | Correct benchmarking methodology — **full folder 35** (lessons 01–04, 08–09): `-O2` req; "kill the optimizer" (`DoNotOptimize`/`ClobberMemory` = zero instructions — 33/14 + 35/09, measured **no-barrier loop = 0.000 ns/op**); **every pitfall demoed** (DCE / const-fold / loop-invariant hoist / cold-start-4-components / timer-overhead>op / one-run / **alignment & code-layout noise** — trust only > noise / **frequency scaling** — report cycles/op / **denormals** — FTZ/DAZ / bench≠reality — `04_benchmark_mistakes.cpp` BUG/FIX pairs); warm-up + min-of-N + distribution; **Amdahl + premature-opt's real meaning + throughput vs latency** (35/01); **`rdtsc` fencing + ticks→ns calibration (busy-wait not sleep) + self-cost subtraction + core-pin** (34/11 + 35/03 — ~2.0 ticks/ns); `<chrono>` discipline (steady vs system, resolution, self-cost — 19/17 + 35/02); Google Benchmark (`State` loop, args, fixtures, `PauseTiming`, `_cv` gate — 35/08 + shim); **surprising-result honesty carried through** — median-of-3 bug (20 `01`), ranges pipeline *faster* (22 `03`), folder 32 (`-O3` interchange / prefetch 3× *slower*), folder 33 (float reduction / aliasing / LTO), **folder 35 (BUG benchmarks really report 0.000 ns/op; cold-start only 1.2× on Windows; tail numbers run-to-run unstable on an unpinned box)** — all taught; LTO/PGO validation = P50/P99 on a replayed capture (24/15) | 06, 07, 13, 14, 19–24, 31–35 |

---

## D. LOW-LATENCY C++ TECHNIQUES

| Status | Topic | Folder |
|---|---|---|
| [✓] | Latency vs throughput vs jitter — **folder 35 lessons 01, 06**: the throughput-vs-latency table (metric, batching effect, who cares), "your HFT metric is always a latency percentile", jitter as a *separate axis* from raw speed ("a path that always takes 800 ns beats one that's usually 500 but sometimes 40 µs"), determinism > speed, whole-stack p99.9; throughput vs tail-latency in the allocator context (14 files 08/10); engineering *posture* in 36 | 14, 35, 36 |
| [✓] | p50 / p99 / p99.9 / p99.99 — measured allocation distribution + "average lies" (14 file 08); full: folder 35 (05) + folder 36 (01–02) — p99.9 is the hot-path scorecard, budget thinking | 14, 35, 36 |
| [✓] | Allocation avoidance aur preallocation — hot-path zero-alloc rule, pre-alloc + first-touch, `reserve`, measured (14 files 08/10); PMR stack-buffer container → 0 global `new` + `null_memory_resource` assertion (19 `10_pmr_demo`); "reserve everything at startup" as design principle (19 file 26); **full: folder 36 lessons 04–05** — measured `new` p99.9 2585 ns (mixed churn); the hidden allocations catalogue; warm-up vs steady state; *proving* zero-alloc (`null_memory_resource` / `new` hook / `perf page-faults`) | 14, 19, 36 |
| [✓] | Memory pools (fixed-size) — free-list pool + placement new, O(1), measured ~190x + flat tail (14 file 10, example 07); PMR `pool_resource` + Allocator-concept pool (19 files 23–24); **full: folder 36 lesson 06 + `02_memory_pool.cpp`** — `FixedPool` built (intrusive free list, O(1)), p99.9 30 ns flat vs `new` 180; cross-thread 'goes home to be freed'; double-free detection. HFT BUILD in 44 | 14, 19, 36, 44 |
| [✓] | Object pools aur object reuse — "return to pool, free hote hi nahi", generation-tag idea (14 files 06/10); slab + free-list-bitmask working-order pool sketch (19 files 26–27); **full: folder 36 lesson 07 + `03_object_pool.cpp`** — construct-on-acquire (p50 30 ns) vs recycle-pre-constructed (p50 20 ns; danger = stale fields); `std::launder`. HFT BUILD in 44 | 14, 19, 36, 44 |
| [✓] | Arena / bump allocators — `alloc`(align+bump)/`reset` O(1), per-event scratch pattern (14 file 10); `ArenaAllocator` + `monotonic_buffer_resource` + `release()` per event (19 files 23–24, examples 09–10); **full: folder 36 lesson 08 + `04`/`05`** — `alloc`=align+bump, `reset`=one store; ~16× vs new/delete; `monotonic_buffer_resource` + `null_memory_resource` tripwire; overflow policy; escaped-pointer = UAF | 14, 19, 36 |
| [✓] | Ring buffers — **lock-free SPSC ring built, optimized, and benchmarked** (28 files 04–05, examples 01–02: release/acquire index hand-off, no CAS, wait-free in practice, no ABA; naive ~52 ns/msg → cached-index ~16–20 ns/msg; power-of-two mask, cache-line padding, cached opposite index, batching, zero-copy slot API); **Vyukov bounded MPMC ring** (28 file 06); back-pressure policy (drop / overwrite / spin); design + "the queue in HFT" (20 file 07); polished HFT **BUILD** in 41/44 | 19, 20, 28, 36, 41 |
| [✓] | Cache locality tuning — measured row/column ~8x + AoS/SoA intro in 07; **cache-aware DSA** synthesis + measured (list vs vector ~40x, flat vs pointer tree 4–11x, `std::map` vs sorted-vec ~3x, "performance ≈ cache misses") in 20 files 06/08/17; **folder 32 files 05/08/15**: loop interchange (~10× measured) / fusion / fission / tiling, prefetcher-friendly vs hostile patterns, flat vs pointer-based structures, sequential vs random line access ~7× (ex 02), and **11 optimization recipes in impact order each with its trade-off** + the apply-one-remeasure-explain discipline; hot-path synthesis in 36 | 07, 20, 32, 36 |
| [✓] | False sharing elimination — measured **~10×** (26 `08`: packed vs `alignas(64)` counters) and **~3.5–4×** (28 `07`: pure false sharing on an SPSC index pair); fixes: `alignas(64)` field + trailing pad, `std::hardware_destructive_interference_size` (+ ABI gotcha → hardcode 64/128), accumulate-locally-then-combine; **the key nuance** — padding `head_`/`tail_` apart does *not* speed up an SPSC ring if the opposite index is reloaded every op; you must **cache the opposite index** (28 `02` V2); `perf c2c` in 32 | 26, 28, 32 |
| [✓] | Branch-free / branchless code — intro + benchmark (06); **branchless binary search** + `[[likely]]`/`[[unlikely]]` (20 file 05, 22 file 12); ranges masked-SIMD vs branchy hand loop (22 `03`); **folder 31 files 04/07/08 + examples 03/04**: measured — unpredictable branch **~6–7×** slower (ex 03), branchless **~6–7×** faster on random data but **~1.2× *slower*** on predictable data (ex 04 — the honest "when NOT branchless" result), `x & -(cond)` masking, `cmov` (compiler does it for simple predicates at `-O2`), SIMD predication, "predictable-by-construction data > branchless"; hot-path posture in 36 | 06, 20, 22, 31, 36 |
| [✓] | Virtual dispatch elimination — **measured** across 3 examples: virtual ~23 ns vs direct/CRTP ~2.2 ns vs `variant` ~16 ns (16); CRTP ~0.56 vs virtual (real boundary) ~2.43 ns, virtual ~2.51 vs template ~1.14 vs `variant`+`visit` ~1.13 ns (21); CRTP / `std::variant`+`visit` / tag-`switch` / templates; **devirtualization caveat** (visible type → virtual == CRTP); full in 36 | 16, 21, 36 |
| [✓] | `std::function` cost aur alternatives — **measured** (19 `08_std_function_cost`): templated/fn-ptr ~1.48 ns/call (inlined) vs `std::function` ~5.11 ns (type-erased, no inline) + **1 heap allocation** for a `std::string` closure; alternatives — template the callable, function pointer + `void* ctx`, `variant`+`visit`, `function_ref` (19 file 16); hot-path application in 36 | 19, 36 |
| [✓] | Lock contention avoidance — the contended-mutex cost model (futex syscall + 2 context switches + preemption stall = P99 spike), lock-free SPSC/MPSC queues + seqlock snapshots as the replacements, **sharding** (K low-contention shards beats one clever lock-free structure), when a mutex is still right (28 files 01, 06, 14); **folder 36 lessons 03/11/17/24** = the hot-path posture (no locks / no syscalls on the hot path; false sharing = a jitter source; per-thread + combine); measured lock-free replacements in 28; HFT thread-map in 41 | 28, 36, 41 |
| [✓] | Busy-spin vs blocking — measured: busy-poll SPSC ring p50 ~0.4 µs vs `mutex+cv` p50 ~6 µs (a futex wake = syscall + context switch per message); cv wakes give the cleanest tail but the worst median; the busy-poll consumer trades a core for latency (28 file 06); **folder 36 lesson 17** = busy-poll (`recv(MSG_DONTWAIT)` + `_mm_pause`) vs blocking, its 100%-CPU-per-core cost, `SO_BUSY_POLL`, `io_uring`+`SQPOLL` (zero syscalls), when to move the syscall to a housekeeping thread; spin-vs-condvar measured in 28; HFT policy in 41 | 28, 36, 41 |
| [✓] | CPU pinning strategy — the canonical 2-socket layout (hot threads one per isolated physical core, siblings idle, housekeeping cores for OS/IRQ/logging), `isolcpus`+`nohz_full`+`rcu_nocbs`+`irqaffinity`, per-thread pinning within the pool, startup self-check (assert core isolated + sibling free), measured tail-tightening (29 `06`); **full: folder 36 lesson 19** — a concrete core plan (NIC-poll/decode/strategy/risk/OS), `isolcpus`+`nohz_full`+`rcu_nocbs`+IRQ affinity, SMT sibling idle, NUMA first-touch, the `SCHED_FIFO` hang hazard, verification (`cpu-migrations`~0); measured tail-tightening in 29/06; HFT thread-map in 41 | 29, 41 |
| [✓] | NUMA-aware allocation — first-touch placement, per-node arenas + `mbind`/`numa_alloc_onnode`, parallel first-touch by the owning thread, `numa_balancing=0`, keep feed→strategy→gateway + NIC + memory on one node (29 file 14); **folder 36 lesson 19** (NUMA placement — NIC+threads+memory on one node, first-touch from the pinned owning thread, `numa_balancing=0`, interleave is for bandwidth not latency); deep treatment in 29/14 | 29, 36 |
| [✓] | Syscall avoidance aur batching — the ~300 ns trap cost, vDSO for time, `recvmmsg`/`sendmmsg` (one syscall N packets), `io_uring` SQ/CQ + SQPOLL (zero syscalls), buffering, `mmap`/shm hand-off, busy-poll instead of blocking, kernel bypass as the endgame (29 file 02, 30 files 05/08/12/13); **full: folder 36 lessons 16–17** — the batching throughput/head-of-line curve (`10_batching.cpp`: B=1024 HoL 3095 ns; opportunistic batching), syscall cost (100–300×), busy-poll, `io_uring` SQ/CQ + `SQPOLL` + registered buffers, kernel bypass, `strace -c`/`perf trace` counting | 29, 30, 36 |
| [✓] | Page fault avoidance — first-touch cost, pre-touch pre-allocated memory at startup (14 files 01/08); **full: folder 36 lesson 18 + `11_page_fault_warmup.cpp`** — COLD p99 2875 ns vs WARM 30 ns; `MAP_POPULATE` / `mlockall` / write-touch every page / stack pre-fault / TLS / library lazy-init; huge pages & THP jitter; verify `perf stat -e page-faults`. Foundations in 29 | 14, 29, 36 |
| [✓] | Instruction cache locality, hot/cold splitting — **folder 32 files 02/15**: hot/cold field splitting recipe (few hot fields in the scanned array, cold fields in a parallel array — 1.5–3×), "hot mutable fields of different threads must not share a line", huge-page `.text` remap for a large hot code segment (file 11); **full: folder 36 lesson 21 + `12_hot_cold_split.cpp`** — Frontend Bound, `[[gnu::cold]]`+`[[unlikely]]`+`.text.unlikely`, PGO/LTO/BOLT function ordering, why `always_inline` everything *hurts*. ⚠️ Rule 2: `12` measured **no difference** in a micro-bench (hot loop fit L1i) — kept honest. Venue-specific in 43 | 32, 36, 43 |
| [✓] | Zero-copy patterns — **folder 36 lesson 22**: `string_view` / `span` / plain `{ptr,len}` (non-owning, the lifetime rule); **in-place parsing** of a binary protocol (overlay a `packed` struct) with the 4 caveats (alignment / endianness / lifetime / aliasing); `std::from_chars` / `std::to_chars` for alloc-free / locale-free / exception-free number↔text; `iovec`/`writev`, `sendfile`/`splice`, `MSG_ZEROCOPY`; when you *must* copy — once, minimal, prefer a number over a string. Venue/NIC detail in 42 | 30, 36, 42 |
| [✓] | Cache warming — **folder 32 files 01/05/11**: startup priming (touch the hot data structures + handlers = page-fault + cache-warm so the first real tick isn't cold), pre-fault + `mlockall` with huge pages, "market-open first tick is cold — warm-up loop / dummy pass over the book"; **full: folder 36 lesson 20** — cold start vs cache decay during quiet periods; dry-run the real hot path with synthetic data (the `dry_run` flag must be `[[likely]]`/branchless, side-effect-proof, representative); keep the core busy (no C-state); move housekeeping off the trading core. Foundations in 32 | 32, 36 |
| [✓] | Trade-off discussion (kab NOT to optimize) — **folder 32 file 15 + examples 06/07**: every one of the 11 recipes documented with its trade-off (CLAUDE.md rule 13); explicit "when NOT" for prefetch (marginal/harmful on a big OoO core), blocking (loses to a good loop order without a tuned microkernel), branchless (~1.2× *slower* on predictable data), SoA (worse for random whole-record access); "measure first, apply one fix, re-measure, keep only what moved the number"; "algorithm/passes before cache tuning"; **full: folder 36 lesson 24** — a hidden-cost table for every technique in the folder; **six explicit "when NOT to"** (not on the hot path / budget already met / cold path / throughput not latency / not measurably faster / can't maintain it / correctness at risk); the measure→profile→one-change→re-measure→explain process; documenting a floor you can't beat. Plus 32/15's 11-recipes-with-trade-offs | 32, 36 |

---

## E. HFT DOMAIN KNOWLEDGE

| Status | Topic | Folder |
|---|---|---|
| [✓] | HFT kya hai, business model — myths vs reality table (front-running/guaranteed-profit/manipulation misconceptions corrected), HFT vs algo-trading vs quant-investing matrix, who does it (37/01) | 37 |
| [✓] | Exchange architecture — order gateway (auth/validate/sequence) → matching engine (single-threaded-per-symbol, why) → market-data-out (public) vs trade-confirm (private, alag paths/races) (37/02) | 37 |
| [✓] | Market microstructure — liquidity vs volume, price discovery as emergent, informed vs uninformed flow, **adverse selection** (the fundamental market-making risk) worked example (37/03) | 37 |
| [✓] | Order types (market/limit/IOC/FOK/stop/iceberg/post-only) — exact fill behavior per type with book diagrams, IOC-vs-FOK for multi-leg arb, iceberg time-priority trade-off (37/04) | 37 |
| [✓] | Bid/ask/spread/depth — **measured**: bps normalization (why absolute spread misleads across price levels), **microprice** (size-weighted mid, formula + intuition), order imbalance (`examples/02_spread_calculator.cpp`) (37/05) | 37 |
| [✓] | Price-time priority — **measured FIFO vs pro-rata side-by-side** (same 4-order book, same incoming order, different fills per rule) + which rule makes speed matter more/less (`examples/04_matching_rules.cpp`) (37/07, 39) | 37, 39 |
| [✓] | Market making vs taking — resting-vs-crossing (not buy/sell), maker-taker fee/rebate model + why it can decide a spread-capture trade's sign, designated-market-maker obligation/benefit trade-off (37/09) | 37 |
| [✓] | Latency/statistical arbitrage — cross-venue + latency-arb concepts, FOK-for-atomicity reasoning, why FIFO venues make this category's speed-competition extreme (37/10) | 37 |
| [✓] | Co-location aur proximity hosting — speed-of-light propagation floor (why code can't fix distance), cross-connects + cable-length equalization, fair-access mechanisms, colocation ≠ the only layer (37/11) | 37 |
| [✓] | Tick size, lot size, price bands — why price is stored as integer ticks not `double` (connects to 03-VARIABLES float-equality trap), pre-trade validation as a "catch it before the exchange rejects it" pattern (37/08) | 37 |
| [✓] | HFT system architecture (end-to-end) — full pipeline diagram (feed handler → order book → strategy → risk → OMS → gateway), market-data-path vs order-path as two concurrent flows, mapped box-by-box to folders 38–44 (37/12) | 37 |
| [✓] | Risk systems — pre-trade check table, fat-finger (relative not absolute anomaly detection), kill switch, **fail-closed as the default** (37/13) | 37, 44 |
| [✓] | Strategy basics — market making / stat-arb / event-driven categories, each one's latency-critical path, explicitly **concepts only, no alpha/signal research** (37/10) | 37, 44 |
| [✓] | Regulatory basics — algo-ID tagging (why useful to the firm itself, not just regulators), audit trail as a hot-path *engineering* problem (async/ring-buffered, not "free"), India (SEBI) vs US (SEC/FINRA) framing (37/16) | 37 |

---

## F. MARKET DATA

| Status | Topic | Folder |
|---|---|---|
| [✓] | L1 / L2 / L3 data — top-of-book vs aggregated-per-level vs individual-order; L3⊃L2⊃L1 (derivable one-way only); why HFT needs L3 for time-priority (38/02) | 38 |
| [✓] | Snapshots vs incremental updates — incremental is bandwidth-efficient but NOT self-sufficient; join-late/miss-one-update = silent permanent book drift (worked example); snapshot+incremental join protocol (38/03) | 38 |
| [✓] | Sequence numbers aur gap detection — `seq==expected`/`>`/`<` cases, **measured**: 96/20000 gaps detected, sanity-verified exact match against actual drops (`08_gap_detection.cpp`) (38/04) | 38 |
| [✓] | ITCH protocol — teaching-purpose ITCH-style protocol built: 16-byte header + 5 message types (Add/Execute/Cancel/Delete/Replace), two-phase header-then-body parsing, honest disclosure vs real Nasdaq ITCH spec (38/06) | 38 |
| [✓] | FIX / FAST — text (FIX) still dominant in order-entry (low volume, debuggable), why market data moved to binary; FAST as FIX's binary/templated compression, when it's chosen (38/07) | 38 |
| [✓] | SBE — schema-first codegen vs hand-written; **why SBE picks native-endian is NOT about swap-cost (measured 0.753 ns/swap, negligible) but codegen-simplicity + correctness** — a Rule-2-style honest correction of the naive assumption (38/08) | 38 |
| [✓] | Binary / zero-copy parsing — `std::bit_cast` a POD out of a byte buffer (aliasing-safe), `std::span<const std::byte>` slices, `from_chars`, "hand-written fixed-field parsing beats regex 10–100x" (19 files 03, 20, 22); **full protocol built + measured: naive owned-copy-into-vector vs overlay-read-into-fixed-sink, p99.9 ratio 22.2× (771.5 vs 40.1 ns), p50 unchanged** (38/09, examples 03-06) | 19, 38 |
| [✓] | Endianness aur wire formats — `std::endian::native`, `__builtin_bswap*` (pre-C++23 `std::byteswap`), big-endian wire ↔ little-endian host at the parse boundary (19 file 22); **measured byteswap cost 0.753 ns/swap; garbled-without-swap proof; alignment-safe `memcpy` read demoed** (38/10, example 02) | 19, 38 |
| [✓] | A/B feed arbitration — concept + algorithm covered (30 file 06): two multicast groups over separate paths, one sequencer, first copy of each sequence wins; **measured: two independently-lossy (~1% each) copies of one feed, arbitrated → ~0.02% loss, ~49.5× improvement, zero round-trips** (38/12, example 09) | 30, 38 |
| [✓] | Recovery aur retransmission — the model covered (30 file 05): track `last_in_order_seq`, don't block on a gap, recover via snapshot feed / retransmit-request (TCP) **off the hot loop**, dedup `seq <= last`, go-stale/flat on an unfillable gap; **3-layer recovery ladder (A/B → retransmit → snapshot resync) + fail-closed "go stale" state machine** (38/13) | 30, 38 |
| [✓] | Timestamping aur clock sync — `SO_TIMESTAMPING` HW/SW RX+TX, cmsg parse, TX stamps on the error queue, clock domains (discipline or jitter-only), PTP (`ptp4l` → PHC, `phc2sys` → system clock), boundary/transparent-clock switches, MiFID II (30 file 14); **exchange-ts vs receive-ts, clock skew (negative-latency diagnostic), why `seq_num` beats timestamp for ordering** (38/14); deep NIC/PTP ops in 42 | 30, 38, 42 |
| [✓] | Conflation — which data is safe to conflate (L2 display, "latest state matters") vs never (trades, own-order fills, L3 for strategy signals); conflation vs explicit drop-and-mark-stale vs silent-fall-behind (38/15) | 38 |
| [✓] | **BUILD:** market data simulator — deterministic (seeded) synthetic feed generator, realistic order lifecycle (Add→Exec/Cancel/Delete/Replace against live orders), 50k-message run: mix 55.2/17.9/11.3/9.0/6.6%, inter-message gap stats, byte-identical-rerun proof (`07_market_data_simulator.cpp`) | 38, 44 |
| [✓] | **BUILD:** feed handler + parser — full pipeline (framing+gap-detection+zero-copy dispatch+fixed book state) as one class; **end-to-end measured p50 30.1 / p99.9 40.1 ns — matches the standalone zero-copy parser exactly, proving framing+gap-check added zero extra tail** (`10_feed_handler.cpp`, 38/16 capstone) | 38, 44 |

---

## G. ORDER BOOK

| Status | Topic | Folder |
|---|---|---|
| [✓] | Order book data structure design space — **3 full order books built and measured on an identical operation workload**: `std::map`+`std::list`+`unordered_map` (V1) vs sorted `std::vector`+`std::deque` (V2) vs tick-indexed flat `std::array`+intrusive-arena-list+custom flat hash (V3); all three proven behavior-identical (cross-version equivalence + fuzz vs a reference model). Earlier trade-off analysis in 19/20 was general-container-level; 39 is the applied, order-book-specific, end-to-end build | 19, 20, 39 |
| [✓] | Naive `std::map` implementation — built + measured: ALL-ops p50 140.3 / p99.9 1502.9 ns; RB-tree-node + list-node per-order heap allocation is the structural bottleneck; `operator[]` cheap for existing levels, allocates+rebalances for new ones (39/03-04) | 19, 20, 39 |
| [✓] | Sorted vector implementation — built + measured: **a Rule-2 finding — OVERALL WORSE than `std::map`** (p50 150.3 vs 140.3, p99.9 2745.2 vs 1502.9 ns), despite Add improving 3.9× (contiguous array beats tree). Root cause found and explained: the order-id index can no longer store a stable iterator (vector reallocation invalidates it), so it stores only `{side,price}`, forcing an `O(level size)` linear scan on every cancel/execute — a regression V1 didn't have. Earlier "~3x faster than map" (19/20) was a simpler, non-order-book benchmark; this is the fuller, honest picture for the actual workload (39/05-06) | 19, 20, 39 |
| [✓] | Flat array-of-price-levels — built + measured: price→index is O(1) arithmetic, no search, no per-level allocation; bounded range (`NUM_LEVELS`) is an explicit, documented trade-off with a return-value contract (silent reject if out of range) (39/07) | 19, 20, 39 |
| [✓] | Intrusive lists for order queues — built: arena-backed (`std::vector<OrderSlot>`, pre-allocated once) + `uint32_t` index links (not pointers — resize-safe, half the size) + a free-list allocator (zero `new`/`delete` on the hot path); this is what actually fixes V2's regression (39/08) | 19, 20, 39 |
| [✓] | Order ID lookup (hash vs slab) — built: a tombstone-based open-addressed flat hash (`FlatIdIndex`) → arena slot, O(1) direct access, zero per-insert heap allocation (vs `unordered_map`'s per-node chaining allocation in V1/V2); tombstones-vs-`EMPTY` deletion trap explained (why marking `EMPTY` breaks probe chains) (39/09) | 19, 20, 39 |
| [✓] | Add / cancel / modify / execute — all 4 implemented consistently across V1/V2/V3 (identical public interface); Execute and Cancel are mechanically the same operation (`reduce`); Replace = remove+add composition (correct-by-construction, not a custom fast-path) (39/12) | 39 |
| [✓] | Price-time priority implementation — FIFO comes from each container's natural append-order (no explicit sort); proven via an `ids_at_price()` accessor returning exact arrival order, identical across all 3 versions; partial-reduce preserves position, Replace does not (new order_id = new, last position) (39/10) | 39 |
| [✓] | Top-of-book fast path — V1/V2 get O(1) `begin()`/`front()` for free (container invariant); V3 needs **explicit** cached `best_bid_idx_`/`best_ask_idx_`, honestly characterized as "typical O(1), worst-case O(NUM_LEVELS) scan on best-level-empties" — not oversold as guaranteed O(1). Includes a real bug story: calling `best_bid()` without checking `has_bid()` first crashed on an out-of-bounds array read during test-writing itself (39/11) | 39 |
| [✓] | **Benchmark: naive vs optimized** — full 3-way comparison suite (`07_comparison_suite.cpp`), same workload same run: **V3 wins every metric** — ALL-ops p99.9 V1 1502.9 / V2 2745.2 / V3 661.3 ns (V3 ~2-4× better than both); Add p99.9 V1 10700.3 / V2 2755.2 / V3 531.0 ns; the full "what changed and why" mechanism-level accounting in 39/15; tested via 58 scripted unit tests + cross-version equivalence (20000+ checkpoints) + a 30000-op fuzz run (~3000 injected edge cases) against an independent reference model, **zero disagreements** | 39, 43 |

---

## H. MATCHING ENGINE

| Status | Topic | Folder |
|---|---|---|
| [✓] | Matching algorithm — price-time priority, two nested loops (best-to-worst crossable levels, then FIFO within a level); built + traced (`matching_engine.hpp`'s `match_against()`), multi-level sweep proven with a 3-trade example (40/02) | 40 |
| [✓] | Limit orders — resting vs aggressing, a single order can be both (partial fill + partial rest) in one `submit()` call; leftover rests on its OWN side (not the opposite side it just crossed) — a real finding from writing the first demo (40/03) | 40 |
| [✓] | Market orders — no price limit, sweeps until qty==0 or book empties; unfilled remainder explicitly documented design choice (voided, never rests — "IOC without a price limit") (40/04) | 40 |
| [✓] | Partial fills — `std::min(incoming.qty, resting.qty)`, both sides decremented; a partially-filled resting order KEEPS its original FIFO position (not re-queued to the back) (40/05) | 40 |
| [✓] | IOC / FOK — IOC voids remainder like Market but is price-limited; FOK requires a pre-match precheck (must decide before committing any trade). **Central correctness finding:** a naive FOK precheck ("sum total resting qty") is provably wrong once combined with self-trade prevention — STP can skip/abort on self-owned liquidity that the naive sum still counted, silently producing a partial fill on an order that promised all-or-nothing. Fixed with an STP-mode-aware precheck that walks the same priority order the real match uses; verified by a hand-crafted test AND a 30000-cmd fuzz run (3774 FOK orders hit, zero violations) against an independent reference engine using a completely different (dry-run-copy) precheck strategy (40/06) | 40 |
| [✓] | Trade events — `Trade` struct, "maker sets the price" convention (aggressor gets price improvement, never a worse price), multi-level sweeps emit one Trade per level touched, a single shared monotonic `seq` counter spans BOTH order arrivals and trades (one event timeline) (40/07) | 40 |
| [✓] | Self-trade prevention — 3 modes built (CancelNewest/CancelOldest/CancelBoth), each with distinct resting-order-fate + incoming-continuation semantics; a dangling-reference trap (reading `resting.qty` after `list::erase()`) caught and documented (40/08) | 40 |
| [✓] | Determinism aur sequencing — exact definition (bit-for-bit same output for same input, always), enumerated non-determinism sources and how each is structurally avoided (no wall-clock, single-threaded, `unordered_map` used ONLY for O(1) point-lookup never iterated for a decision, integer prices/qtys, no RNG on the matching path) (40/09) | 40 |
| [✓] | Event sourcing aur replay — command log (Submit/Cancel) as the source of truth, state as a derived/reconstructible cache; **proven, not claimed**: `05_event_sourcing.cpp` replays an identical 20000-command log into two independent fresh engines and diffs every trade field-by-field — byte-identical (4342 trades, resting_count=5245, both engines) (40/10) | 40 |
| [✓] | Testing + fuzzing — 43/43 scripted unit tests (including the FOK+STP interaction test); an independent O(n) reference engine (`RefEngine`, plain `vector` + linear scan, deliberately simple) cross-checked over 30000 random commands — **0 disagreements, 0 self-trade leaks, 0 FOK invariant violations**; single-threaded-per-symbol design explicitly justified (locks stop data races, not thread-scheduling non-determinism — sharding by symbol is the real parallelism) (40/11, 40/14, 40/15) | 40 |

---

## I. HFT CONCURRENCY

| Status | Topic | Folder |
|---|---|---|
| [✓] | Single-writer principle — full treatment: per-VARIABLE ownership rule (not "single-threaded program"), demonstrated across `spsc_queue.hpp`'s `head_`/`tail_`, `Seqlock`'s `value_`, `DisruptorRing`'s `cursor_`/`consumer_seqs_[i]`, and 40's `MatchingEngine` (41/02) | 28, 40, 41 |
| [✓] | Shared-nothing design — message-passing vs sharing contrasted explicitly; controlled-sharing escape hatch (seqlock) for genuinely-shared "current state"; a real 3-stage thread-per-stage pipeline built (`08_pipeline_demo.cpp`, running 40's actual `MatchingEngine`) proving zero shared mutable state between stages (41/03) | 27, 28, 41 |
| [✓] | **BUILD:** SPSC queue — built, optimized, and benchmarked (28 files 04–05, examples 01–02); packaged as a reusable `SpscQueue<T,Capacity>` template (41/`spsc_queue.hpp`) and reused across 3 more examples (async logger, pipeline demo); re-measured p50 400.8 ns / p99.9 1.88ms (tail OS-scheduler-jitter-dominated on this unpinned box, consistent with 28's own finding) (41/04). Polished HFT deliverable version in 44 | 20, 28, 41, 44 |
| [✓] | LMAX Disruptor pattern — a working simplified Disruptor built from scratch (`DisruptorRing<T,Capacity>`): single-producer gated-fan-out ring, N INDEPENDENT consumers each seeing every event (not split like a queue), natural batching via `wait_for()`, correctness-verified (2 consumers × 4M events, 0 mismatches, ~11% of reads were multi-event batches). Documented simplifications vs the real library (no pluggable WaitStrategy, no consumer dependency graphs, single-producer only) (41/05, `03_disruptor.cpp`) | 28, 41 |
| [✓] | Seqlock for snapshots — packaged as reusable `Seqlock<T>` (41/`seqlock.hpp`); re-measured under an ADVERSARIAL max-rate-writer stress test: **~2500–3500× faster than `std::shared_mutex`** (vs 28's gentler ~80–100×) — `shared_mutex` throughput collapses to ~0.04 M reads/s under this contention, torn=0 for both. High retry% (up to ~1800%) under the unthrottled writer explicitly explained as a stress-test artifact, not a defect (41/06) | 28, 41 |
| [✓] | Busy-spin vs condvar trade-off — the OTHER half of 28/06's latency-only measurement now added: real per-thread CPU time (`GetThreadTimes` via a `DuplicateHandle` fix for MinGW's `native_handle()` returning a `pthread_t` not a Win32 `HANDLE`) alongside latency, plus a third **hybrid** (spin-then-block) strategy. Measured: spin p50 210ns/~100% CPU, block p50 8997ns/~28% CPU, hybrid p50 7354ns/~25% CPU (41/07) | 28, 41 |
| [✓] | Core pinning strategy — `SetThreadAffinityMask` (Windows-native, since `29`'s `pthread_setaffinity_np`/`sched_setaffinity` already covers Linux) + measured scheduling-jitter A/B: pinned showed FEWER hiccups (511 vs 782 / 20M) but a WORSE single max outlier (15.6M ticks vs 829K) — an honest, measured demonstration that affinity ≠ isolation (real tail-latency control needs `isolcpus`/`nohz_full`, already in 29) (41/08, `06_core_pinning.cpp`) | 29, 41 |
| [✓] | Lock-free logging — hot path enqueues a fixed-size POD `LogRecord` (no allocation, no formatting, no syscall) into an `SpscQueue`; background thread formats+writes. Measured ~13.9× mean hot-path-cost reduction (naive sync format+write 1194.7 ns vs async enqueue-only 86.0 ns) against a fair same-sink baseline; explicit drop-on-full policy (logger must never be able to slow the hot path) (41/09, `07_async_logger.cpp`) | 36, 41 |
| [✓] | NUMA-aware thread placement — conceptual treatment on top of 29's already-built first-touch/`mbind` material: co-locate a pipeline's stages (and their memory) on ONE NUMA node (cross-node hand-offs are the failure mode), and combine with 40/11's symbol-sharding for a natural per-node-per-symbol placement with zero cross-node traffic by construction (41/12) | 29, 41 |
| [✓] | Wait-free reads (extension beyond the original checklist) — lock-free vs wait-free progress-guarantee distinction sharpened (seqlock reader = lock-free, NOT wait-free; atomic `shared_ptr` snapshot swap = genuinely wait-free, bounded-step, at the cost of a per-publish allocation + refcount atomics); ties directly into 28's hazard-pointer/epoch/RCU reclamation material (41/10) | 27, 28, 41 |
| [✓] | Priority inversion (extension beyond the original checklist) — classic 3-priority scenario explained, traditional fixes (priority inheritance/ceiling, Linux `SCHED_FIFO` + `PTHREAD_PRIO_INHERIT`) contrasted with HFT's structural fix: shared-nothing design has no shared lock to invert priority over, so the async logger (09) is priority-inversion-safe by construction, not by runtime protocol (41/11) | 29, 41 |

---

## J. HFT NETWORKING

> Foundations laid in folder 30; folder 42 is the HFT-specific deep dive
> + hardware — now complete.

| Status | Topic | Folder |
|---|---|---|
| [✓] | Multicast receive path — an `INetworkReceiver` abstraction (backend-agnostic: kernel-socket now, Onload/ef_vi/DPDK config-swappable later) wrapping `IP_ADD_MEMBERSHIP`, `SO_RCVBUF` tuning, and a realistic binary market-data message with sequence-gap detection (38/09's mechanism, on a real socket, deterministic drop pattern for reproducible verification) (42/01, 42/02, `01_multicast_receiver.linux.cpp`) | 30, 42 |
| [✓] | Kernel bypass deep dive — the full ladder (kernel-socket → `SO_BUSY_POLL` → Onload → ef_vi → DPDK) with an explicit trade-off table (latency vs application-rewrite vs vendor-lock); Onload's `LD_PRELOAD` transparent-intercept model, ef_vi's raw pre-post/poll queue API, DPDK's poll-mode-driver + hugepages, all explained with an honest "why this course doesn't implement the last three" (vendor SDK + special hardware required) (42/03–06) | 30, 42 |
| [✓] | Busy-poll sockets — the REAL kernel `SO_BUSY_POLL` sockopt built and measured against 30/10's app-level `MSG_DONTWAIT` spin AND plain blocking, three-way, on a one-way multicast path; the honest driver-dependency caveat (silently a no-op on loopback/virtual interfaces — no driver `ndo_busy_poll`) explicit in both code comments and the EXPECTED-output block (42/07, `02_busy_poll_receiver.linux.cpp`, `03_receiver_benchmark.linux.cpp`) | 30, 42 |
| [✓] | Hardware timestamping — fixes 30/09's exact limitation (RX-kernel-ts vs app-steady_clock, different epochs, relative-jitter-only) by taking TX timestamps too (`MSG_ERRQUEUE`, async retry-poll) from the SAME kernel clock domain as RX — the subtraction is now genuinely meaningful, not just relative. PTP (`ptp4l`/`phc2sys`) covered for cross-machine sync (42/08, `04_hw_timestamps.linux.cpp`) | 30, 42 |
| [✓] | NIC + IRQ tuning — a real, syntax-checked (`bash -n`) tuning script covering ring buffers, coalescing, GRO/LRO off, pause frames, RSS+per-queue IRQ pinning (explicitly kept OFF the hot-path cores that 41/08 pins threads to), RPS off, qdisc, sysctls, and verify-by-drop-counters (not "feels faster") (42/09, 42/10, `07_nic_tuning.sh`) | 29, 30, 42 |
| [✓] | TCP tuning for gateways — `TCP_NODELAY` on BOTH ends (a common half-fix), `writev()` to combine header+body into one packet regardless of Nagle state, keepalive triple + `TCP_USER_TIMEOUT` (fail a stalled gateway connection in seconds, 37/13's fail-closed principle); a corrected claim mid-development (writev's win is a small consistent one, NOT a re-creation of 30/03's 40ms stall, since `TCP_NODELAY` on the sender is exactly what prevents that specific interaction) (42/11, `05_order_gateway.linux.cpp`) | 30, 42 |
| [✓] | FPGA offload (intro) — what it is (reconfigurable hardware circuits, no instruction-fetch/decode overhead), where it fits in HFT (simple, FIXED, ultra-latency-critical decisions — pre-trade risk checks, basic threshold logic), explicitly NOT complex/dynamic strategy logic, and explicitly out of this course's scope (Verilog/VHDL/HLS is a separate career/domain, consistent with the SPECIALIZED/DOMAIN-SPECIFIC list) (42/13) | 42 |
| [✓] | Wire-to-wire latency measurement — a full capstone (`06_wire_to_wire.linux.cpp`) chaining TX+RX kernel timestamps across TWO network hops (market-data receive, order send) plus an internal-processing stage, all in one shared clock domain — an explicit breakdown (not just a single opaque total), with the honest note that in this trivial demo "processing" is tiny but in a real system the ratio inverts (network hops shrink with bypass/hardware, processing becomes the dominant, most controllable cost) (42/14, 42/16) | 30, 42 |
| [✓] | **BUILD:** low-latency multicast receiver — a full 6-layer design (backend abstraction, tuned non-blocking receive path, kernel timestamping, isolated-core threading via 41's SPSC, NIC/IRQ system tuning, drop-counter observability) synthesized in 42/16, with an explicit honest caveat on when it's over-engineered (microsecond, not nanosecond, requirements need far less). Polished HFT deliverable version in 44 | 42, 44 |

---

## K. HFT PROJECTS (code deliverables)

> **Section K is now COMPLETE** — folder 44 (PHASE 32, the capstone). All
> 11 deliverables built as a connected `MiniHftEngine`
> (MarketData → Parser → L2Book → Strategy → Risk → OMS → Venue → fills →
> PnL), single-threaded + deterministic, `./build.ps1 folder` → 12/12 OK.
> Reuse over rewrite: folder 40's `MatchingEngine` is the venue, folder
> 41's `SpscQueue` is the hand-off, folder 39/14/36/43 patterns for
> book/pools/strategy. The capstone optimization is `template <class
> Venue>`: `NaiveEngine` (std::map `MatchingEngine` venue) vs
> `OptimizedEngine` (`FastVenue` flat-array + per-level FIFO sweep), with
> the correctness gate proving them byte-identical across 5 configs
> **before** the ~1.5–1.6× end-to-end speedup is reported (43 methodology).

| Status | Project | Folder |
|---|---|---|
| [✓] | Market Data Simulator — deterministic non-crossing L3 feed + 38-byte BE wire encode; `01_market_data_sim.cpp` verifies seq/ts/non-cross invariants + replay identical | 44 |
| [✓] | Binary Feed Parser — `parse_v1` (portable, bounds-checked) + `parse_v3` (memcpy+bswap); 200k-frame agreement, 0 mismatch; `-O2` v1≈v3 (Rule-2 null, cf. 43/08) | 44 |
| [✓] | Limit Order Book — flat-array L2 (`L2Book`, 39/43 V3) + cached BBO + `resolve_cross`; BBO matches a `std::map` reference exactly; ~17 ns/apply | 44 |
| [✓] | Matching Engine — folder 40's engine reused as `ExecutionSimulator` ("the venue"); IOC fills at maker price, deterministic replay | 40, 44 |
| [✓] | Memory Pool + benchmarks — `FixedPool<T,N>` (typed, intrusive free list); alloc+free ~2.0× (p50) / ~5.0× (p99.9) vs `new`/`delete` | 14, 36, 44 |
| [✓] | Object Pool — `ObjectPool<T>` with **generation-checked handles**; stale handle → nullptr even after the slot recycles (use-after-free guard) | 25, 44 |
| [✓] | SPSC Ring Buffer + benchmarks — folder 41's `SpscQueue` reused for the MdMessage wire→engine hand-off (~6 M msg/s, every message in order); design ladder in 28 | 28, 41, 44 |
| [✓] | Strategy Simulator — `SpreadCrossStrategy` (mechanical, **NOT alpha** — 37 SPECIALIZED list) + deterministic backtest infra + parameter sweep | 37, 43, 44 |
| [✓] | Risk Engine — fat-finger / price-collar / position / rate-limit / kill-switch, all O(1); 14/14 checks; rate-limit ≠ kill (backpressure vs violation) | 37, 44 |
| [✓] | Order Manager + Execution Simulator — OMS state machine on a gen-checked pool; accounting always settles (no leaked orders, no phantom fills) | 44 |
| [✓] | **MINI HFT ENGINE (full pipeline)** — `template <class Venue> MiniHftEngine`; naive (std::map venue) vs optimized (`FastVenue`) proven byte-identical across 5 seed/config combos, then ~1.5–1.6× end-to-end (book stage ~150 → ~67 ns/msg). Determinism + invariants held. 41's `08_pipeline_demo` (3-thread) + 43's `pipeline.hpp` (v0→v3 ~60–80×) were the prototypes | 40, 41, 43, 44 |

---

## L. INTERVIEW READINESS

**Section L is now COMPLETE** — folder 46 (PHASE 33) is a **layered**
question bank (`02`–`14` = Layer 1 types → Layer 13 tick-to-trade
architecture) + interview strategy (`01` how HFT interviews work), system
design (`15`), brainteasers/probability (`16`), C++ trick questions
(`17`), behavioural (`18`), mock scripts + rubrics (`19`), resume (`20`).
Every Q&A answer cross-refs the source course folder. 8 verified coding
examples + 4 worked designs + 4 mock transcripts. **Folder 47 (PHASE 33)**
adds the 250-problem graded practice bank + `11-solutions/` + 10 more
verified examples — the last `[~]` row (DSA / coding problems) is now
`[✓]`.

| Status | Area | Folder |
|---|---|---|
| [✓] | C++ fundamentals questions — folder 46 file 02 (types/sizes/impl-defined vs UB vs unspecified, init forms, static-init, scope vs lifetime, control flow, functions, output-prediction) + per-lesson sets in folders 02–08 | 46 |
| [✓] | Pointers/memory questions — folder 46 file 03 (pointer vs reference, `const` placement, stack vs heap, leak/UAF detection + prevention, dangling sources, smart pointers, `alignas`/placement-new/`mlockall`) + embedded sets in folders 12/13/14 | 12, 13, 14, 46 |
| [✓] | OOP/vtable questions — folder 46 file 04 (vtable mechanism, virtual dtor mechanism + cost, ctor/dtor virtual calls, slicing, `override`/`final`, diamond/virtual base, `dynamic_cast` cost, Rule of 0/3/5, EBO, padding) + embedded sets in folder 16 | 16, 46 |
| [✓] | RAII / smart-pointer questions — folder 46 files 03 + 07 (RAII one-liner, `unique`/`shared`/`weak_ptr`, `shared_ptr` refcount ≠ thread-safe instance, `make_shared` trade-offs, custom deleters + EBO, ownership transfer by value) + embedded sets in folder 17 | 17, 46 |
| [✓] | STL questions — folder 46 file 05 (container choice on 3 axes: complexity / mutation pattern / cache; `deque`/`array`/`string` SSO; iterator categories + invalidation rules per container; `std::sort`/`lower_bound`; amortized push_back; `emplace` vs `push`; `vector<bool>` trap; STL-in-HFT) + embedded sets in folder 19 | 19, 46 |
| [✓] | Templates questions — folder 46 file 06 (instantiation timing, `typename`/`template` disambiguation, forwarding-reference deduction + reference collapsing, `constexpr`/`consteval`/`if constexpr`, SFINAE → concepts, CRTP 4-line example + trade-off, code bloat, two-phase lookup) + embedded sets in folder 21 | 21, 46 |
| [✓] | Move semantics questions — folder 46 file 07 (lvalue/prvalue/xvalue, "`std::move` doesn't move", moved-from = valid-but-unspecified, **`noexcept` move ⇒ vector relocates via moves not copies**, Rule of 5, `return std::move(local)` pessimization, perfect forwarding) + embedded sets in folder 18 | 18, 46 |
| [✓] | Concurrency questions — folder 46 file 08 (`jthread`, race vs data-race, `lock_guard`/`unique_lock`/`scoped_lock`, Coffman's 4 + break-circular-wait, gdb deadlock signature, `recursive_mutex`/`shared_mutex` smells, spinlock vs mutex, `cv.wait` predicate = spurious+lost+stolen, shared-nothing HFT posture, priority inversion) + embedded sets in folders 26/28 | 26, 28, 46 |
| [✓] | Memory model questions — folder 46 file 09 (`atomic` vs `volatile`, `is_lock_free`, `cmpxchg` weak vs strong, `fetch_add` returns old, the 6 orders one-liner each, acquire-release canonical pattern, `relaxed` safe use, happens-before definition, lock-free vs wait-free, ABA + tagged pointers, SPSC = no CAS, seqlock, false-sharing memory-model angle, `seq_cst` cost) + embedded sets in folder 27 | 27, 46 |
| [✓] | Linux/systems questions — folder 46 file 10 (syscall cost, `fork`/`vfork`/`posix_spawn`, `mmap` HFT uses, minor vs major page fault, `epoll` vs `select`, CFS problems for HFT, `SCHED_FIFO` danger, **affinity ≠ isolation**, NUMA first-touch, ctx-switch direct+indirect cost, `clock_gettime` vs `rdtsc`, `SIGPIPE`, async-signal-safety, shm + the sync hazard, the tuning checklist) + embedded sets in folder 29 | 29, 46 |
| [✓] | Networking questions — folder 46 file 11 (MD-UDP-multicast vs OE-TCP + why, `TCP_NODELAY` + Nagle/delayed-ACK stall, TCP HOL blocking, `SO_REUSEPORT`, multicast join API, A/B arbitration, sequence gap + snapshot recovery, alloc-free in-place parse, kernel-bypass ladder rungs, busy-poll vs interrupt, NIC coalescing, wire-to-app breakdown, hardware timestamping + clock-domain gotcha, `recvmmsg`) + embedded sets in folder 30 | 30, 46 |
| [✓] | CPU/cache questions — folder 46 file 12 (the latency numbers to memorize: L1/L2/L3/DRAM/mispredict/syscall/ctx-switch/network; cache-line = unit; 3+1 C's; branch predictor + sorted-vs-unsorted + branchless-isn't-always-faster; `[[likely]]` = layout not prediction; ILP/OoO/superscalar; row vs column; AoS vs SoA; prefetch; TLB/huge pages; false sharing + MESI + `perf c2c`; why pointer-chasing can't pipeline) | 31, 32, 46 |
| [✓] | Performance/optimization questions — folder 46 file 13 ("optimize this" first 3 moves; latency vs throughput / batching HoL; why p99.9 not mean + tail-at-scale + coordinated omission; when to stop; `perf stat` signal → problem class → fix; benchmark mistakes; `-O0` benchmark worthless; a surprising result is a lesson; `rdtsc` pitfalls; 5 hot-path rules; the hand-vectorized-code keep-or-revert judgement; strict-aliasing 12%-faster reject; lookup-table trade-off) | 35, 43, 46 |
| [✓] | HFT architecture design rounds — folder 46 files 14 + 15 (draw the tick-to-trade pipeline + each stage's job/data-structure/failure-mode; single-threaded per instrument + shard across cores; determinism → replay; order-book design = flat array by tick + cached BBO + dense id index; L2 vs L3; matching price-time priority; risk 5 O(1) checks; OMS state machine + gen-checked handles; the design-round arc + rubric + 4 worked designs in `examples/design/`) | 37–44, 46 |
| [✓] | DSA / coding problems — folder 47 (PHASE 33): **250 graded problems** across 10 themed files (basics/arrays-strings/pointers/OOP/STL/templates/concurrency/lock-free/optimize-this/HFT), each with a pattern hint + approach + complexity; `11-solutions/` full worked code; 10 verified runnable examples. Plus folder 20 file 18 (problem sets) + folder 46 `examples/*.cpp` (8 classic interview problems, each verified). | 20, 46, 47 |
| [✓] | Probability/brainteasers — folder 46 file 16 (EV thinking; the make-a-market game = two-sided quote + spread + update on the trade for information AND inventory; Monty Hall, 100 seats, HH-vs-HT expected wait 6 vs 4, 25 horses, ants on a stick, two-kids; Fermi estimation; mental-math drills; Kelly + volatility-drag intuition) + mock transcript `examples/mocks/04_market_making_game.md` | 46 |

---

## Current HFT readiness

**Sections A–L ALL COMPLETE** (prerequisites + low-latency C++ techniques
+ HFT domain knowledge + market data + order book + matching engine + HFT
concurrency + HFT networking + optimization methodology + **HFT projects**
+ **interview readiness**).
**Folder 44 (PHASE 32) — the CAPSTONE — is done**: everything from folders
01–43 assembled into one deterministic `MiniHftEngine`
(MarketData → Parser → L2Book → Strategy → Risk → OMS → Venue → fills →
PnL), with the 43-methodology optimization applied end-to-end
(`template <class Venue>` naive-vs-optimized, correctness gate first,
~1.5–1.6× with output proven byte-identical across 5 configs). **Only the
wrap-up folders 47–49 + the final gap audit remain** (folders 45 debugging
and 46 interview-prep — done, PHASE 33). Folder 43 (PHASE 31) added the optimization *methodology*
— measure/profile/change-one/re-measure/explain with a correctness gate
that runs first, applied to a connected tick-to-order pipeline (v0→v3
~60–80×, output byte-identical), plus per-technique measured numbers for
fixed-point, division elimination, struct layout, and an honest
hot/cold-split null result. Historically: folder 39 ne HFT ke classic
interview-question
data structure (order book) ko 3 baar banaya, measured, aur ek genuine
Rule-2 regression (V2's sorted vector, jo naive `std::map` se OVERALL
WORSE nikla) root-caused + V3 mein fix kiya; folder 40 ne usi book ke upar
actual matching logic banayi — Limit/Market/IOC/FOK, self-trade
prevention, aur ek genuine correctness finding (naive FOK precheck + STP
combination FOK's all-or-nothing contract silently violate kar sakta tha,
fixed + proven via fuzzing against an independent reference engine);
folder 41 ne 40's engine ko ek REAL multi-threaded pipeline mein daala —
production SPSC queue, ek working simplified LMAX Disruptor (fan-out +
gating + batching, khud se built), seqlock snapshots (adversarial stress
test mein ~2500-3500x `shared_mutex` se behtar measured), busy-spin vs
blocking ka full trade-off (latency AUR real CPU-time dono), aur ek
honest finding ki core-AFFINITY genuinely core-ISOLATION nahi hai (measured:
pinning se hiccup-frequency kam hui, PAR ek WORSE max-outlier bhi aaya);
folder 42 ne wire-to-wire path poora cover kiya — kernel-bypass ladder
(kernel-socket → SO_BUSY_POLL → Onload → ef_vi → DPDK), 30/09's clock-
domain limitation ko FIX kiya (TX+RX dono kernel-clock se), NIC/IRQ
tuning ka real script, aur ek full wire-to-wire breakdown (2 hops +
processing, sab ek clock se). Is folder ka MAJORITY code genuinely Linux-
only hai (`.linux.cpp`, is Windows/WSL-less dev box pe verify nahi ho
saka) — established 29/30 pattern follow kiya, hand-reviewed carefully.

Aur yeh **bilkul theek hai**. HFT track shuru karne se pehle sections A, B, C,
**D, E, F, G, H aur I** (prerequisites + low-latency C++ techniques + domain
knowledge + market data + order book + matching engine + HFT concurrency)
complete hone chahiye — woh sab ho chuke.
Folders 12–45 — **12–45 SAARE ho chuke**
(pointers, references + move intro, memory / allocation cost / pools,
classes / OOP / vtable / **virtual dispatch cost measured** / CRTP, **RAII / smart
pointers**, **copy-move / value categories / `noexcept` move**, **full STL** —
container perf table + allocators + PMR + `std::function` cost + "STL in HFT"
synthesis, **algorithms & cache-aware DSA** — sorting/search/hash-table/graphs +
"Big-O lies when the constant is a cache miss" + measured list-vs-vector ~40x /
flat-vs-pointer-tree 4–11x / map-vs-sorted-vec ~3x + own open-addressing hash
table, **templates / zero-cost abstraction** — full folder incl. concepts, CRTP,
policy-based design, compile-time dispatch (CRTP ~0.56 vs virtual ~2.43 ns
measured), **modern C++ 11→23** audit, **error handling** — `-fno-exceptions`
rationale (throw ≈ ~4000× a return, measured) + `std::expected` / `error_code`
toolkit + UB catalog + sanitizers, **compilation & linking** — ODR (silent
IFNDR!), static-vs-dynamic linking (PLT/GOT cost, why HFT static-links), Make /
CMake, and **the release-build config: static + LTO + PGO + `-march=native`**,
**object model** — lifetime / storage-reuse + `std::launder`, trivial /
standard-layout / `has_unique_object_representations` (what unlocks `memcpy` /
`memcmp` / wire structs), placement new (pools), **strict aliasing** (`bit_cast`
not `reinterpret_cast` — measured `-O2` divergence), vtable/ABI, **UB catalog +
how `-O2` exploits it**, **concurrency** — data race = UB (measured ~70% lost),
mutex contended-case cost (why no locks on the hot path), deadlock, **false
sharing measured ~10×**; **atomics & the C++ memory model** — the formal
data-race definition, all 6 memory orders (`relaxed`/acquire-release/`seq_cst`
with measured costs — `seq_cst` store ~18× a relaxed store on x86), CAS, happens-
before, fences, x86-TSO vs ARM, ABA, litmus tests; **lock-free structures** —
SPSC & MPMC rings built + optimized + benchmarked, Treiber/Michael-Scott,
reclamation (hazard pointers / epochs / RCU), **seqlock** (~80–100× faster reads
than `shared_mutex`), and the honest **"lock-free ≠ fast"** result (a contended
Treiber stack measured ~5× *slower* than `mutex + std::vector`); the
single-pinned-thread + lock-free-queue posture) — **plus 29 (Linux systems) and
30 (networking): Section B (systems prerequisites) is now COMPLETE at the lesson
level.** Syscall cost + vDSO, `fork`/COW/`exec`, async-signal-safe signal
handling, fd internals, `mmap` + POSIX shared memory (folder-28's ring in
`/dev/shm`), CFS vs `SCHED_FIFO` (+ "isolated CFS busy-poll often wins"),
`sched_setaffinity` + `isolcpus`/`nohz_full` isolation, huge pages, `mlockall` +
pre-fault + dry-run = zero-fault steady state, NUMA first-touch + `mbind`,
IRQ/softirq affinity, `CLOCK_MONOTONIC` (vDSO) vs `rdtscp`, cgroups/rlimits, and
the full production tuning checklist (BIOS → cmdline → sysctl → runtime → systemd
→ app → verify-by-effect). Networking: OSI/TCP-IP, IP/MTU/fragmentation, TCP deep
+ **Nagle/delayed-ACK ~40 ms**, UDP + why market data is UDP + **A/B feed
arbitration**, multicast/IGMP, sockets API + framing, blocking vs busy-poll +
`SO_BUSY_POLL`, select/poll O(N) → epoll O(ready), socket options, zero-copy
(`sendfile`/`splice`/`MSG_ZEROCOPY`/`io_uring`), **kernel bypass** (DPDK/Onload/
ef_vi/VMA/AF_XDP), **hardware timestamping + PTP**, NIC/IRQ tuning, and the
V1→V2→V3 (blocking echo → epoll → UDP-multicast feed handler) server progression.
Examples are `*.linux.cpp` — real Linux code, skipped on this MinGW box; numbers
in `EXPECTED` blocks are typical / "not measured on your machine" (Rule 2).

**Plus folder 31 (CPU architecture): Section C's microarchitecture rows are now
COMPLETE** — pipelining (latency vs throughput, hazards, flush/refill),
superscalar (ports, ILP, IPC), out-of-order (ROB / renaming / RS, speculation,
why pointer-chasing can't be hidden), branch prediction (**measured ~6–7×**
mispredict cost; the "default `-O2` already `cmov`s it" Rule 2 lesson),
speculative execution (Spectre/Meltdown, KPTI cost, `mitigations=off`
preconditions), instruction latency/throughput + critical-path + the division
problem, SIMD (**scalar → SSE 4.1× → AVX2 ~9.5×** measured; auto-vec **~2.5–3×**;
SoA prerequisite; AVX-512 downclock), **SMT** (shares L1/ports/ROB/predictor →
HFT disables it or isolates the sibling), **frequency & power** (lock the
frequency, kill deep C-states, thermal throttle, AVX freq offset), NUMA hardware
(sockets + UPI/IF, chiplet/CCX & SNC = NUMA within a socket), Intel-vs-AMD +
ARM/Graviton re-audit. Portable `.cpp` examples, benchmarks measured on an
AMD Zen 2 box (~2 GHz — shapes/ratios, not production absolutes).

**Plus folder 32 (cache & memory performance): Section C is now COMPLETE at the
lesson level.** Memory hierarchy + latency ladder, 64-B cache lines, set-
associativity + conflict misses (**critical stride** = size/assoc, power-of-two
dims poison), the 3 C's + MLP, locality (interchange / fusion / tiling — row vs
column **~10×** measured), **prefetching** (⚠️ Rule 2: SW prefetch **~1.1×** on
an independent gather — MLP already overlaps ~10 misses — and **3× *slower*** on
a memory-saturated loop), **false sharing** (**~6× to ~44×, run-to-run** — the
variance *is* the lesson), cache-friendly data structures (`map` ~20 serial
misses vs `flat_hash_map` ~1; arena + index links; CSR), **AoS-vs-SoA + DOD**
(SoA **~2.0–2.4×** on sequential scans, AoS **~3×** on random whole-record
access; `vector<Base*>`+`virtual` hot-loop cost; existence-based processing;
handles + generation counters), **TLB + huge pages** (4-level walk, dTLB/STLB
reach, 2 MiB → 512× reach, explicit hugetlbfs + pre-fault + `mlockall` vs THP
jitter), store buffers + RFO + NT stores, memory bandwidth + Little's Law (one
core ≈ 18 % of socket peak) + roofline ("SIMD a memory-bound loop = wasted"),
`perf stat`/`--topdown`/`perf c2c`/cachegrind measuring, **11 optimization
recipes** in impact order each with its trade-off. Two more Rule-2 stories: `-O3`
auto loop-interchange erases a 10× gap; a naive 64×64 blocked matmul is ~10–15 %
*slower* than a plain `ikj` loop order (needs a tuned microkernel).

**Plus folders 33 (compiler optimization) + 34 (assembly): Section C is complete
and Section D's compiler/asm/ABI/benchmarking rows are ✓.** Folder 33 is the full
flag-and-pass treatment — `-O` levels (the `-O0→-O1` ~6× cliff), inlining (`inline`
= ODR not a directive; inlining as the *enabler* of everything else across a
boundary), loop opts, auto-vectorization (**a float reduction won't vectorize at
`-O2` — reassociation — ~4× only with `-ffast-math`**), constant-folding/DCE,
devirtualization (the honest "won't fire for a heterogeneous container"), branch
hints (**~1.3× — layout not prediction**), aliasing & `__restrict` (**~3.5×**,
but the pessimism *only* shows with `[[gnu::noinline]]` — inlining/LTO proves
non-aliasing itself), LTO (**~2.3× on a throughput loop, none on a carried one**),
PGO (+ AutoFDO), `-march`/`-mtune` (v1..v4, `native` wrong for shipping, AVX-512
downclock), **`-ffast-math` dangers** (`-ffinite-math-only` compiles your `isnan`
guard to `false` — never on priced/audited code), the benchmark barriers
(`DoNotOptimize`/`ClobberMemory` = zero instructions — measured no-barrier loop =
0.00 ms), and reading optimized output as a verification checklist. **Rule 2 fired
three times and is taught.** Folder 34 teaches *reading* x86-64 assembly (never
writing it) for verification / optimization / debugging: registers + the
sub-register zeroing rule, AT&T vs Intel, the ~20 common instructions (`lea` ≠
`mov [..]`, recognize `idiv`/`rep movs`/`lock`), addressing modes (recover
`sizeof`/field offsets from a loop), stack frames (spills = register pressure,
tail call → `jmp`, Win64 shadow space), **System V vs Windows x64 calling
conventions**, pattern recognition (`if`/`cmov`/loop/`switch`/virtual `call
[reg+off]`/constant-fold/reciprocal-multiply division), SIMD asm (scalar vs packed
suffix, width from `add ptr,16/32`, the horizontal-reduce cluster, `vgather` is
slow), inline asm (missing-clobber = silent `-O2` corruption; use it only for the
barrier / `cpuid` / `rdtsc` / `pause`), `rdtsc` timing (fencing, ticks≠cycles≠ns,
core hopping — measured calibration ~2.0 ticks/ns, plain ~1 tick, fenced ~20),
and the disassembly tools (`-S` / `objdump -dS` / `perf annotate` / `gdb` /
`addr2line` / `llvm-mca`).

**Plus folder 35 (profiling & benchmarking): the last prerequisite gap is now
closed — BATCH 9 COMPLETE (folders 29–35).** The full measurement stack:
**why measure** (Amdahl with the numbers, premature-opt's real meaning,
throughput vs latency table); **`<chrono>`** (steady vs system vs
high_resolution — the alias trap; resolution / self-cost — `steady_clock::now()`
~38 ns measured); **`rdtsc`** (fencing recipes, ticks→ns calibration via
busy-wait, self-cost subtraction, core-pin — ~2.0 ticks/ns); **statistics**
(mean lies on right-skewed latency; median / min / max; σ useless for non-normal
→ CV / MAD / IQR; **bimodal = two code paths**; sample-size vs the percentile you
claim); **percentiles** (nearest-rank vs interpolation; "nines" language;
**fan-out tail amplification** — one backend's p99 becomes the user's median;
**coordinated omission** + the interval-correction fix; percentiles don't average
→ merge histograms); **jitter** (SW: timer tick / scheduler / IRQ / page fault /
`malloc` / syscall / lock; HW & firmware: frequency scaling / C-states / **SMI**
/ SMT sibling / NUMA / thermal; `cyclictest` / `rtla` / `hwlatdetect`; the
**"quiet core" recipe** — BIOS + kernel cmdline `isolcpus`+`nohz_full`+
`rcu_nocbs` + runtime pin / `mlockall` / zero-alloc / busy-poll); **histograms**
(why linear buckets fail for latency, **log-linear / HdrHistogram**,
relative-error ↔ `SUB_BITS`, CDF / percentile plot > PDF bars, mergeable + O(1) +
CO-correction — `08_latency_recorder.cpp`: ~2 ns/record, 29.5 KB, ≤1.3% error,
bimodal log-plot); **Google Benchmark** (`State` loop, `DoNotOptimize` /
`ClobberMemory`, `Range` / `Args`, `PauseTiming`, fixtures, `--benchmark_
repetitions` / `_cv`, the 10 mistakes the library can't fix — plus a
`minibench.hpp` shim so the example runs dependency-free); **benchmark pitfalls**
(DCE / const-fold / loop-invariant hoist / cold-start-4-components /
timer-overhead>op / one-run / **alignment & code-layout noise** — trust only >
noise / **frequency scaling** — report cycles/op / **denormals** — FTZ/DAZ /
bench≠reality — all demoed BUG/FIX); **`perf`** (stat + IPC + cache rates +
**top-down** Retiring/Frontend/Backend/Bad-Spec; record/report/annotate; **skid →
`:pp` PEBS/IBS**; `cycle_activity.stalls_l3_miss`; **`perf mem`** per-access
latency+source; **`perf c2c`** false sharing / HITM; LBR → AutoFDO; counter
multiplexing); **flame graphs** (axes, top plateau, on- vs off-CPU, differential,
`stackcollapse`→`flamegraph.pl`); **Valgrind** (cachegrind `Ir` = deterministic
CI gate; callgrind + KCachegrind; massif; **DHAT** per-alloc-site usage / churn);
**VTune / top-down** (recursive Backend→Memory→DRAM→**Bandwidth-vs-Latency** tree
with different fixes; `toplev.py` for the same on plain `perf`; AMD uProf /
Instruments); **sanitizers** (ASan / UBSan / TSan / MSan — what each catches;
`volatile` vs `std::atomic`; **never for perf numbers**; ASan+UBSan+TSan CI
matrix); **production measurement** (6 requirements; inline `rdtsc` + per-thread
HdrHistogram snapshot by a housekeeping thread; **SPSC ring → aggregator** for
zero hot-path math; sampling — 1-in-N / time-gated / exceedance; **coordinated
omission in prod** = timestamp at arrival not dequeue; per-thread → merge, never
average p99s; white box vs black box + NIC HW timestamps; USDT / uprobe / eBPF /
LTTng). Measured on an unpinned Windows/MinGW Zen 2 box — **and that itself is a
lesson**: `02`/`03` tail numbers are deliberately run-to-run unstable, contrasted
with what a pinned Linux isolated core would give (03/06).

**Section A is now COMPLETE** — all 10 rows `[✓]`: move semantics, RAII, templates
/ zero-cost abstraction, STL container performance, custom allocators + PMR, UB
awareness, exceptions & `-fno-exceptions` trade-off, object layout / alignment /
placement new / strict aliasing, **threads / mutexes / condition variables /
deadlock / false sharing**, **atomics & memory ordering** (27), **lock-free data
structures** (28). **Section D (low-latency C++ techniques) is now essentially COMPLETE** — folder
36 (PHASE 24) lifted every remaining row to `[✓]`: allocation avoidance &
preallocation, fixed-size memory pools (`FixedPool` built + benchmarked, p99.9
flat), object pools (construct vs recycle), arenas / bump / PMR, tail-latency
thinking, lock-contention avoidance & busy-spin-vs-blocking, CPU pinning
strategy, NUMA-aware allocation, syscall avoidance & batching, page-fault
avoidance, I-cache / hot-cold splitting, **zero-copy patterns**, cache warming,
and the honest trade-off discussion (lesson 24). "Virtual dispatch elimination",
"`std::function` cost", "ring buffers", "false sharing", "branch-free",
"cache locality" were already `[✓]`. **Every technique is paired with its hidden
cost and an explicit "when NOT to" per CLAUDE.md spec Rule 12–13**, and Rule 2
fired 3× (all taught).
Section I: **"BUILD: SPSC queue" `[✓]`**, **"Seqlock for snapshots" `[✓]`**,
"busy-spin vs condvar" / "single-writer principle" / "shared-nothing" / "LMAX
Disruptor" `[~]` (folder 28; full treatment in 41). Section G (order book)
design-space rows `[~]`. Section L (interview readiness) — **COMPLETE**
via folder 46's consolidated layered question banks + folder 47's
250-problem practice bank (the DSA/coding-problems row is now `[✓]`).

Agar aap HFT pe seedha jump karoge to woh cargo-cult programming hogi — code copy
karoge, samjhoge kuch nahi, aur interview mein pehle follow-up question pe atak jaoge.

**Section E is COMPLETE** — folder 37 (PHASE 25) covers all 14 domain-
knowledge rows: HFT myths vs reality, exchange architecture, market
microstructure + adverse selection, order types, spread/microprice/imbalance
(measured), price-time-priority vs pro-rata (measured), maker-taker
economics, strategy categories (no alpha), colocation physics, tick/lot/band
constraints, the full system architecture diagram, risk systems (fail-
closed), tick-to-trade latency budgeting (measured tool), India-vs-US market
structure, and regulatory basics. Every lesson connects back to a C++/
systems concept from folders 00–36 (integer ticks ↔ float-equality trap,
audit-trail logging ↔ ring buffers, risk-check speed ↔ branch-free/
allocation-free hot path) — domain knowledge isn't disconnected from the
engineering, it's the "why" behind it.

**Section F is now COMPLETE** — folder 38 (PHASE 26) covers all 14 market-
data rows and is the domain track's first **build**: one shared teaching
protocol (`wire_protocol.hpp`, ITCH-style, 16-byte header + 5 message
types), and the full CLAUDE.md process run end-to-end — simple/correct
parser (03) → measured (04: p99.9 771.5 ns) → optimized (05: overlay-read,
zero allocation) → re-measured (06: **p99.9 ratio 22.2×**, p50 unchanged) →
integrated (10: end-to-end feed handler == standalone optimized parser,
proving framing+gap-detection added zero extra tail). Plus: byteswap cost
measured (0.753 ns/swap — corrects the "binary is fast because swap is
avoided" myth from 38/05, feeds directly into 38/08's honest SBE-
native-endian explanation), A/B feed arbitration measured (**~49.5× loss
reduction, zero round-trips**), gap detection sanity-verified exactly
against injected drops. L3 market data (`AddOrder`/`Cancel`/`Delete`/
`Execute`/`Replace`) is now literally the input format **39-ORDER-BOOK**
will consume.

**Section G is now COMPLETE** — folder 39 (PHASE 27) covers all 10 order-
book rows via the classic "3 versions, measure each" build: V1 (`std::map`
+ `std::list` + `unordered_map`) → V2 (sorted `std::vector` + `std::deque`
— **measured OVERALL WORSE than V1**, a genuine Rule-2 finding: Add got
3.9× better via contiguous-array access, but the order-id index could no
longer store a stable iterator (vector reallocation invalidates it), so
cancel/execute fell back to an `O(level size)` linear scan, making the
whole workload worse) → V3 (tick-indexed flat array + intrusive arena
list + tombstone-based flat hash — wins every metric, p99.9 ~2-4× better
than both V1 and V2). All three proven behavior-identical: cross-version
equivalence checked after every op (20000+ checkpoints) and a 30000-op
fuzz run with ~3000 deliberately-invalid ops (duplicate ids, nonexistent-
id ops, over-execute clamping) against an independent `O(n)` reference
model — zero disagreements. A real precondition bug (`best_bid()` called
without `has_bid()`) crashed during test-writing itself and is documented,
not hidden. Full mechanism-level "what changed and why" accounting in
39/15 — the spec's explicit requirement, not just a numbers table.

**Section H is now COMPLETE** — folder 40 (PHASE 28) covers all 10
matching-engine rows: price-time-priority matching (nested best-level +
FIFO loops) on top of 39's V1-style resting-order storage, all 4 order
types (Limit/Market/IOC/FOK), 3 self-trade-prevention modes, and a
genuine correctness finding — a naive FOK precheck ("sum total resting
qty at crossable levels") silently violates FOK's all-or-nothing
contract once combined with self-trade prevention (STP can skip/abort on
self-owned liquidity the naive sum still counted). Fixed with an
STP-mode-aware precheck that walks the exact same priority order the
real match uses. Verified two ways: a hand-crafted scripted test that
demonstrates the exact naive-vs-aware divergence (sum 70 vs true-
available 30, target 50), AND a 30000-command fuzz run against an
independent O(n) reference engine (`RefEngine`) whose FOK precheck uses
a *completely different* strategy (copy-the-state, practice-match,
discard) — 0 disagreements, 0 self-trade leaks, 0 FOK violations across
3774 FOK orders hit. Determinism proven (not claimed) via byte-identical
dual-engine event-log replay (20000 commands, 4342 trades). 43/43
scripted unit tests pass. Full accounting in 40/06 (the FOK+STP fix) and 40/15 (the fuzz design).

**Section I is now COMPLETE** — folder 41 (PHASE 29) covers all 9
original HFT-concurrency rows plus 2 extension rows (wait-free reads,
priority inversion). This is the domain track's SECOND real build: a
production `SpscQueue<T,Capacity>` (packaged from 28's design), a
working simplified LMAX Disruptor built from scratch (single-producer,
N independent fan-out consumers, gating sequences, natural batching —
correctness-verified over 4M events with 0 mismatches), a `Seqlock<T>`
snapshot mechanism, and a genuine capstone (`08_pipeline_demo.cpp`) — a
real 3-stage thread-per-stage pipeline carrying 40's ACTUAL
`MatchingEngine`, not a toy stand-in. Two headline measured findings:
(1) seqlock beats `std::shared_mutex` by **~2500-3500×** under an
adversarial max-rate-writer stress test — far more dramatic than 28's
original ~80-100× under gentler conditions, and (2) core-pinning
(`SetThreadAffinityMask`) reduced scheduling-hiccup FREQUENCY (511 vs
782 / 20M iterations) but produced a WORSE single max-delay outlier
(15.6M ticks vs 829K) — an honest, measured demonstration that affinity
is not isolation, motivating the `isolcpus`/`nohz_full` material already
built in folder 29. A real MinGW toolchain quirk (`std::thread::
native_handle()` returns a `pthread_t`, not a Win32 `HANDLE`, breaking
`GetThreadTimes`) was found and fixed via `DuplicateHandle(GetCurrentThread())`,
needed to measure the busy-spin-vs-blocking CPU-cost trade-off (28/06 had
only measured the latency half). Async logging measured ~14x hot-path
cost reduction. Full accounting in 41/06, 41/07, 41/08, 41/13.

**Section J is now COMPLETE** — folder 42 (PHASE 30) covers all 9
HFT-networking rows. The full kernel-bypass ladder (kernel-socket →
`SO_BUSY_POLL` → Onload → ef_vi → DPDK) explained with an honest
trade-off table, backed by an `INetworkReceiver` abstraction
(`08_bypass_abstraction.hpp`) that makes backend choice a config-time
decision rather than an application rewrite. Two genuine technical
fixes to earlier folders' limitations: (1) hardware timestamping now
takes BOTH TX (via `MSG_ERRQUEUE`) and RX timestamps from the SAME
kernel clock domain, fixing 30/09's "different epochs, relative-jitter-
only" limitation with a genuinely meaningful latency number; (2) a
mid-development correction on the TCP order-gateway example — `writev`'s
win was initially (incorrectly) framed as recreating 30/03's ~40ms
Nagle/delayed-ACK stall, corrected to the accurate mechanism (that
specific stall needs the SENDER's Nagle to be ON, which `TCP_NODELAY`
here prevents; `writev`'s actual win is a smaller, consistent one).
A full wire-to-wire (tick-to-trade) capstone chains kernel timestamps
across 2 network hops + internal processing in one shared clock domain.
**This folder's code is majority Linux-only** (`.linux.cpp`, matching
29/30's established pattern) — this Windows/MinGW dev box has no WSL,
so these files could not be compiled or run here; they were written
carefully (reusing 30's already-verified structural patterns) and
hand-reviewed for sign-compare issues, unbounded blocking recv calls,
and empty-vector UB (several real ones caught and fixed), with every
number presented as an explicit `EXPECTED (... not measured)` estimate,
never as a fabricated "measured" result. Full accounting in 42/02,
42/08, 42/11.

**Folder 44 (PHASE 32) — THE CAPSTONE. Section K complete; Sections A–K
all done.** Everything from folders 01–43 assembled into one
`MiniHftEngine` (MarketData → Parser → L2Book → Strategy → Risk → OMS →
Venue → fills → PnL), single-threaded, fully deterministic. 15 lessons +
12 drivers + 11 shared headers, 12/12 OK. **Reuse:** folder 40's
`MatchingEngine` = the venue, folder 41's `SpscQueue` = the hand-off,
39/14/36/43 patterns for book/pools/strategy — `#include`, not
copy-paste. **The capstone optimization:** `template <class Venue>` —
`NaiveEngine` (std::map `MatchingEngine` venue) vs `OptimizedEngine`
(`FastVenue` flat-array + per-level FIFO sweep). Correctness gate proves
them byte-identical across 5 configs **before** the ~1.5–1.6× end-to-end
speedup is reported (book stage ~150 → ~67 ns/msg). Per-component:
L2Book ~17 ns/apply (BBO = std::map ref exactly), FixedPool ~2.0×/~5.0×
vs new/delete, ObjectPool stale-handle rejection, SPSC ~6 M msg/s
in-order, risk 14/14, OMS accounting always settles, feed parser v1≈v3
(Rule-2 null). **No alpha** (`SpreadCrossStrategy` mechanical, 37
SPECIALIZED list). Full accounting in 44/12, 44/13, 44/14.

**Folder 43 (PHASE 31) — optimization methodology, applied end-to-end.**
Not a new capability row — it deepens Sections C/D by *applying* the
technique toolbox to a connected pipeline and attributing every delta.
The 6-step loop (measure → profile → hypothesize → change one → re-measure
→ **explain what changed and why**) with a **correctness gate that runs
first**: `04_before_after.cpp` verifies v0 and v3 produce an identical
order-fire tick stream (110/110) before it will report the ~60–80×
speedup. Per-technique measured (Zen 2, ratios): fixed-point (`09` —
`0.1×10 ≠ 1.0` shown exact; correctness is the point, ~20% speed a bonus),
division elimination (`10`/`06` — `div`→shift ~17×, →magic-multiply ~13×,
→reciprocal-multiply ~7×, *only the divide timed* per the CLAUDE.md
warning), struct layout (`07` — field-reorder 24→16 B, fat→SoA ~8×,
memory-traffic-bound), hot/cold split (`08` — **~1%, an honest Rule-2
null**: frontend isn't the bottleneck at this scale, carries 36/12; the
`[[gnu::cold]]`→`.text.unlikely` move verified in asm, attribute kept
because its cost is 0). Two case-study lessons re-frame folders 39
(order book V1→V2→V3, the V2 regression) and 38 (feed handler) *inside*
the pipeline. `16-when-to-stop.md` is the diminishing-returns / judgement
lesson. `02_profile_analysis.sh` maps each `perf` signal
(frontend-bound / branch-miss / LLC-miss / `idiv` / HITM) to the lesson
that addresses it. Full accounting in 43/01, 43/13–15, 43/16.

**Rasta:** 01 → **44 ho chuka** (saare C++/systems/low-latency
prerequisites + HFT domain knowledge + market data + order book + matching
engine + HFT concurrency + HFT networking + optimization methodology +
**the capstone mini HFT engine**). **HFT build track COMPLETE** (folders
36–44). Wrap-up folders bhi: ~~**45** (debugging)~~ ✅ · ~~**46** (interview
prep — Section L)~~ ✅ · ~~**47** (coding problems — 250-problem practice
bank)~~ ✅ · ~~**48** (cheatsheets — 13 quick-reference sheets)~~ ✅ ·
~~**49** (projects — 17 verified reference implementations)~~ ✅
(sab PHASE 33). **Folders 00–49 all built.** Bacha sirf: **final gap audit.**
