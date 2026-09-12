# 02 — Time sahi se naapo: `std::chrono`

## Prerequisites
- `01-why-measure.md`
- `03-VARIABLES-DATA-TYPES` (integer types, overflow)
- Thoda `<chrono>` exposure (folder 22 modern C++)

## Yeh topic abhi kyun
Baseline number lene ke liye **ek bharosemand timer** chahiye. C++ mein woh
`<chrono>` hai — par usme teen clocks hain aur teenon ka kaam alag. Galat
clock chunne se tumhaara benchmark **NTP adjustment se peechhe kood** sakta
hai (negative duration!), ya resolution itni moti ho ki tum kuch naap hi na
sako. Yeh lesson: kaunsa clock, kaise, aur uski **limits**.

---

## `<chrono>` ka model: teen cheezein

```cpp
#include <chrono>
namespace ch = std::chrono;
```

1. **Clock** — "abhi kitne baje" ka source. `ch::steady_clock`,
   `ch::system_clock`, `ch::high_resolution_clock`.
2. **time_point** — ek clock pe ek instant. `clock::now()` yeh deta.
3. **duration** — do time_points ka farak. `t1 - t0`. Ismein ek **rep**
   (number type) aur ek **period** (ratio, e.g. `std::nano`) hota.

```cpp
auto t0 = ch::steady_clock::now();          // time_point
// ... kaam ...
auto t1 = ch::steady_clock::now();
ch::duration<double, std::nano> d = t1 - t0; // duration, double nanoseconds
double ns = d.count();                       // plain number
// ya:
auto ins = ch::duration_cast<ch::nanoseconds>(t1 - t0).count();  // int64 ns
```

`duration_cast` **truncates** (rounds toward zero). Sub-unit precision
chahiye to `ch::duration<double, ...>` use karo, cast mat karo.

---

## Teen clocks — kaunsa kab

### `steady_clock` — **intervals ke liye YEH**

- **Monotonic**: kabhi peeche nahi jaata. `t1 >= t0` guaranteed.
- NTP / user / DST se **affected nahi**.
- "Epoch" arbitrary hai (aksar boot time) — absolute value bekaar, **farak**
  meaningful.
- Linux pe `clock_gettime(CLOCK_MONOTONIC)` wrap karta; Windows pe
  `QueryPerformanceCounter`.

```cpp
// har benchmark ke liye default:
using Clock = std::chrono::steady_clock;
static_assert(Clock::is_steady);
```

### `system_clock` — wall clock, "kab hua"

- Real-world time (Unix epoch se). `time_t` mein convert hota, print hota.
- **NTP se adjust hota** — aage/peeche kood sakta. `t1 - t0` **negative**
  aa sakta agar beech mein clock peeche gaya!
- Use: logging timestamps, "yeh event 14:32:05 UTC pe hua", scheduling.
- **Interval timing ke liye kabhi nahi.**

### `high_resolution_clock` — naam pe mat jao

- Standard kehta: "sabse chhoti tick period wala clock." Par implementation
  ise aksar **`steady_clock` ka alias** bana deti (libstdc++, MSVC), kabhi
  `system_clock` ka (purana libc++) — **non-monotonic!**
- **Portable code mein use mat karo.** `steady_clock` explicitly likho —
  guarantee bhi milti, intent bhi clear.

| Clock | Monotonic? | NTP-safe? | Kis liye |
|---|---|---|---|
| `steady_clock` | ✅ haan | ✅ haan | **intervals, benchmarks** |
| `system_clock` | ❌ nahi | ❌ nahi | wall-clock timestamps, logging |
| `high_resolution_clock` | ⚠️ impl-defined | ⚠️ | **kuch nahi — steady_clock likho** |

**Measured (`01_timing_methods.cpp`, is Windows box):** teenon clocks ka
smallest observable step = **100 ns**. Yani is box pe `high_resolution_clock`
= `steady_clock` (dono QPC), aur ~100 ns granularity.

---

## Resolution vs precision vs accuracy

- **Resolution / granularity** — clock kitni chhoti tick de sakta. Windows
  QPC ~100 ns (measured), Linux `CLOCK_MONOTONIC` ~1–40 ns. Agar tumhara
  region 50 ns ka hai aur resolution 100 ns, to reading 0 ya 100 aayegi —
  **useless**.
- **Precision** — kitni consistent reading (run-to-run spread).
- **Accuracy** — reading kitni sach ke kareeb (bias). Ek timer 1% fast chal
  sakta — sab readings consistently 1% zyada.

**Self-cost** — `now()` call khud kitna leta. Measured (is box):

| Timer | Self-cost |
|---|---|
| `steady_clock::now()` | **~38 ns** |
| plain `__rdtsc()` | ~9 ns |
| `lfence; rdtsc; lfence` | ~18 ns |

