# 10 — CPU scheduling: CFS, SCHED_FIFO, priorities

## Prerequisites
- `01-linux-architecture.md` (scheduler as a kernel subsystem)
- `26-CONCURRENCY` (threads, preemption)

## Yeh topic abhi kyun
Scheduler decide karta **kaunsa thread kaunse core pe, kab, kitni der**. HFT ke
liye yeh direct latency-jitter ka source hai: har baar tumhare hot thread ko
preempt kiya gaya = ek P99.9 spike. Yeh lesson batata scheduler kaise sochta,
aur uski marzi se kaise bacha jaaye — priorities, real-time policies, aur
(sabse effective) core isolation (`11`).

---

## Linux scheduling classes (priority order)

Kernel har runnable thread ko ek **scheduling class** mein rakhta; higher class
hamesha lower ko preempt karta:

```
  STOP        (kernel internal: CPU hotplug, migration)   -- highest
  DEADLINE    (SCHED_DEADLINE: runtime/deadline/period)
  RT          (SCHED_FIFO, SCHED_RR: priority 1..99)
  CFS/EEVDF   (SCHED_OTHER, SCHED_BATCH, SCHED_IDLE)       -- normal, lowest
  IDLE-task   (swapper, jab kuch aur na ho)
```

99% userspace threads **CFS** (SCHED_OTHER) mein hote — including tumhara
default `std::thread`.

---

## CFS / EEVDF — the "fair" scheduler

CFS (Completely Fair Scheduler; kernel 6.6+ mein EEVDF ne replace kiya, idea
similar) har thread ko **CPU time ka fair share** deta:

- Har thread ka **`vruntime`** (virtual runtime) — kitna CPU use kiya, `nice`
  weight se scaled. Scheduler hamesha **sabse kam `vruntime`** wala runnable
  thread chunta (red-black tree se O(log n)).
- Ek thread jitna chala, uska `vruntime` badhta → doosron ka number aata.
- **`nice`** (−20 se +19, default 0): CPU-share weight. `nice -20` ≈ 88× share
  of `nice 0`; `nice 19` ≈ 1/88×. Sirf **ratio** — nice −20 thread bhi preempt
  ho sakta agar aur runnable threads hain.
- **Timeslice** dynamic (`sched_min_granularity`, `sched_latency`) — typically
  ~1–6 ms. Har timeslice ke end pe (ya jab koi higher-vruntime-deficit thread
  wake ho) → **preemption**.

**Iska matlab HFT ke liye:** ek CFS hot thread ko OS *guarantee* nahi karta ki
woh core chhodega nahi. Koi bhi doosra runnable thread (logging, GC, cron,
kernel worker) usse timeslice pe preempt kar sakta → context switch (~1–5 µs)
+ cache pollution + possible migration.

`SCHED_BATCH` (CPU-bound, no interactivity boost), `SCHED_IDLE` (sirf jab kuch
aur na ho — background junk) bhi CFS ke andar.

---

## Real-time: `SCHED_FIFO` aur `SCHED_RR`

RT class CFS ke **upar** — koi bhi RT thread saare CFS threads ko preempt karta,
hamesha.

### `SCHED_FIFO` (First-In-First-Out)
- **Koi timeslice nahi.** Ek FIFO thread jab tak chahe chalta — jab tak woh
  khud block/yield na kare, ya koi **higher-priority** RT thread ready na ho.
- Priority 1 (low) se 99 (high). Same priority pe: jo pehle aaya woh chalta jab
  tak chhode na.

### `SCHED_RR` (Round-Robin)
- Same as FIFO par same-priority threads ke beech ek timeslice
  (`sched_rr_timeslice_ms`, ~100 ms) pe round-robin.

```cpp
#include <sched.h>
sched_param sp{};
sp.sched_priority = 80;                              // 1..99
if (sched_setscheduler(0, SCHED_FIFO, &sp) != 0)
    perror("need CAP_SYS_NICE / root / limits.conf rtprio");
```
**Example `10`** CFS vs FIFO ka periodic-wakeup jitter compare karta.

