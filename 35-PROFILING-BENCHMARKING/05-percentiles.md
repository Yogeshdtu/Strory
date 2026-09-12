# 05 — Percentiles: p50, p99, p99.9, aur "nines"

## Prerequisites
- `04-statistics.md`
- `examples/02_percentiles.cpp`, `examples/08_latency_recorder.cpp`

## Yeh topic abhi kyun
Lesson 04 mein tay hua: latency ke liye **percentiles** report karo. Ab —
kaunse percentiles, unhe **compute** kaise karo, "p99.9" ka exact matlab
kya, aur **tail kyun itna zaroori** (especially fan-out systems aur HFT mein).

---

## Percentile kya hai

**p_N** = woh value jiske neeche data ka **N%** hota.

- **p50** = median. Aadhe samples isse kam.
- **p90** = 90% samples isse kam, 10% zyada.
- **p99** = "1 out of 100 request isse bura."
- **p99.9** = "1 out of 1000."
- **p99.99** = "1 out of 10000."
- **p100** = max.

`02_percentiles.cpp` measured (is box):
```
  p50    :   691 ns
  p90    :   922 ns
  p99    :   992 ns
  p99.9  :  1653 ns
  p99.99 :  4849 ns
  max    : 43843 ns
```
Har "nine" add karne pe number **jump** karta — p99→p99.9 65% zyada,
p99.9→p99.99 **3x**. Yeh tail ki shakl hai.

---

## Compute kaise — nearest-rank

Sabse simple aur common:

```cpp
// sorted = ascending sorted samples
double percentile(const std::vector<double>& sorted, double p) {
    if (sorted.empty()) return 0;
    // nearest-rank: rank = ceil(p/100 * N), 1-indexed
    size_t rank = (size_t)std::ceil(p / 100.0 * sorted.size());
    rank = std::clamp<size_t>(rank, 1, sorted.size());
    return sorted[rank - 1];
}
```

- **Sort** chahiye — `O(n log n)`. Bade n pe yeh cost. Alternatives:
  - `std::nth_element(v.begin(), v.begin()+k, v.end())` — `O(n)` average,
    ek percentile ke liye (partial sort).
  - **Histogram** (lesson 07, `08_latency_recorder.cpp`) — `O(1)` record,
    `O(buckets)` query, no full sort, fixed memory. Production ke liye yeh.
  - **t-digest / DDSketch** — streaming, mergeable, bounded relative error.

### Interpolation variants
"p99" ka exact value definition pe depend karta:
- **Nearest-rank** (upar) — hamesha ek actual sample. Simple, thoda
  conservative (rounds up).
- **Linear interpolation** (NumPy default, `R-7`) — do samples ke beech
  interpolate. Thoda "smoother", par ek made-up value.
- Bade N pe farak trivial. Chhote N pe (say 100 samples ka p99) definitions
  10–20% alag de sakte. **Apni definition batao** jab report karo.

---

## "Nines" ka language

Log yeh bolte hain:

| Phrase | Percentile | Frequency |
|---|---|---|
| "two nines" | p99 | 1 in 100 |
| "three nines" | p99.9 | 1 in 1000 |
| "four nines" | p99.99 | 1 in 10000 |
| "five nines" | p99.999 | 1 in 100000 |

Har nine ~10x rarer event hai, aur usko measure karne ko ~10x zyada samples
chahiye (lesson 04). "p99.999 latency" claim karne ko **millions** of
samples chahiye — warna woh 3–4 samples ka noise hai.

---

## Tail kyon itna maayne rakhti — fan-out amplification

Ek user request aksar **kai backend calls** mein fan out hoti. Agar ek
request ko complete hone ke liye **100 services** se jawab chahiye, aur
har service ka **p99 = 10 ms** (yani 1% calls slow), to:

```
P(saare 100 fast)     = 0.99^100 ≈ 0.366
P(kam se kam ek slow)  = 1 - 0.366 ≈ 0.634
```