Matlab: `steady_clock` se ek **40 ns region** naapna bekaar — reading ka
aadha hissa timer khud hai. Aise region ke liye:
1. **Batch karo** — ek `t0`/`t1` pair, andar 10⁶ ops. `ns_per_op = total / 10⁶`.
2. Ya **`rdtsc`** use karo (lesson 03), fenced + self-cost subtract.

`04_benchmark_mistakes.cpp` case 5: ek `mix()` (~1 ns) ko akele time karne
pe **36.8 ns/op** (≈ timer self-cost); batch of 20M → **2.99 ns/op** (sach).

---

## `steady_clock` se benchmark — sahi shakl

```cpp
#include <chrono>
namespace ch = std::chrono;
using Clock = ch::steady_clock;

template <class T> static inline void sink(T&& v) {
    asm volatile("" : : "r,m"(v) : "memory");   // lesson 09 / folder 33-14
}

double bench_ns_per_op(std::size_t iters) {
    // warm-up: cache/branch-predictor/frequency ko settle karo (lesson 09)
    for (std::size_t i = 0; i < iters / 10; ++i) sink(work(i));

    double best = 1e300;
    for (int rep = 0; rep < 15; ++rep) {                 // min-of-N
        auto t0 = Clock::now();
        for (std::size_t i = 0; i < iters; ++i) sink(work(i));
        auto t1 = Clock::now();
        double ns = ch::duration<double, std::nano>(t1 - t0).count();
        best = std::min(best, ns / static_cast<double>(iters));
    }
    return best;                                          // ns per op
}
```

Points:
- `Clock = steady_clock` — monotonic.
- **warm-up** pehle — pehla run cold.
- **batch** — `iters` bade (timer self-cost amortize).
- **min-of-N** — sabse kam-perturbed run (lesson 04 mein "min kyun").
- **`sink`** — warna `-O2` `work()` ko delete kar deta (lesson 09).
- `duration<double, nano>` — cast nahi, precision rakho.

---

## `clock()` aur CPU-time — alag cheez

`<ctime>` ka `std::clock()` **CPU time** deta (process ne CPU pe kitne tick
kiye), wall time nahi. `sleep(5)` ke dauraan `clock()` aage nahi badhta.

- Use: "yeh 3s wall mein chala par CPU 0.4s — matlab 2.6s I/O wait / blocked."
- `CLOCKS_PER_SEC` se divide karke seconds.
- **Granularity mota** — POSIX pe theoretically 1 µs, par aksar scheduler
  tick (~1–10 ms) pe hi update hota. **Measured is MinGW box pe:
  `CLOCKS_PER_SEC = 1000`** → **1 millisecond** resolution. Micro-benchmark
  ke liye bilkul bekaar.
- Behtar: Linux `clock_gettime(CLOCK_PROCESS_CPUTIME_ID)` / `getrusage`
  (`ru_utime` + `ru_stime`), Windows `GetProcessTimes`.

`time ./prog` shell command yahi split deta: `real` (wall), `user` (CPU in
user code), `sys` (CPU in kernel). `user + sys >> real` → multi-threaded.
`real >> user + sys` → I/O-bound / blocked.

---

## Steady par perfect nahi: baaki masle

`steady_clock` monotonic hai, par:

1. **Resolution floor** — is box pe 100 ns. Us se chhota naapne ke liye
   `rdtsc` (lesson 03).
2. **Self-cost** — ~38 ns, region se subtract karo ya batch karo.
3. **VDSO vs syscall** (Linux) — `CLOCK_MONOTONIC` vDSO se (~20 ns, no
   kernel entry); `CLOCK_MONOTONIC_RAW` ek real syscall (~250 ns). `chrono`
   pehla wala use karta.
4. **OoO / compiler reorder** — `now()` ke around compiler aur CPU code
   move kar sakte. `chrono::now()` ek function call hai (compiler barrier),
   par CPU ke liye `rdtsc`-jaisa fence nahi. Microsecond+ intervals ke liye
   koi baat nahi; sub-100 ns ke liye maayne rakhta (lesson 03).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `system_clock` se interval
NTP tick beech mein → `t1 - t0` **negative** ya spike. Hamesha `steady_clock`.

### Trap 2 — `high_resolution_clock` "kyunki naam"
Alias hai — kabhi `system_clock` ka (non-monotonic). `steady_clock` likho.

### Trap 3 — `duration_cast` se precision khona
`duration_cast<nanoseconds>` truncates. Ek 12.7 ns region → `12`. Sub-unit
ke liye `duration<double, std::nano>`.

### Trap 4 — resolution se chhota region naapna
Region 40 ns, resolution 100 ns → reading `0` ya `100`, garbage. Batch, ya
`rdtsc`.

### Trap 5 — self-cost ignore
`steady_clock::now()` ~38 ns. Ek 100 ns region ka reading ~38 ns inflated
(ya region ke andar float karke variable). Batch (self-cost / iters → 0) ya
empty-region subtract.

