# 06 — Jitter: HFT ka asli dushman

## Prerequisites
- `04-statistics.md`, `05-percentiles.md`
- `31-CPU-ARCHITECTURE/13-frequency-and-power.md` (C-states, turbo, SMI)
- `29-LINUX-SYSTEMS/06` (CPU affinity), `29/15` (timers), `32` (cache/TLB)
- `examples/03_jitter_measure.cpp`

## Yeh topic abhi kyun
Ek function jo **normally 200 ns** leta par **kabhi-kabhi 50000 ns** — uska
mean shayad theek dikhe, par woh 50 µs spike woh moment ho sakta jab market
move ho raha tha aur tum order nahi bhej paaye. **Jitter = same kaam, alag
time.** Tail latency (lesson 05) ka root cause aksar jitter hi hota. Yeh
lesson: jitter aata kahan se, naapo kaise, aur kaam kaise karo.

---

## Jitter ki definition

Ek **deterministic** piece of work (same code, same data, same size) ko
baar-baar chalao. Ideal duniya mein har run **exactly same time**. Asli
duniya mein:

```
  run time:  200  201  200  205  200  1400  200  200  38000  201  ...
             \________ bulk ________/  ^spike        ^big spike
```

**Jitter** = in run times ka variation. Metrics:
- **min** — jitter-free floor (asli cost)
- **max − min**, ya **max / min** — jitter ka magnitude
- **p99.9 − p50** — tail spread
- **spike count** — kitne samples > (say) 2× min

**Measured (`03_jitter_measure.cpp`, is unpinned Windows box, identical
128-element hash per iteration, 300k iters):**

```
           min    med    p99    p99.9      max      max/min   spikes(>2x)/300k
  CLEAN   200.4  200.4  260.5   460.9   55124.8      275x        ~2800
  NOISY   200.4  200.4  460.9   611.2  131149.2      655x        ~6300
```

`min` dono mein **200.4 ns** — asli kaam wahi hai. Sab upar ka **pure
interference** hai. CLEAN phase (kuch aur nahi ho raha) mein bhi **~1%
iterations 2× se dheemi**, aur ek sample **55 µs** (275× floor) — ek OS
timer interrupt / scheduler tick. NOISY phase (har 200 iters pe ek
`malloc`+`free`+`yield`) — spike count **2×**, max **131 µs**.

> Single `max` khud noisy hai (CLEAN ka max NOISY se bada nikal aaya ek run
> mein — dono OS-interrupt outliers). **Robust jitter signal = spike count +
> p99.9**, ek `max` nahi.

---

## Jitter ke sources (kam se zyada control)

### Software / OS
| Source | Kya hota | Fix |
|---|---|---|
| **Timer interrupt** (tick) | har CPU pe periodic IRQ (100–1000 Hz) → ~µs stall | `nohz_full` (tickless), isolated cores |
| **Scheduler preemption** | tumhara thread off-CPU, doosra chala | pin (`taskset`/`sched_setaffinity`) + `isolcpus` + `SCHED_FIFO` |
| **Other IRQs** (NIC, disk, timers) | interrupt handler tumhare core pe chala | IRQ affinity → non-trading cores; `irqbalance` off |
| **Page fault** | pehli baar page touch → minor fault (~µs) ya major (ms, disk) | pre-fault + `mlockall`; huge pages; no lazy alloc on hot path |
| **`malloc`/`free` on hot path** | free-list walk, `mmap`/`munmap`, lock | pre-allocate, object pools (folder 14), no alloc in steady state |
| **Syscalls on hot path** | kernel entry (~hundreds ns) + maybe sleep/reschedule | busy-poll instead of blocking; batch; `io_uring` |
| **Logging / I/O** | format + write + maybe fsync, often with a lock | async ring buffer, format off hot path (lesson 16) |
| **Contended lock / false sharing** | spin/futex; cache-line ping-pong (folder 27) | lock-free (folder 28), pad shared data, per-thread state |
| **Signal delivery** | handler runs inline | block signals on hot threads |
| **THP compaction / `khugepaged`** | kernel background merges pages → stall | tune or disable THP for the process |
| **Context switch cost** | TLB flush, cache cold, register save | fewer threads than cores; pin; isolate |

