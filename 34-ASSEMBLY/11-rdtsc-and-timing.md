# 11 — `rdtsc`, `rdtscp`, serialization, cycle counting

## Prerequisites
- `31-CPU-ARCHITECTURE/09-instruction-latency-throughput.md`, `13-frequency-and-power.md`
- `31-CPU-ARCHITECTURE/examples/08_rdtsc_timing.cpp`
- `05_rdtsc.cpp` example

## Yeh topic abhi kyun
`rdtsc` sabse fine-grained timer hai jo aap userspace se le sakte ho — ek
64-bit counter, ~sub-nanosecond resolution, no syscall. HFT instrumentation
(tick-to-trade, per-stage latency) aksar isi pe khadi hoti. Par usko sahi
use karna mushkil hai: out-of-order execution, frequency scaling, aur
core-hopping teenon galat number de sakte hain. Yeh lesson dono cheezein
sikhata — asm mein kaisa dikhta, aur correctly kaise use karo.

---

## What `rdtsc` reads

The **Time-Stamp Counter** — a per-core (architecturally) 64-bit counter.
On modern CPUs it's **invariant TSC**: it ticks at a **fixed rate**
(usually the CPU's *base* frequency, or a platform crystal rate), **not** the
current core clock — so turbo/throttle don't change the TSC rate (folder 31
lesson 13). It counts across sleep states and is synchronized across cores by
firmware (usually).

```asm
; __rdtsc()
        rdtsc                    ; edx = high 32, eax = low 32
        sal     rdx, 32
        or      rax, rdx         ; rax = full 64-bit counter

; __rdtscp(&aux)
        rdtscp                  ; edx:eax = counter, ecx = IA32_TSC_AUX (core/node id)
        mov     [rdi], ecx
        sal     rdx, 32
        or      rax, rdx
```

---

## Problem 1: out-of-order execution

`rdtsc` is **not a serializing instruction**. The CPU can execute it earlier
or later than its position in program order → your `t0`/`t1` may not bracket
the code between them.

Fixes (increasing cost, increasing accuracy):

| Method | asm | note |
|---|---|---|
| plain `__rdtsc()` | `rdtsc` | cheap (~1 tick self-cost measured — see below), but OoO-loose. Fine for **long** intervals where the slop is negligible. |
| `lfence; rdtsc` | `lfence` then `rdtsc` | `lfence` waits for all *prior* instructions to retire → `rdtsc` doesn't float earlier. Doesn't stop *later* code floating before `rdtsc`. |
| `lfence; rdtsc; lfence` | fence both sides | tight bracket for a short region. ~20 ticks measured. |
| `rdtscp` | `rdtscp` | has a built-in "wait for prior instructions" (partial serialization) + gives the core id in `ecx`. Still add a trailing `lfence` to stop later code. |
| `cpuid; rdtsc` | full serialize | most correct, but `cpuid` is ~100+ cycles and variable — overkill; historical. |

**Measured (this box, `05_rdtsc.cpp` / folder 31 `08`, ~2 GHz):**
```
  __rdtsc (plain)      : ~1 tick self-cost   (~0.5 ns)  -- near free
  lfence;rdtsc;lfence  : ~20 ticks           (~10 ns)
  rdtscp + lfence      : ~20 ticks           (~10 ns)
```
So: **short region (< ~100 ns) → fence it** (the ~10 ns overhead matters,
subtract it). **Long region → plain `rdtsc`** (10 ns of slop on a 10 µs
measurement is noise).

---

## Problem 2: TSC ticks ≠ core cycles ≠ nanoseconds

- Invariant TSC ticks at the **base** rate. If the core is in turbo at 4.2
  GHz and base is 3.0 GHz, 3.0 billion TSC ticks = 1 second, but the core
  did 4.2 billion *actual* cycles. So "TSC ticks" ≈ "reference cycles",
  **not** work done.
- To get **nanoseconds**, calibrate: read TSC and a known clock
  (`clock_gettime(CLOCK_MONOTONIC)` / `steady_clock`) at two points ~100+ ms
  apart, `ticks_per_ns = Δticks / Δns`. `05_rdtsc.cpp` does this →
  **~2.0 ticks/ns** on this ~2 GHz box.
- Re-calibrate periodically if the platform's TSC rate could drift (rare with
  invariant TSC, but VMs and some laptops).

---

## Problem 3: core hopping

The thread migrates to another core between `t0` and `t1` → if the two cores'
TSCs aren't perfectly synced (firmware usually syncs them, but not always),
`t1 - t0` can be garbage (even negative). Fixes:
- **Pin the measuring thread** (`sched_setaffinity` / `SetThreadAffinityMask`
  — folder 29 lesson 06).
- Use `rdtscp` and check `aux` (the core id) is the same at `t0` and `t1`;
  discard the sample if it changed.
- On a properly configured HFT box (pinned, `isolcpus`, synced TSC) this is a
  non-issue.

