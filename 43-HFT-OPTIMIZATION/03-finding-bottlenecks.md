# 03 — Finding bottlenecks: profiling the full pipeline

## Prerequisites
- `02-establishing-baseline.md`
- `35-PROFILING-BENCHMARKING/05-perf-basics.md`, `07-perf-stat-counters.md`

## Yeh topic abhi kyun

Baseline number hai (~1900 ns/tick). Ab **kahan** ja raha hai? Aur zyada
important — **kyun** (memory? branch? divide? frontend?). Galat answer =
mahine barbaad.

---

## Do level: WHERE aur WHY

### WHERE — kaunsa stage / function

**(a) Poor-man's profiler — manual checkpoints.** `01`'s (B):

```
parse   :  ~880 ns/tick   (~45%)
book    :  ~930 ns/tick   (~48%)
signal  :  ~100 ns/tick   (~5%)
```

Fayda: zero setup, kaam karta har platform pe, exact stage boundaries tum
decide karte. Nuksaan: probe cost (fast code pe), aur "parse" ke andar
kya (string alloc? stod? find?) nahi batata.

**(b) Sampling profiler — `perf record` / VTune / Instruments.**
`02_profile_analysis.sh`:

```bash
perf record -F 2000 --call-graph dwarf -- ./base
perf report --stdio --percent-limit 1
```

Expected top symbols yahan: `std::stod` / `__strtod` / `std::string`
ctor / `std::_Rb_tree_increment` (map traversal) / `operator new`.
→ confirms (B): parse + book.

`perf annotate <symbol>` — us function ki **kaunsi instruction** pe
samples. `idiv` dikhe → lesson 10. Load ke aage pipeline stall → cache
miss → lesson 07.

### WHY — kya "bound" hai

`perf stat -d -d -d ./base`. Padho:

| Counter | Signal | Lesson |
|---|---|---|
| **IPC** (instr/cycle) < 1.0 | stalled — memory ya frontend | neeche |
| `LLC-load-misses` high | data cache miss — RAM ja raha | `07`, `11`, `36/10` |
| `branch-misses` > 2-3% | predictor hurt | `04`, `05`, `36/06` |
| `stalled-cycles-frontend` high | I-cache / decode / iTLB | `06` |
| `arith.divider_active` high | division bound | `09`, `10` |
| time in `malloc`/`_Rb_tree` | allocation / node-chasing | `13`, `14`, `36/04` |

