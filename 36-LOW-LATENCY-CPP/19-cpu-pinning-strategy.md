# 19 — CPU pinning strategy: which thread on which core

## Prerequisites
- **`29-LINUX-SYSTEMS/06`** (`sched_setaffinity`, `isolcpus`, `nohz_full`,
  `rcu_nocbs`), **`31-CPU-ARCHITECTURE/12-14`** (SMT, NUMA, frequency),
  **`35-PROFILING-BENCHMARKING/06`** (jitter, the "quiet core" recipe)

## Yeh topic abhi kyun
An unpinned thread migrates on every scheduler decision → cross-core cache
& TLB cold, cross-core TSC skew, preemption by anything the scheduler wants
to run. Pinning + core isolation is the **foundation** every other technique
in this folder sits on. Yeh lesson: a concrete placement plan.

---

## The layers

```
  BIOS/firmware:  turbo off (or locked all-core), C-states -> C1 max,
                  hyperthreading off on trading cores, "max performance" profile

  Kernel cmdline (isolate cores 2-9 on a 12-core box, keep 0-1 for the OS):
     isolcpus=2-9  nohz_full=2-9  rcu_nocbs=2-9  irqaffinity=0,1
     intel_pstate=disable  processor.max_cstate=1  idle=poll
     transparent_hugepage=never

  IRQs:  echo 3 > /proc/irq/<n>/smp_affinity   (all device IRQs -> cores 0-1)
         irqbalance disabled

  Runtime (per hot thread):
     sched_setaffinity(one isolated core)      // pin
     optionally SCHED_FIFO (carefully — see traps)
     mlockall + pre-fault (lesson 18)
     the NIC RX poll thread, strategy thread, order thread each on their own core
```

`isolcpus` alone only keeps the **scheduler** off the core — timers, RCU
callbacks, and kernel per-CPU work still run there until you add `nohz_full`
+ `rcu_nocbs` (35/06 ex 2).

---

## A placement plan (12-core, 1 socket, 1 NIC)

| Core | Assignment |
|---|---|
| 0–1 | OS: kernel threads, IRQs, `irqbalance`, cron, monitoring, logging thread, housekeeping/aggregator, the `SQPOLL` kernel thread |
| 2 | **NIC RX poll** — busy-polls the NIC ring / `ef_vi` / `io_uring` CQ |
| 3 | **feed decode / book build** |
| 4 | **strategy** |
| 5 | **risk + order encode + TX** |
| 6–9 | additional strategy shards (per-symbol-group), or a backtest/research pool (SMT *on* there for throughput) |
| 10–11 | spare / failover |

- Each hot thread on **one** isolated core, alone.
- The NIC's IRQ (if any) and the `SQPOLL` thread on the **OS cores**, not
  the poll core.
- Hot cores' **SMT siblings idle** (or SMT off) — a sibling running anything
  steals L1/ports/ROB → jitter (31/12).

---

## NUMA placement (multi-socket)

- Put the hot threads, their memory, and the NIC on the **same NUMA node**.
  A remote memory access is ~1.5–2× local (31/14, 32).
- **First-touch**: memory is placed on the node of the thread that first
  writes it → do the pre-fault (lesson 18) **from the thread that will use
  it**, pinned to the right node. Or `numactl --membind` / `mbind`.
- Check the NIC's node (`cat /sys/class/net/<if>/device/numa_node`) and pin
  the RX thread there.
- `numa_balancing` **off** (`kernel.numa_balancing=0`) — its background page
  migration is a jitter source.

---

## `SCHED_FIFO` — careful

RT priority above CFS, no timeslice → your thread runs until it yields or a
higher-priority RT thread preempts. Benefits: no CFS preemption by normal
tasks. **Hazards**:
- A `SCHED_FIFO` thread that **busy-spins without yielding on a non-isolated
  core** starves everything else on that core, including kernel housekeeping
  → the box can lock up. `sched_rt_runtime_us` throttling exists as a safety
  net but interferes with your timing.
- On a **properly isolated** core (`isolcpus` + `nohz_full` + `rcu_nocbs`,
  nothing else scheduled there), plain **`SCHED_OTHER` (CFS) busy-poll often
  matches or beats `SCHED_FIFO`** with far less risk — there's nothing to
  preempt you anyway (29/09 measured this).
- If you use `SCHED_FIFO`, set `RLIMIT_RTTIME`, keep the priority sane, and
  never spin without an occasional yield on a shared core.

---

## Verifying

```bash
taskset -cp <tid>                       # confirm the affinity mask
cat /proc/<tid>/status | grep Cpus_allowed_list
grep -c '' /proc/<tid>/stat            # or watch `processor` field (39th) not changing
perf stat -e cpu-migrations,context-switches -p <tid> -- sleep 30   # should be ~0
cat /proc/interrupts                    # device IRQs incrementing only on cores 0-1?
cyclictest -m -p 99 -a 2-5 -i 100 -h 400   # scheduling-jitter histogram on the trading cores
```
`cpu-migrations` and `context-switches` per hot thread should be **~0** over
a soak. If not, something is still scheduled on that core (`nohz_full`/
`rcu_nocbs` missing, a monitoring agent, a `SCHED_FIFO` misconfig).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `isolcpus` without `nohz_full` + `rcu_nocbs`
Timers and RCU callbacks still run on the "isolated" core → periodic µs
stalls (35/06 ex 2).

