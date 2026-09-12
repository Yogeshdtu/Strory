# 01 — Measure karo, guess mat karo

## Prerequisites
- Folder `31-CPU-ARCHITECTURE` (pipeline, OoO, latency vs throughput)
- Folder `32-CACHE-MEMORY-PERFORMANCE` (cache, prefetch, measured benchmarks)
- Folder `33-COMPILER-OPTIMIZATION` (`-O2` kya karta, DCE, hoisting)
- Folder `34-ASSEMBLY` (asm padhna — "compiler ne actually kya banaya")

## Yeh topic abhi kyun
Tumne folders 31–34 mein **techniques** seekhi — cache-friendly layout,
branchless code, `__restrict`, LTO, SIMD. Har ek ka claim tha "yeh tez hai".
Woh claim **kaise verify** karte ho? Aur kisi bade program mein **kaunsa
hissa** dheema hai — yeh kaise pata karte ho?

Yeh folder wahi hai: **measurement, statistics, aur profiling**. Har
optimization ka pehla step. Bina iske tum andhere mein tir chala rahe ho.

---

## Rule 0: teri intuition galat hai

Performance ke baare mein programmer ka andaaza **aksar galat** hota. Kyun:

| Tum sochte ho | Asliyat |
|---|---|
| "yeh function slow hoga, complex jo hai" | compiler ne use 3 instructions mein kar diya |
| "yeh loop fast hai, simple jo hai" | har iteration ek cache miss = 100+ cycles |
| "`x / 2` aur `x / 7` same cost" | `/2` = ek shift; `/7` = ~20-cycle divide |
| "function call mehenga hai" | inline ho gaya, cost zero |
| "mera code CPU-bound hai" | 80% time `malloc` aur memory stalls mein |
| "yeh change 2x tez karega" | measured: 3% — bottleneck kahin aur tha |

Is repo mein bhi (CLAUDE.md Rule 2): ek division benchmark **modulo ki cost
chhupa raha tha**; ek bit-flags demo **zero saving** dikha raha tha kyunki
struct padding ne fark kha liya. Dono galat the — measure karne pe pakde gaye.

> **Isliye:** koi bhi performance statement — apna ya kisi aur ka — jab tak
> tune **chala ke number na dekha ho**, tab tak "shayad" hai, "hai" nahi.

---

## Premature optimization

> "Premature optimization is the root of all evil" — Knuth (1974)

Poora quote, jo log bhool jaate:

> "We *should* forget about small efficiencies, say about 97% of the time:
> premature optimization is the root of all evil. **Yet we should not pass
> up our opportunities in that critical 3%.**"

Matlab:
1. **Pehle sahi likho, saaf likho.** Chalne do.
2. **Measure karo** — kahan time ja raha (profiler).
3. **Us 3% ko optimize karo** jahan time ja raha. Baaki chhod do.
4. **Re-measure** — sach mein fark pada? (aksar nahi.)

"Premature" ka matlab **"measure kiye bina"** — timing ka nahi. Design phase
mein sahi algorithm (`O(n)` vs `O(n²)`) chunna premature nahi, **basic
engineering** hai. `int` ko `int16_t` banane se pehle profiler dekhna
chahiye — woh premature ho sakta.

---

## Amdahl's Law — kitna sudhaar possible hai

Agar program ka ek hissa jo total time ka fraction **p** leta hai, use tum
**s** guna tez kar do, to poore program ka speedup:

```
              1
  S  =  ---------------
        (1 - p) + p / s
```

**Intuition:** jo hissa tum touch nahi kar rahe (`1 - p`) woh ek **floor**
bana deta. Us se neeche nahi ja sakte, chahe optimized hissa infinitely
tez ho.

### Numbers

| p (hot part ka share) | s (uska speedup) | Poora program speedup |
|---|---|---|
| 0.10 | ∞ | **1.11x**  (90% untouched) |
| 0.50 | 2 | 1.33x |
| 0.50 | ∞ | **2.0x**  (aadha untouched → max 2x) |
| 0.90 | 2 | 1.82x |
| 0.90 | 10 | 5.26x |
| 0.95 | 20 | **10.3x** |
| 0.99 | ∞ | **100x** |

**Sabak 1:** ek chhote hisse ko bahut tez karne ka faayda kam. Pehle
profiler se pata karo **p** kitna bada hai — agar hot function total ka 5%
hai, use 10x karne se poora program sirf ~4.7% tez hoga.