### `SCHED_DEADLINE` (EDF + CBS)
Har thread: `(runtime, deadline, period)` — "har `period` mein `runtime` CPU
chahiye, `deadline` se pehle". Kernel admission control karta (overload reject).
Periodic tasks ke liye theoretically ideal; HFT mein kam use (busy-poll model
period-based nahi).

---

## RT ke khatre

### 1. Starvation / system lockup
`SCHED_FIFO` thread jo yield nahi karta (busy loop, no `sleep`/`nanosleep`/
blocking I/O) → us core pe **kuch aur chalega hi nahi**, including kernel
threads. Single-core box pe = hang.

**Guardrails:**
- `kernel.sched_rt_runtime_us` / `sched_rt_period_us` — default: RT ko 1 second
  mein max 0.95s milta, 0.05s CFS ke liye reserved. HFT boxes ispe `-1` (no
  throttle) set karte **par tabhi jab** RT thread isolated core pe ho aur
  well-behaved.
- `RLIMIT_RTTIME` — ek RT thread continuous kitna CPU le sakta bina blocking,
  warna `SIGXCPU`.

### 2. Priority inversion
Low-prio RT thread ne lock liya, high-prio RT thread us lock pe block → mid-prio
thread chalta rehta. `pthread_mutexattr_setprotocol(PTHREAD_PRIO_INHERIT)` se
priority inheritance, ya — HFT way — hot path pe koi shared lock hi nahi
(folder `28`).

### 3. Migration / load-balance stalls
RT threads bhi migrate ho sakte cores ke beech (agar affinity se pinned nahi)
→ cache cold. Hamesha RT thread ko ek core pe pin karo (`11`).

---

## The HFT reality: kai shops `SCHED_OTHER` + isolation + busy-poll

Counter-intuitive: bahut saare HFT desks `SCHED_FIFO` **nahi** use karte. Instead:
- Hot core ko `isolcpus`/`cpuset` se scheduler ke general pool se hata do (`11`).
- Us core pe **sirf ek** pinned thread (CFS, default priority) — jab core pe
  koi aur runnable thread hi nahi, to CFS "fair share" trivially 100% deta.
- Thread ek **busy-poll loop** — kabhi block nahi karta → kabhi voluntary
  context switch nahi.
- Result: FIFO ke starvation/inversion risks ke bina, FIFO jitna (ya behtar)
  determinism. `nonvoluntary_ctxt_switches` ~0.

`SCHED_FIFO` tab jeetta jab tumhe **shared** cores pe priority chahiye, ya ek
periodic RT task jo sona bhi hai. Pure busy-poll + isolated core ke liye woh
extra risk deta hai, extra determinism nahi.

---

## Internal working

- **Tick + tickless:** timer interrupt (`CONFIG_HZ`, usually 250/1000 Hz) pe
  scheduler check karta preempt karna hai ya nahi. `nohz_full=<cpu>` boot param
  se: agar core pe sirf 1 runnable task hai to timer tick bhi band (kam jitter,
  `15`).
- **Wakeup preemption:** ek thread jo I/O se wake hota, agrecfeature uska
  vruntime deficit bada hai to woh running thread ko turant preempt kar sakta
  (`WAKEUP_PREEMPTION`) — latency ke liye achha, throughput ke liye thoda bura.
- **Load balancing:** har few ms, scheduler cores ke beech runnable threads
  redistribute karta (`sched_domains`, NUMA-aware). Yeh migration ka source —
  affinity se rok do.
- **CPU capacity / frequency:** `schedutil` governor scheduler load se frequency
  decide karta; latency ke liye `performance` governor + fixed freq (`06`).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `nice -20` ko "ab koi preempt nahi karega" samajhna
`nice` sirf CFS share ratio. `nice -20` thread abhi bhi timeslice khatam hone
pe / higher-deficit wake pe preempt hoga. Determinism chahiye → isolation ya RT.

