# 16 — Clocks aur timers: clock_gettime, TSC, vDSO

## Prerequisites
- `02-syscalls.md` (vDSO — syscall bina trap)
- `06-proc-and-sys.md` (`clocksource` in `/sys`)

## Yeh topic abhi kyun
HFT poora time-stamping ka game hai: har market-data event, har order decision,
har fill — sab ko precise, consistent, sasta timestamp chahiye latency
measurement aur regulatory (MiFID II ~µs, exchange-specific) ke liye. Galat clock
choice = ya to har timestamp 300 ns (`CLOCK_MONOTONIC_RAW` trap), ya
non-monotonic jumps (`CLOCK_REALTIME` + NTP step), ya coarse (~1 ms). Yeh lesson:
kaunsa clock kab. Example `09` sabko naapta hai.

---

## Linux clocks (`clock_gettime(clockid, timespec*)`)

| clockid | Kya | Monotonic? | vDSO (fast)? | Use |
|---|---|---|---|---|
| `CLOCK_MONOTONIC` | boot se beeta time, NTP frequency-adjusted (slew, no jumps) | haan | **haan** (~15–25 ns) | **latency measurement, timeouts** |
| `CLOCK_MONOTONIC_RAW` | boot se, koi NTP adjust nahi (raw hardware) | haan | aksar **nahi** (~250+ ns trap) | clock-drift analysis; NOT hot path |
| `CLOCK_MONOTONIC_COARSE` | MONOTONIC par sirf last tick pe updated (~1–4 ms granularity) | haan | haan (~5–8 ns) | "roughly now", rate limiting; NOT for latency |
| `CLOCK_REALTIME` | wall-clock (Unix epoch), NTP steps + slews, `date` set kar sakti | **nahi** (jump back/forward) | haan (~15–25 ns) | logging timestamps, cross-machine correlation, regulatory |
| `CLOCK_REALTIME_COARSE` | REALTIME, coarse | nahi | haan | cheap wall-clock |
| `CLOCK_TAI` | REALTIME + leap-second offset (no leap smear) | nahi (steps) | haan | precise wall-clock without leap ambiguity |
| `CLOCK_PROCESS_CPUTIME_ID` / `THREAD_CPUTIME_ID` | CPU time consumed | n/a | nahi | profiling |
| `CLOCK_BOOTTIME` | MONOTONIC + suspend time | haan | haan | wall-ish uptime |

**Rule of thumb:**
- **Measuring an interval** (latency, "kitni der laga"): `CLOCK_MONOTONIC`.
  Kabhi peeche nahi jaata, koi jump nahi.
- **Recording "kab hua" for logs / exchange correlation**: `CLOCK_REALTIME` (ya
  `CLOCK_TAI`). Par jaano yeh NTP se adjust ho sakta.
- Never use `CLOCK_REALTIME` for `end - start` — NTP step beech mein aa jaye to
  negative ya huge interval.

