# 09 — Benchmark ke jaal: compiler, cache, frequency

## Prerequisites
- `08-microbenchmarking.md`
- `33-COMPILER-OPTIMIZATION/14-preventing-optimization.md` (DCE, `DoNotOptimize`)
- `33-COMPILER-OPTIMIZATION/06-constant-folding.md`, `04-loop-optimizations.md`
- `examples/04_benchmark_mistakes.cpp`

## Yeh topic abhi kyun
Google Benchmark iteration-count aur warm-up handle karta (lesson 08), par
woh **tumhare code ko compiler se nahi bacha sakta**. `-O2` ek naive
microbenchmark ko literally **0 ns** bana deta — aur woh number "10x faster"
jaisi jhoothi kahani ka aadha hissa ban jaata. Yeh lesson: har jaal, uska
demo, aur fix.

`04_benchmark_mistakes.cpp` — har bug ka BUG aur FIX side-by-side.
**Measured (is box, `-O2`):**

```
=== 1. Dead code elimination ===          BUG 0.000    FIX 0.848 ns/op
=== 2. Constant folding ===               BUG 0.000    FIX 0.867
=== 3. Loop-invariant hoisting ===        BUG 0.000    FIX 1.010
=== 4. Cold start ===        run[0] 242000 ns   min-of-20 204500 ns  (1.2x)
=== 5. Timer overhead ===    BUG 36.82 ns/op    FIX (batch) 2.99
=== 6. One measurement ===   one 0.961   min/med/max 0.822/0.842/1.108 (spread 35%)
```

---

## Jaal 1: Dead Code Elimination (DCE)

Agar benchmark ka **result kahin use nahi hota**, `-O2` poora computation
delete kar deta.

```cpp
for (auto _ : state) {
    uint64_t s = 0;
    for (int x : v) s += x;
    // s use nahi -> loop GAYAB -> 0 ns
}
```

**Fix — sink:**
```cpp
benchmark::DoNotOptimize(s);          // "koi s padh sakta" -> compute zaroori
```
Ya haath ka barrier (folder 33/14):
```cpp
template <class T> void sink(T const& v) { asm volatile("" : : "r,m"(v) : "memory"); }
```
Zero instructions emit; sirf optimizer ko batata "yeh value chahiye."

`04` case 1: BUG **0.000**, FIX **0.848 ns/op**.

---

## Jaal 2: Constant folding

Agar **input compile-time known** hai, compiler poora result compile time pe
nikaal deta.

```cpp
for (auto _ : state) {
    double r = std::pow(2.0, 10.0);   // = 1024.0 at compile time; no call
    benchmark::DoNotOptimize(r);       // sink hai, par kaam ho hi nahi raha
}
```

**Fix — input opaque:**
```cpp
double base = state.range(0);          // runtime value
benchmark::DoNotOptimize(base);        // ab compiler ise fold nahi kar sakta
double r = std::pow(base, 10.0);
benchmark::DoNotOptimize(r);
```

`04` case 2: `mix(42)` (literal) → BUG **0.000**; `mix(opaque_seed + i)` →
FIX **0.867 ns/op**.

> **DoNotOptimize dono taraf lagta hai:** output pe (DCE rokne ko) **aur**
> input pe (const-fold / closed-form rokne ko). `04` case 2 ka helper ek
> `volatile` global se seed leta — guaranteed runtime.

---

## Jaal 3: Loop-invariant code motion (hoisting)

Agar benchmark body **loop iteration pe depend nahi karta**, compiler use
loop ke bahar ek baar compute karke andar sirf `sink` chhod deta.

```cpp
uint64_t x = get_runtime_value();      // runtime, par loop mein constant
for (int i = 0; i < N; ++i)
    sum += mix(x);                     // mix(x) invariant -> hoisted
                                       // loop ab: sum += const, N times (add speed)
```

`04` case 3: `mix(x)` with fixed `x` → BUG **0.000** (measures nothing);
`mix(in[i & mask])` with varying input → FIX **1.010 ns/op**.

**Fix — body ko iteration pe depend karao:**
- input array se index: `data[i % n]`
- ya ek carried dependency: `x = mix(x);` (har iter agli ka input)