### Trap 2 — `SCHED_FIFO` bina isolation + bina yield
Shared core pe ek non-yielding FIFO thread → kernel threads (RCU, workqueue,
softirq) starve → system-wide weirdness, watchdog, ya hang. `sched_rt_runtime_us`
throttle bacha leta par tab tumhe periodic 50ms CFS-window ki jitter milti.

### Trap 3 — RT thread jo `malloc`/`log`/`mutex` karta
RT thread ka page fault ya lock-wait poore latency budget ko blow karta, aur
priority inversion la sakta. RT thread: pre-allocated, `mlock`ed, lock-free,
no syscalls on hot path.

### Trap 4 — `sched_setscheduler` fail hone pe silently CFS pe chalna
Privilege na ho (`CAP_SYS_NICE`, `/etc/security/limits.conf` `rtprio`) → call
`-1`/`EPERM` deta, thread CFS pe hi. Return check karo; production mein hard-fail.

### Trap 5 — hyperthread sibling ignore karna
Core 3 pe pin kiya, par core 3 ka SMT sibling (e.g. CPU 19) pe OS ne kuch aur
schedule kiya → dono logical CPUs ek physical core ke execution units share
karte → tumhare hot thread ki IPC gir gayi. Sibling ko bhi isolate/idle rakho.

### Trap 6 — `cond_var`/`futex` wale design mein RT priority daalna
RT + blocking sync = classic priority inversion + wakeup latency (futex syscall
~1–2 µs). RT threads ko busy-poll (spin) design chahiye, sleep-wake nahi.

---

## > **HFT relevance**

> - **Preemption = jitter.** Har `nonvoluntary_ctxt_switch` (`/proc/self/status`,
>   `06`) ek unwanted P99.9 event. Goal: hot thread pe yeh count ~0.
> - **Two valid setups:**
>   1. Isolated core (`isolcpus`+`nohz_full`) + pinned CFS busy-poll thread +
>      `mlockall`. Simple, safe, deterministic. Most common.
>   2. `SCHED_FIFO` prio 80–90 + pinned + isolated + `sched_rt_runtime_us=-1` +
>      well-behaved (yields or is alone). For mixed workloads / periodic RT.
> - **Housekeeping cores** (`0` and its sibling, usually) OS, IRQs, cron,
>   monitoring, logging threads le lete. Hot cores ko chhod do.
> - **Never** RT priority on a thread that `malloc`s, logs, or takes a mutex on
>   its hot path.
> - **Measure it:** `perf sched latency`, `/proc/<pid>/schedstat`, and
>   `nonvoluntary_ctxt_switches` before/after a tuning change.

---

## Hands-on

