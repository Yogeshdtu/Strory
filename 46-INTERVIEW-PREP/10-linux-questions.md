# 10 — Layer 9: Linux (syscalls, processes, scheduling, mmap)

## Prerequisites
Folder `29-LINUX-SYSTEMS`. HFT runs on tuned Linux — interviewers expect
you to know where the OS costs latency and how to remove it.

---

## A — Syscalls & processes

### A1. Syscall ka cost kyun matter karta HFT mein?
<details><summary>Answer</summary>
A syscall = user→kernel mode switch (save/restore state, ~100–300 ns even
for a trivial one, more with mitigations/`KPTI`), possible reschedule if
it blocks. Multiply by call frequency. Hot path pe **zero syscalls** the
goal: batch I/O, `io_uring` with `SQPOLL` (no submit syscall), kernel
bypass (`ef_vi`/DPDK), read time via `rdtsc` not `clock_gettime` (or the
vDSO fast path), pre-fault memory so no fault-driven syscalls. (`29/02`,
`36/16-17`.)
</details>

### A2. `fork` vs `vfork` vs `posix_spawn` vs threads?
<details><summary>Answer</summary>
`fork` — copy-on-write clone of the whole process (page tables copied,
COW faults later). `vfork` — child shares parent's memory until `exec`
(faster, dangerous). `posix_spawn` — fork+exec combined, avoids COW of a
huge parent. **Threads** (`clone` with shared address space) — for
concurrency within a process. HFT engines are usually a single
multi-threaded process, threads pinned to cores; `fork` mostly at
startup / for tooling. (`29/03`.)
</details>

### A3. `mmap` — what it does, HFT uses.
<details><summary>Answer</summary>
Maps files or anonymous memory into the address space. Uses: (1)
`MAP_ANONYMOUS` — large allocations without `brk` fragmentation. (2)
File-backed — zero-copy file access, or an mmap'd ring log the kernel
flushes. (3) `MAP_HUGETLB` / THP — 2 MiB pages → 512× TLB reach, fewer
page walks. (4) `MAP_SHARED` — shared memory between processes (feed
handler → strategy). Pre-fault (`MAP_POPULATE` or touch every page) +
`mlock` to avoid faults on the hot path. (`29/07-08`, `32/11`.)
</details>

### A4. Page fault — minor vs major, cost.
<details><summary>Answer</summary>
**Minor** — page is in RAM but not mapped in this process's page table
(first touch of freshly-allocated memory, COW): kernel maps a frame,
~1–5 µs. **Major** — page must come from disk (swap, or file not
cached): ~ms. Both are hot-path killers. Fix: `mlockall(MCL_CURRENT|MCL_FUTURE)`
+ prefault at startup; verify with `perf stat -e minor-faults,major-faults`
→ ~0 in steady state. (`29/13`, `45/11`.)
</details>

### A5. `epoll` vs `select`/`poll` — why epoll?
<details><summary>Answer</summary>
`select`/`poll` — O(n) per call, you pass the whole fd set every time,
kernel scans all. `epoll` — O(1) readiness notification; register fds
once (`epoll_ctl`), `epoll_wait` returns only the ready ones. Scales to
many connections. Edge-triggered (`EPOLLET`) — notified once per
transition, must drain fully. For lowest latency on a single socket,
**busy-poll** (`SO_BUSY_POLL` / userspace spin) beats epoll's wakeup
latency. (`29/05`, `30`, `36/16`.)
</details>

---

## B — Scheduling & CPU

### B1. CFS scheduler — basics, aur HFT ke liye kyun problem.
<details><summary>Answer</summary>
Completely Fair Scheduler — each runnable thread gets a fair slice
(vruntime-ordered). Problems for HFT: (1) your hot thread can be
**preempted** by any other runnable thread → jitter, cache blown. (2)
Timer tick / scheduler bookkeeping on the core. Fixes: `isolcpus` +
`nohz_full` + `rcu_nocbs` to evict the OS from hot cores, pin threads
with `sched_setaffinity`, optionally `SCHED_FIFO` (careful — a busy-loop
at RT priority can hang the box). (`29/10-11`, `36/19`.)
</details>

### B2. `SCHED_FIFO` / `SCHED_RR` — kya, aur khatra.
<details><summary>Answer</summary>
Real-time policies: a `SCHED_FIFO` thread runs until it blocks or yields
— **no preemption** by normal threads. Great for a hot loop. **Danger:**
if it busy-loops and never yields on a core the kernel also needs (no
`isolcpus`), it can starve kernel threads → soft lockup / unresponsive
box. Use with isolated cores + a watchdog + `RLIMIT_RTTIME`. (`29/10`.)
</details>

### B3. CPU pinning — how, and why "affinity ≠ isolation".
<details><summary>Answer</summary>
`sched_setaffinity` / `pthread_setaffinity_np` / `taskset` pins a thread
to a core. But **affinity alone** doesn't stop *other* threads (yours or
the OS's) from also being scheduled there — you've only constrained *your*
thread's placement. **Isolation** (`isolcpus`/`nohz_full`/IRQ affinity
away) removes everyone else. HFT needs both. Also idle the SMT sibling.
Verify: `perf stat -e cpu-migrations` → ~0. (`29/11`, `29/15`, `35/06`.)
</details>

