# 04 — Statistics: average jhooth bolta hai

## Prerequisites
- `02-timing-correctly.md`, `03-rdtsc-timing.md`
- `examples/02_percentiles.cpp`

## Yeh topic abhi kyun
Tumne 100000 latency samples le liye. Ab kya report karo? **"Average 210 ns"
sabse aam jawab hai — aur sabse galat.** Latency ka distribution
**right-skewed** hota (lambi upar wali tail), aur us par mean, median,
stddev, min sab **alag kahani** kehte. Yeh lesson: har number ka matlab,
aur kaunsa kab.

---

## Ek distribution, kai numbers

`02_percentiles.cpp` — ek chhota hash (~500 ns), par ~1% runs mein ek cold
buffer touch (cache-miss storm). 200k samples. **Measured (is box):**

```
  min      :    400.8 ns
  mean     :    697.8 ns    <- tail ne upar khincha
  median   :    691.3 ns    <- "typical" run (p50)
  stddev   :    247.0 ns
  p90      :    921.8 ns
  p99      :    991.9 ns
  p99.9    :   1653.1 ns
  p99.99   :   4849.2 ns
  max      :  43843.3 ns    <- worst dekha  (median se 63x!)
```

Ek hi data, aur:
- **mean 698** — "ausat"
- **median 691** — beech wala sample
- **min 401** — jitter-free asli cost
- **max 43843** — ek out-of-200000 din
- **p99.9 1653** — 1-in-1000

Kaunsa "the number" hai? **Depends kya poochh rahe ho.**

---

## Mean (average) — aur woh kyon dhoka deta

```
mean = (Σ xᵢ) / n
```

**Problem: mean har outlier ko poora weight deta.** Ek 40000 ns spike 200000
samples mein → akela mean ko `40000 / 200000 = 0.2 ns` upar kheenchta. Sau
aise spikes → +20 ns. Right-skewed data mein mean **peak ke daayen** baith
jaata, jahan actually kam samples hote.

`02` mein: **sirf ~47% samples mean se neeche/upar** balanced hai yahan
kyunki bulk symmetric hai — par jab distribution zyada skewed ho (jaise
`08_latency_recorder` ka lognormal), **~65–70% samples mean se neeche** hote.
Yani "average" wala run actually **typical se dheema** hota.

**Mean kab theek:** jab tum **total throughput** nikaal rahe ho — "1M ops
mein 210 ms lage → 210 ns/op average" — yeh valid hai, kyunki tumhe sum
chahiye tha. Latency **distribution** describe karne ke liye mean bekaar.

---

## Median (p50) — "typical"

Sorted data ka beech wala. **Outliers se affected nahi** — ek sample 10x
bada ho ya 100x, median wahi. Isliye "typical run kitna leta" ke liye median.

`02`: median **691**, mean **698** — kareeb, kyunki is workload ka bulk
tight hai. Skewed workload mein median << mean.

**Median akela bhi kaafi nahi** — woh tail ke baare mein kuch nahi kehta.
p50 = 691 aur p99.9 = 1653 dono chahiye poori tasveer ko.

---

## Min — jitter-free floor

`02`: **min = 400.8 ns**. Yeh woh run hai jahan **koi interference nahi
hui** — no interrupt, no cache miss, no frequency dip, no context switch.

> **Micro-benchmark mein min hi "asli cost" ke sabse kareeb hai.** Baaki
> sab = min + kuch overhead. Isliye `bench_ns_per_op` min-of-N leta hai
> (lesson 02).

**Caveat:** min sirf tab meaningful jab tum **ek fixed piece of work** ko
baar-baar naap rahe ho (deterministic). Agar har call ka input alag hai
(alag branch, alag data size), to min "sabse asaan input" hai, "typical"
nahi — tab median/mean of the workload distribution chahiye.

**Production mein min bekaar** — wahan tumhe interference *samet* chahiye,
woh reality hai.

---

## Variance aur standard deviation

```
variance σ² = Σ(xᵢ - mean)² / n
stddev   σ  = √variance
```

`σ` = "spread" — samples mean se ausatan kitna door.

**Problem: `σ` bhi outliers se blow up hota** (squared term!). `02` mein
`σ = 247` — par distribution ke ~99% samples 400–990 mein hain (spread ~590,
`σ` ~247 ka ~2.4x). `σ` un few 40000 ns spikes se inflated hai.

**`σ` sirf tab useful jab distribution ~normal (Gaussian) ho** — tab
`mean ± 2σ` ≈ 95% data. Latency **normal nahi hoti** (skewed, bounded below
by min, unbounded above). Isliye latency reports mein `σ` **mostly bekaar** —
percentiles better.

