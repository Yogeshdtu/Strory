# 02 — Establishing a baseline: reproducible benchmarks

## Prerequisites
- `01-optimization-methodology.md`
- `35-PROFILING-BENCHMARKING/03-microbenchmarking-pitfalls.md`

## Yeh topic abhi kyun

Step 1 hai poore process ka. Ek **kharaab** baseline (noisy, non-reproducible,
ya galat cheez measure karta) tumhari saari aage ki decisions ko poison kar
deta. Pehle isko theek se karo.

---

## Ek baseline number ke 6 requirements

| # | Requirement | Kyun |
|---|---|---|
| 1 | **-O2** (ya jo prod use karta) | `-O0` pe `std::` sab kuch out-of-line — number fake |
| 2 | **Fixed, deterministic input** | Alag input = alag number = comparison invalid |
| 3 | **Warm** (caches, branch predictor, pages) | Pehla run hamesha dheema — cold-start alag cheez hai |
| 4 | **Distribution, mean nahi** | p50/p99/p99.9 — HFT tail-sensitive (`36/02`) |
| 5 | **Measurement overhead accounted** | rdtsc/`now()` khud ~15-40 ns leta |
| 6 | **Committed** (git SHA + number) | Regression detect karne ke liye reference chahiye |

---

## `01_baseline_pipeline.cpp` — do measurements, jaan-boojh kar

### (A) TRUE per-tick average — ek hi clock, koi in-loop probe nahi

```cpp
const auto t0 = ch::steady_clock::now();
for (std::size_t i = 0; i < n; ++i) { p.set_index(i); p.process_one(i, false); }
const double sec = ch::duration<double>(ch::steady_clock::now() - t0).count();
// ns/tick = sec * 1e9 / n
```

Ek `now()` shuru mein, ek end mein. Loop ke **andar kuch nahi**. Yeh number
sabse **honest** hai — measurement ne kuch add nahi kiya.

```
(A) TRUE per-tick average (clean loop, 1 clock) :   ~1900 ns/tick
```

**Yeh woh number hai jo commit hota hai.**

### (B) per-stage attribution — 4 rdtsc probes per tick

```cpp
t0 = tsc(); parse(...);  t1 = tsc(); book(...);  t2 = tsc(); signal(...); t3 = tsc();
```

Yeh batata **kahan** time ja raha (parse/book/signal ka ratio). Par **har
probe khud** `lfence; rdtsc; lfence` = ~20-40 cycles. 4 probes = ~80-160
cycles = ~40-80 ns **added** per tick.

v0 (~1900 ns/tick) pe 80 ns = **4% noise** — ratios ke liye theek.
v3 (~25 ns/tick) pe 80 ns = **300%+ noise** — (B) v3 pe bekaar:

```
v3 (B):  parse ~34   book ~36   signal ~27   ns/tick   (sum ~108 — sab probe cost!)
v3 (A):  ~25 ns/tick   <-- yeh sach hai
```

**Sabak:** jaise code tez hota jaata, tumhara profiler ka probe naya
bottleneck ban jaata. Fast code ke liye: bade batches time karo (per-tick
amortize), ya sampling profiler (`perf`), ya hardware trace (Intel PT).

---

## Reproducibility — run-to-run variance

Is unpinned Windows desktop pe v0 (A) run-to-run:

```
run 1: 1906 ns    run 2: 1727 ns    run 3: 2148 ns    run 4: 1880 ns
```

~20% spread! OS scheduler, DVFS (CPU clock badalta), turbo, background
processes. Isliye:

- **Best-of-N** lo (`bench()` `std::min` leta) — noise sirf UPAR add karta,
  neeche nahi. Min = "is code ne kam se kam kitna liya."
- Ya **many samples + percentile** (p50 stable, p99 noisy).
- **Ratios quote karo, absolutes nahi.** "v3 v0 se ~80× tez" — yeh
  reproduce hota. "v3 25.1 ns" — yeh nahi.
- Serious measurement: core pin karo (`41/06`), DVFS lock, isolated core
  (`isolcpus`), `SCHED_FIFO`. Tab absolutes bhi ~repeatable.

> **HFT relevance:** production benchmark rigs = dedicated hardware, BIOS
> mein turbo/C-states off, `isolcpus` + `nohz_full` + IRQ steering, ek
> pinned thread. Aim: p99.9 run-to-run variance < 2%. Tabhi ek 5% regression
> detect hota.

---

## Deterministic input

```cpp
const pipe::Feed feed = pipe::make_feed(120000);   // seed fixed = 43
```

