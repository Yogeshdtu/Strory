# 12 — Hyperthreading (SMT): resource sharing, why HFT disables it

## Prerequisites
- `05-superscalar.md` (ports, ILP), `06-out-of-order-execution.md` (ROB/RS/PRF)
- `29-LINUX-SYSTEMS` file 11 (SMT siblings, isolation)

## Yeh topic abhi kyun
**SMT** (Simultaneous MultiThreading; Intel calls it Hyper-Threading) ek physical
core ko **do logical CPUs** dikhata hai. Idea: jab ek thread stall ho (cache
miss, dependency chain), doosra thread us core ke idle execution units use kar
le → better *throughput*. Par HFT *latency* chahiye, aur SMT latency ke liye
**bura** hai — do threads ek core ke L1, ports, ROB share karte, ek doosre ki
predictability tod dete. Isi liye HFT boxes SMT **disable** karte. Yeh lesson
kyun.

---

## What SMT actually shares

A physical core has one set of execution resources. SMT lets two threads' µops
flow through them. What's **shared** vs **partitioned**:

| Resource | Under SMT |
|---|---|
| **Execution ports** (ALU, mul, div, load, store, vec) | **shared** — the two threads compete every cycle |
| **L1 data cache** (~32–48 KB) | **shared** — half the effective capacity per thread; evictions from the sibling |
| **L1 instruction cache / µop cache** | **shared** |
| **L2 cache** | shared (already shared per-core) |
| **TLB** (data + instruction) | shared |
| **Branch predictor / BTB** | **shared** — the sibling's branches pollute your history |
| **ROB, Reservation Stations, Physical Register File** | **statically partitioned** (~half each) or competitively shared, depending on µarch — either way, **your OoO window shrinks** |
| **Store buffer / load buffer** | partitioned |
| Architectural registers, program counter | per-thread (that's the point) |

So on a core with SMT active and both siblings busy, each thread gets:
~½ the L1, ~½ the OoO window, a polluted branch predictor, and full contention
for the execution ports.

---

## Why SMT helps *throughput* (the case for it)

If thread A is stalled on a cache miss (200+ cycles, file `06`), its execution
ports sit idle. Thread B's ready µops fill them. Aggregate work/second goes up —
typically **~1.1–1.3×** for mixed server workloads, sometimes more for
memory-latency-bound code (databases, web servers), less or negative for
compute-bound code that already saturates the ports.

SMT is a good deal when: you care about total throughput, your threads stall a
lot (memory-bound), and per-thread latency doesn't matter.

---

## Why SMT hurts *latency* (the case against, for HFT)

| Effect | Consequence for a latency-critical thread |
|---|---|
| **Port contention** | your ready µop waits because the sibling grabbed the port → variable, unpredictable extra cycles |
| **L1 pollution** | the sibling evicts your hot lines → your loads that "should" be L1 hits become L2/L3 → +tens of cycles, randomly |
| **Halved OoO window** | fewer in-flight µops → less latency hiding, more stalls |
| **Branch predictor pollution** | the sibling's branch history/BTB entries collide with yours → more mispredicts (file `07`) |
| **Non-determinism** | your thread's timing now depends on **what the sibling is doing**, which you don't control → jitter, fat p99 tail |
| **Security** | cross-sibling side channels (MDS, L1TF, PortSmash) — one more reason (file `08`) |

The killer is the **last two**: HFT is about a tight, predictable latency
distribution. A hot thread whose per-iteration time depends on a co-scheduled
sibling has an irreducible jitter source.

---

## The HFT decision: SMT off (usually)

Most low-latency trading shops **disable SMT** so each physical core runs exactly
one thread, with the full L1/ports/ROB/predictor to itself.

**How:**
- **BIOS/UEFI:** disable Hyper-Threading / SMT — the cleanest (logical CPUs
  simply don't exist).
- **Kernel cmdline:** `nosmt` (folder 29 file 18) — offlines the sibling
  hyperthreads.
- **Runtime:** `echo off > /sys/devices/system/cpu/smt/control`.
- **Partial:** keep SMT on but **leave the sibling of each hot core idle**
  (isolate both logical CPUs, pin your thread to one, never schedule anything on
  the other — folder 29 file 11). Gets you most of the determinism without
  losing the siblings for housekeeping.

**When a shop keeps SMT on:** non-latency workloads (research, backtests,
market-data recording, risk batch) on the *other* socket or *other* cores —
those benefit from the throughput. The trading path's cores are SMT-off (or
sibling-idle).

---

## Detecting SMT and finding siblings

```bash
lscpu | grep -E 'Thread|Core|Socket'
cat /sys/devices/system/cpu/smt/active            # 1 = SMT on
cat /sys/devices/system/cpu/cpu3/topology/thread_siblings_list   # e.g. "3,19"
lscpu -e=CPU,CORE,SOCKET,NODE                     # CPU 3 and 19 share CORE 3
```

If `thread_siblings_list` for your hot core shows two CPUs, pin your hot thread
to one and make sure the other is isolated and idle (folder 29 file 11 startup
self-check).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — pinning to a logical CPU without checking its sibling
You pin the feed thread to CPU 3; the OS schedules a `kworker` (or your logging
thread) on CPU 19 — same physical core. Now your feed thread shares L1/ports
with it. Isolate **both** siblings; use only one.

### Trap 2 — "8 cores" that are really 4 cores × 2 threads
`std::thread::hardware_concurrency()` returns 8; you make an 8-thread pool; it's
4 physical cores. On a compute-bound pool that's ~4-5× effective, not 8×, plus
the threads fight over L1. Size pools to *physical* cores for compute work.

### Trap 3 — benchmarking with SMT on, deploying with it off (or vice versa)
Your numbers won't transfer. Match the dev/prod SMT config, or at least measure
both.

### Trap 4 — expecting SMT to double a single hot thread's speed
It does nothing for a single thread (there's no sibling to fill the idle
cycles). It only helps *aggregate throughput* of two+ threads, and only when
they stall.

### Trap 5 — leaving SMT on "for the housekeeping cores" but not isolating hot-core siblings
Fine to keep SMT for cores 0–1 (OS, IRQs, logging). The mistake is forgetting
that hot core 6's sibling (say CPU 22) is still schedulable — the scheduler will
put stuff there. `isolcpus`/`cpuset` must cover both.

### Trap 6 — ignoring the security angle
If your box runs any code you don't fully trust (containers, a research job on a
sibling), cross-SMT side channels are real. SMT-off closes them (file `08`).

