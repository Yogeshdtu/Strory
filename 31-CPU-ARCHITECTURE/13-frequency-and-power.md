# 13 — Frequency & power: turbo, C-states, P-states, thermal throttling

## Prerequisites
- `01-how-cpu-works.md` (clock, cycles vs ns)
- `29-LINUX-SYSTEMS` file 06 (governor, `/sys`), file 18 (tuning checklist)

## Yeh topic abhi kyun
Ek "cycle" ki keemat ns mein tab constant hoti jab frequency constant ho. Real
CPUs frequency ko **constantly badalti** — idle pe girati (power save), load pe
badhati (turbo), garam hone pe girati (throttle), AVX-512 pe girati. HFT ke liye
yeh **jitter ka bada source** hai: wahi code alag-alag time leta depending on
what frequency the core happens to be at. Isi liye HFT box frequency ko **lock**
karta aur deep idle states **disable** karta. Yeh lesson: kaunse knobs, kyun.

---

## P-states — performance (frequency/voltage) levels

The CPU runs at one of several **P-states**: (frequency, voltage) pairs. Higher
P-state = higher GHz, higher power, more heat.

| P-state | Meaning |
|---|---|
| **P0** | max non-turbo (the "base" or "nominal" frequency stamped on the box) |
| **turbo / boost** (above P0) | opportunistic — allowed when few cores active, thermal + power headroom exists; **not guaranteed**, varies second-to-second |
| **P1..Pn** | lower frequency/voltage steps for power saving |

Who chooses: the **cpufreq governor** (`ondemand`, `schedutil`, `powersave`,
**`performance`**) via `intel_pstate` / `amd_pstate` / `acpi-cpufreq` drivers.

```bash
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor        # performance = pin high
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq        # right now (kHz)
watch -n0.2 'grep MHz /proc/cpuinfo'                             # live per-core
turbostat --interval 1                                          # Bzy_MHz, C-state %, PkgWatt
```

- `governor = performance` → the core sits at max frequency (base + turbo when
  possible) instead of ramping up from idle (which has a latency: ~tens of µs to
  a few ms to reach turbo). **Removes the "first request is slow because the
  core was at 800 MHz" jitter.**
- **Turbo itself is still non-deterministic** — it depends on how many cores are
  busy and the package temperature. Some HFT setups **disable turbo**
  (`echo 1 > /sys/devices/system/cpu/intel_pstate/no_turbo`) and run at a fixed,
  guaranteed base frequency — trading a bit of peak speed for a **constant** ns/
  cycle.

---

## C-states — idle (sleep) levels

When a core has nothing to run, it enters a **C-state** to save power. Deeper =
more power saved, **longer to wake up**.

| C-state | What's off | Wake latency (typical) |
|---|---|---|
| **C0** | running | — |
| **C1 / C1E** | clock gated | ~1 µs or less |
| **C3** | L1/L2 flushed, PLL off | ~few–tens of µs |
| **C6** | core powered off, state saved to L3/RAM | ~tens–100+ µs |
| **C7+ / package C-states** | whole package sleeps, LLC flushed | ~100s of µs |

**The problem:** an event-driven thread that blocks (epoll, condvar, `nanosleep`)
lets its core drop to C6. The next event has to wake it — **+tens to 100+ µs of
pure latency**, and the L1/L2 are cold (they were flushed). This is a classic
"why is my p99.9 terrible" cause.

**Fixes (HFT):**
- **Busy-poll** so the core never idles (folder 30 file 8) — it stays in C0.
- **Disable deep C-states:** cmdline `processor.max_cstate=1
  intel_idle.max_cstate=1`, or `idle=poll` (spins instead of sleeping — max
  determinism, max power/heat), or `cpupower idle-set -D 0` at runtime, or the
  PM QoS interface (`/dev/cpu_dma_latency` — write a small µs value to cap wake
  latency).