---

## Correct usage recipe (short interval)

```cpp
#include <x86intrin.h>
static inline uint64_t tsc_start() {
    _mm_lfence(); uint64_t t = __rdtsc(); _mm_lfence(); return t;
}
static inline uint64_t tsc_end() {
    unsigned aux; uint64_t t = __rdtscp(&aux); _mm_lfence(); return t;
}
// ...
uint64_t a = tsc_start();
hot_path();                      // the region under test
uint64_t b = tsc_end();
uint64_t ticks = b - a - FENCE_OVERHEAD;      // subtract the ~20-tick self-cost
double ns = ticks / ticks_per_ns;             // from calibration
```
Run it thousands of times, take the **minimum** (least perturbed by
interrupts / frequency dips — folder 32 example 01 uses min-of-N).

---

## `perf` / `clock_gettime` alternatives

- `clock_gettime(CLOCK_MONOTONIC)` — via **vDSO** (no syscall), ~20 ns on
  this class of box, returns nanoseconds directly (no calibration). Good for
  intervals ≥ ~1 µs. `CLOCK_MONOTONIC_RAW` = a real syscall (~250 ns), not
  NTP-adjusted. `CLOCK_MONOTONIC_COARSE` ~6 ns but ~1 ms granularity.
  (folder 29 lesson 15.)
- `std::chrono::steady_clock` — wraps `clock_gettime`; fine for
  ≥ microsecond timing, portable, no calibration.
- **`rdtsc` only when** you need sub-100-ns resolution *and* you've handled
  fencing + calibration + pinning.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — plain `rdtsc` for a 20-ns region
The OoO slop + the (small) self-cost are a large fraction of 20 ns. Fence
it, and subtract the measured fence overhead.

### Trap 2 — treating TSC ticks as cycles or nanoseconds
Invariant TSC ticks at base rate ≈ reference cycles. Calibrate to get ns;
don't assume `ticks == core_cycles` under turbo (folder 31 lesson 13).

### Trap 3 — no pinning
Thread migrates → cross-core TSC skew → nonsense (possibly negative) deltas.
Pin, or check `rdtscp`'s `aux`.

### Trap 4 — measuring once
Interrupts, frequency transitions, cache state → huge variance. Thousands of
runs, take the min (or a low percentile), report the distribution.

### Trap 5 — `rdtsc` in a VM
Hypervisors may trap/emulate `rdtsc` (slow, ~hundreds of ns) or offset it.
`rdtscp`/`cpuid` behaviour varies. Check `constant_tsc`/`nonstop_tsc` flags
and the hypervisor config; often `clock_gettime` is safer in a VM.

### Trap 6 — forgetting the fence is a *compiler* concern too
Between `__rdtsc()` and the code, the **compiler** can also reorder. The
`_mm_lfence()` is a compiler barrier as well as a CPU one; a bare `__rdtsc()`
with no barrier can be moved by the optimizer. (folder 33 lesson 14.)

---

## > **HFT relevance**

> - **Tick-to-trade instrumentation** — timestamp at each stage (packet in,
>   parsed, decision, order out) with fenced `rdtsc`, subtract the calibrated
>   self-cost, accumulate percentiles. `rdtscp` + `aux` check for core moves.
> - **Calibrate once at startup** (TSC ticks/ns) on the pinned core; store
>   the ratio; convert to ns for reporting.
> - **Frequency-lock the box** (folder 31 lesson 13) so "TSC ticks" and "core
>   cycles" and "wall ns" all move together and stay comparable across runs.
> - **Prefer `clock_gettime(CLOCK_MONOTONIC)` via vDSO** for anything ≥ 1 µs
>   — it's ~20 ns, returns ns directly, no calibration, no core-hop worry.
> - **Report distributions, not means** — p50/p99/p99.9/max (folder 14, 35).
>   `rdtsc` deltas over a soak, min-subtracted, bucketed.

---

## Hands-on