`make_feed` ka RNG splitmix64 hai, seed hard-coded. **Har run bilkul wahi
120000 messages.** v0 aur v3 ko **wahi feed** milta (`04` mein ek hi `feed`
object dono ko pass hota). Agar input random hota:
- v0 run pe ek feed, v3 run pe dusra → "speedup" mein feed-difference ghus jaata
- agreement check impossible

Real replay: production mein ek **recorded PCAP / market-data capture** ko
loop mein feed karte — asli distribution, asli edge cases, deterministic.

---

## `keep()` / DoNotOptimize — warna benchmark khali

```cpp
template <class T> static inline void keep(T& v) { asm volatile("" : "+r,m"(v) : : "memory"); }
```

Agar benchmark ka result kahin use nahi hota, `-O2` poora loop **delete**
kar deta ("dead code"). `keep(v)` compiler ko bolta "yeh value escape kar
gayi, isse compute karna zaroori hai" — bina koi actual instruction emit
kiye. `35/03` mein detail.

Symptom: "meri optimization ne code ko **0 ns** kar diya!" → 99% chance
compiler ne loop hata diya. `keep()` lagao.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — pehla run measure karna
Cold I-cache, cold D-cache, cold branch predictor, cold TLB, lazy pages,
CPU abhi turbo pe nahi chadha. Pehla run 2-5× dheema ho sakta. Warm-up
iterations chalao (discard karo), phir measure.

### Trap 2 — mean report karna
Ek 50ms GC-style pause (yahan: `std::vector<std::string>` realloc) mean ko
kha jaata. p50 usse bacha rehta. HFT: p99.9 report karo, mean kabhi nahi.

### Trap 3 — `-O0` ya default pe benchmark
`std::string`, `std::map`, `std::stod` — sab `-O0` pe function calls,
no inlining. Number 10× galat. **Hamesha `-O2`** (`./build.ps1 fast`).

### Trap 4 — measurement code ko optimization ka hissa maanna
v3 ka (B) sum ~108 ns dikhaata jabki (A) ~25 ns. Agar (B) ko baseline
maano to v3 "improvement" chhota dikhega. **(A) is truth.**

### Trap 5 — different machine pe compare
Baseline laptop pe, "optimized" desktop pe → speedup meaningless. Same
box, same session, ideally same binary (`04` dono ek process mein).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Ek run kaafi hai | Best-of-30 ya 100+ samples, percentile |
| `now()` free hai | ~15-25 ns; fast code pe matters — batch karo |
| Random input zyada "realistic" | Deterministic replay — realistic AUR reproducible |
| Mean latency | p50/p99/p99.9 histogram |
| "Optimized ne 0 ns kiya" = win | Compiler ne loop delete kiya — `keep()` lagao |

---

## Hands-on

```bash
./build.ps1 fast 43-HFT-OPTIMIZATION/examples/01_baseline_pipeline.cpp
# 3-4 baar chalao. (A) number kitna vary karta? Ratio (B parse : B book) stable?
```

---

## Exercises

1. `01` ko 5 baar chalao. (A) ka min, max, spread %? Kaunsa number
   commit karoge?
   <details><summary>Answer</summary>
   Is box pe ~1700–2150, spread ~20%. Commit karo: **min** (ya p50 over
   many samples) + "measured on <box>, unpinned, ±20% run-to-run". Ratio
   ko primary banao.
   </details>

2. `01`'s (B) mein v0 ka `signal` ~100 ns dikhta. v3 ka bhi ~27 ns.
   Dono real hain?
   <details><summary>Answer</summary>
   v0 ~100 ns: mostly real (deque re-sum + 2 divisions + occasional string
   alloc), thoda probe cost. v3 ~27 ns: **mostly probe cost** — actual v3
   signal work < 5 ns. v3 ke (B) numbers ko "≈ 2×(lfence+rdtsc)" samjho,
   truth (A) hai.
   </details>

3. Tumne `keep(v)` hata diya `bench()` se. `06_division_elimination.cpp`
   ka output kya hoga?
   <details><summary>Answer</summary>
   Sab variants ~0.000 ns/elem (ya identical tiny number) — `-O2` ne
   result unused dekhkar poore loops delete kar diye. Benchmark khali.
   `keep()` wapas lagao.
   </details>

---

## Interview questions

1. Reproducible benchmark ke liye kya-kya fix karna padta (input, flags,
   machine, warm-up)?
2. Best-of-N kyun (mean/median nahi) ek latency **floor** measure karne ke
   liye?
3. Tumhara profiler har call pe 30 ns add karta. 25 ns/op wale code ko
   kaise measure karoge?
4. Run-to-run variance 20% hai. 5% ki optimization kaise detect karoge?
5. `keep()` / `DoNotOptimize` kya karta, kaise (bina instruction emit kiye)?

---

## Next
→ [`03-finding-bottlenecks.md`](03-finding-bottlenecks.md)