### Trap 2 — SMT sibling not idled
The sibling core runs a backtest / a logger → it contends L1/L2/ports with
your hot thread → jitter. Idle the sibling, or SMT off on trading cores.

### Trap 3 — `SCHED_FIFO` busy-spin on a shared core
Starves kernel housekeeping → box hang. Isolated core + `SCHED_OTHER`
busy-poll is usually the right answer.

### Trap 4 — pre-faulting memory from the wrong node
First-touch places the page on the faulting thread's node. Pre-fault from
the thread that will use it, pinned.

### Trap 5 — a monitoring agent pinned nowhere
It floats onto your trading cores. `taskset -c 0,1` every non-trading
process, and check `/proc/*/stat` for interlopers.

### Trap 6 — pinning but leaving `numa_balancing` / `irqbalance` on
Both migrate things behind your back. Disable them.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`isolcpus` isolates the core" | scheduler only; add `nohz_full` + `rcu_nocbs` for timers/RCU |
| "SMT on is free throughput" | on trading cores the sibling steals resources → jitter |
| "`SCHED_FIFO` = lower latency always" | on an isolated core, CFS busy-poll often ties/wins, safer |
| "pin the thread, done" | + IRQ affinity + NUMA + disable balancing + idle the sibling |
| "one big affinity mask (cores 2-9)" | pin each hot thread to ONE core, alone |

---

## Exercises

1. Ek strategy thread core 4 pe pinned hai, `isolcpus=2-9` set hai,
   `mlockall` done. `perf stat -e cpu-migrations,context-switches` dikhata
   0 migrations par ~40 context-switches/sec. Kaun sa "context switch"
   ho raha jab kuch aur schedule nahi ho sakta?

   <details><summary>Answer</summary>

   `isolcpus` keeps the **CFS scheduler** from placing *runnable user tasks*
   on core 4, and 0 migrations confirms the thread isn't moving. But
   context-switches can still come from **kernel-side** activity that
   `isolcpus` alone doesn't stop:
   (1) **Scheduler tick / timer interrupt** — without `nohz_full=2-9`, the
   1000 Hz (or `CONFIG_HZ`) tick still fires on core 4; each is a brief
   switch into the timer softirq and back. `nohz_full` makes the core
   tickless when running a single task.
   (2) **RCU callbacks** — without `rcu_nocbs=2-9`, RCU grace-period work
   runs on core 4 periodically.
   (3) **`kworker` / per-CPU kernel threads** — some are bound per-CPU and
   run regardless (vmstat updates, etc.); reduced by `nohz_full` +
   `rcu_nocbs` but a few remain.
   (4) **The thread itself yielding** — a syscall that briefly blocks (a
   stray `clock_gettime(_RAW)`, a `futex`, a `write`), or `_mm_pause` in a
   poll loop is fine but an actual `sched_yield` isn't.
   Fixes: add `nohz_full=2-9 rcu_nocbs=2-9` to the cmdline, move any
   remaining IRQs off (`/proc/irq/*/smp_affinity`), and audit the thread for
   stray syscalls (`strace -c`). Target: context-switches ~0-2/sec (a few
   unavoidable kernel housekeeping ones).
   </details>

2. Do-socket box. NIC on node 0. Ek engineer strategy threads ko node 1 ke
   cores pe pin karta (node 1 pe zyada free cores the), memory `numactl
   --interleave=all` se allocate. p99 lab se 2× bura production mein. Kya
   galat, sahi placement?

   <details><summary>Answer</summary>

   Two NUMA mistakes compounding:
   (1) **Strategy threads on node 1, NIC on node 0.** Every packet the NIC
   DMAs lands in node-0 memory; the strategy thread on node 1 reads it
   **across the socket interconnect** (UPI/Infinity Fabric) — ~1.5–2× the
   latency of a local access, and under load the interconnect itself
   contends. The feed→strategy hot path is now paying a remote hop per
   message.
   (2) **`--interleave=all`** spreads every allocation's pages round-robin
   across both nodes → ~half of *all* hot-path memory accesses (pools,
   books, the ring) are remote, unpredictably. Interleave is for bandwidth-
   bound analytics that touch everything evenly, not for a latency path.
   Correct placement: **everything on node 0** — pin the NIC RX poll thread,
   the feed decoder, the strategy, and the order thread all to node-0 cores;
   allocate their memory on node 0 (`numactl --membind=0`, or pre-fault
   from node-0-pinned threads so first-touch places pages locally — lesson
   18); confirm with `numactl --hardware` and `/sys/class/net/<if>/device/
   numa_node`. If node 0 doesn't have enough cores, that's a capacity /
   hardware-selection problem, not something interleaving fixes. Put
   research/backtest (bandwidth-bound, latency-insensitive) on node 1.
   Also `kernel.numa_balancing=0`.
   </details>

---

## Interview questions

1. `isolcpus` vs `nohz_full` vs `rcu_nocbs` — what each stops.
2. A concrete core placement for NIC-poll / decode / strategy / risk / OS.
3. SMT on trading cores — why it's a jitter source; the two mitigations.
4. `SCHED_FIFO` vs CFS busy-poll on an isolated core — the trade-off and the hazard.
5. NUMA — first-touch, keeping NIC + threads + memory on one node, `numa_balancing` off.
6. Verifying a pin worked (`cpu-migrations`, `context-switches`, `/proc/interrupts`).

---

## Next
→ [`20-cache-warming.md`](20-cache-warming.md)