### Hardware / firmware
| Source | Kya hota | Fix |
|---|---|---|
| **Frequency scaling** | P-state change; turbo ramp; AVX downclock | `performance` governor, fixed freq, disable turbo for consistency (folder 31/13) |
| **C-states** | idle core sleeps; wake latency (µs) + TSC pause (no `nonstop_tsc`) | `processor.max_cstate=1` / `idle=poll` on trading cores |
| **SMI** (System Management Interrupt) | firmware steals the core, OS-invisible, 10s–100s of µs | BIOS: disable USB legacy, thermal SMI; check `hwlat` detector |
| **Cache/TLB pollution** | co-tenant thread evicts your lines | LLC partitioning (Intel CAT), isolate, prefetch-friendly layout (folder 32) |
| **SMT (hyperthread) sibling** | shares execution ports / L1 / L2 with your thread | leave sibling idle, or disable SMT on trading cores |
| **NUMA remote access** | memory on the other socket → ~2× latency | pin memory local (`numactl --membind`), first-touch on the right node |
| **DVFS / thermal throttle** | sustained load → clock drop | cooling, power headroom, monitor `MSR_PERF_STATUS` |
| **Memory refresh / bank conflicts** | small periodic DRAM stalls | mostly unavoidable; keep working set in cache |

---

## Jitter naapo kaise

`03_jitter_measure.cpp` ka pattern:

```cpp
// 1. ek FIXED unit of work — deterministic
uint64_t unit(const uint32_t* a) { /* 128-element hash, always same */ }

// 2. har iteration timestamp, delta record
std::vector<double> samples; samples.reserve(N);
for (int i = 0; i < N; ++i) {
    uint64_t t0 = tsc_start();
    uint64_t h  = unit(buf);
    uint64_t t1 = tsc_end();
    sink(h);
    samples.push_back(double(t1 - t0) / ticks_per_ns);
}

// 3. min, p50, p99, p99.9, max, spike-count, worst-N list
```

Extra techniques:
- **Inter-arrival jitter** — agar tum ek periodic loop chala rahe ho (har
  10 µs ek iteration hona chahiye), to har iteration ke **start timestamp**
  record karo aur consecutive difference dekho — target 10 µs se kitna
  bhataka. Yeh "scheduling jitter" alag se dikhata.
- **`hwlatdetect`** (Linux `rtla` / `hwlat` tracer) — SMI aur hardware
  latency spikes detect karta jo OS ko dikhte bhi nahi.
- **`cyclictest`** (rt-tests) — RT scheduling jitter ka standard tool.
- **`perf sched`** / **`perf timehist`** — kaun sa thread kab kis core pe
  chala, preemption events.
- Long **soak** (hours) — thermal, cron jobs, log rotation jaise slow
  periodic sources sirf lambe run mein dikhte.

---

## Jitter kam karo — ek HFT box ka "quiet core" recipe

```
BIOS / firmware:
  - turbo OFF (ya locked all-core), C-states -> C1 max, P-state -> fixed
  - hyperthreading OFF on trading cores (ya sibling idle)
  - disable legacy USB, thermal-SMI knobs; enable "max performance" profile

Kernel cmdline (isolate cores 2-5, say):
  isolcpus=2-5  nohz_full=2-5  rcu_nocbs=2-5  irqaffinity=0,1
  intel_pstate=disable  processor.max_cstate=1  idle=poll
  transparent_hugepage=never  mce=off (careful)  audit=0

Runtime:
  - pin each hot thread to one isolated core (sched_setaffinity)
  - SCHED_FIFO / SCHED_RR priority (careful: can hang the box if it spins
    without yielding on a non-isolated core)
  - mlockall(MCL_CURRENT | MCL_FUTURE)  -> no page faults
  - pre-allocate ALL memory at startup; object pools; zero malloc steady-state
  - huge pages for big buffers (reduce TLB misses + faults)
  - move all IRQs off the isolated cores (/proc/irq/*/smp_affinity)
  - no logging / syscalls / locks on the hot path -> SPSC ring to a
    non-trading "housekeeping" thread
  - busy-poll the NIC (DPDK / kernel busy_poll / io_uring SQPOLL)
  - warm every code path + cache before going live
```

After all that, `03_jitter_measure`-style measurement pe CLEAN phase ki
tail bahut girti — spikes ~0, p99.9 ~= p50 + few ns. **Yeh hi "deterministic
latency" ka matlab hai** jo HFT firms poore stack pe engineer karte.

