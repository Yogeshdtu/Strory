# 17 — `<chrono>`: durations, time points, clocks, timing code correctly

## Prerequisites
- [`16-functional.md`](16-functional.md), folder 03 (integer types, ratios)
- Folder 35 preview (profiling) — this is the foundation

## Yeh topic abhi kyun
Har benchmark, har latency measurement, har timeout `<chrono>` pe khada hai.
Iska type system pehli baar confusing lagta (`duration<Rep, Period>`,
`time_point<Clock>`) par woh aapko **unit bugs se bachata** — seconds ko
nanoseconds mein add nahi kar sakte galti se. Aur **kaunsi clock** — `steady_clock`
vs `system_clock` — ek classic mistake hai.

`examples/07_chrono_timing.cpp` sahi timing ka template hai.

---

## Durations

```cpp
#include <chrono>
using namespace std::chrono;

duration<long long, std::nano>  d1 = nanoseconds(1500);
nanoseconds  ns  = microseconds(2);        // 2000ns  -- implicit: no precision lost
milliseconds ms  = 5ms;                     // <chrono> literals: ns, us, ms, s, min, h
auto         hz  = 1s / 3;                  // duration arithmetic

// convert:
auto us = duration_cast<microseconds>(nanoseconds(2500));    // 2  -- TRUNCATES toward zero
double us_f = duration<double, std::micro>(nanoseconds(2500)).count();   // 2.5  -- keep fraction
long long n = ns.count();                    // raw number in the duration's own unit

// arithmetic keeps units straight:
auto total = 3ms + 200us;                    // 3200us, as the common type
// auto bad = 3ms + 5;                        // ❌ won't compile -- 5 what? unit safety
```