- Housekeeping cores can keep C-states (they're not latency-critical).

```bash
cpupower idle-info                              # available C-states + residency
turbostat --show CPU%c1,CPU%c6,Pkg%pc6          # how much time in each
cat /proc/cmdline | grep -o 'max_cstate=[^ ]*'
```

---

## Thermal throttling

If the die hits its temperature limit (Tjmax, ~95–105 °C), the CPU **forcibly
drops frequency** (and can insert stall cycles — "thermal throttling" /
`PROCHOT`) regardless of the governor. Sustained heavy load + inadequate cooling
= your carefully-locked frequency drops mid-session, silently.

- `turbostat` shows `PkgTmp` and a throttle flag; `/sys/class/thermal/`,
  `perf stat -e thermal.throttled` (some µarchs).
- HFT boxes: serious cooling, good airflow, sometimes under-clocked deliberately
  to stay well under Tjmax, monitored temperature with alerts.

---

## AVX / AVX-512 frequency offsets

On some Intel parts (Skylake-X, Cascade Lake), running "heavy" 256-bit AVX2/FMA
or *any* AVX-512 makes the core drop to a **lower licence frequency** — and it
stays low for ~hundreds of µs *after* the SIMD code, so neighbouring scalar code
also slows (file `10` trap 5).

- Check: `turbostat` frequency while running AVX vs scalar; or Intel's
  documented "AVX/AVX-512 turbo" tables for your SKU.
- Mitigations: prefer 128-bit SIMD (no/small offset), keep AVX bursts short,
  measure end-to-end. Ice Lake+, Sapphire Rapids, and AMD Zen 4 largely removed
  the penalty.

---

## Uncore / interconnect frequency

The L3 (LLC), memory controller, and inter-core mesh/ring run at their own
**uncore frequency**, also power-managed. Under light load it can drop, making
L3 and cross-core latency (folder 32) *worse* and jittery. HFT: pin uncore high
(`UNCORE_RATIO_LIMIT` MSR, or vendor tools like Intel's `wrmsr` scripts / the
`intel-uncore-frequency` sysfs on newer kernels).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — benchmarking on a laptop / default box, quoting absolute ns
Frequency scaling means the same code takes 1.2 ns/op at 2 GHz and 0.6 ns/op at
4 GHz — and it drifts *within a run* as the CPU ramps and throttles. This
folder's examples measure ~2 GHz on this power-save laptop; an HFT box is
locked ~3–5 GHz. **Quote ratios; measure absolutes on the target with frequency
locked.**

### Trap 2 — `governor = performance` but leaving turbo + C-states default
`performance` pins the P-state high, but turbo is still opportunistic
(non-deterministic) and deep C-states still apply when the core idles. For full
determinism: `performance` + `no_turbo` (or accept turbo variance) +
`max_cstate=1` / busy-poll.

### Trap 3 — deep C-states on a latency core
An event-driven thread that blocks → core to C6 → next event +tens-to-100 µs
wake + cold cache. Busy-poll or disable deep C-states on that core.

### Trap 4 — ignoring thermal throttle under sustained load
Your locked 4.2 GHz silently becomes 3.6 GHz at hour 3 of the trading day
because cooling is marginal. Monitor `PkgTmp` and frequency; alert on drops.

### Trap 5 — AVX-512 "4× faster loop" that downclocks the core
Net end-to-end can be a loss (file `10`). Measure wall time, not the loop in
isolation. Narrower SIMD or skip it on affected SKUs.

### Trap 6 — `idle=poll` on every core
It forces *all* cores to spin instead of sleeping → huge power draw, heat →
thermal throttle → *worse* frequency. Scope it: hot cores busy-poll at the app
level anyway; you don't need `idle=poll` for them, and housekeeping cores should
keep C-states.

### Trap 7 — forgetting uncore frequency
CPU core at 4 GHz but uncore at 1.6 GHz under light load → L3 and cross-core
latency inflated and jittery. Pin uncore high on the trading box.

---

## > **HFT relevance**

> - **Lock the frequency.** `governor = performance` on all cores; on the
>   trading cores, `no_turbo` for a *guaranteed constant* base frequency (or
>   accept turbo's variance if you've measured it's small). Goal: ns/cycle is a
>   constant.
> - **Kill deep idle on latency cores.** `processor.max_cstate=1
>   intel_idle.max_cstate=1` (cmdline) and/or busy-poll so the core stays in C0.
>   `/dev/cpu_dma_latency` PM-QoS as a belt. Housekeeping cores keep C-states.
> - **Pin uncore frequency high** so L3 / memory-controller / mesh latency
>   doesn't sag under light load.
> - **Monitor temperature and actual frequency** (`turbostat`) with alerts —
>   thermal throttle silently defeats all of the above.
> - **AVX/AVX-512:** check the downclock offset on your SKU; prefer 128-bit or
>   keep bursts short if it's significant (file `10`, `15`).
> - **Cooling matters** — serious airflow, headroom under Tjmax, sometimes a
>   deliberate under-clock for stability.
> - Full list: folder 29 file 18 (tuning checklist), sections 0 (BIOS) and 3
>   (runtime).

---

## Hands-on

```bash
# live frequency + C-state residency + temperature
turbostat --interval 1 --show Core,CPU,Bzy_MHz,CPU%c1,CPU%c6,PkgTmp,PkgWatt

# lock frequency high
for g in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do echo performance | sudo tee $g; done
echo 1 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo         # optional: fixed base

# disable deep C-states at runtime
sudo cpupower idle-set -D 0

# example 08: this box's effective frequency (calibrated from rdtsc vs steady_clock)
./build.ps1 fast 31-CPU-ARCHITECTURE/examples/08_rdtsc_timing.cpp        # ~2.0 GHz here (laptop)

# OS-level jitter that frequency/C-states cause
sudo cyclictest -m -p99 -t4 -a2-5 -i200 -D30                            # max latency
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "a cycle is a fixed amount of time" | ns/cycle changes with frequency; frequency changes constantly |
| "`governor=performance` = fully deterministic" | turbo still opportunistic; C-states still apply on idle |
| "C-states only affect power" | deep C-state wake = +tens–100 µs latency + cold cache |
| "the CPU stays at the frequency I set" | thermal throttle overrides everything at Tjmax |
| "AVX is always a win" | AVX/AVX-512 downclock can make it a net loss (file 10) |
| "`idle=poll` everywhere for latency" | all cores spinning → heat → thermal throttle → slower |

---

## Exercises

1. Ek event-driven strategy thread `epoll_wait` pe block karti hai jab market
   quiet ho. Ek burst aane pe pehla event handle karne mein extra ~80 µs lagta
   hai vs busy period mein. Frequency/power ke nazariye se kya ho raha, do fixes?

   <details><summary>Answer</summary>

   During the quiet period the core had no work → it dropped to a deep C-state
   (C6): powered off, L1/L2 flushed, and (with `ondemand`/`schedutil` governor)
   the frequency also fell toward minimum. The burst's first event must: wake the
   core (~tens of µs for C6 exit), ramp the frequency back up (~tens of µs to ms
   to reach turbo), and re-populate cold L1/L2 (misses on the working set). Fixes:
   (1) **Busy-poll** instead of blocking — the core stays in C0 at full
   frequency, caches warm. (2) If it must block: `governor=performance` +
   `processor.max_cstate=1` (or write a low µs value to `/dev/cpu_dma_latency`
   PM-QoS) so wake latency is capped and frequency doesn't sag. (Also warm the
   caches periodically with a heartbeat.)
   </details>

2. `turbostat` dikhata: trading hours ke pehle 2 ghante `Bzy_MHz` ~4200,
   phir ~3600, `PkgTmp` 98 °C. Kya ho raha, fix?

   <details><summary>Answer</summary>

   **Thermal throttling.** The die reached ~Tjmax (~98–100 °C), so the CPU
   forcibly dropped frequency (4200 → 3600 MHz, ~14%) to shed heat, overriding
   your `performance` governor and any locked P-state. Every hot-path function is
   now ~14% slower and the transition itself is a jitter event. Fixes: improve
   **cooling** (airflow, heatsink, ambient temperature, fan curves), run with
   **headroom** (a deliberate under-clock so sustained load stays well under
   Tjmax, e.g. cap at 3.8 GHz fixed), reduce **power draw** (disable turbo,
   fewer active cores), and **monitor** `PkgTmp` + frequency with alerts so you
   catch it before it costs a trading session.
   </details>

3. HFT box pe `governor=performance` set hai par pehla order har subah ~1 ms
   slow hai (folder 29 file 13 ne page faults cover kiye — yeh us se alag ek
   frequency angle hai). Kya?

   <details><summary>Answer</summary>

   Even with `performance`, if turbo is enabled the core has to *ramp* from base
   to turbo frequency when it first gets sustained work — that ramp is ~tens of
   µs to a couple of ms depending on the µarch and the "energy performance
   preference" hints. If the core was also in a deep C-state overnight (idle),
   add the C6 wake. The first order of the day hits a core that's at base
   frequency and possibly just waking. Fixes: **disable turbo** (`no_turbo=1`) so
   there's no ramp — the core is always at the guaranteed base frequency; keep
   the core out of deep C-states (`max_cstate=1` / a warm-up heartbeat that keeps
   it in C0); and a **pre-open warm-up** that runs the hot path so the core is
   already at steady frequency and C0 before the first real order.
   </details>

4. Tumhare SIMD-heavy risk-aggregation loop AVX2 se 3× tez hai (isolated
   benchmark). Production Cascade Lake box pe end-to-end tick-to-trade sirf
   1.3× improve hua, aur *non-SIMD* code bhi thoda slow lagta hai us waqt.

   <details><summary>Answer</summary>

   Cascade Lake (Skylake-X family) applies an **AVX2/AVX-512 frequency offset**:
   running heavy 256-bit FMA drops the core to a lower licence frequency, and it
   **stays low for ~hundreds of µs after** the SIMD burst ends — so the scalar
   code that runs right after (the rest of tick-to-trade) also executes at the
   reduced frequency. Your 3× loop, run at ~0.75× frequency and dragging the
   neighbouring code to ~0.85×, nets ~1.3× end-to-end. Options: use **128-bit
   AVX** (much smaller/no offset), keep the SIMD burst as short as possible and
   isolated, or on this SKU skip vectorization for this loop and spend the cycles
   elsewhere. On a Zen 4 / Sapphire Rapids box the offset is gone and the 3×
   would carry through — so it's a per-µarch decision (file `15`).
   </details>

5. Ek dev laptop pe folder 31 ke examples ~2 GHz pe measure karte hain (example
   `08` calibration). Un absolute ns numbers ko production HFT box (locked 4.5
   GHz) pe kaise interpret karo?

   <details><summary>Answer</summary>

   Convert via the frequency ratio, but only as a rough guide, and trust the
   **ratios** the examples report, not the absolutes. E.g. example `02`: serial
   chain ~1.2–1.7 ns/op at 2 GHz ≈ ~2.5–3.5 cycles/op → at 4.5 GHz that's
   ~0.6–0.8 ns/op. The *ratio* "4 parallel chains ≈ 4× the serial rate" holds on
   both boxes (it's a property of the microarchitecture's port structure, not the
   frequency). Same for example `05` (SSE 4×, AVX2 ~9.5×), `03`/`04` (~6–7×
   misprediction cost). To get real absolute numbers for a latency budget: run
   the benchmark **on the production box, frequency locked, C-states off, SMT
   off** — that's the only measurement that feeds a tick-to-trade budget.
   </details>

---

## Interview questions

1. P-states vs C-states — what each is, which affects latency and how.
2. Turbo — why it's non-deterministic; why an HFT box might disable it.
3. Deep C-state wake latency — the number, and why it's an HFT problem.
4. Thermal throttling — what triggers it, how it defeats a locked frequency, how to detect.
5. AVX/AVX-512 frequency offset — what it is, when it makes SIMD a net loss.
6. Uncore frequency — what it governs, why pin it high.
7. Why must you quote *ratios* from a laptop benchmark and measure *absolutes* on a frequency-locked target?

---

## Next
→ [`14-numa-architecture.md`](14-numa-architecture.md)