---

## > **HFT relevance**

> - **Jitter = missed trades.** Ek 50 µs spike jab ek stock 2 ticks move
>   kiya = tum late, doosron ne le liya, ya tumhe adverse fill mila.
> - **Determinism > raw speed.** Ek path jo hamesha 800 ns leta > ek path
>   jo usually 500 ns par kabhi 40 µs. p99.9 flat rakhna hi asli kaam.
> - **Whole-stack.** NIC → kernel bypass → parse → strategy → risk → order
>   encode → NIC. Har stage ka p99.9 measure, har jitter source (upar wali
>   tables) systematically kill.
> - **Measure continuously in production** — `rdtsc` timestamps per stage,
>   always-on histogram (lesson 16), p50/p99/p99.9 dashboards. Ek nayi
>   jitter source (kernel update, BIOS change, noisy neighbour) turant
>   dikhe.
> - **The "quiet core" is sacred** — koi cron, koi monitoring agent, koi
>   `top`, kuch bhi us core ko touch na kare.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — jitter ko "measurement noise" samajhna
Woh noise hai — par **real system noise**, jo production mein bhi hoga.
Usko measure karke kill karna hi kaam hai, ignore karna nahi.

### Trap 2 — sirf `max` dekhna
Single sample, infinitely noisy. **Spike count** (> 2× min) aur **p99.9**
robust hain.

### Trap 3 — pin kiye bina "deterministic latency"
Unpinned thread har scheduler tick pe migrate/preempt ho sakta. Pinning +
`isolcpus` foundation hai.

### Trap 4 — `SCHED_FIFO` bina soche
Agar woh thread ek non-isolated core pe busy-spins aur yield nahi karta, to
woh core ka sab kuch (including kernel housekeeping) bhookha mar jaata → box
hang. Isolated core + watchdog awareness zaroori.

### Trap 5 — `malloc` "sirf startup pe" maan lena
`std::vector::push_back` grow pe `malloc` karta; `std::string` SSO se bada
hone pe; exceptions; `std::function`; `shared_ptr` control block. Hot path
audit karo — ek bhi allocation ~µs spike de sakta.

### Trap 6 — SMI ko bhoolna
BIOS/firmware core churata hai, OS ko pata bhi nahi. Agar unexplained
10–100 µs spikes hain aur software sab tuned hai — `hwlatdetect` chalao,
BIOS SMI knobs dekho.

---

## Hands-on

```bash
./build.ps1 fast 35-PROFILING-BENCHMARKING/examples/03_jitter_measure.cpp
```
CLEAN vs NOISY: min dono 200 ns; NOISY ka spike-count ~2×, max bahut bada.
Yeh unpinned Windows box hai — CLEAN mein bhi ~1% iterations 2×+ (timer
interrupt). Ek pinned Linux isolated core pe CLEAN ki tail lagbhag flat hoti.

