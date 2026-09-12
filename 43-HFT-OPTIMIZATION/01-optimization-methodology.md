# 01 — Optimization methodology: measure → profile → hypothesize → change → re-measure

## Prerequisites
- `35-PROFILING-BENCHMARKING` (poora)
- `36-LOW-LATENCY-CPP` (techniques ka toolbox)
- `42-HFT-NETWORKING/14-wire-to-wire-measurement.md` (per-hop breakdown)

## Yeh topic abhi kyun

Ab tumhare paas **techniques** hain (36), ek **matching engine** (40), ek
**order book** (39), ek **feed handler** (38), ek **pipeline** (41). Is folder
mein hum unhe **systematically** optimize karenge — guess se nahi, **process** se.

Yeh folder ka **poora** folder ek hi rule pe khada hai. Baaki 16 lessons is
rule ke steps hain.

---

## The loop

```
        ┌───────────────────────────────────────────────┐
        │                                               │
        ▼                                               │
   1. MEASURE  ──►  2. PROFILE  ──►  3. HYPOTHESIZE  ──► 4. CHANGE (ek)
   (baseline)      (kahan time)     (kya + kyun)        │
        ▲                                               │
        │                                               ▼
        └──────────  6. EXPLAIN  ◄──────  5. RE-MEASURE ─┘
                    (kya badla, kyun)     (same harness)
```

Har box zaroori hai. Sabse common galtiyan:

| Galti | Kya hota hai |
|---|---|
| 1 skip (no baseline) | "Lagta hai tez ho gaya" — proof nahi, regression pakad nahi sakte |
| 2 skip (no profile) | Galat jagah optimize — 5% wale code pe 2 hafte |
| 4 mein 3 changes ek saath | Kaunsa kaam aaya pata nahi; ek ne dusre ko chhupa diya |
| 5 skip (no re-measure) | "Optimization" ne kuch nahi kiya (ya ulta slow kiya) — pata nahi |
| 6 skip (no explain) | Agli baar wahi galti; team seekh nahi paati; cargo-culting |

---

## Step 1 — MEASURE (baseline)

Ek **number**, **reproducible**, **committed**. `01_baseline_pipeline.cpp`:

```
(A) TRUE per-tick average (clean loop, 1 clock) :   ~1900 ns/tick
    orders fired = 110
```

Rules (detail: `02-establishing-baseline.md`):
- `-O2` minimum. `-O0` ka number bekaar hai.
- Fixed input (yahan: `pipe::make_feed(120000)`, deterministic seed).
- Warm run, best-of-N ya percentile — mean nahi.
- Ise **likh lo** (commit message, lab notebook, ek `baseline.txt`). Har
  future change isi se compare hoga.

> **HFT relevance:** production mein baseline = ek saved p50/p99/p99.9
> latency histogram, ek known git commit pe. CI har commit pe re-measure
> karta; regression > threshold → build red.

---

## Step 2 — PROFILE (kahan time ja raha hai)

Guess **mat** karo. Do tareeke:

**(a) Poor-man's — per-stage rdtsc checkpoints.** `01`'s (B) output:

```
parse   :  ~880 ns/tick   (~45%)
book    :  ~930 ns/tick   (~48%)
signal  :  ~100 ns/tick   (~5%)
```

→ parse + book = 93%. Signal ko optimize karne ka koi matlab nahi.

**(b) Real profiler — `perf`.** `02_profile_analysis.sh`. `perf stat` batata
**kya** bound hai (memory / frontend / branch / divide), `perf report` batata
**kaunsa function**, `perf annotate` batata **kaunsi line**.

Amdahl's law: agar parse 45% hai aur tum use **infinitely fast** kar do,
best-case speedup = `1 / (1 - 0.45)` = **1.8×**. Bas. Isliye pehle sabse
bada slice.

---

## Step 3 — HYPOTHESIZE (kya + kyun)

Profile ne bola "parse 45%, `std::stod` + `std::string` alloc top pe".
Hypothesis: *"Hand-rolled integer parse on a `const char*`, price as
fixed-point int64, koi allocation nahi → parse ~5-10× sasta."*

Hypothesis mein **mechanism** hona chahiye ("kyun tez hoga"), sirf
"ye tez hoga" nahi. Mechanism galat ho sakta — tabhi to measure karte.

---

## Step 4 — CHANGE (ek, sirf ek)

Ek hypothesis, ek change. `git commit` uske pehle (clean point). Change
karo. Compile. **Correctness pehle** (neeche).

Agar ek "logical" change 3 files chhuta hai (jaise "fixed-point price"
parse + book + signal sab jagah) — theek, par **ek concept**. 5 alag concepts
ek commit mein mat ghusao.

---

## Step 5 — RE-MEASURE (wahi harness)

Bilkul wahi input, wahi build flags, wahi machine, wahi measurement code.
`04_before_after.cpp` dono versions ek process mein chalata:

```
v0 (naive)     :    ~1900 ns/tick
v3 (optimized) :      ~25 ns/tick
speedup        :      ~80x
```

Agar re-measure mein **change nahi dikha** ya **ulta slow** — change
**revert** karo (ya samjho kyun). "Shayad microarchitecture..." — nahi,
measure. Kabhi-kabhi tumhari hypothesis galat thi aur wahi asli seekh hai
(`36/12` hot-cold: expected win, mila ~0%).

