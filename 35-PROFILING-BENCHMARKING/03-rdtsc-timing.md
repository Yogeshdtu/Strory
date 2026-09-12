# 03 — `rdtsc`: sub-nanosecond timing (aur uske jaal)

## Prerequisites
- `02-timing-correctly.md` (chrono, resolution, self-cost)
- **`34-ASSEMBLY/11-rdtsc-and-timing.md`** — yeh lesson usi ko benchmarking
  ke nazariye se aage badhata; asm-level detail wahan hai
- `34-ASSEMBLY/examples/05_rdtsc.cpp`, `31-CPU-ARCHITECTURE/13-frequency-and-power.md`

## Yeh topic abhi kyun
`chrono::steady_clock` ki floor is box pe **100 ns** hai, self-cost **~38 ns**.
Ek 20–80 ns hot function (matcher inner loop, parser stage) usse naapna
bekaar. `rdtsc` (Time-Stamp Counter) userspace se **sub-ns resolution**,
**no syscall** deta — par usko sahi use karne ke liye teen problem
handle karne padte: OoO execution, ticks≠ns, aur core hopping.

---

## `rdtsc` kya hai (recap — detail 34/11 mein)

- Per-core 64-bit counter. Modern CPU pe **invariant TSC**: **fixed rate**
  se tick karta (usually base freq / platform crystal), current core clock
  se nahi. Turbo/throttle se TSC rate **nahi** badalti.
- `__rdtsc()` (`<x86intrin.h>`) → `edx:eax` combine → 64-bit value.
- **Not serializing** — CPU ise program order se pehle/baad execute kar
  sakta.

```cpp
#include <x86intrin.h>
uint64_t a = __rdtsc();
hot();
uint64_t b = __rdtsc();      // b - a = ticks  (ns nahi!)
```

---

## Problem 1: OoO → fence chahiye

`rdtsc` float kar sakta. Fixes (badhti cost, badhti accuracy):

| Method | Kya |
|---|---|
| plain `__rdtsc()` | cheap (~**1 tick** self-cost measured), OoO-loose. **Lambe** interval (>~1 µs) ke liye theek — slop noise hai. |
| `lfence; __rdtsc()` | `lfence` prior instructions retire hone ka wait. `rdtsc` aage float nahi karta. |
| `_mm_lfence(); __rdtsc(); _mm_lfence();` | dono taraf fence — chhote region ka tight bracket. **~18–20 ticks** measured. |
| `__rdtscp(&aux); _mm_lfence();` | `rdtscp` mein partial serialize built-in + `aux` = core id. Trailing `lfence` for later code. |
| `cpuid; rdtsc` | full serialize; `cpuid` ~100+ cyc variable — overkill, historical. |

```cpp
static inline uint64_t tsc_start() { _mm_lfence(); uint64_t t = __rdtsc(); _mm_lfence(); return t; }
static inline uint64_t tsc_end()   { unsigned a; uint64_t t = __rdtscp(&a); _mm_lfence(); return t; }
```

**Measured (is box, `01_timing_methods.cpp` / `34/05`, ~2 GHz):**
```
  __rdtsc (plain)       : ~9 ns self-cost   (~18 ticks — includes call overhead in the loop)
  __rdtsc (raw, 34/05)  : ~1 tick           (tightest inline)
  lfence;rdtsc;lfence   : ~18 ns  (~37 ticks)
```
(Numbers depend on how tightly inlined — `34/05` measures the raw instruction;
`01_timing_methods` measures a realistic helper in a loop.)

**Rule:** region **< ~100 ns → fence + subtract self-cost**. Region
**> ~1 µs → plain `rdtsc`** (10 ns slop on 10 µs = noise).

---

## Problem 2: ticks ≠ nanoseconds → calibrate

Invariant TSC **base rate** pe tick karta. Agar core turbo 4.2 GHz pe hai
aur base 3.0 GHz, to 3.0e9 ticks = 1 sec, par core ne 4.2e9 **actual**
cycles kiye. Yani **TSC ticks ≈ reference cycles**, "work done" nahi.

**ns chahiye → calibrate:** TSC aur ek known clock ko do points pe padho
(~100+ ms apart), ratio nikaalo:

```cpp
double calibrate_ticks_per_ns() {
    auto c0 = std::chrono::steady_clock::now();
    uint64_t r0 = tsc_start();
    // busy ~150 ms (sleep nahi — sleep ke dauraan C-state / migration)
    volatile uint64_t s = 0;
    while (std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - c0).count() < 150) s = s + 1;
    uint64_t r1 = tsc_end();
    auto c1 = std::chrono::steady_clock::now();
    double dns = std::chrono::duration<double, std::nano>(c1 - c0).count();
    return double(r1 - r0) / dns;                 // ticks per ns
}
```