```bash
./build.ps1 fast 34-ASSEMBLY/examples/05_rdtsc.cpp
#   calibration ~2.0 ticks/ns ; __rdtsc self-cost ~1 tick ; fenced ~20 ticks
./build.ps1 asm  34-ASSEMBLY/examples/05_rdtsc.cpp
#   tsc_plain: `rdtsc; sal rdx,32; or rax,rdx`
#   tsc_lfenced: `lfence; rdtsc; ...; lfence`
#   tsc_p: `rdtscp; mov [..], ecx; ...; lfence`

./build.ps1 fast 31-CPU-ARCHITECTURE/examples/08_rdtsc_timing.cpp   # deeper: cyc/ns, loop cost

# check your CPU's TSC properties (Linux):
grep -o 'constant_tsc\|nonstop_tsc\|tsc_reliable\|rdtscp' /proc/cpuinfo | sort -u
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`rdtsc` gives cycles" | reference ticks at base rate; calibrate for ns |
| "plain `rdtsc` is fine for any interval" | short (<100 ns) → fence + subtract self-cost |
| "TSC rate changes with turbo" | invariant TSC ticks at a fixed rate |
| "no need to pin" | core hop → cross-core skew → garbage delta |
| "one measurement" | thousands, take the min, report the distribution |
| "`__rdtsc()` can't be moved by the compiler" | it can — the `lfence` is also a compiler barrier |

---

## Exercises

1. Tum ek ~30 ns hot function ko `t0=__rdtsc(); f(); t1=__rdtsc();` se time
   kar rahe ho aur numbers wildly vary karte hain (kabhi 10, kabhi 80).
   Kya-kya galat, sab theek karo.

   <details><summary>Answer</summary>

   (1) **No fencing** — `rdtsc` is not serializing; the CPU floats it around
   `f()`, so `t1 - t0` sometimes includes/excludes parts of `f` and
   surrounding code. Use `lfence; rdtsc; lfence` for `t0` and `rdtscp;
   lfence` for `t1`. (2) **Self-cost not subtracted** — the fenced read is
   ~20 ticks (~10 ns) itself; on a 30-ns function that's a third of the
   measurement. Measure the empty-region cost and subtract. (3) **Compiler
   reorder** — a bare `__rdtsc()` can be moved by the optimizer; the
   `_mm_lfence()` also acts as a compiler barrier. (4) **One shot** — a
   single sample catches interrupts / frequency dips. Run it 100k times, take
   the **minimum**. (5) **Not pinned** — pin the thread (or check `rdtscp`'s
   `aux`) so a core migration doesn't inject cross-core TSC skew. (6)
   **Not calibrated** — convert ticks→ns with a measured ticks/ns ratio.
   After all that: min-of-100k, fenced, self-cost-subtracted, on a pinned
   frequency-locked core → a stable number.
   </details>

2. `perf` `/proc/cpuinfo` mein `constant_tsc` hai par `nonstop_tsc` nahi.
   Kya matlab, kya risk?

   <details><summary>Answer</summary>

   `constant_tsc` = the TSC ticks at a **constant rate regardless of the
   core's current frequency** (turbo/throttle don't change the TSC rate) —
   so it's usable as a stable time base. `nonstop_tsc` (a.k.a.
   `invariant_tsc` when both are present) = the TSC **also keeps ticking in
   deep C-states** (when the core is halted/idle). Without `nonstop_tsc`, if
   the core enters a deep sleep state, the TSC may **pause** — so a `t1 - t0`
   that spans an idle period undercounts, and cross-core TSCs can drift apart
   because they slept different amounts. Risk for timing: if your measuring
   thread ever idles (blocks, sleeps, waits on a futex) between `t0` and
   `t1`, the interval is wrong. Mitigations: busy-poll instead of blocking on
   the hot path, disable deep C-states on the timing cores
   (`processor.max_cstate=1` — folder 31 lesson 13), and pin. On a properly
   tuned HFT box you'd want full invariant TSC.
   </details>

3. `clock_gettime(CLOCK_MONOTONIC)` vs fenced `rdtsc` — ek per-stage
   tick-to-trade instrument ke liye kaunsa, kyun?

   <details><summary>Answer</summary>

   **Depends on the stage granularity.** If stages are ~microseconds or more
   (packet-in → parsed → decision → order-out on a normal path),
   `clock_gettime(CLOCK_MONOTONIC)` via the **vDSO** is the better default:
   ~20 ns to call, returns nanoseconds **directly** (no calibration, no
   ticks-to-ns), NTP-smoothed so it's comparable across the machine and over
   time, and it doesn't care about core hops. If you need **sub-100-ns**
   resolution on individual stages (e.g. inside the matcher), **fenced
   `rdtsc`** wins — but only if you've handled: fencing (`lfence`/`rdtscp`),
   self-cost subtraction, one-time calibration to ns, thread pinning, and a
   frequency-locked box. Many HFT shops use `rdtsc` for the innermost hot
   segments and `clock_gettime` for the coarser stage boundaries, all
   converted to a common ns timeline, and report p50/p99/p99.9 over a soak.
   </details>

---

## Interview questions

1. What the TSC counts (invariant TSC — base-rate reference ticks).
2. Why `rdtsc` needs fencing, and the `lfence`/`rdtscp` options.
3. TSC ticks vs core cycles vs nanoseconds — how to get ns (calibration).
4. Core hopping — the failure, and 2 mitigations.
5. `rdtsc` self-cost (plain vs fenced) — approximate numbers, and when it matters.
6. `constant_tsc` vs `nonstop_tsc`.
7. When to prefer `clock_gettime(CLOCK_MONOTONIC)` over `rdtsc`.

---

## Next
→ [`12-disassembly-tools.md`](12-disassembly-tools.md)