**Coefficient of variation** `CV = σ / mean` — "kitna consistent?" Google
Benchmark ise report karta. `CV < 2%` → clean benchmark; `CV > 5%` →
noisy, run pin/isolate karke dobara.

**Robust alternative: MAD** (Median Absolute Deviation) = `median(|xᵢ -
median|)`. Outlier-resistant spread. Ya bas IQR = p75 − p25.

---

## Distribution ki shakl padho

Sirf numbers nahi — **histogram dekho** (lesson 07). Latency me common
shapes:

```
  UNIMODAL + right tail        BIMODAL (do raste)
  (aam case)                   (fast path + slow path)
    #                            #              #
    ##                           ##            ##
    ###__                        ###          ###
    ####----____                 ####        ####
    ##############___........     .....______.....
    ^peak    ^tail               ^fast     ^slow (e.g. lock miss, cache miss)
```

**Bimodal → do alag code paths.** `08_latency_recorder` ka measured log-plot
exactly yeh dikhata: ek bulk hump **~276 ns** pe, ek alag hump **~10–22 µs**
pe (injected spikes). Mean (357 ns) **kisi bhi hump pe nahi baithta** —
do modes ke beech ek "khaali" jagah pe. Mean of a bimodal = meaningless.

---

## Kitne samples chahiye?

- **Deterministic micro-bench** (min-of-N): N = 15–50 kaafi. Tum min dhoondh
  rahe ho, tail nahi — jaldi converge.
- **Percentile estimation:** p99 ke liye kam se kam **~1000 samples** (10
  us se upar), p99.9 ke liye **~10000+**, p99.99 ke liye **~100000+**.
  Rule of thumb: `pN` estimate karne ko `~100 / (1 - N/100)` samples chahiye
  bare minimum, `10x` woh comfort ke liye.
- **`02` 200k samples leta** → p99.99 (= 20 samples us se upar) barely
  meaningful, p99.9 (200 samples) solid.

Zyada samples ≠ zyada accurate agar system state drift kar raha (thermal,
background load). **Balance:** enough samples for the percentile you claim,
collected in a window short enough that conditions are stable.

---

## Kya report karna hai

| Scenario | Report |
|---|---|
| Micro-benchmark, "kitna fast" | **min** (+ optionally p50, and CV as a noise flag) |
| Latency SLA / user-facing | **p50, p99, p99.9** (aur max agar hard limit hai) |
| Throughput | **mean** (total work / total time) — yahan sahi |
| Regression check (CI) | min ya p50, + threshold; noise ke liye CV gate |
| HFT tick-to-trade | **p50, p99, p99.9, p99.99, max** per stage |
| Comparing A vs B | dono ka **full distribution** overlay, ya p50+p99 dono; sirf mean nahi |

**Kabhi akela "average" mat report karo latency ke liye.** Agar ek hi number
dena hai — **median**, mean nahi.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — mean se latency report karna
Tail mean ko upar kheenchti; "average" typical se dheema hota. Median +
percentiles.

### Trap 2 — `σ` ko normal-distribution maan ke use karna
Latency skewed hai. `mean ± 2σ` 95% cover nahi karta. Percentiles ya MAD.

### Trap 3 — bimodal data pe koi bhi single number
Do modes → mean beech ki khaali jagah pe, median ek mode pe (jo majority),
`σ` huge. Histogram dekho, do modes ko alag analyze karo.

### Trap 4 — production mein min report karna
Min = best case, interference-free. Production reality = interference
included. Wahan p99/p99.9.

### Trap 5 — p99.99 claim, 5000 samples
p99.99 = "0.01% se bura" = 5000 mein 0.5 samples. Statistical noise. Us
percentile ke liye 100k+ samples.

### Trap 6 — samples ke beech state drift
1 hour ka soak: pehle 10 min cold, phir thermal throttle kick, phir
background cron. "Distribution" actually 3 alag distributions ka mixture.
Stable window mein collect karo, ya time-bucket karke dekho.

---

## Hands-on

```bash
./build.ps1 fast 35-PROFILING-BENCHMARKING/examples/02_percentiles.cpp
```
Dekho: `mean (698) ~ median (691)` — is workload ka bulk symmetric. Par
`max / median = 63x`, `p99.99 / median = 7x`. "Mean theek dikh raha" ≠
"tail theek". Run dobara — tail numbers (p99.99, max) **badal jaate**, bulk
(min, median, p90) stable.