**Measured (is box): `1.9962 ticks/ns`** → TSC ~2.0 GHz. Har example jo
`rdtsc` use karta wahi calibration karta.

Startup pe **ek baar** calibrate karo, ratio store karo, aage sab reading
ns mein convert karo. VM / kuch laptops pe periodically re-calibrate.

---

## Problem 3: core hopping

Thread `t0` aur `t1` ke beech doosre core pe migrate → agar dono cores ke
TSC perfectly synced nahi (firmware usually karta, hamesha nahi), `t1 - t0`
garbage (even negative). Fixes:
- **Pin karo** measuring thread (`sched_setaffinity` / `SetThreadAffinityMask`
  — folder 29/06).
- `rdtscp` ka `aux` (core id) `t0` aur `t1` pe compare karo; badla to sample
  discard.
- Tuned HFT box (pinned, `isolcpus`, synced TSC) pe non-issue.

---

## Overhead subtraction — methodology

Fenced read ka self-cost ~18–20 ticks hai. Ek 40 ns (=~80 tick) region pe
woh 25% error. Isliye:

```cpp
// 1. self-cost measure karo (empty region), min-of-N
uint64_t fence_cost = UINT64_MAX;
for (int i = 0; i < 200000; ++i) {
    uint64_t a = tsc_start();
    uint64_t b = tsc_end();               // andar kuch nahi
    fence_cost = std::min(fence_cost, b - a);
}

// 2. asli region, self-cost minus
uint64_t best = UINT64_MAX;
for (int i = 0; i < 100000; ++i) {
    uint64_t a = tsc_start();
    hot_region();
    uint64_t b = tsc_end();
    best = std::min(best, b - a - fence_cost);
}
double ns = double(best) / ticks_per_ns;
```

**Min** lo (max-perturbed samples — interrupts, freq dips — discard). Ya ek
low percentile agar distribution chahiye.

---

## `rdtsc` vs `chrono` — kaunsa kab

| Situation | Timer |
|---|---|
| Interval ≥ ~1 µs, portable code | **`chrono::steady_clock`** — ns-direct, no calib, no core-hop worry |
| Linux, interval ≥ ~1 µs, low overhead | `clock_gettime(CLOCK_MONOTONIC)` via vDSO (~20 ns) |
| Interval < ~100 ns, aur tune kiya (fence + calib + pin) | **fenced `rdtsc`** |
| Per-instruction / uarch analysis | `perf` counters (lesson 10–11), `llvm-mca` (folder 34/12) |
| VM / container | prefer `chrono` — hypervisor `rdtsc` trap/offset kar sakta |

HFT: innermost hot segments `rdtsc`, coarser stage boundaries
`clock_gettime`, sab ek common ns timeline pe, p50/p99/p99.9 report.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — plain `rdtsc` for a 20 ns region
OoO slop + self-cost = 20 ns ka bada hissa. Fence, aur measured fence
overhead subtract.

### Trap 2 — TSC ticks ko cycles ya ns maan lena
Base-rate reference ticks. Calibrate for ns. Turbo pe `ticks != core_cycles`.

### Trap 3 — no pinning
Core hop → cross-core skew → nonsense (possibly negative) delta.

### Trap 4 — `sleep()` for the calibration wait
`sleep` ke dauraan deep C-state / migration → calibration window corrupt.
Busy-wait (`volatile` spin).

### Trap 5 — `__rdtsc()` ko compiler move kar deta
Bare `__rdtsc()` bina barrier ke optimizer move kar sakta. `_mm_lfence()`
CPU **aur** compiler dono ke liye barrier hai — isliye woh recipe safe.

### Trap 6 — measuring once
Interrupts / freq transitions / cache state → huge variance. Thousands of
runs, **min** (ya low percentile), distribution report.

### Trap 7 — VM mein `rdtsc`
Hypervisor `rdtsc` trap/emulate (slow, ~hundreds of ns) ya offset kar
sakta. `constant_tsc`/`nonstop_tsc` flags check karo; VM mein aksar
`clock_gettime` safer.

---

## Hands-on

```bash
./build.ps1 fast 35-PROFILING-BENCHMARKING/examples/01_timing_methods.cpp
./build.ps1 fast 34-ASSEMBLY/examples/05_rdtsc.cpp     # asm + raw self-cost
./build.ps1 asm  34-ASSEMBLY/examples/05_rdtsc.cpp     # rdtsc / rdtscp / lfence dekho
```
`01_timing_methods` mein calibration (~2.0 ticks/ns), teen timer ka
self-cost, aur ek 94 µs workload dono timers se (agree karte).