---

## > **HFT relevance**

> - **SMT off on trading boxes** (BIOS or `nosmt`), so every hot thread owns its
>   physical core's full L1, ports, OoO window, and branch predictor — and its
>   latency doesn't depend on an uncontrolled sibling.
> - **Or**: SMT on, but every hot core's sibling is isolated and left idle
>   (`isolcpus` covers both siblings; startup self-check asserts the sibling is
>   unused — folder 29 file 11).
> - **Non-latency work** (research, backtests, recording, risk batch) can keep
>   SMT on — on the *other* socket or non-hot cores — for the throughput.
> - **Size compute thread pools to physical cores**, not `hardware_concurrency()`.
> - **The reason is jitter, not average:** SMT's throughput gain doesn't help a
>   latency distribution; its L1/predictor/port contention widens the p99 tail.

---

## Hands-on

```bash
# is SMT on, and what are the siblings
lscpu | grep -E 'Thread\(s\) per core|Core\(s\)'
cat /sys/devices/system/cpu/smt/active
for c in /sys/devices/system/cpu/cpu[0-9]*; do
  echo "$c: siblings $(cat $c/topology/thread_siblings_list)"
done

# turn SMT off at runtime (needs root)
echo off | sudo tee /sys/devices/system/cpu/smt/control

# a feel for the contention: run example 02 (compute-bound) with 2 threads pinned
# to the same physical core vs 2 different cores -- the same-core pair is slower
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "SMT = 2× the cores" | 2 logical CPUs sharing one core's L1/ports/ROB; ~1.1–1.3× throughput |
| "SMT speeds up my single hot thread" | no sibling to fill idle cycles → no benefit for one thread |
| "pinned to CPU 3, so I'm alone" | CPU 3's sibling (e.g. 19) is still schedulable — isolate both |
| "hardware_concurrency() = physical cores" | counts logical CPUs; halve for physical under SMT |
| "SMT only affects throughput, not latency" | L1/predictor/port contention = per-iteration jitter |
| "keep SMT for the whole box" | fine for housekeeping/research cores; off (or sibling-idle) for hot cores |

---

## Exercises

1. Ek core pe SMT on hai, dono siblings busy. Ek sibling ek compute-bound loop
   chala raha (ports 90% busy), doosra ek pointer-chase (memory-bound). Kaunsa
   sibling zyada slow hota vs alone chalne se, aur kyun?

   <details><summary>Answer</summary>

   The **compute-bound** loop slows down the most. It was already using ~90% of
   the execution ports alone; the memory-bound sibling, while mostly stalled on
   misses, still issues occasional µops that steal port slots and evict L1 lines
   → the compute loop's IPC drops noticeably. The memory-bound loop barely
   changes — it was stalled anyway, and now the "idle" cycles it would have
   wasted are used by the sibling (this is exactly the throughput case for SMT).
   Net: aggregate throughput up, but the compute thread's latency got worse and
   jittery. For HFT the compute thread is the one you care about → SMT off.
   </details>

2. `thread_siblings_list` for your hot core (CPU 6) says `6,22`. You pin the
   strategy thread to CPU 6 and `isolcpus=6`. Kya galat, fix?

   <details><summary>Answer</summary>

   `isolcpus=6` isolates only CPU 6 — CPU 22 (the SMT sibling, same physical
   core) is still in the scheduler's general pool. The OS will schedule
   `kworker`s, your logging thread, cron, etc. on CPU 22, and they share the
   physical core's L1, execution ports, and branch predictor with your strategy
   thread → jitter. Fix: `isolcpus=6,22` (both siblings), pin the strategy
   thread to one, leave the other completely unused; or disable SMT entirely
   (`nosmt` / BIOS) so CPU 22 doesn't exist. A startup self-check should read
   `thread_siblings_list`, confirm the sibling is isolated and idle, and
   hard-fail otherwise (folder 29 file 11).
   </details>

3. Ek research/backtest cluster hai jo raat bhar historical replay chalata.
   SMT on ya off?

   <details><summary>Answer</summary>

   **On.** Backtesting is a throughput job — you want maximum total work/hour
   across the cluster, per-run latency doesn't matter, and replay is often
   memory-latency-bound (streaming historical data, cache-missing on lookups) —
   exactly where SMT's ~1.1–1.3× (sometimes more) applies. Run it on machines
   *separate* from the live trading boxes (or on the trading boxes' *other*
   socket / non-hot cores), with SMT on, and let the scheduler pack both
   siblings. Live trading path: SMT off. The two configs coexist because they're
   different machines / different core sets.
   </details>

4. SMT off karne se ek 16-logical-CPU box 8-physical-CPU ban gaya. Tumhare
   trading process ke thread layout pe kya asar (folder 29/30 se connect)?

   <details><summary>Answer</summary>

   You now have 8 real cores to allocate instead of 16 logical CPUs. Plan: e.g.
   cores 0–1 = housekeeping (OS, NIC IRQs, logging, monitoring — folder 29 file
   15/17); cores 2–5 = the four hot pipeline stages (feed decode, book build,
   strategy, order encode — folder 30 file 16), one pinned thread each, each
   owning its core's full L1/ports/predictor; cores 6–7 = spare / control plane
   / warm standby. Boot: `isolcpus=2-7 nohz_full=2-7 rcu_nocbs=2-7
   irqaffinity=0-1`. You "lost" 8 logical CPUs but gained deterministic latency
   on the 6 you use for trading — a good trade. Non-latency work goes to the
   other socket or a different box.
   </details>

5. `perf stat` par SMT-on aur SMT-off dono config mein ek hot loop chalaya:
   SMT-on IPC 1.8, SMT-off IPC 2.6, but SMT-on ka *aggregate* (both siblings)
   "instructions/sec" zyada. Kaunsa config chuno HFT ke liye aur kyun?

   <details><summary>Answer</summary>

   **SMT-off.** The single hot thread's IPC went from 1.8 → 2.6 (44% faster
   per-iteration, and more importantly *more consistent* — no sibling contention
   variance). The aggregate throughput being higher with SMT-on is irrelevant to
   HFT: you don't care how much *total* work the box does, you care that *your
   one latency-critical thread* completes each iteration fast and predictably.
   SMT-on trades your thread's latency + determinism for aggregate throughput
   you don't need. (If the box also runs non-latency batch work, put *that* on
   different cores/socket with SMT on, and keep the trading cores SMT-off.)
   </details>

---

## Interview questions

1. SMT — what a physical core exposes, what's shared vs partitioned.
2. Why SMT helps throughput (stall-filling) and by roughly how much.
3. Why SMT hurts latency — port contention, L1 pollution, halved OoO window, predictor pollution, non-determinism.
4. The HFT decision — SMT off, or SMT on with hot-core siblings isolated; when each.
5. `thread_siblings_list` — how to find a core's sibling; the pinning mistake.
6. Sizing a compute thread pool — physical cores vs `hardware_concurrency()`.
7. SMT and side channels — the security angle (file `08`).

---

## Next
→ [`13-frequency-and-power.md`](13-frequency-and-power.md)