```bash
./build.ps1 fast 35-PROFILING-BENCHMARKING/examples/08_latency_recorder.cpp
```
Iska log-plot **bimodal** — mean (357 ns) kisi hump pe nahi.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "average latency = the number" | median + p99/p99.9; average sirf throughput |
| "mean ≈ typical" | skew mein typical (median) < mean |
| "±2σ = 95% range" | sirf normal distributions; latency skewed |
| "min bekaar hai" | micro-bench mein min = jitter-free floor |
| "zyada samples = better" | sirf agar conditions stable; drift mixture banata hai |
| "ek number kaafi" | distribution — kam se kam p50 + p99 |

---

## Exercises

1. Do benchmarks, A aur B, ka same code alag machine pe:
   - A: mean 250 ns, median 210 ns, p99 240 ns, max 9000 ns
   - B: mean 230 ns, median 225 ns, p99 400 ns, max 800 ns

   Kaunsa "faster" hai? Kaunsa tum HFT hot path pe chahoge?

   <details><summary>Answer</summary>

   **"Faster" (typical): A** — median 210 < 225, p99 240 < 400. A ka bulk
   tighter aur quicker. A ka mean (250) median (210) se upar hai kyunki woh
   9000 ns max spike (aur uske jaise kuch) mean ko kheench rahe — par woh
   rare hain (p99 tak nahi pahunche).
   **HFT hot path: B** — A ka **max 9000 ns** ek disaster hai; ek missed
   quote / blown risk limit. B ka worst case **800 ns** bounded hai. HFT
   mein "typical thoda dheema par tail bounded" >> "typical fast par kabhi
   9000 ns". B predictable hai. (Ideal: A ka bulk + B ka tail — spike ka
   source dhoondo aur A mein fix karo.)
   </details>

2. Ek latency histogram do humps dikhata: ~80% samples ~300 ns pe, ~20%
   samples ~3000 ns pe. mean kya hoga approx? Woh mean kya "represent"
   karta?

   <details><summary>Answer</summary>

   `mean ≈ 0.8 × 300 + 0.2 × 3000 = 240 + 600 = 840 ns`. Yeh 840 ns
   **kisi bhi actual run ke kareeb nahi** — na 300 (fast path), na 3000
   (slow path). Koi request ~840 ns nahi leti. Mean yahan ek **arithmetic
   artefact** hai. Sahi analysis: "80% requests 300 ns (fast path), 20%
   requests 3000 ns (slow path — kya? lock? cache? branch mispredict?)."
   Slow path ka **trigger** dhoondo (yeh 20% kyun?) — woh optimization ka
   asli target hai. p50 = 300 (fast, majority), p90 ≈ 3000 (slow path shuru),
   p99 ≈ 3000+.
   </details>

3. Tumhara CI benchmark min-of-20 report karta aur ek threshold pe fail
   karta. Ek din woh flaky ho jaata (kabhi pass, kabhi 15% slow). Root
   cause kya ho sakte, aur min-of-20 ke bawajood kyun?

   <details><summary>Answer</summary>

   Min-of-20 tabhi robust jab **ek clean run milne ka chance** ho har baar.
   Agar CI machine **hamesha loaded** hai (doosre jobs same box pe, noisy
   neighbours in cloud), to 20 ke 20 runs perturbed → "min" bhi inflated,
   aur uska inflation run-to-run alag → flaky. Causes: (a) shared CI runner
   pe co-tenant load, (b) **frequency scaling** — CI box thermal/power-capped,
   turbo kabhi milta kabhi nahi (±15% easily), (c) **address-space layout
   randomization** → code/stack alignment change → ±5–10% (lesson 09), (d)
   noisy `perf`/coverage instrumentation left on. Fixes: dedicated/isolated
   runner, pin frequency (`cpupower frequency-set`), pin to isolated cores,
   `-no-pie` ya alignment control, more reps + compare **p50** with a
   **relative** threshold (±X% vs a rolling baseline) not absolute.
   </details>

---

## Interview questions

1. Latency distribution ke liye mean kyon misleading; kaunsa number instead.
2. Mean / median / min / max — har ek kab report karo.
3. Standard deviation latency ke liye kyun kam useful; alternatives (CV, MAD, IQR).
4. Bimodal distribution — kya batata, aur single summary number kyun fail.
5. p99.9 claim karne ke liye kitne samples chahiye (order of magnitude), kyun.
6. Micro-benchmark mein "min" aur production mein "p99" — dono kyun sahi apne context mein.

---

## Next
→ [`05-percentiles.md`](05-percentiles.md)