```bash
# Linux jitter tools:
sudo cyclictest -m -p 99 -i 100 -h 400 -q     # RT scheduling jitter histogram
sudo rtla timerlat hist -c 2 -d 10s           # timer latency (SMI + IRQ)
perf sched record -- ./app ; perf sched latency
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "average latency 800 ns, hum theek hai" | p99.9 dekho — jitter wahan chhupi hai |
| "jitter = measurement error" | real system interference; production mein bhi |
| "faster code = less jitter" | alag axis — determinism alag se engineer karo |
| "pinning se bas thoda fayda" | pinning + isolcpus = jitter reduction ka base |
| "malloc sirf startup" | vector grow / string / exceptions / shared_ptr bhi |
| "software tune kiya, spikes gaye" | SMI / C-states / freq — hardware/firmware bhi |

---

## Exercises

1. Ek hot function ka jitter measurement: min 300 ns, p50 305, p99 320,
   p99.9 **9000 ns**, max 9200. Bulk bahut tight hai, par p99.9 pe ek cliff.
   Yeh kis tarah ka jitter source suggest karta (ek discrete ~9 µs event,
   ~0.1% frequency)?

   <details><summary>Answer</summary>

   Tight bulk + ek **discrete, consistent ~9 µs jump** at ~0.1% rate → yeh
   ek **periodic, fixed-cost event** hai, random noise nahi. Candidates: (a)
   **timer tick / scheduler tick** — agar function ~300 ns hai aur tick 250
   Hz (har 4 ms), aur function har ~4 µs call hota, to ~0.1% calls tick ke
   saath overlap — par tick usually ~µs, 9 µs thoda zyada. (b) **IRQ
   coalescing** — NIC har N packets ya T µs pe ek interrupt, handler ~9 µs,
   tumhare core pe. (c) **TLB shootdown** — koi doosra thread `munmap`/`mprotect`
   kar raha, IPI se tumhare core ka TLB flush (~µs, scales with cores). (d)
   **C-state exit** agar function bursty hai (beech mein core soya). Fix
   path: `perf record -e irq:*` / `perf sched` us 0.1% ko catch karne ko;
   IRQ affinity check; `nohz_full` + isolate; agar TLB shootdown — dekho kaun
   `mmap`/`madvise` kar raha steady-state mein.
   </details>

2. Tumne thread ko core 3 pe pin kiya, `isolcpus=3` set kiya, `mlockall`
   kiya, zero malloc steady-state — phir bhi har ~1 second pe ek 30 µs
   spike. Kya baaki bacha?

   <details><summary>Answer</summary>

   Per-second periodicity → kuch **1 Hz** ho raha. Candidates: (a)
   **`vmstat` / kernel per-CPU housekeeping** — kernel abhi bhi isolated
   core pe kuch timers rakhta jab tak `nohz_full=3` + `rcu_nocbs=3` na ho
   (sirf `isolcpus` scheduler ko rokta, timers ko nahi). (b) **RCU callbacks**
   — `rcu_nocbs` ke bina RCU grace-period work tumhare core pe. (c) **`mlock`
   ke bawajood THP `khugepaged`** har ~1s scan karta —
   `transparent_hugepage=never` ya `defrag=never`. (d) **monitoring agent** —
   koi telemetry daemon har 1s `/proc` scan karta aur scheduler use tumhare
   core pe daal deta (agar woh `SCHED_OTHER` hai aur core technically
   "isolated" nahi CPU-mask se). (e) **CPU freq governor** sampling har 1s.
   Fixes: `nohz_full=3 rcu_nocbs=3` add karo, THP off, freq governor
   `performance` (no sampling), agent ko explicit `taskset -c 0,1` do,
   verify `cat /proc/interrupts` + `/proc/timer_list` for core 3.
   </details>

3. `cyclictest` clean (max 8 µs) dikhata par tumhari application ka p99.9
   phir bhi 40 µs hai. `cyclictest` OK hone ke bawajood application jitter
   kyun?

   <details><summary>Answer</summary>

   `cyclictest` sirf **scheduling / wakeup jitter** naapta — ek sleeping
   thread ko time pe wake karna. Woh application-specific jitter miss karta:
   (a) **cache / TLB misses** in the app's actual working set (cyclictest ka
   footprint tiny hai) — folder 32; (b) **branch mispredicts** on cold
   paths; (c) **lock contention** / false sharing between the app's own
   threads (folder 27); (d) **`malloc`** or other allocations the app does
   that cyclictest doesn't; (e) **data-dependent work** — app ka "same"
   request actually alag amount of work karta (alag order book depth, alag
   message size); (f) **NIC / kernel-bypass path** jitter jo cyclictest touch
   hi nahi karta. `cyclictest` clean = "OS/scheduler foundation theek hai";
   ab app ko khud instrument karo (per-stage `rdtsc` + histogram, lesson 16)
   aur uske apne jitter sources dhoondo.
   </details>

---

## Interview questions

1. Jitter ki definition; kaunse metrics (min, spike count, p99.9−p50) — max kyun nahi.
2. Jitter ke 6+ sources, software aur hardware/firmware dono.
3. `isolcpus` vs `nohz_full` vs `rcu_nocbs` — teenon kya karte, teenon kyun chahiye.
4. `malloc` hot path pe kyun jitter; kaunse "chhupe" allocations (vector/string/shared_ptr).
5. SMI kya hai, kyun OS-invisible, kaise detect.
6. `cyclictest` kya cover karta aur kya nahi — clean cyclictest ke baad bhi app jitter kyun.
7. HFT "quiet core" — 5 cheezein jo tum us core se door rakhoge.

---

## Next
→ [`07-histograms.md`](07-histograms.md)