- `duration<Rep, Period>` — `Rep` is the number type (int, `long long`,
  `double`), `Period` is a `std::ratio` (`std::nano` = 1/1'000'000'000 s).
- Integer→finer (ms→ns) is implicit (exact). Coarser or lossy needs
  `duration_cast` (truncates) or `round`/`floor`/`ceil<>` (C++17).
- Do timing math in the **integer** domain (`nanoseconds`), convert to `double`
  only for the final print.

## Clocks

```cpp
steady_clock::now();      // MONOTONIC -- never jumps, only goes forward. USE FOR INTERVALS / BENCHMARKS
system_clock::now();      // wall clock -- can jump (NTP, DST, manual set). USE FOR TIMESTAMPS / calendar
high_resolution_clock;    // an alias for one of the above -- unspecified. Don't rely on it; prefer steady_clock

// wall time:
auto now = system_clock::now();
std::time_t t = system_clock::to_time_t(now);      // -> C calendar
// C++20: std::chrono::zoned_time, std::format("{:%F %T}", now)
```

**The rule:**
- Measuring "how long did X take" / scheduling "wake me in 5ms" → **`steady_clock`**.
  If you use `system_clock` and NTP steps the clock back mid-measurement, you get
  a negative duration.
- Recording "when did this event happen" (logs, order timestamps you compare to
  other machines) → `system_clock` (and sync your clocks — PTP in HFT).

## Time points

```cpp
steady_clock::time_point start = steady_clock::now();
// ... work ...
nanoseconds elapsed = steady_clock::now() - start;   // time_point - time_point = duration
std::printf("%lld ns\n", elapsed.count());

auto deadline = steady_clock::now() + 10ms;
while (steady_clock::now() < deadline) { /* spin */ }
```

`time_point` = "a duration since a clock's epoch". You can subtract two
`time_point`s (→ `duration`), add a `duration` to a `time_point`, compare them.
You **cannot** add two `time_point`s (meaningless) or mix time points from
different clocks.

---

## Timing code correctly

`examples/07` is the reference. The essentials:

```cpp
// 1. -O2. At -O0 you're measuring un-optimized code -> meaningless for real perf.
// 2. steady_clock, integer nanoseconds until the final divide.
// 3. WARM UP -- run the code once (or a few times) before timing:
//    caches cold, branch predictor untrained, CPU not at boost clock, pages not faulted in.
// 4. Repeat, take the MIN (or a low percentile). The min is the run with least
//    OS/scheduler noise -> closest to the true cost. Mean/max are noise-dominated.
// 5. SINK the result so the optimizer can't delete the work:
//    volatile sink, or `benchmark::DoNotOptimize`, or accumulate into a global.
// 6. Measure ONLY what you claim. Don't include setup, allocation, or RNG in the timed region
//    (this repo had a bug once where a loop's `%` hid inside a "division" benchmark).
```

Skeleton:
```cpp
auto bench = [](auto&& fn, int reps) {
    fn();                                            // warm-up
    auto best = nanoseconds::max();
    for (int i = 0; i < reps; ++i) {
        auto t0 = steady_clock::now();
        fn();
        auto t1 = steady_clock::now();
        best = std::min(best, duration_cast<nanoseconds>(t1 - t0));
    }
    return best;
};
```

**Clock resolution matters.** On this box `steady_clock::now()` resolves to
~100 ns and the call itself costs tens of ns. Timing a single 5 ns operation is
impossible directly — **loop it N times** (N large enough that the total ≫ clock
resolution and call cost), then divide by N. `examples/07` measures a
sum-of-`sqrt` at ~6.35 ns/element this way.

---

## Andar kya hota hai

- `duration` is a single `Rep` member; all the unit machinery is types resolved
  at compile time → `duration_cast` is a compile-time-known multiply/divide by a
  ratio, often a shift. Zero runtime overhead vs a bare `long long` of
  nanoseconds.
- `steady_clock::now()` → `clock_gettime(CLOCK_MONOTONIC, ...)` on Linux, which
  on modern kernels is a **vDSO** call reading the TSC — no syscall, ~15–30 ns.
  `system_clock::now()` → `CLOCK_REALTIME`, similar cost but the value can step.
- The raw hardware counter is the **TSC** (`rdtsc`/`rdtscp`); on current CPUs
  it's invariant (fixed rate regardless of frequency scaling). HFT code often
  reads `rdtscp` directly (~10–20 cycles) and converts to ns with a calibrated
  factor, avoiding even the vDSO overhead.
- `high_resolution_clock` is just a typedef — libstdc++ makes it `system_clock`.
  Don't use it for intervals; name `steady_clock` explicitly.

> **HFT relevance:** timestamping is core — every inbound packet and outbound
> order gets a nanosecond stamp. In-process latency deltas use a **monotonic**
> source (`steady_clock`, or raw `rdtscp` for the lowest overhead); wall-clock
> stamps for cross-host correlation come from a **PTP-disciplined**
> `system_clock`. `steady_clock` for intervals is non-negotiable — a
> `system_clock` NTP step mid-measurement produces negative or huge deltas.
> Benchmarks follow the `examples/07` discipline (warm-up, min-of-reps, sink,
> `-O2`, measure only the target) because a sloppy microbenchmark that hides
> allocation or lets the optimizer delete the work has repeatedly misled this
> repo and the industry.

---

## Hands-on

```bash
./build.ps1 fast 19-STL/examples/07_chrono_timing.cpp
```

Shows the clock resolution probe, the wrong way (no warm-up, no sink — often
prints 0 or nonsense) vs the right way (warm-up + min of 10 reps), duration
arithmetic with literals, and the `system_clock` timestamp note.

---

## ⚠️ Traps

### Trap 1 — `system_clock` for an interval
```cpp
auto t0 = system_clock::now(); work(); auto dt = system_clock::now() - t0;   // ⚠️ NTP/DST step -> dt can be negative or huge. steady_clock
```

### Trap 2 — `duration_cast` silently truncating
```cpp
auto ms = duration_cast<milliseconds>(1500us);   // 1, not 2 -- truncates toward zero. round<milliseconds>(1500us) -> 2
```

### Trap 3 — timing at `-O0`
```cpp
// ./build.ps1 file.cpp   (debug, -O0)  -> numbers are 5-50x the real cost. Use ./build.ps1 fast
```

### Trap 4 — the optimizer deletes the work
```cpp
auto t0 = steady_clock::now();
for (int i = 0; i < N; ++i) compute(i);      // ⚠️ result unused -> whole loop removed -> "0 ns". sink it
auto t1 = steady_clock::now();
```

### Trap 5 — timing one tiny operation directly
```cpp
auto t0 = steady_clock::now(); int x = f(3); auto dt = steady_clock::now() - t0;
// ⚠️ dt is dominated by now()'s own ~20-100 ns. Loop f N times, divide by N
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`high_resolution_clock` is the most precise / right choice" | It's an alias (→ `system_clock` in libstdc++); name `steady_clock` for intervals |
| "`system_clock` is fine for measuring durations" | It can jump — use `steady_clock` for elapsed time |
| "`duration_cast` rounds" | It **truncates** toward zero; use `round`/`floor`/`ceil<>` to round |
| "One `now(); f(); now();` gives f's time" | Dominated by clock overhead + no warm-up; loop + min-of-reps |
| "`-O0` timing is representative" | It's not — benchmark at `-O2` (`./build.ps1 fast`) |

---

## Exercises

1. **Which clock:** (a) stamp each received UDP packet, (b) measure order round-
   trip time in-process, (c) fire a callback every 100 ms, (d) log "trade
   executed at" for a compliance report.

   <details><summary>Answer</summary>

   (a) `system_clock` (a timestamp to correlate across hosts — PTP-synced). (b)
   `steady_clock` (interval). (c) `steady_clock` (interval / deadline). (d)
   `system_clock` (wall-clock event time).
   </details>

2. **Resolution:** `steady_clock::now()` resolves to ~100 ns here. You want the
   cost of a function that takes ~8 ns. How do you measure it, and what N?

   <details><summary>Answer</summary>

   Loop it N times inside one timed region, divide the total by N. Pick N so the
   total ≫ resolution + call overhead — e.g. N = 1,000,000 → total ~8 ms, clock
   error ~0.1 µs → <0.002% error. Warm up first, take the min over several reps,
   sink the accumulated result.
   </details>

3. **Truncation fix:** `duration_cast<seconds>(1999ms)` gives 1. Get 2 (nearest)
   and also the ceiling.

   <details><summary>Answer</summary>

   `std::chrono::round<seconds>(1999ms)` → 2. `std::chrono::ceil<seconds>(1999ms)`
   → 2. (`floor<seconds>` → 1, same as `duration_cast` here.)
   </details>

4. **Spot the bug:** `auto t0 = steady_clock::now(); std::vector<int>
   v(1'000'000); std::sort(v.begin(), v.end()); auto dt = steady_clock::now() -
   t0;` — you wanted to time the sort.

   <details><summary>Answer</summary>

   The timed region includes the 1M-element **allocation and value-init** of `v`,
   which can dwarf sorting an all-zero vector (and sorting zeros isn't
   representative anyway). Build + fill `v` with real data *before* `t0`; time
   only `std::sort`; sink `v[0]` after.
   </details>

5. **rdtsc vs now():** why might HFT code read `rdtscp` directly instead of
   `steady_clock::now()`, and what's the catch?

   <details><summary>Answer</summary>

   `rdtscp` is ~10–20 cycles vs ~15–30 ns for the vDSO `clock_gettime` — lower
   overhead and no function-call boundary, so it perturbs the measured path
   less. Catches: you must convert cycles→ns with a calibrated, per-boot factor;
   pin the thread (TSC is per-core, though invariant TSC keeps cores in sync on
   modern CPUs); and it's not portable.
   </details>

---

## Interview questions

1. `steady_clock` vs `system_clock` — kaunsa interval, kaunsa timestamp, kyun?
2. `high_resolution_clock` ka problem kya?
3. `duration_cast` round karta ya truncate? Round kaise?
4. Sahi microbenchmark ke 5 elements (warm-up, min, sink, -O2, scope)?
5. 8 ns ka operation kaise measure karein jab clock 100 ns resolution ka hai?
6. `duration<Rep,Period>` ka type system unit bugs se kaise bachata?

---

## Next
→ [`18-random.md`](18-random.md)
