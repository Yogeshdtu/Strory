# 07 — Histograms aur latency distributions

## Prerequisites
- `04-statistics.md`, `05-percentiles.md`, `06-jitter-and-tail-latency.md`
- `examples/02_percentiles.cpp`, `examples/08_latency_recorder.cpp`

## Yeh topic abhi kyun
Percentiles (lesson 05) ek **number** dete. Histogram poori **shakl** deta —
unimodal? bimodal? kahan spike? Aur production mein latency track karne ka
practical data structure histogram hi hai: `O(1)` record, fixed memory, no
sort, **mergeable**. Yeh lesson: kaunse buckets, kaise plot karo, aur
HdrHistogram kyun standard hai.

---

## Histogram = value → count

Value range ko **buckets** mein baanto, har sample ka bucket `++`. Baad mein:
- **percentile**: buckets ko lo-to-hi walk karo, cumulative count target tak.
- **plot**: bucket vs count.
- **merge**: do histograms ke corresponding bucket counts add karo (agar
  same bucket layout).

```cpp
// naive fixed-width
int idx = (value - min_val) / bucket_width;
++counts[clamp(idx, 0, NBUCKETS - 1)];
```

**Problem: bucket_width kya rakhein?**

---

## Linear buckets latency ke liye kyun fail

Latency **100 ns se 100 ms** tak faili hoti — **6 orders of magnitude**.

- **width = 100 ns** → 100 ms cover karne ko **10⁶ buckets** (8 MB@8B). Aur
  fast region (100–500 ns) mein sab kuch 4 buckets mein — resolution wahan
  chahiye thi jahan nahi mili.
- **width = 100 µs** → 10 ms cover karne ko 100 buckets, par ab **p50, p90,
  p99 sab bucket 0 mein** (agar sab < 100 µs) — tail dikhta, bulk nahi.

`02_percentiles.cpp` ka linear histogram exactly yeh dikhata: 40 buckets
`min..p99.5` pe, aur bulk itna tight hai ki pehle ~20 buckets bhare, baaki
khaali, aur `>p99.5` overflow alag se count hota. **Ek fixed linear width
kabhi dono (bulk resolution + tail range) nahi de sakti.**

---

## Log-spaced (exponential) buckets

Har bucket ek **fixed ratio** se bada: `[1,2), [2,4), [4,8), [8,16), ...`
Ya `×1.1` per bucket for finer. Ab:
- **Relative error bounded** — bucket `[100, 110)` mein koi bhi value ≤10%
  off. `[10000, 11000)` bhi ≤10% off. **Har scale pe same % accuracy.**
- **6 decades ~200 buckets** mein (`log2` → ~20 octaves × ~10 sub each).
- p50 aur p99.99 dono ko meaningful resolution.