⚠️ Carried dependency **latency** naapta (har iter previous ka wait), throughput
nahi. `04` case 5 FIX ise dikhata: `x = mix(x)` chain → 2.99 ns/op (ek `mix`
ki latency); independent `mix` calls hote to CPU 3–4 overlap karta →
throughput ~4x behtar. **Kya naapna hai — latency ya throughput — decide
karke pattern chuno.**

---

## Jaal 4: Cold start — cache aur page faults

Pehla timed run:
- **Code cold** — I-cache miss on first execution of the benchmark body.
- **Data cold** — arrays not in cache; first pass all misses.
- **Page faults** — freshly `malloc`'d memory not yet mapped; first touch =
  minor fault (~µs each) ya major (disk, ms).
- **Branch predictor / BTB cold** — first few iterations mispredict.
- **TLB cold**, **frequency low** (turbo not ramped yet).

**Fix — warm-up:** benchmark body ko measure se pehle kai baar chalao, phir
timing shuru. Google Benchmark yeh **partly** karta (calibration runs), par
bade data structures ke liye tum bhi ek explicit warm-up pass rakho, aur
`run[0]` explicitly discard karo.

`04` case 4: 64 MB buffer, har 4 KB pe ek write → **run[0] 242000 ns**,
**min-of-next-20 204500 ns** (~1.2x). Yahan farak chhota (Windows eagerly-ish
commits, aur sirf 16K pages touch hote) — par disk-backed / bigger working
set pe yeh **2–100x** ho sakta.

---

## Jaal 5: Timer overhead > operation

`steady_clock::now()` ~38 ns (lesson 02). Ek `mix()` (~1 ns) ko akele time
karo → tum ~97% timer naap rahe.

`04` case 5: `t0 = now(); x = mix(x); t1 = now();` per op → **36.82 ns/op**
(≈ timer self-cost). **Batch:** ek `t0`/`t1` pair, andar 20M ops →
**2.99 ns/op** (sach).

**Fix:** ek timestamp pair ke andar bahut saare ops; `ns_per_op = total /
count`. Ya `rdtsc` fenced + self-cost subtracted (lesson 03) agar per-op
distribution chahiye.

---

## Jaal 6: Ek measurement

Ek run = ek interrupt / freq dip / co-tenant burst ka shikaar.

`04` case 6: `one run 0.961`, `min/median/max 0.822/0.842/1.108`,
**spread 35%**. Ek run 0.961 report karna → 15% inflated vs the true ~0.822.

**Fix:** min-of-N (micro-bench, deterministic work) ya full distribution
(lesson 04). Google Benchmark `--benchmark_repetitions` + `_median` + `_cv`.

---

## Jaal 7: Alignment / code layout noise

Yeh sabse sneaky. **Exactly same source**, sirf ek unrelated function add
karne se, ya ek env var ka naam badalne se, hot loop ka number **5–15%**
badal sakta:

- **Loop alignment** — hot loop 16/32-byte boundary pe align hai ya nahi →
  frontend fetch efficiency. `-falign-loops` isse tune karta.
- **Function placement** — linker order → I-cache set conflicts, iTLB.
- **Stack alignment** — `main`'s stack depth se ek array ka alignment change
  → cache set / 4K-aliasing.
- **Environment size** — `argv`/`envp` stack pe hote; env vars ka total
  size stack ka starting address shift karta → sab kuch shift ([Mytkowicz
  et al., "Producing Wrong Data Without Doing Anything Obviously Wrong"]).

**Mitigation:**
- Ek change ka effect measure karte waqt, **kai alignment scenarios** average
  karo (e.g. `-falign-functions` sweeps, ya different link orders).
- **Stabilizer** (research tool) — layout ko randomize karke statistically
  valid comparison.
- Practically: agar do versions ka farak alignment-noise (±10%) ke andar
  hai, to woh **"koi farak nahi"** hai. Sirf farak jo noise se clearly bada
  ho, believe karo.
- `perf stat` counters (IPC, cache-misses) dekho — agar woh nahi badle par
  time badla, layout noise hai.

---

## Jaal 8: Frequency scaling

Modern CPU ka clock **fixed nahi**:
- **Turbo/boost** — light load pe upar (~1.3–1.5x base). Short benchmark
  turbo pe, sustained load pe drop.
- **AVX/AVX-512 downclock** — wide SIMD instructions core frequency giraa
  dete (folder 33/12).
- **Thermal / power throttle** — laptop / dense server sustained load pe.
- **Governor** — `powersave` governor slow ramp; `performance` = max.