**Top-down method** (Intel's TMA, `perf stat --topdown` ya `toplev`):
har cycle ko 4 bucket mein daalta — Retiring (useful), Bad Speculation
(mispredict), Frontend Bound (fetch/decode), Backend Bound (execute/memory).
Sabse bada bucket = tumhari class of problem.

---

## Amdahl's law — pehle sabse bada slice

```
speedup_overall = 1 / ( (1 - p) + p / s )
```
`p` = optimize kiye jaane wale hisse ka fraction, `s` = us hisse ka local speedup.

Is pipeline mein:

| Slice | p | Agar s = ∞ (infinitely fast) | Realistic |
|---|---|---|---|
| signal | 0.05 | 1.05× max | ~1.03× |
| parse | 0.45 | 1.8× max | ~1.6× |
| book | 0.48 | 1.9× max | ~1.7× |
| parse + book | 0.93 | **14×** max | ~10-80× (measured: ~80×) |

Signal ko 100× tez karo → poore pipeline 1.03×. Parse+book dono attack
karo → measured **~80×**. **Pehle sabse mota slice.**

Aur: ek slice hataao → agla slice ab bada % → naya bottleneck. `03`'s v3
(B): parse/book gir gaye, ab teenon ~barabar (aur mostly probe cost).
Optimization **iterative** hai.

---

## Per-stage rdtsc — kaise sahi lagayein

```cpp
std::uint64_t t0 = tsc();
parse(line, ...);
std::uint64_t t1 = tsc();          // parse ka time = t1 - t0
book_apply(...);
std::uint64_t t2 = tsc();          // book ka time  = t2 - t1
```

- `tsc()` mein `lfence` dono taraf — warna out-of-order CPU `rdtsc` ko
  aage-peeche kar deta, garbage deltas.
- Accumulate karo (`ns_.parse += t1 - t0`), har tick print mat karo.
- End mein `/ tpns / ticks` — average per stage.
- **Probe cost ko yaad rakho**: 4 probes ka ~80 ns har tick ke total mein
  add hai. Ratios trust karo, absolutes ko "±80 ns" samjho.

Better for very fast code: **stage ko alag benchmark karo** — sirf parse
ko 10M baar loop mein, ek clock. Par tab cache warm ho jaata (real
pipeline mein parse ke baad book cache thrash karta) — number optimistic.
Trade-off. In-pipeline attribution + isolated micro-bench, dono dekho.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `-O0` / no-inline binary profile karna
Har `std::` call ek symbol → profile "std::string::string 30%" dikhaayega
jo `-O2` pe inline ho jaata. **`-O2 -g -fno-omit-frame-pointer`** se profile.

### Trap 2 — sirf "kaunsa function" dekhna, "kyun" nahi
`perf report` bola "`match()` 60%". Toh? Memory-bound? Branch-bound?
Divide? `perf stat` + `perf annotate` bina, tum abhi bhi guess kar rahe.

### Trap 3 — self-time vs children-time gadbad
`perf report` mein "main 95%" — obviously, sab uske andar hai. `--no-children`
ya flamegraph se **self-time** dekho (jo us function ne KHUD kiya).

### Trap 4 — profile overhead workload ko badal deta
`perf record -F 40000` (bahut high freq) khud 20%+ overhead → sample
distribution skew. `-F 1000-4000` kaafi. Ya PMU-based (`-e cycles`)
precise events.

### Trap 5 — ek run ka profile
Ek run mein ek background process ne cache thrash kiya → "memory-bound"
dikha. 3-4 baar profile karo, consistent picture dekho.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Lagta hai parse slow hai" | `perf`/checkpoints se confirm, phir chhuo |
| `perf report` = poora answer | + `perf stat` (kya bound) + `annotate` (kaunsi line) |
| Sabse pehle jo dikha optimize karo | Amdahl: sabse bada % pehle |
| Ek optimization = done | Slice hata → naya bottleneck → repeat |
| Profiler overhead ignore | High sample-rate workload distort karta |

---

## Hands-on

```bash
./build.ps1 fast 43-HFT-OPTIMIZATION/examples/01_baseline_pipeline.cpp   # (B) attribution
# Linux: ./43-HFT-OPTIMIZATION/examples/02_profile_analysis.sh           # perf stat/report/annotate
```

---

## Exercises

1. `01`'s (B): parse ~45%, book ~48%. Dono ko 5× tez kar do. Amdahl se
   poore pipeline ka speedup?
   <details><summary>Answer</summary>
   p = 0.93, s = 5:  `1 / (0.07 + 0.93/5)` = `1 / 0.256` ≈ **3.9×**.
   (Measured ~80× is bahut zyada hai kyunki parse+book actually ~30-50×
   tez hue, 5× nahi — `std::map`→array aur `stod`→int-parse huge hain.)
   </details>

2. `perf stat` bole: IPC 0.4, LLC-load-misses bahut high, branch-misses
   0.5%. Kaunsi class ka problem? Kaunsa lesson?
   <details><summary>Answer</summary>
   **Memory-bound** (low IPC + high LLC miss, branches theek). Data cache
   problem → `07` (struct layout / SoA), `11` (lookup table cache
   trade-off), `36/10` (cache locality), `36/04-05` (preallocation — map
   nodes RAM mein bikhre hote).
   </details>

3. v3 ke (B) mein teenon stage ~30 ns dikh rahe, sum ~108, par (A) ~25.
   Iska matlab?
   <details><summary>Answer</summary>
   v3 ka actual work itna kam hai ki 4 rdtsc probes (~80 ns total) usse
   bade hain. (B) v3 pe measurement-dominated — ratios bhi shaky. v3 ko
   profile karna ho to batch-timing ya `perf` sampling use karo, per-op
   rdtsc nahi.
   </details>

---

## Interview questions

1. "WHERE" aur "WHY" profiling mein farak? Kaunse tools har ek ke liye?
2. Top-down (TMA) ke 4 buckets? Har ek ka matlab?
3. Amdahl: 80% code ko 10× tez → overall? (`1/(0.2+0.08)` ≈ 3.6×)
4. `perf report` "function X 40%" bola. Aage kaunse 2 commands, kya
   dekhne?
5. Poor-man's rdtsc profiler kab kaafi, kab `perf` chahiye?

---

## Next
→ [`04-hot-cold-path-separation.md`](04-hot-cold-path-separation.md)