### Trap 6 — `auto d = t1 - t0;` phir `d` ko `int` maan lena
`d` ek `duration` hai, plain number nahi. `.count()` chahiye, aur pehle
`duration_cast` ya `duration<double,...>` se unit fix karo — warna `.count()`
ka unit `steady_clock::period` (implementation-defined!) hoga.

---

## Hands-on

```bash
./build.ps1 fast 35-PROFILING-BENCHMARKING/examples/01_timing_methods.cpp
```
Expected (is box, ~2 GHz):
```
Resolution: steady/hires/system = 100 ns ; clock() = 1000000 ns ; rdtsc = 1 tick
Calibration: ~1.996 ticks/ns
Self-cost: steady_clock::now() ~38 ns ; __rdtsc ~9 ns ; fenced rdtsc ~18 ns
94 us workload: steady_clock 93800 ns  vs  fenced rdtsc 93818 ns   (0.02% apart)
```
Dekho: bade interval (94 µs) pe dono timer **agree** karte — trust dono.
Chhote interval pe sirf `rdtsc` kaam ka.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`high_resolution_clock` = best" | alias; `steady_clock` explicitly |
| "`system_clock` fine for timing" | NTP jump → negative/spike; sirf wall timestamps |
| "`now()` free hai" | ~38 ns; batch ya subtract |
| "`duration` ek number hai" | `.count()` + unit fix (`duration_cast` / `duration<double>`) |
| "`clock()` = stopwatch" | CPU time, ~1 ms granularity (is box) |
| "chrono se 10 ns naap lunga" | resolution 100 ns; `rdtsc` chahiye |

---

## Exercises

1. Yeh code kabhi-kabhi negative `ms` print karta. Kyun, aur fix?
   ```cpp
   auto t0 = std::chrono::system_clock::now();
   do_work();
   auto t1 = std::chrono::system_clock::now();
   std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
   ```
   <details><summary>Answer</summary>

   `system_clock` **NTP-adjusted** hai. Agar `do_work()` ke dauraan NTP daemon
   ne clock **peeche** set kiya (drift correction), to `t1 < t0` → duration
   negative. Kabhi bada positive spike bhi (clock aage kood gaya). Fix:
   `steady_clock` use karo — monotonic, NTP-immune. `system_clock` sirf tab
   jab tumhe asli wall-clock time chahiye (log line), interval nahi.
   </details>

2. Tum ek function time kar rahe ho jo ~200 ns leta, `steady_clock` se, ek
   loop mein 1000 baar, aur har reading store kar rahe ho. Median reading
   ~240 ns aati hai. Function 200 ns ka hai — 40 ns extra kahan se?

   <details><summary>Answer</summary>

   **Timer self-cost.** `steady_clock::now()` ~38 ns is box pe. Tum `t0 =
   now(); f(); t1 = now();` kar rahe ho — `t1` ki reading mein `f()` ke baad
   wale `now()` ka andar ka kaam included ho sakta, aur do `now()` calls ka
   overhead measurement window ke andar/border pe aata. ~40 ns ≈ ek `now()`.
   Fix: (a) empty region (`t0 = now(); t1 = now();`) ka cost measure karo aur
   har reading se subtract, ya behtar (b) **batch** — `t0 = now(); for(1e6)
   f(); t1 = now();` → `(t1-t0)/1e6`, self-cost per-op ~0. Individual-sample
   distribution chahiye to `rdtsc` (fenced, self-cost subtracted — lesson 03).
   </details>

3. `time ./prog` deta: `real 0m8.0s   user 0m1.2s   sys 0m0.3s`. Program
   single-threaded hai. Kya chal raha hai? Agla profiling step?

   <details><summary>Answer</summary>

   `user + sys = 1.5s` CPU, par `real = 8.0s` wall → program **6.5s kisi
   cheez ka wait** kar raha, CPU nahi jala. Single-threaded hai to
   parallelism nahi — yeh **blocking**: disk I/O, network, `sleep`, lock
   contention, ya `futex` wait. `perf stat` yahan IPC dikhayega jo CPU-time
   pe based hai (misleading yahan); asli tool: `strace -T -c ./prog` (kaun
   se syscalls, kitna time), ya off-CPU profiling (lesson 12) —
   `perf sched` / `bpftrace` offcputime. Wall-clock hotspot dhoondo, CPU
   hotspot nahi.
   </details>

---

## Interview questions

1. `<chrono>` ke teen clocks — properties aur use-case har ek ka.
2. `steady_clock` vs `system_clock` — interval timing ke liye kaunsa aur
   kyun (NTP wala scenario).
3. `high_resolution_clock` use karne mein kya dikkat?
4. Resolution vs precision vs accuracy vs self-cost — chaaron define karo.
5. `duration` aur `time_point` ka farak; `duration_cast` kya karta (rounding).
6. `clock()` / CPU-time vs wall-time; `time` command ka `real`/`user`/`sys`.
7. Ek 30 ns region ko `chrono` se kyun nahi naap sakte, kya karo instead.

---

## Next
→ [`03-rdtsc-timing.md`](03-rdtsc-timing.md)