**Sabak 2:** iska ulta — **"death by a thousand cuts"**. Koi ek 40% hotspot
nahi, par 20 functions har ek 3–5%. Tab flat profile hota, aur har ek ko
thoda-thoda theek karna padta (ya architecture badalna padta).

### HFT twist: Amdahl tail pe lagao

HFT mein "time" nahi, **p99 latency** minimize karni hoti. Amdahl wahan bhi
lagta: agar tick-to-trade ke 8 stages hain aur p99 pe stage 3 (parse) 60%
hai, to baaki 7 stages ko shave karne se p99 mushkil se hilega. Stage 3 ka
p99 girao.

---

## Throughput vs latency — kya optimize kar rahe ho?

Yeh **do alag cheezein** hain, aur optimization aksar ek ko behtar dusre ko
bura karti (folder 31 lesson 09):

| | **Throughput** | **Latency** |
|---|---|---|
| Sawaal | "per second kitna kaam?" | "ek kaam mein kitna time?" |
| Metric | ops/sec, GB/s, msgs/sec | ns/op, p50/p99/p99.9 |
| Batching | **madad karta** (amortize) | **nuksaan** (queue wait) |
| Kiske liye | batch jobs, analytics, encoding | HFT, trading, RPC, games |
| Measure | total_work / total_time | per-op distribution |

Ek server jo **2M req/s** handle karta par har req **50 ms** leta — high
throughput, bad latency. HFT mein latency raja hai; throughput tabhi jab
capacity chahiye.

> **HFT relevance:** tumhara metric **hamesha** ek latency percentile hoga —
> p50/p99/p99.9 tick-to-trade, ya per-stage. "Average throughput" HFT mein
> lagbhag bekaar. Isliye folders 04–07 (statistics, percentiles, jitter,
> histograms) is folder ka dil hain.

---

## Measurement ka workflow (poore folder ka roadmap)

```
  1. BASELINE      simple version likho, chalao, ek REAL number lo
                   (lesson 02–03: timing sahi se)
        |
  2. PROFILE       poore program pe: time KAHAN ja raha?
                   (lesson 10–14: perf, flame graph, callgrind, VTune)
        |
  3. HYPOTHESIS    "stage 3 slow hai kyunki cache misses" — ek theory
        |
  4. MICRO-BENCH   us hisse ka isolated benchmark (lesson 08–09)
                   — pitfalls se bacho (DCE, const-fold, cold start)
        |
  5. CHANGE ONE    ek cheez badlo (layout / algo / flag)
        |
  6. RE-MEASURE    same benchmark, min-of-N + distribution
                   (lesson 04–05: mean jhooth, percentiles sach)
        |
  7. EXPLAIN       kya badla aur KYUN — asm/perf-counters se confirm
                   (folder 34; lesson 11)
        |
  8. loop 2–7 jab tak target na mile, ya Amdahl floor na aa jaaye
```

Yeh **CLAUDE.md ka HFT performance engineering process hai** — har project
mein: build simple → measure → profile → bottleneck → optimize →
re-benchmark → **explain kya badla**. Kabhi seedha step 5 pe mat kudo.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — bina baseline ke optimize karna
"Yeh tez lag raha" — kis se compare? Pehla kaam: current code ka ek
reproducible number. Us ke bina "improvement" measure hi nahi hota.

### Trap 2 — micro-benchmark ko reality maan lena
Ek function ka isolated benchmark 5x tez — par program mein woh function
memory-bandwidth wall pe tha, ya cache alag tarah warm tha → real gain 0.
Micro-bench hypothesis test karta, reality **profiler in-context** batata.

### Trap 3 — `-O0` ya debug build pe benchmark
`-O0` numbers **poori tarah bekaar** — koi inlining, koi register allocation,
har variable stack pe. Benchmark hamesha `-O2` (ya release flags) pe. (folder
33 lesson 01, 14.)

### Trap 4 — ek run
Ek measurement = ek interrupt / frequency dip / cold cache ka shikaar. Min
of N (ya distribution) chahiye. (lesson 04, 09.)