### B4. NUMA — what it is, one HFT rule.
<details><summary>Answer</summary>
Multi-socket: each CPU has local memory; accessing another socket's
memory is slower (cross-socket interconnect). Rule: **first-touch**
allocation — the socket whose thread first writes a page owns it. So
allocate + prefault on the **same** core/socket that will use it, keep
the NIC, its IRQs, and the polling thread on **one** NUMA node.
`numactl --cpunodebind --membind`. (`29/14`.)
</details>

### B5. Context switch cost — direct + indirect.
<details><summary>Answer</summary>
**Direct:** save/restore registers, kernel scheduler work — ~1–3 µs.
**Indirect (bigger):** cold caches and TLB after the switch — the new
thread refills L1/L2 and TLB from its working set, tens of µs of degraded
performance. This is why an involuntary preemption of a hot thread is so
costly and why isolation matters. `perf stat -e context-switches`. (`29/10`,
`31`, `32`.)
</details>

---

## C — Time, signals, IPC

### C1. `clock_gettime(CLOCK_MONOTONIC)` vs `rdtsc` for timestamps?
<details><summary>Answer</summary>
`CLOCK_MONOTONIC` via **vDSO** (~20 ns, no real syscall) is portable,
monotonic, in nanoseconds. `rdtsc` (~5–20 ns with fencing) reads the CPU
timestamp counter directly — need an **invariant TSC** (constant rate
across freq/C-states), a calibrated ticks→ns factor, and pinned threads
(TSC can differ slightly per core; usually synced on modern boxes). HFT
uses `rdtsc` in the very hot path, calibrated once at startup. (`29/16`,
`34/11`, `35/03`.)
</details>

### C2. `SIGPIPE` — kyun aata hai, aur handle kaise?
<details><summary>Answer</summary>
Writing to a socket/pipe whose other end is closed → `SIGPIPE`, default
action **terminates** the process. Servers must handle: ignore it
(`signal(SIGPIPE, SIG_IGN)`) and check `write`'s `EPIPE` return, or use
`send(..., MSG_NOSIGNAL)`, or `SO_NOSIGPIPE` (BSD). (`29/04`, `30`.)
</details>

### C3. Signal handler — kya safely kar sakte ho andar?
<details><summary>Answer</summary>
Sirf **async-signal-safe** functions (`write`, `_exit`, `sig_atomic_t`
stores, some others). **No** `malloc`, `printf`, locks, most of the STL —
if the signal interrupts one of those mid-operation, calling it again
deadlocks/corrupts. Common pattern: handler sets a
`volatile sig_atomic_t` flag (or writes a byte to a self-pipe / eventfd),
the main loop reacts. (`29/04`.)
</details>

### C4. Shared memory between processes — mechanism + one hazard.
<details><summary>Answer</summary>
`shm_open` + `mmap(MAP_SHARED)` (POSIX) or `mmap` an `memfd`. A feed
handler process writes market data into a shared ring; strategy processes
read it — zero-copy, no socket. Hazards: **synchronization** across
processes (need atomics / seqlock in the shared region, no
`std::mutex` unless `PTHREAD_PROCESS_SHARED`), and a crashed writer can
leave the ring in a bad state (versioned / self-describing layout,
recovery logic). (`29/08`, `41`.)
</details>

### C5. `/proc` and `/sys` — one debugging use each.
<details><summary>Answer</summary>
`/proc/<pid>/status` (VmRSS, threads, voluntary/nonvoluntary ctxt
switches), `/proc/<pid>/task/<tid>/stat` (per-thread CPU, last core),
`/proc/interrupts` (IRQ distribution across cores). `/sys/devices/system/cpu/`
(governors, C-states, frequency), `/sys/kernel/mm/transparent_hugepage/`.
Used to verify tuning actually took effect. (`29/06`.)
</details>

---

## D — HFT tuning checklist (be able to recite ~6)

<details><summary>Answer</summary>
Isolate cores (`isolcpus`, `nohz_full`, `rcu_nocbs`); pin every hot
thread, idle SMT siblings; steer NIC IRQs to a housekeeping core; disable
C-states / fix CPU frequency (`intel_pstate=disable`, `cpupower`);
`mlockall` + prefault all memory; explicit huge pages + pre-fault;
`tuned-adm profile latency-performance` or manual; disable THP
(`khugepaged` jitter); TSC as clocksource, verified invariant; NIC:
disable interrupt coalescing / enable busy-poll / kernel bypass; disable
`irqbalance`; set `net.core.busy_read`/`busy_poll`; turbo/SMI awareness.
(`29/18`, `36/19`.)
</details>

---

## Interview tips for Layer 9

- Every answer ties back to **latency and determinism**: "this syscall /
  fault / preemption costs X, here's how HFT removes it."
- "Affinity ≠ isolation" is a strong, specific thing to know.
- Know page fault minor vs major cost, and `mlockall` + prefault as the
  fix.
- `epoll` for scale, **busy-poll** for a single hot socket's latency.
- Signal handlers: async-signal-safe only, set a flag / self-pipe.

## Next
→ [`11-networking-questions.md`](11-networking-questions.md)