```bash
# Linux pe -- example 10: CFS vs FIFO periodic wakeup jitter
g++ -std=c++20 -O2 -pthread 29-LINUX-SYSTEMS/examples/10_realtime_thread.linux.cpp -o /tmp/rt
sudo /tmp/rt                    # FIFO ke liye privilege

# ek thread ki scheduling policy dekho
chrt -p <tid>                  # policy + priority
ps -eLo pid,tid,cls,rtprio,ni,psr,comm | grep -i trade   # cls: TS=CFS, FF=FIFO, RR

# RT throttle setting
cat /proc/sys/kernel/sched_rt_runtime_us   # 950000 default (0.95s per 1s)

# preemption dekho
grep ctxt /proc/self/status
perf sched record -- sleep 5 && perf sched latency | head
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`nice -20` = no preemption" | sirf CFS share ratio; preempt still hota |
| "SCHED_FIFO hamesha behtar HFT ke liye" | isolated CFS busy-poll aksar barabar/behtar, kam risk |
| "RT thread migrate nahi hoga" | hoga, unless affinity-pinned |
| "timeslice fixed 100ms hai" | CFS timeslice dynamic ~1–6 ms; FIFO ka koi nahi |
| "pin kiya to sibling se farak nahi" | SMT sibling execution units share karta — use bhi idle rakho |
| "sched_setscheduler fail = crash" | `-1`/`EPERM`, silently CFS — return check |

---

## Exercises

1. Ek CFS thread ko dedicated core pe pin kiya, us core pe koi aur runnable
   thread nahi. Kya woh 100% CPU paata? `nice` value maayne rakhti?

   <details><summary>Answer</summary>

   Haan, ~100% — CFS "fair share" tabhi bat-ta hai jab **multiple** runnable
   threads hon. Akela runnable thread pura core le leta. `nice` irrelevant
   (koi kis se fair hona hai?). Isi liye "isolated core + pinned CFS thread"
   pattern kaam karta bina RT priority ke. (Timer tick abhi bhi aayegi unless
   `nohz_full` — `15`.)
   </details>

2. `SCHED_FIFO` prio 50 thread ek `pthread_mutex` (default) lock ke liye block
   karta hai jo ek CFS thread ne hold kiya. Kya ho sakta hai?

   <details><summary>Answer</summary>

   Priority inversion: FIFO thread block, CFS lock-holder ko koi RT-level boost
   nahi → agar aur CFS threads hain, holder ko CPU milne mein der → FIFO thread
   lamba wait. `PTHREAD_PRIO_INHERIT` protocol lock-holder ko temporarily
   FIFO-50 tak boost karta. HFT fix: hot path pe lock hi mat lo.
   </details>

3. `kernel.sched_rt_runtime_us = 950000` ka matlab, aur HFT box pe kyun `-1`
   karte hain (aur kya guard chahiye)?

   <details><summary>Answer</summary>

   Default: har 1s (`sched_rt_period_us`) mein RT threads ko max 0.95s milta,
   0.05s CFS/kernel ke liye reserved — taaki ek runaway FIFO thread system ko
   hang na kar de. Iska side-effect: har 1s mein ~50ms ka window jahan tumhara
   FIFO thread **force-throttled** — ek periodic jitter spike. `-1` = no
   throttle. Guard: FIFO thread ek **isolated** core pe ho (koi kernel thread
   starve na ho) aur woh occasionally yield kare ya alone ho.
   </details>

4. Tuning change ke baad kaise verify karoge ki hot thread ab preempt nahi ho
   raha?

   <details><summary>Answer</summary>

   `grep nonvoluntary_ctxt_switches /proc/<tid>/status` before/after a fixed
   workload — ~0 hona chahiye isolated+pinned ke baad. `perf sched latency` se
   per-thread wait/switch stats. `/proc/<tid>/schedstat` (field 2 = time waiting
   on runqueue). `cat /proc/<tid>/stat` field 39 (`processor`) same rehna
   chahiye (no migration).
   </details>

5. Kyun kuch HFT teams `SCHED_FIFO` ke bajaye "isolated core + CFS busy-poll"
   chunti hain?

   <details><summary>Answer</summary>

   Isolated core pe akela thread already ~100% CPU + no preemption (koi
   competitor nahi). FIFO uspe koi extra determinism nahi deta, par deta hai:
   starvation risk (kernel RCU/workqueue threads), priority-inversion surface,
   `sched_rt_runtime` throttle jitter, aur privilege/config complexity. Kam
   moving parts, same result → CFS + isolation. FIFO tab jeetta jab core shared
   ho ya thread ko sona bhi ho.
   </details>

---

## Interview questions

1. Linux scheduling classes priority order — DEADLINE/RT/CFS.
2. CFS `vruntime` + `nice` — thread kaise chuna jaata?
3. `SCHED_FIFO` vs `SCHED_RR` — timeslice ka farak.
4. `SCHED_FIFO` ke 3 khatre (starvation, inversion, migration) + guards.
5. `nice -20` deterministic latency kyun nahi deta?
6. "Isolated core + CFS busy-poll" FIFO se compare — trade-offs.
7. `sched_rt_runtime_us` throttle — kya, kyun, HFT box pe kaise handle.

---

## Next
→ [`11-cpu-affinity.md`](11-cpu-affinity.md)