**63% user requests ek slow backend call hit karengi** → user-facing p50
ab backend ke **p99** se driven hai! Yeh Google ka famous
["The Tail at Scale"](https://research.google/pubs/pub40801/) result hai.

**Sabak:** fan-out systems mein har component ka **tail (p99, p99.9)**
system ka **median** ban jaata. Isiliye tail latency pe itna focus.

### HFT version
HFT mein fan-out nahi, par **rate**: agar market data ka ek burst 100k
messages/sec hai aur tumhara parser ka **p99.9 = 50 µs** (baaki 5 µs), to
har second **100 messages** 10x slow — aur ek slow message ka matlab tum
uss tick pe order miss kar gaye, jab market actually move ho raha tha.
**Tail = missed opportunities aur adverse fills.** p99.9/p99.99 hi asli KPI.

---

## Coordinated omission — ek subtle bug

Load-testing / latency-measuring tools mein ek classic mistake (Gil Tene ne
popularize kiya):

**Setup:** tum har 1 ms pe ek request bhejne wale ho, aur uski latency
naapte ho. Ek request **100 ms** ke liye stuck ho jaati (GC pause / lock).

**Bug:** us 100 ms ke dauraan tumne agli **99 requests bheji hi nahi** (kyunki
tum pehli ka jawab wait kar rahe the). Woh 99 requests — jo agar bheji jaati
to sab slow record hoti — **tumhare data mein hain hi nahi**. Sirf **ek**
100 ms sample record hua, 100 ke bajaye.

**Result:** tumhari p99 latency **artificially achhi** dikhti. Real system
mein woh 99 requests aati rahengi aur sab pile up → real p99 bahut bura.

### Fix
- **Constant rate load** — jawab ka wait kiye bina schedule pe bhejte raho
  (async / separate sender thread).
- **Correction:** agar ek response `T` late hai aur expected interval `I`
  hai, to us ek sample ke saath `T-I, T-2I, T-3I, ...` synthetic samples bhi
  record karo (jo requests "hoti"). **HdrHistogram ka
  `recordValueWithExpectedInterval(value, I)`** yahi karta.
- Tools jo isse handle karte: `wrk2`, `HdrHistogram`, modern `fortio`.
  Purane `wrk`, naive loops → coordinated omission se peedit.

`08_latency_recorder.cpp` ka closing note isi ko point karta.

---

## Percentiles ko average mat karo

```
❌ p99_overall = (p99_serverA + p99_serverB + p99_serverC) / 3
```

**Percentiles additive/averageable nahi hain.** 3 servers ka combined p99
nikaalne ke liye:
- Sabhi servers ke **raw samples** (ya histograms) **merge** karo, phir
  merged data ka p99.
- Isiliye histograms **mergeable** hone chahiye (HdrHistogram, t-digest add
  ho sakte). Yeh production aggregation ka core requirement hai (lesson 16).

Similarly: "average p99 over the last hour" bhi galat — har minute ke
histograms merge karke hourly p99, ya har minute ka p99 alag plot karo
(time series), par unka mean mat lo.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — percentiles ka mean/sum lena
Non-linear. Raw data ya histograms merge karo, phir percentile.

### Trap 2 — coordinated omission
Fixed-rate load mein response ka wait karke agli request na bhejna → tail
under-count. Constant-rate sender + interval correction (HdrHistogram).

### Trap 3 — p99.9 claim, 2000 samples
2000 mein "0.1% se bura" = 2 samples. Noise. Us nine ke liye 10k–100k+.

### Trap 4 — max ko p100 samajh ke SLA banana
Max ek single sample hai — infinitely noisy (agli run pe 2x ho sakta). SLA
p99.9 ya p99.99 pe rakho; max ko "worst observed, informational" ke roop mein.

### Trap 5 — interpolation definition na batana
Chhote N pe nearest-rank vs linear-interp 10–20% alag. Report mein likho
konsi.

### Trap 6 — sirf p50 aur p99
p99 aur p99.9 ke beech bada gap ho sakta (bimodal tail). p99.9 aur max bhi
dekho — asli worst-case exposure wahan hai.

---

## Hands-on

```bash
./build.ps1 fast 35-PROFILING-BENCHMARKING/examples/02_percentiles.cpp
# p50 691, p90 922, p99 992, p99.9 1653, p99.99 4849, max 43843
# har nine pe jump; bulk (p50/p90) stable run-to-run, tail (p99.99/max) nahi

./build.ps1 fast 35-PROFILING-BENCHMARKING/examples/08_latency_recorder.cpp
# histogram se percentiles: p50/p90/p99/p99.9/p99.99 sab exact ke ~1% andar
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "p99 = 99% ka average" | p99 = woh value jiske neeche 99% data |
| "p99 across servers = avg of p99s" | merge raw/histograms, phir p99 |
| "max = meri SLA" | max noisy single sample; SLA pe p99.9/p99.99 |
| "p99.99 mera number" (10k samples) | us nine ko 100k–1M samples chahiye |
| "fixed-rate load test, wait for reply" | coordinated omission → tail hidden |
| "tail rare hai, ignore" | fan-out mein tail → median; HFT mein tail → losses |

---

## Exercises

1. Ek service ka p99 = 20 ms. Ek user request 50 aise independent services
   ko fan out karti aur sab ke jawab ka wait karti. User-facing request ke
   liye "kam se kam ek slow (>20 ms) call" milne ka probability?

   <details><summary>Answer</summary>

   Har call fast (≤ p99) hone ka P = 0.99. 50 independent calls sab fast:
   `0.99^50 ≈ 0.605`. Kam se kam ek slow: `1 - 0.605 ≈ 0.395` → **~40%**
   user requests ek 20 ms+ call se guzrengi. Yani service ka p99 user ki
   **p60** ke aaspaas dikhega. 100 calls pe yeh ~63%, 200 calls pe ~87%.
   Isliye fan-out systems mein har backend ka **p99/p99.9 shave karna** hi
   user latency improve karta — median optimize karna kaafi nahi.
   </details>

2. Tumhara load tester 10000 req/s (har 100 µs ek) bhejne ke liye set hai,
   ek connection pe, response milne pe hi agla bhejta. Server normally 50 µs
   leta. Ek 20 ms GC pause aata hai. Us pause ki wajah se kitne samples
   *record* honge (approx), aur kitne *hone chahiye the*? p99 pe kya asar?

   <details><summary>Answer</summary>

   Pause ke dauraan (20 ms) tester **kuch nahi bhej paaya** kyunki woh atki hui
   request ka wait kar raha. Woh 20 ms / 100 µs = **200 requests** jo bheji
   jaani thi, bheji hi nahi gayin. Recorded: **1 sample** (~20 ms). Hona
   chahiye tha: ~200 samples, values ~20 ms, 19.9 ms, 19.8 ms, ... down to
   ~50 µs (agar woh queue mein wait karke serve hoti). Effect: p99 mein 1
   bura sample vs 200 bure samples ka farak — recorded p99 ~50 µs dikhega
   (pause invisible), real p99 (constant-rate load ke saath) **milliseconds**
   mein hoti. Fix: async constant-rate sender + HdrHistogram
   `recordValueWithExpectedInterval(v, 100µs)` → woh 200 synthetic samples
   bharta hai.
   </details>

3. Tumhare paas 3 minute ke 3 alag histograms hain (ek per minute):
   minute-1 p99 = 1 ms, minute-2 p99 = 8 ms, minute-3 p99 = 1 ms. "3-minute
   p99" kya hai? Nikaalne ka sahi tarika?

   <details><summary>Answer</summary>

   "**(1 + 8 + 1) / 3 = 3.33 ms**" **galat** — percentiles average nahi
   hote. Sahi: teenon minute ke **histograms merge karo** (bucket counts add
   karo — isiliye histograms mergeable design karte hain), phir merged
   histogram ka p99 nikaalo. Merged p99 kya hoga depend karta har minute ne
   kitne requests serve kiye: agar teenon ne barabar (say 1M each), aur
   minute-2 ka poora distribution shifted tha, to merged p99 ~2–8 ms ke beech
   (minute-2 ke bure samples ab total ke 1/3 hmain, to woh p99 tak pahunch
   sakte). Agar minute-2 mein bahut kam traffic tha, uska asar merged p99 pe
   kam. Point: **counts matter**, aur sirf merge se sahi answer aata.
   Time-series ke liye teenon p99 alag-alag plot karo (spike minute-2 pe
   dikhega) — "3-minute p99" ek single number ki tarah kam useful.
   </details>

---

## Interview questions

1. p99 / p99.9 / p99.99 ka exact matlab; "nines" terminology.
2. Percentile compute karne ke tareeke: sort, `nth_element`, histogram,
   t-digest — trade-offs.
3. "The tail at scale" — fan-out amplification, ek backend ka p99 user ki
   median kaise ban jaata.
4. Coordinated omission — kya hai, kaise tail ko chhupata, kaise fix.
5. Percentiles kyun average nahi kar sakte; multi-source aggregation ka sahi tareeka.
6. p99.9 statistically meaningful hone ke liye kitne samples; max ko SLA kyun na banao.

---

## Next
→ [`06-jitter-and-tail-latency.md`](06-jitter-and-tail-latency.md)