### Trap 5 — Amdahl bhoolna
2 hafte lagaake ek function 10x kiya jo profile mein 4% tha → poora program
~3.6% tez. Woh 2 hafte 40% wale hotspot pe lagte.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "profiling baad ki cheez hai" | pehla step — baseline + hotspot |
| "mujhe pata hai kya slow hai" | ~har baar profiler surprise deta |
| "optimization = premature evil" | *measure kiye bina* optimization evil |
| "average latency 200 µs" (HFT) | p50/p99/p99.9 batao — average tail chhupata |
| "10x faster" (bina chalaye) | chalao, number lo, wahi likho |
| "throughput aur latency same baat" | ulte trade-off — kaunsa chahiye pehle tay karo |

---

## Exercises

1. Ek program ka profile: `main` 100%, usme `parse()` 8%, `compute()` 82%,
   `format()` 10%. Tumhare paas `parse` ko 5x aur `compute` ko 1.3x karne ka
   option hai (dono mein similar effort). Amdahl se dono ke poore-program
   speedup nikaalo. Kaunsa karoge?

   <details><summary>Answer</summary>

   `parse` 5x: `p = 0.08`, `s = 5`. `S = 1 / (0.92 + 0.08/5) = 1 / (0.92 +
   0.016) = 1 / 0.936 = 1.068x` → ~6.8% faster.
   `compute` 1.3x: `p = 0.82`, `s = 1.3`. `S = 1 / (0.18 + 0.82/1.3) = 1 /
   (0.18 + 0.6308) = 1 / 0.8108 = 1.233x` → ~23% faster.
   **`compute` karo** — 3x bada win, even though 5x < 1.3x "sounds" weaker.
   Amdahl: `p` (share) jeetta hai `s` (local speedup) se, jab tak `p` chhota
   na ho. Agar dono option same effort ke hain, hamesha bade `p` wale pe jao.
   </details>

2. Tumhara colleague kehta "maine yeh loop `std::transform` se replace kiya,
   ab yeh 4x tez hai." Tumhe kya-kya poochna chahiye is claim ko believe
   karne se pehle?

   <details><summary>Answer</summary>

   (1) **Kis flag pe?** `-O0` pe `std::transform` ka abstraction penalty
   dikhega jo `-O2` pe gayab. (2) **Kaise measure kiya?** Ek run ya min-of-N?
   Kya distribution dekha? (3) **Result use hota hai?** Bina sink ke ek loop
   0 ns dikha sakta (DCE) — "4x" do noise numbers ka ratio ho sakta. (4)
   **Input opaque tha?** Compile-time-known input pe dono loop fold ho sakte.
   (5) **Warm-up?** Pehla run cold-cache / page-fault. (6) **Poore program
   mein farak?** Yeh loop total ka kitna % hai (Amdahl)? (7) **Kis machine /
   compiler pe?** Reproduce hota? Answer: "chala ke dikhao, `-O2`, min-of-N,
   input `argv` se, aur profile mein yeh loop kitna % tha."
   </details>

3. Kab micro-benchmark **bekaar** hota hai even agar sab pitfalls avoid kiye
   ho? Do situation batao.

   <details><summary>Answer</summary>

   (1) **Working set mismatch** — micro-bench mein data L1/L2 mein fit hota,
   real program mein woh L3 se aage. Cache behaviour totally alag → ns/op
   3–10x off. (2) **Context / co-tenancy** — real program mein doosre threads
   memory bandwidth, LLC, aur SMT sibling share karte; micro-bench akela
   chalta. (3) **Branch predictor / BTB state** — micro-bench ek hi branch
   pattern hazaaron baar → predictor perfect; real program mein woh branch
   thanda hota. (4) **Frequency** — sustained real load pe CPU downclock
   karta (AVX / thermal); 1-second micro-bench turbo pe. In sab mein: micro-
   bench se hypothesis banao, phir **profiler se in-context confirm karo**.
   </details>

---

## Interview questions

1. Knuth ke "premature optimization" quote ka poora matlab — "premature"
   kis cheez ke against hai?
2. Amdahl's law — formula, aur "ek chhote hotspot ko infinitely tez"
   karne pe poora speedup kyun bounded hai.
3. Throughput vs latency — do metric, kab kaunsa, aur ek optimization jo
   ek ko behtar dusre ko bura karti.
4. Tumhe kaise pata karoge ki ek program CPU-bound hai ya memory-bound?
   (foreshadow: lesson 10 `perf stat` IPC / cache-misses.)
5. "Death by a thousand cuts" profile kaisa dikhta, aur us pe kya strategy?

---

## Next
→ [`02-timing-correctly.md`](02-timing-correctly.md)