**Effect:** run-to-run ±10–30%, aur ek "faster" version jo zyada AVX use
karti woh actually lower frequency pe chal ke net slower ho sakti.

**Mitigation:**
```bash
sudo cpupower frequency-set -g performance          # governor
sudo cpupower frequency-set -f 2.5GHz               # ya fixed
echo 0 | sudo tee /sys/devices/system/cpu/cpufreq/boost   # turbo off
# ya BIOS: fixed all-core frequency
```
`perf stat` ka `cycles` vs `task-clock` — agar `GHz` (`cycles/task-clock`)
run-to-run badalta, frequency unstable hai. Report ns/op **aur** cycles/op —
cycles frequency-invariant hai (folder 31/13).

---

## Jaal 9: Denormals aur data-dependent FP

Floating-point mein **denormal (subnormal)** numbers — bahut chhoti values
`< ~1e-38` — hardware pe **10–100x slower** ho sakti hain (microcode
assist). Agar benchmark ka data accidentally denormals produce karta (e.g.
ek decaying filter jo zero ki taraf jaata), to number data pe depend karega
aur "random" lagega.

**Mitigation:** `-ffast-math` (ya `_MM_SET_FLUSH_ZERO_MODE`) FTZ/DAZ enable
karta → denormals ko 0 treat, consistent speed. Ya realistic data use karo,
aur agar spikes dikhen to `perf stat -e fp_assist.any` check karo.

---

## Jaal 10: Benchmark ≠ reality

Sab pitfalls avoid karne ke baad bhi micro-bench galat ho sakta (lesson 01
trap 2/3):
- **Working set** — L1/L2-resident bench vs L3-miss real → 3–10x off.
- **Co-tenancy** — akela vs other threads sharing LLC / bandwidth / SMT.
- **Predictor state** — ek branch pattern 1M baar → perfect prediction;
  cold in reality.
- **Frequency** — 1s bench turbo; sustained real load downclocked.
- **Input distribution** — bench mein uniform random; real mein skewed
  (jo alag branches, alag cache behaviour).

**Isliye:** micro-bench = **hypothesis test**. Final proof = **profiler
in-context** (perf, lesson 10–11) on the real program with the real
workload.

---

## ⚠️ Traps / Common mistakes (summary)

| # | Jaal | Fix |
|---|---|---|
| 1 | DCE | `DoNotOptimize` output |
| 2 | Const-fold | `DoNotOptimize` input (opaque source) |
| 3 | Hoisting | body ko iteration pe depend karao |
| 4 | Cold start | warm-up pass, discard run[0] |
| 5 | Timer > op | batch ops per timestamp, ya fenced rdtsc |
| 6 | One run | min-of-N / distribution / repetitions |
| 7 | Alignment noise | multiple layouts; trust only > noise; check counters |
| 8 | Frequency | pin governor/freq, turbo off, report cycles/op too |
| 9 | Denormals | FTZ/DAZ (`-ffast-math`), realistic data |
| 10 | Bench ≠ reality | confirm with in-context profiler |

---

## Hands-on

```bash
./build.ps1 fast 35-PROFILING-BENCHMARKING/examples/04_benchmark_mistakes.cpp
```
Har bug ka BUG (0.000 ya inflated) aur FIX (real number) side-by-side.