**Trade-off:** absolute error bade values pe bada (`[10000,11000)` ka error
1000 ns), par latency mein hum **relative** error care karte ("p99 within
1%").

### Log-linear (HdrHistogram ka scheme)

Pure log ke bhi issue: chhoti values pe (0–63) log2 jump too coarse.
**HdrHistogram** hybrid karta:
- Ek **"significant digits"** parameter (1–5) — kitni precision chahiye.
  `sigfigs = 3` → 3 significant digits, yani ~0.1% relative error.
- Layout: har **octave** (`[2^k, 2^(k+1))`) ko `2^sigbits` **linear**
  sub-buckets mein baanto. Yani chhote scale pe fine linear, bade scale pe
  woh linear steps proportionally bade — net effect log-linear.

`08_latency_recorder.cpp` ka `LatencyHistogram` yahi (chhota version):
`SUB_BITS = 6` → 64 sub-buckets/octave → **~1.5% relative error**, 58
octaves → full `uint64_t` range, **3776 buckets = 29.5 KB fixed**.

```cpp
size_t index_of(uint64_t v) {
    if (v < SUB) return v;                        // linear bottom
    int msb = 63 - __builtin_clzll(v);            // floor(log2 v)
    int octave = msb - SUB_BITS;
    uint64_t sub = (v >> (msb - SUB_BITS)) & (SUB - 1);
    return SUB + octave * SUB + sub;
}
```

**Measured accuracy (`08`, vs exact sorted data):**
```
              hist       exact      rel.err
  p50         252 ns     251 ns     +0.40%
  p90         400 ns     396 ns     +1.01%
  p99         624 ns     616 ns     +1.30%
  p99.9    26112 ns   26111 ns     +0.00%
  p99.99   46080 ns   46021 ns     +0.13%
```
Sab ≤1.3% off, **29.5 KB** mein, **~2 ns/record** (vs `vector.push_back` +15
MB + `O(n log n)` sort).

---

## Plotting: CDF > PDF for latency

**PDF** (probability density — normal histogram bars) latency ke liye kam
useful — bulk peak sab dikha deta, tail invisible (`02` ka linear plot).

**CDF** (cumulative — "% of samples ≤ x") better:
- x-axis **log scale** (latency spans decades).
- y-axis 0–100%, ya better **"nines"**: `0, 90, 99, 99.9, 99.99` unequally
  spaced so the tail stretches out. Yeh **HdrHistogram plotter** ka default
  view — x = latency (log), y = percentile (log of `1/(1-p)`).
- Tail ek **flat line** ban jaata agar bounded, ya **upar shoot** karta
  agar unbounded — turant dikhta.

```
  percentile
  99.99 |                                  .-- yahan cliff = bimodal tail
  99.9  |                            .-----'
  99    |                  .--------'
  90    |         .-------'
  50    |  ------'
        +----+----+----+----+----+----  latency (log)
        100n 300n  1u   3u  10u  30u
```

`08_latency_recorder.cpp` ek **log-spaced 32-bin PDF** print karta — usme
bhi **bimodal** clearly dikhta: bulk hump **~276 ns**, alag spike hump
**~10–22 µs**. Linear x-axis pe woh doosra hump bilkul invisible hota.

---

## HdrHistogram — kyun standard

[HdrHistogram](http://hdrhistogram.org/) (Gil Tene) — "High Dynamic Range"
histogram. C, C++, Java, Rust, Go, Python ports.

Features jo isse production-grade banate:
1. **Configurable precision** (sigfigs 1–5) + **max trackable value** →
   fixed memory, computed upfront.
2. **`O(1)` lock-free record** (single writer) — hot path safe.
3. **Mergeable** — `histA.add(histB)` → combined percentiles (multi-thread /
   multi-host aggregation, lesson 16).
4. **Coordinated-omission correction** —
   `recordValueWithExpectedInterval(value, expectedInterval)`: agar
   `value > expectedInterval`, woh synthetic samples bhi bhar deta jo
   "hoti" (lesson 05). Load testers (`wrk2`) isi pe.
5. **`.hgrm` file format** + plotter — logs se percentile-over-time charts.
6. **Recorder pattern** — writer thread record karta, reader thread ek
   consistent snapshot leta bina writer ko rok ke (double-buffered).

Jab apna chhota version (`08` jaisa) kaafi:
- Single-threaded ya per-thread + simple merge.
- Fixed known range, ~1–2% relative error acceptable.
- No coordinated-omission concern (tum sender control karte).

Jab real HdrHistogram lo:
- Multi-writer aggregation, mergeable across hosts.
- Load testing (coordinated omission correction).
- Standard tooling / dashboards / `.hgrm` interchange chahiye.

---

## Sampling vs full recording

- **Full**: har event record. Accurate, par high-rate pe (10M events/s)
  even `~2 ns/record` = 2% overhead + the histogram is always exact.
- **Sampled**: har Nth event, ya reservoir sampling (uniform random subset),
  ya time-based (har 1 ms ek). Overhead ~0, par tail (p99.99) estimate
  weaker — rare events sample mein kam aate.
- **Histogram is already a lossy summary** — usually full-record into a
  histogram (cheap, bounded memory) beats sampling raw values. Sample tabhi
  jab record cost khud problem ho ya tumhe raw traces chahiye.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — linear buckets for latency
Ek width se bulk resolution + tail range dono nahi milte. Log / log-linear
(HdrHistogram).

### Trap 2 — PDF plot, linear x-axis
Bulk peak sab kha jaata, tail invisible. Log x-axis + CDF / percentile plot.

### Trap 3 — non-mergeable histogram
Har source alag bucket layout → merge impossible → multi-host p99 nahi nikaal
sakte. Fixed shared layout (HdrHistogram guarantees).

### Trap 4 — coordinated omission ignore (load testing)
`wrk` (v1) / naive loop → tail under-report. `wrk2` / HdrHistogram
`recordValueWithExpectedInterval`.

### Trap 5 — bucket ki upper vs lower edge
Percentile query pe bucket ka **upper edge** report karna conservative
(over-estimate). Lower edge → under-estimate. Consistent raho, aur error
bound (±bucket width) mention karo.

### Trap 6 — histogram ka `min`/`max` bhoolna
Log buckets `min` aur `max` ko round karte. Alag se exact `min`/`max` track
karo (ek comparison per record) taaki true extremes bhi pata hon (`08`
karta).

---

## Hands-on

```bash
./build.ps1 fast 35-PROFILING-BENCHMARKING/examples/08_latency_recorder.cpp
```
Dekho: 3776 buckets / 29.5 KB; `record()` ~2 ns; percentiles exact ke ≤1.3%
andar; **log-spaced plot bimodal** (bulk ~276 ns + spike hump ~10–22 µs).

```bash
./build.ps1 fast 35-PROFILING-BENCHMARKING/examples/02_percentiles.cpp
```
Iska **linear** histogram — bulk tight, `>p99.5` overflow alag; ek linear
width se poora tail capture nahi hota (yahi log buckets ki zaroorat).

```bash
# real HdrHistogram (Linux):
git clone https://github.com/HdrHistogram/HdrHistogram_c
# ya C++ header-only: HdrHistogram/HdrHistogram_cpp
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "linear buckets fine" | latency spans decades → log / log-linear |
| "histogram bars (PDF) best plot" | CDF / percentile plot, log x-axis |
| "har thread apna histogram, alag layout" | shared layout → mergeable |
| "raw samples + sort at the end" | histogram: O(1) record, fixed mem, no sort |
| "HdrHistogram overkill" | multi-host / load-test / dashboards → worth it |
| "bucket count = exact value" | ±bucket width; track exact min/max separately |

---

## Exercises

1. Tumhe 100 ns – 10 sec latency track karni hai, **1% relative error** ke
   saath. Log-linear scheme (`08` jaisa) mein `SUB_BITS` kya rakhoge, aur
   approx kitne buckets / bytes?

   <details><summary>Answer</summary>

   Relative error ≤ `1 / SUB` = `1 / 2^SUB_BITS`. 1% ke liye `2^SUB_BITS ≥
   100` → `SUB_BITS = 7` (`128` sub-buckets, ~0.78% error). Range: 100 ns to
   10 s = 10^10 ns ≈ `2^33.2`. Octaves `SUB_BITS..34` ≈ 27 octaves. Buckets
   ≈ `SUB + octaves × SUB = 128 + 27×128 ≈ 3584` buckets × 8 bytes ≈ **28
   KB**. (`08` ne `SUB_BITS=6` = 64 sub, ~1.5% error, 3776 buckets, 29.5 KB
   for the full u64 range.) 1% chahiye to `SUB_BITS=7`; agar 0.1% (HdrHistogram
   sigfigs=3 jaisa) chahiye → `SUB_BITS=10` (1024 sub) → ~10x buckets → ~280
   KB.
   </details>

2. Ek CDF plot pe latency curve p99 tak flat (~2 µs) hai phir p99 aur
   p99.99 ke beech **90 degree upar** shoot karta (p99.99 = 5 ms). Kya
   diagnose karta?

   <details><summary>Answer</summary>

   Bulk (99% traffic) bahut consistent (~2 µs) — fast path healthy. Par
   **top 1% ek totally alag regime** mein — 2 µs se 5 ms, **2500x**. Yeh
   **bimodal** hai: ek rare slow path jo occasionally hit hota. Candidates:
   (a) GC / allocator pause, (b) lock contention jab ek specific hot lock
   miss hota, (c) cache/TLB cold path (ek rare code branch jiska working set
   L3 se bahar), (d) a slow downstream dependency called on 1% of requests,
   (e) coordinated-omission-style pile-up during a hiccup. Curve ka **shape**
   (flat then vertical) = "99% ek distribution, 1% doosri". Fix: us 1% ke
   **trigger** ko identify karo (kaunsa input / condition slow path leta) —
   tag those requests, trace them (lesson 12/16). p99.99 ko bulk ke kareeb
   laana = us slow path ko eliminate ya bound karna.
   </details>

3. Do machines ke HdrHistograms merge karne pe combined p99 kisi bhi
   individual p99 se **zyada** aa gaya. Bug hai ya expected?

   <details><summary>Answer</summary>

   **Expected, bug nahi.** Percentiles non-linear hain (lesson 05). Maano
   machine A ke 1M samples ka p99 = 3 ms (yani ~10000 samples > 3 ms), aur
   machine B ke 1M ka p99 = 3 ms bhi. Merge = 2M samples. Ab "top 1%" = 20000
   samples. A ke 10000 + B ke 10000 = 20000 samples > 3 ms — matlab merged
   p99 ~3 ms ke aaspaas... par agar A ki tail B se **worse** thi (A ka p99.5
   = 8 ms, B ka p99.5 = 3.2 ms), to merge mein A ke woh 5000 samples (3–8 ms)
   ab combined top-1% ka bada hissa → merged p99 **A ke p99 se upar** jaa
   sakta, 4–5 ms. Yeh sahi hai: merged distribution ki tail dono ki tails ka
   union hai. **Isliye** aggregation raw-merge se hi sahi — p99s ko average
   ya max karna galat answer deta (yeh 3 ms ya 3 ms deta, jabki sach 4–5 ms).
   </details>

---

## Interview questions

1. Linear vs log vs log-linear buckets — latency ke liye kaunsa aur kyun.
2. HdrHistogram ke 4 defining features (fixed mem, O(1), mergeable, CO-correction).
3. CDF / percentile plot vs PDF bars — latency visualize karne ko kaunsa.
4. Log-linear scheme mein relative error `SUB_BITS` se kaise relate karta.
5. Coordinated omission correction — histogram level pe kaise kaam karta.
6. Full recording into a histogram vs sampling raw values — kab kaunsa.

---

## Next
→ [`08-microbenchmarking.md`](08-microbenchmarking.md)