`std::chrono::steady_clock` → `CLOCK_MONOTONIC` (libstdc++). `std::chrono::
system_clock` → `CLOCK_REALTIME`. `std::chrono::high_resolution_clock` → usually
an alias for `steady_clock` (don't rely on it; use `steady_clock` explicitly).

---

## TSC (Time Stamp Counter) — `rdtsc` / `rdtscp`

CPU ka ek 64-bit counter jo har cycle badhta. `rdtsc`/`rdtscp` instruction se
padho — **sabse tez** time read (~8–15 ns), finest resolution.

```cpp
static inline uint64_t rdtscp() {
    uint32_t lo, hi, aux;
    __asm__ __volatile__("rdtscp" : "=a"(lo), "=d"(hi), "=c"(aux));
    return (uint64_t(hi) << 32) | lo;
}
```

### Caveats (jaan lo, warna trap)
1. **Cycles, not nanoseconds.** Ek calibration chahiye: startup pe
   `clock_gettime` ke against `rdtsc` measure karke `cycles_per_ns` nikaalo.
2. **`rdtsc` vs `rdtscp`:** plain `rdtsc` out-of-order execute ho sakta (thoda
   pehle/baad). `rdtscp` partially serializing + `aux` mein CPU id deta (core
   migrate detect). Modern: `rdtsc` + `lfence` before, ya `rdtscp`.
3. **Invariant TSC:** purane CPUs pe TSC frequency scaling ke saath badalta tha,
   aur cores ke beech skew hota. Modern (`constant_tsc` + `nonstop_tsc` +
   `tsc_reliable` in `/proc/cpuinfo`): TSC fixed rate, synced across cores of a
   socket at boot. Check these flags before trusting `rdtsc` cross-core.
4. **Cross-socket:** TSC ek socket ke andar synced, sockets ke beech usually
   bhi (BIOS sync) par verify. Cross-socket comparison ho to careful.
5. **Not a wall clock:** TSC se "abhi kya time hai" nahi milta, sirf intervals.

`clocksource=tsc` (`/sys/.../current_clocksource`) hone pe `CLOCK_MONOTONIC`
khud andar `rdtsc` + scale karta (vDSO mein) — isi liye woh ~20 ns. Agar
`clocksource=hpet` → `CLOCK_MONOTONIC` ~500–1000 ns (MMIO read). **Isi liye HFT
box pe `clocksource=tsc` mandatory** (`06`, `18`).

---

## vDSO recap (`02`)

`clock_gettime`, `gettimeofday`, `getcpu`, `time` — kernel inhe ek shared
read-only page (vDSO) ke through userspace mein serve karta. `constant_tsc` +
`clocksource=tsc` ke saath = ek `rdtsc` + arithmetic, **no ring transition**.

```bash
cat /proc/self/maps | grep vdso            # [vdso] region present
# aur confirm: strace -e trace=clock_gettime ./prog  -> almost 0 calls dikhengi
```

To practical latency-critical timestamp: `CLOCK_MONOTONIC` via vDSO (~20 ns,
portable, gives ns directly) **ya** raw `rdtscp` + calibrated scale (~10 ns,
needs care). Dono acceptable; `rdtscp` sirf tab jab woh 10 ns bhi matter kare.

---

## Timers — code ko "baad mein" chalana

| API | Note |
|---|---|
| `nanosleep` / `clock_nanosleep(TIMER_ABSTIME)` | sleep; `ABSTIME` = drift-free periodic (example `10`) |
| `timerfd_create` + `epoll` | timer as an fd — event loop mein uniformly handle (`09`) |
| `timer_create` (POSIX) + `SIGEV_THREAD`/signal | callback timer; signal-based → async-safety issues (`04`) |
| `setitimer` / `alarm` | legacy, `SIGALRM` |
| busy-wait to a `rdtsc`/`CLOCK_MONOTONIC` deadline | **HFT: sub-µs precise "act at time T"** — no syscall, no sleep granularity |

Kernel timer granularity `nanosleep` pe: even `sleep(1ns)` → actual wakeup
~50–100 µs later (tick + scheduler) on a normal box. For precise timing
(release an order at exactly T), **busy-spin** to the deadline:

```cpp
void spin_until(uint64_t target_ns) {
    while (now_monotonic_ns() < target_ns) _mm_pause();   // ya __builtin_ia32_pause()
}
```

---

## Internal working

- vDSO data page: kernel har tick (aur NTP update pe) `tk->tkr_mono` (base
  time + TSC value + mult/shift) update karta. userspace: `now = base +
  ((rdtsc() - tsc_base) * mult) >> shift`. Ek seqlock (folder `28` file 12!)
  se data page ka consistent read.
- `CLOCK_MONOTONIC` vs `_RAW`: `_RAW` NTP `mult` adjustments skip karta — uska
  path historically vDSO mein nahi tha (kernel version pe depend; newer kernels
  ne add kiya). Test karo apne kernel pe (`09` / `strace`).
- `_COARSE`: sirf `tk->xtime` (last tick pe set) return, koi TSC read nahi →
  fastest, ~tick granularity.
- TSC `constant_tsc`: P-state/frequency change se independent (counts at nominal
  frequency). `nonstop_tsc`: C-state (deep idle) mein bhi chalta rehta.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `end - start` with `CLOCK_REALTIME`
NTP ne beech mein clock ko 3 ms peeche kiya → `end - start` negative ya wraps to
a huge number. Intervals ke liye **hamesha `CLOCK_MONOTONIC`**.

### Trap 2 — `CLOCK_MONOTONIC_RAW` "kyunki raw sounds accurate"
Aksar vDSO se nahi → har call ek ~250+ ns syscall. Hot path pe 10M timestamps =
2.5 seconds sirf timekeeping. `CLOCK_MONOTONIC` use karo.

### Trap 3 — `rdtsc` ko ns maan lena
`rdtsc` cycles deta. 3 GHz CPU pe 3000 cycles = 1 µs. Calibrate: startup pe
`t0=clock_gettime; c0=rdtsc; sleep; t1; c1;` → `ns_per_cycle = (t1-t0)/(c1-c0)`.

### Trap 4 — `rdtsc` cross-core bina `rdtscp`/flag check
Core A pe `rdtsc`, thread migrate, core B pe `rdtsc` → skewed delta (purane
hardware pe). `rdtscp` (aux = core id, detect migration) + verify `constant_tsc
nonstop_tsc` flags. Best: hot thread pinned (`11`) → migration hi nahi.

### Trap 5 — `clocksource=hpet` / `acpi_pm`
`CLOCK_MONOTONIC` ~1 µs (MMIO). `cat /sys/devices/system/clocksource/
clocksource0/current_clocksource` → `tsc` chahiye. Cmdline `clocksource=tsc
tsc=reliable`.

### Trap 6 — `nanosleep(1 µs)` se precise timing expect karna
Actual wakeup 50–100 µs baad (tick + scheduler). Sub-µs "act at T" ke liye
busy-spin to a monotonic/TSC deadline.

### Trap 7 — leap second
`CLOCK_REALTIME` leap second pe repeat/step ho sakta (ya "smeared" by the NTP
server over hours). Exchange timestamps ke liye `CLOCK_TAI` (no leap
ambiguity), ya smeared-time-aware. `MONOTONIC` leap se unaffected (intervals
safe).

---

## > **HFT relevance**

> - **Latency timestamps:** `CLOCK_MONOTONIC` (vDSO, ~20 ns) or calibrated
>   `rdtscp` (~10 ns). Bracket measurement (one read in, one read out), never
>   per-iteration in a tight loop.
> - **Event timestamps for logs / exchange reconciliation:** `CLOCK_REALTIME`
>   (or `CLOCK_TAI`), synced by **PTP** (hardware timestamping, `30/14`) not
>   plain NTP — sub-µs to the exchange.
> - **`clocksource=tsc`** + `constant_tsc nonstop_tsc tsc_reliable` verified at
>   startup; hard-fail if `hpet`. Part of the box checklist (`18`).
> - **Precise action timing** ("send this order at T"): busy-spin to a monotonic
>   deadline; `nanosleep` granularity (~50 µs) is far too coarse.
> - **Pin the timestamping thread** (`11`) so `rdtsc` never crosses cores.
> - **Calibrate `cycles_per_ns` at startup** and periodically re-check against
>   `CLOCK_MONOTONIC` (frequency should be constant, but verify).

---

## Hands-on

```bash
# Linux pe -- example 09: har clock ki cost + resolution
g++ -std=c++20 -O2 29-LINUX-SYSTEMS/examples/09_clock_comparison.linux.cpp -o /tmp/clk && /tmp/clk

# clocksource
cat /sys/devices/system/clocksource/clocksource0/available_clocksource
cat /sys/devices/system/clocksource/clocksource0/current_clocksource     # tsc chahiye

# TSC flags
grep -o -E 'constant_tsc|nonstop_tsc|tsc_reliable|rdtscp' /proc/cpuinfo | sort -u

# clock_gettime vDSO se aa raha? (calls ~0 dikhni chahiye)
cat > /tmp/c.c <<'EOF'
#include <time.h>
int main(){struct timespec t;for(int i=0;i<1000000;i++)clock_gettime(CLOCK_MONOTONIC,&t);}
EOF
gcc -O2 /tmp/c.c -o /tmp/c && strace -c /tmp/c 2>&1 | grep -E 'clock_gettime|calls' || echo "no clock_gettime syscalls (vDSO OK)"
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| interval ke liye `CLOCK_REALTIME` | `CLOCK_MONOTONIC` — NTP jumps se safe |
| `CLOCK_MONOTONIC_RAW` sabse accurate/fast | aksar syscall (no vDSO); `MONOTONIC` faster |
| `rdtsc` returns nanoseconds | cycles; calibrate to ns |
| `rdtsc` cross-core safe | check `constant_tsc`/`nonstop_tsc`; use `rdtscp`; pin thread |
| `nanosleep(1us)` = 1 µs wait | ~50–100 µs actual; busy-spin for precision |
| `high_resolution_clock` best choice | often just `steady_clock`; use `steady_clock` explicitly |

---

## Exercises

1. `auto s = system_clock::now(); work(); auto e = system_clock::now();
   auto d = e - s;` — kabhi-kabhi `d` negative. Kyun, fix?

   <details><summary>Answer</summary>

   `system_clock` = `CLOCK_REALTIME` — NTP daemon ne `work()` ke dauraan clock
   ko peeche step kiya (ya `date` command). Interval ke liye `steady_clock`
   (`CLOCK_MONOTONIC`) use karo — woh kabhi peeche nahi jaata. `system_clock`
   sirf "kaunsa wall-clock time tha" ke liye.
   </details>

2. Hot loop mein `clock_gettime(CLOCK_MONOTONIC)` 10M baar — profiler kehta 20%
   time yahin. Fix bina timestamp khoye?

   <details><summary>Answer</summary>

   (1) Bracket measurement: loop ke bahar ek read start pe, ek end pe; per-iter
   time = `(end-start)/iters` (agar sirf average chahiye). (2) Agar per-event
   timestamp chahiye: `rdtscp` (~10 ns vs ~20 ns — 2×) + calibrated scale, ya
   store raw TSC and convert offline. (3) Sample: har 100th iteration timestamp.
   (4) Ensure `clocksource=tsc` (agar `hpet`, ~1 µs each — that's the real bug).
   </details>

3. Startup pe `cycles_per_ns` calibrate karna hai. Method + ek pitfall.

   <details><summary>Answer</summary>

   `t0 = clock_gettime(MONOTONIC); c0 = rdtscp(); busy-wait ~100 ms;
   t1 = clock_gettime(MONOTONIC); c1 = rdtscp(); cycles_per_ns = (c1-c0) /
   (t1_ns - t0_ns);`. Pitfalls: (a) calibration window pe thread ko preempt
   kiya gaya → `t1-t0` includes sleep, ratio galat — pin the thread, use
   best-of-N. (b) CPU frequency ramp during calibration (governor not
   `performance`) — but `constant_tsc` means TSC rate is nominal freq
   regardless, so actually OK; still, lock governor. (c) Too-short window →
   quantization error; ~50–100 ms is enough.
   </details>

4. `CLOCK_MONOTONIC` `strace` mein dikhta hai (syscalls ho rahe), ~300 ns each.
   Kya galat?

   <details><summary>Answer</summary>

   vDSO se serve nahi ho raha. Wajah: `clocksource` `tsc` nahi hai (`hpet`/
   `acpi_pm` ke saath kernel vDSO fast-path disable kar deta hai kyunki MMIO
   read userspace se safe/consistent nahi), ya CPU mein `constant_tsc` nahi, ya
   kernel ne TSC ko unstable mark kar diya (`dmesg | grep -i tsc` → "Marking
   TSC unstable"). Fix: `clocksource=tsc tsc=reliable` cmdline (agar hardware
   supports), ya hardware/BIOS TSC issue address karo.
   </details>

5. Order ko exactly `T = now + 500 µs` pe bhejna hai (auction timing). `nanosleep`
   kaafi kyun nahi, alternative?

   <details><summary>Answer</summary>

   `nanosleep`/`clock_nanosleep` ki granularity ~50–100 µs (tick + scheduler
   wakeup latency), aur wakeup late-biased — 500 µs target pe ±50 µs error,
   unpredictable. Alternative: busy-spin — `uint64_t deadline = now_ns() +
   500'000; while (now_ns() < deadline) _mm_pause();` on a pinned isolated core.
   Sub-µs precision, no syscall. CPU burn hai, par HFT box pe woh core already
   busy-polling hota.
   </details>

---

## Interview questions

1. `CLOCK_MONOTONIC` vs `CLOCK_REALTIME` — interval measurement ke liye kaunsa, kyun.
2. `CLOCK_MONOTONIC_RAW` slow kyun aksar (vDSO)?
3. `CLOCK_*_COARSE` — kab use, resolution.
4. TSC / `rdtscp` — 3 caveats (cycles-not-ns, cross-core, invariant flags).
5. vDSO `clock_gettime` ko kaise fast banata (seqlock + rdtsc + scale)?
6. `clocksource=tsc` vs `hpet` — `CLOCK_MONOTONIC` latency pe asar.
7. Sub-µs "act at time T" — kyun busy-spin, `nanosleep` nahi.

---

## Next
→ [`17-cgroups-and-limits.md`](17-cgroups-and-limits.md)