```bash
# alignment noise khud dekho: ek dummy function add karo aur re-measure
./build.ps1 asm 35-PROFILING-BENCHMARKING/examples/04_benchmark_mistakes.cpp | grep -A2 align

# frequency (Linux):
perf stat -e task-clock,cycles,instructions ./bench   # GHz = cycles/task-clock stable?
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`DoNotOptimize` lagaya, ab safe" | output pe lagaya — input const-fold / hoist bhi check |
| "warm-up matlab CPU garam" | cache/TLB/predictor/page-tables warm; run[0] discard |
| "`now()` free" | ~38 ns; batch ya rdtsc |
| "same source = same number" | ±10% alignment/layout noise possible |
| "ns/op frequency-independent" | nahi; cycles/op hota hai — dono report |
| "sab jaal avoid = number = truth" | still verify in-context (profiler) |

---

## Exercises

1. Ye benchmark `-O2` pe 0.3 ns/iter (impossibly fast for a sqrt). Teen
   possible causes, aur har ke liye fix.
   ```cpp
   double x = 2.0;
   for (auto _ : state) { double r = std::sqrt(x); benchmark::DoNotOptimize(r); }
   ```
   <details><summary>Answer</summary>

   Sink laga hai (`DoNotOptimize(r)`), phir bhi 0.3 ns → (1) **`x` compile-
   time constant** (`= 2.0`) → `std::sqrt(2.0)` folded to a constant at
   compile time → no runtime `sqrt`. Fix: `double x = state.range(0);
   DoNotOptimize(x);`. (2) **Hoisting** — even if `x` runtime, `sqrt(x)` is
   loop-invariant → computed once outside the `for(_:state)` loop, body just
   re-sinks the same `r`. Fix: `x = std::sqrt(x) + 1.0;` (carried) ya array
   se `sqrt(data[i])`. (3) **`x` runtime + varied, par `sqrt` inlined to a
   single `sqrtsd`** which is ~4–6 cyc pipelined → at ~2 GHz that's ~2–3 ns
   throughput if independent, but if the shim/GB is dividing wall time by an
   inflated iteration count... unlikely. Most likely (1)+(2). After fixing:
   ~5–15 ns/iter for a real dependent `sqrt` chain.
   </details>

2. Version A aur B: A = 4.10 ns/op, B = 3.80 ns/op, dono min-of-50. B "7%
   faster" claim karein? `perf stat` A aur B dono pe: instructions same,
   cycles A = 1.05e10, B = 1.04e10, **branch-misses A = 2000, B = 55000**,
   L1-misses same. Kya chal raha?

   <details><summary>Answer</summary>

   Cycles almost identical (1.05e10 vs 1.04e10, ~1%) — **actual CPU work
   barely changed.** Instructions same. Par wall-time 7% alag → **frequency
   difference** between the two runs (ns/op = cycles/op ÷ GHz; cycles/op
   same, so GHz differed ~7%). B ka run turbo pe zyada tha, ya A thermally
   throttled. The `branch-misses` difference (2000 vs 55000) is real but
   55000 misses over 1e10 cycles is ~negligible time (~1e6 cycles = 0.01%).
   **Conclusion: A and B are the same speed**; the 7% is frequency noise.
   Fix: pin frequency (`cpupower -f`), turbo off, re-run. Report **cycles/op**
   (frequency-invariant): both ~identical → no real difference. This is why
   you always check counters, not just wall time.
   </details>

3. Tumhara microbenchmark stable hai (cv 1%) par jab woh code production
   binary mein jaata, wahan woh 4x slower profile karta. Micro-bench mein
   koi pitfall nahi tha. 3 reasons.

   <details><summary>Answer</summary>

   (1) **Working set** — micro-bench ka data L1/L2 mein fit tha; production
   mein yeh function ek bade pipeline ke beech chalta jahan L2/L3 doosre
   data se bhara → yeh ab har access L3/DRAM miss (folder 32). 4x easily.
   (2) **I-cache / iTLB pressure** — micro-bench mein sirf yeh function loop
   mein → I-cache mein resident. Production mein ye ek 500 KB hot path ka
   hissa → har call I-cache miss, BTB cold. (3) **Frequency** — micro-bench
   1s turbo pe; production sustained multi-core load pe all-core frequency
   (no turbo) + maybe AVX downclock from neighbouring code. (4) **Branch
   predictor** — micro-bench ek fixed data pattern → predictor perfect;
   production mein varied inputs → mispredicts. (5) **Contention** — shared
   locks / allocator / false sharing with other threads. Fix: profile the
   **real binary** with `perf` (lesson 10–11), look at cache-misses / IPC /
   frontend-bound **for this function in situ** — the micro-bench told you
   the instruction cost, the profiler tells you the real cost.
   </details>

---

## Interview questions

1. `-O2` ek naive benchmark ko 0 ns kaise banata — DCE, const-fold, hoisting teenon.
2. `DoNotOptimize` output pe **aur** input pe kyun.
3. Cold-start ke 4 components (code, data, page faults, predictor) aur warm-up.
4. Alignment / layout noise — kya hai, kaise mitigate, "trust only > noise".
5. Frequency scaling benchmark ko kaise distort karta; cycles/op kyon report karo.
6. Sab pitfalls avoid karne ke baad bhi micro-bench production se kyun alag.

---

## Next
→ [`10-perf-basics.md`](10-perf-basics.md)