```bash
# Linux: apne CPU ki TSC properties
grep -o 'constant_tsc\|nonstop_tsc\|tsc_reliable\|rdtscp' /proc/cpuinfo | sort -u
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`rdtsc` gives cycles" | base-rate reference ticks; calibrate for ns |
| "plain `rdtsc` any interval ke liye fine" | short (<100 ns) → fence + self-cost subtract |
| "TSC rate turbo se badalta" | invariant TSC fixed rate |
| "pin karne ki zaroorat nahi" | core hop → cross-core skew → garbage |
| "ek reading kaafi" | thousands, min, distribution |
| "`rdtsc` = best timer, hamesha use karo" | ≥1 µs → `chrono`/`clock_gettime` simpler + safe |

---

## Exercises

1. Tum ek ~30 ns hot function ko `t0 = __rdtsc(); f(); t1 = __rdtsc();` se
   time kar rahe ho, numbers 10–80 ticks wildly vary karte. Sab galtiyan
   aur fix. *(34/11 se — dobara socho, benchmarking angle se.)*

   <details><summary>Answer</summary>

   (1) **No fencing** — `rdtsc` float. `lfence;rdtsc;lfence` (t0),
   `rdtscp;lfence` (t1). (2) **Self-cost not subtracted** — fenced read ~18
   ticks, 30 ns pe bada hissa. Empty region ka min measure, subtract. (3)
   **Compiler reorder** — `_mm_lfence()` compiler barrier bhi. (4) **One
   shot** — 100k runs, **min**. (5) **Not pinned** — pin ya `rdtscp` `aux`
   check. (6) **Not calibrated** — ticks→ns ratio. (7) **Not `-O2`** — debug
   build ka number bekaar. Result: min-of-100k, fenced, self-cost-subtracted,
   pinned, calibrated, `-O2` → stable.
   </details>

2. Calibration ke liye tumne `steady_clock` ke saath 150 ms busy-wait kiya
   aur `1.996 ticks/ns` mila. Agar tum busy-wait ki jagah `std::this_thread::
   sleep_for(150ms)` karte, ratio galat kyun aa sakta?

   <details><summary>Answer</summary>

   `sleep_for` thread ko **block** karta → OS use de-schedule karta. Us
   dauraan: (a) core **deep C-state** mein ja sakta — agar TSC `nonstop_tsc`
   nahi hai to TSC **pause** ho jaata (par `steady_clock` chalta rehta) →
   `Δticks` kam, `Δns` poora → ratio **under-estimate**. (b) thread wapas
   **doosre core** pe schedule ho sakta → cross-core TSC skew `Δticks` mein
   mix. (c) `sleep_for` ki granularity ~1–15 ms hai → actual sleep 150 nahi
   ~155–160 ms, `Δns` measurement thoda off (yeh chhota). Busy-wait: core
   awake, same core, TSC continuously ticking → clean ratio.
   </details>

3. `perf` `/proc/cpuinfo` mein `constant_tsc` hai par `nonstop_tsc` nahi.
   Benchmark ke liye kya risk?

   <details><summary>Answer</summary>

   `constant_tsc` = TSC rate core frequency se independent (turbo/throttle
   se nahi badalti) — stable time base. `nonstop_tsc` = TSC deep C-states
   mein **bhi** ticking rehta. Iske bina: agar measuring thread `t0`/`t1` ke
   beech kabhi **idle** ho (block, sleep, futex wait), core deep sleep mein
   jaake TSC pause kar sakta → interval undercount, aur cross-core TSC drift
   (alag cores alag der soye). Benchmark mitigation: hot path pe block mat
   karo (busy-poll), timing cores pe deep C-states disable
   (`processor.max_cstate=1` — folder 31/13), pin. Ya `rdtsc` chhod ke
   `clock_gettime(CLOCK_MONOTONIC)` (jo C-state se affected nahi).
   </details>

---

## Interview questions

1. TSC kya count karta (invariant TSC — base-rate reference ticks).
2. `rdtsc` ko fence kyun; `lfence` / `rdtscp` options aur cost.
3. Ticks → ns: calibration procedure, aur kyun `sleep` se nahi.
4. Core hopping — failure aur 2 mitigations.
5. Fenced read ka self-cost kaise subtract karte (empty-region min).
6. `rdtsc` vs `clock_gettime(CLOCK_MONOTONIC)` — kab kaunsa.
7. `constant_tsc` vs `nonstop_tsc`; VM mein `rdtsc` ka masla.

---

## Next
→ [`04-statistics.md`](04-statistics.md)