---

## Step 6 — EXPLAIN (kya badla, kyun) — spec requirement

Har optimization ke saath **ek paragraph**: "parse 880→X ns kyunki
`std::string` copy + `std::stod`'s locale/rounding machinery hat gaya;
ab ek `const char*` walk + `*10 + digit`. Fixed-point isliye ki price
compare ab exact-integer, float rounding gone."

Yeh spec ka rule hai (`CLAUDE.md` §2.12–2.13). Bina explanation ke
optimization = cargo cult. Lessons `13/14/15` (case studies) poore
is step ke around hain.

---

## ⚠️ The correctness gate — NON-NEGOTIABLE

**Fast + galat = bekaar.** Har optimization ke baad:

```
=== output agreement (order-fire ticks) ===
  v0 fired 110 orders,  v3 fired 110 orders
  positional matches: 110 / 110
  -> IDENTICAL
```

`04_before_after.cpp` **pehle** agreement check karta, **phir** speedup
report karta. Agar agreement fail — speedup ka number chhapta bhi nahi
matter karta, optimization **reject**.

HFT mein yeh literal paisa hai: ek fixed-point rounding bug jo 0.01%
orders ko galat price pe bhej de — speedup se kahin zyada mehnga.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — "Ye toh obviously tez hoga" (guessing)
`std::map` → flat array "obviously" tez? Haan yahan. Par `std::list` →
`std::vector` for a queue? `36` ne dikhaya kabhi-kabhi barabar. **Measure.**

### Trap 2 — Micro-benchmark jo asli workload se match nahi karta
Parse ko isolation mein benchmark kiya, 10× tez. Pipeline mein daala,
2% farak — kyunki asli bottleneck book tha, parse ka output cache mein
already tha. Poore pipeline mein measure karo.

### Trap 3 — Ek change, do effects, net zero
Struct chhota kiya (cache better) par ek field ko recompute karna pada
(more instructions). Net: same. Dono ko alag-alag measure karo.

### Trap 4 — Optimize kiya, correctness test nahi chalaya
Sabse mehngi galti. Har change ke baad `06_engine_tests` / agreement check.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Pehle likh do, baad mein optimize" — bina baseline | Baseline pehle, warna "optimized" ka koi reference nahi |
| Ek commit mein 5 optimizations | Ek concept per commit — warna attribution impossible |
| Mean latency track karo | p50/p99/p99.9 — HFT mein tail hi sab kuch (`36/02`) |
| "Profiler overhead 10% hai, ignore" | rdtsc/perf khud cost — `03` isko explicitly handle karta |
| Speedup mila → done | Correctness gate pass hua? Explain likha? Tab done |

---

## Hands-on

```bash
./build.ps1 fast 43-HFT-OPTIMIZATION/examples/01_baseline_pipeline.cpp   # baseline
./build.ps1 fast 43-HFT-OPTIMIZATION/examples/03_optimized_pipeline.cpp  # optimized
./build.ps1 fast 43-HFT-OPTIMIZATION/examples/04_before_after.cpp        # both + agreement
# profile (Linux): 43-HFT-OPTIMIZATION/examples/02_profile_analysis.sh
```

---

## Exercises

1. `04_before_after.cpp` chalao. Speedup kitna? `orders fired` dono mein
   same? Agar seed badal do (`make_feed(120000, 7)`), agreement rehta?
   <details><summary>Answer</summary>
   Speedup is box pe ~80×. Orders dono mein barabar (110). Seed badalne pe
   bhi agreement 100% rehna chahiye — fixed-point signal math float ko
   exactly reproduce karta is workload pe. Agar kabhi 1-2 mismatch aaye,
   woh boundary float-rounding hai (`09-fixed-point.md`) — hide mat karo.
   </details>

2. Pipeline ka `signal` stage 5% hai. Tum use 0 ns kar do (magic). Poore
   pipeline ka naya time? Speedup?
   <details><summary>Answer</summary>
   Amdahl: `1 / (1 - 0.05)` = **1.053×**. ~1900 → ~1805 ns. 2 hafte ki
   mehnat, 5% gain. Isliye profile pehle — parse+book (93%) pe lago.
   </details>

3. Tumne parse optimize kiya, `04` chalaya, speedup 1.9× aaya, par
   `orders fired` 110 → 108. Kya karoge?
   <details><summary>Answer</summary>
   **Revert / debug — speedup irrelevant.** Correctness gate fail. 2 orders
   kahan gaye? Sambhavtah parse ne ek edge-case price galat parse kiya
   (leading zero? negative? missing decimal?). Fix karo, phir 110/110 pe
   speedup dobara measure.
   </details>

---

## Interview questions

1. Optimization ke 6 steps batao. Kaunsa sabse zyada skip hota hai aur
   kya nuksaan?
2. "Ye change tez hai" — bina baseline ke tum kaise jaante ho?
3. Amdahl's law: ek 30%-of-time function ko 3× tez karne se poore program
   ka speedup?  (Ans: `1 / (0.7 + 0.3/3)` = 1.25×)
4. Fast lekin output badal gaya — kya karoge, aur kyun?
5. rdtsc/perf khud latency add karte — is measurement bias ko kaise handle
   karoge?

---

## Next
→ [`02-establishing-baseline.md`](02-establishing-baseline.md)
