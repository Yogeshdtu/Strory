# 01 — Latency, throughput, jitter: teenon ka fark

## Prerequisites
- Poora folder `35-PROFILING-BENCHMARKING` (measurement, percentiles, jitter)
- `31-CPU-ARCHITECTURE` (latency vs throughput at the instruction level)

## Yeh topic abhi kyun
Folder 35 mein tumne yeh teen cheezein **measure** karna seekhi. Ab folder 36
ka poora kaam hai inhe **engineer** karna. Har technique aage aane wali —
pool, arena, branchless, ring buffer — in teenon mein se kisi ek ko behtar
karti hai, aksar doosre ki keemat pe. To pehle yeh bilkul saaf hona chahiye:
**tum kya optimize kar rahe ho?**

---

## Teen alag cheezein

| | **Latency** | **Throughput** | **Jitter** |
|---|---|---|---|
| Sawaal | "ek operation mein kitna time?" | "per second kitne operations?" | "same operation, run-to-run kitna vary?" |
| Unit | ns/op (ek number, ya distribution) | ops/sec, GB/s, msgs/sec | max−min, p99.9−p50, spike count |
| Ek metric | p50 / p99 / p99.9 / max | total_work / total_time | stddev (bura), IQR, spike rate |
| Batching ka asar | **badhaati** (queue wait) | **badhaati** (amortize) | usually badhaati |
| Kiske liye | HFT, trading, RPC, games, RT audio | analytics, encoding, batch jobs, ETL | **HFT ka #1** |

**Yeh alag axes hain.** Ek system:
- high throughput + high latency: ek batch job jo 10M rows/sec process karta
  par har row 200 ms baad emit karta (buffered).
- low latency + low throughput: ek RPC jo har call 5 µs mein jawab deta par
  sirf 50k calls/sec handle kar sakta.
- low latency + low jitter: HFT ka goal — har tick-to-trade **hamesha**
  ~800 ns, kabhi 40 µs nahi.

---

## HFT mein: latency + jitter, throughput sirf capacity

Trading system ka objective **tail latency** minimize karna hai — p99.9
tick-to-trade. Throughput tabhi maayne rakhta jab tumhe **peak market data
burst** absorb karni ho (100k+ msgs/sec) bina queue banaaye. Aur jitter?

> **Jitter = missed trades.** Ek 50 µs spike jab stock 2 ticks move kiya =
> tum late, doosron ne le liya, ya adverse fill. Average tez hone se koi
> faayda nahi agar p99.9 pe tum window miss karte ho.

Isliye is folder ki har technique ka evaluation:
1. **p50 latency** — typical hot-path time.
2. **p99.9 latency** — worst realistic case (the number that matters).
3. **jitter** — spike count, distribution shape.
4. **throughput** — sirf yeh check karne ke liye ki burst absorb hoti hai.

`average` is folder mein lagbhag kabhi report nahi hoti.

---

## Budget thinking

Tick-to-trade ka ek **latency budget** hota — say **1 µs** wire-to-wire.
Usko stages mein baanto:

```
  NIC RX + kernel bypass    120 ns
  feed decode (parse)       150 ns
  book update               180 ns
  strategy decision         200 ns
  risk check                 90 ns
  order encode              110 ns
  NIC TX                    150 ns
  ----------------------------------
  total (p50)             1000 ns   <- budget
```

Har stage ka apna **p50 aur p99.9** budget hai. Optimization ka kaam:
1. Profile → kaunsa stage budget se upar (folder 35 lesson 10–14).
2. Us stage ka **p99.9** girao (folder 35 lesson 05 — tail hi target).
3. Re-measure end-to-end. Amdahl (35/01) — chhote stage ko shave karne se
   total mushkil se hilega.

**Budget ke bina** "yeh function tez karo" ka koi maane nahi — kitna tez?
kaafi kya hai? Budget answer deta.

---

## Ek design tool: "the three questions"

Koi bhi hot-path code review karte waqt:

1. **Latency:** iss operation ka worst realistic case (p99.9) kya hai?
   Kahan se aa sakta — allocation? syscall? cache miss? lock? branch
   mispredict? (folder 35 lesson 03, 06.)
2. **Throughput:** peak burst mein yeh keep up karega? Ya queue banegi
   (aur queue = latency)?
3. **Jitter:** is operation ka time **data pe** ya **system state pe**
   depend karta? (variable-size work, cold cache, contended lock →
   jitter.) Ise **deterministic** kaise banayein?

Har "haan par kabhi-kabhi..." ek jitter source hai jise kill karna hai.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — throughput optimize karke latency todna
`std::vector` ko batch mein fill karna throughput deta, par ek item ko
"batch ready" hone tak wait karna padta → latency. (Example 10.)

### Trap 2 — p50 dekh ke khush hona
p50 800 ns, p99.9 45 µs — yeh system HFT mein fail hai. p99.9 report karo.

### Trap 3 — "average latency" ko target banana
Tail average ko upar kheenchta; average typical se dheema; aur average
kisi bhi actual run ke kareeb nahi jab distribution bimodal ho (35/04).

### Trap 4 — budget ke bina optimize
"10x faster" — kaafi hai? Budget ke bina pata nahi. Stage-wise budget
banao pehle.

### Trap 5 — jitter ko "measurement noise" samajhna
Woh real system interference hai jo production mein bhi hoga (35/06).
Measure karke kill karo, ignore nahi.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "latency aur throughput ~same baat" | ulte trade-off; kaunsa chahiye pehle tay karo |
| "average latency 800 ns" (HFT) | p50/p99/p99.9; average tail chhupata |
| "batching hamesha achhi" | throughput haan, latency nahi (35/05, example 10) |
| "jitter = noise, ignore" | missed trades; engineer it out |
| "tez code = kam jitter" | alag axis; determinism alag se banao |
| "yeh function optimize karo" | pehle latency budget, phir profile, phir stage |

---

## Exercises

1. Ek stage ka p50 = 150 ns, p99 = 165 ns, p99.9 = **8 µs**, max = 9 µs.
   Ek doosra: p50 = 220 ns, p99 = 240 ns, p99.9 = 260 ns, max = 400 ns.
   Total budget p99.9 = 1 µs. Kaunsa stage design "theek" hai HFT ke liye,
   aur pehle wale mein kya dhoondoge?

   <details><summary>Answer</summary>

   **Doosra stage theek hai** — bounded: worst realistic case 260 ns, max
   400 ns, poora p99.9 budget (1 µs) ke andar. p50 thoda zyada (220 vs 150)
   par predictable.
   **Pehla stage HFT mein fail** — p50/p99 great (150/165), par p99.9 = 8 µs
   akele poora 1 µs budget **8x** blow karta. Yeh ek **discrete ~8 µs
   event** hai jo ~0.1% baar hit hota — random noise nahi (p99.9 aur max
   dono ~8-9 µs, tight). Candidates (35/06): ek page fault (fresh memory),
   ek `malloc` jo `mmap` karta, ek periodic flush/log, ek lock jo usually
   uncontended, ek TLB shootdown, C-state exit. Fix path: `perf record -e
   ...` us 0.1% ko catch karo; allocation/syscall/lock hot path se hatao
   (baaki folder). p99.9 ko p99 ke kareeb laana = us event ka source
   eliminate karna.
   </details>

2. Tumhare paas ek market-data decoder hai jo **1 message at a time** 400
   ns leta (p50), aur burst mein 200k msgs/sec aate hain. Kya yeh keep up
   karega? Agar tum ise 32-message batches mein decode karo to per-message
   150 ns ho jaata — ab? Aur is change ka latency pe kya asar?

   <details><summary>Answer</summary>

   **1-at-a-time:** 400 ns/msg → max **2.5M msgs/sec** capacity. 200k/sec
   burst easily fits (8% utilization) — **koi queue nahi banegi**, latency
   = processing latency. Batching ki **zaroorat nahi** yahan.
   **32-batch:** 150 ns/msg → 6.6M msgs/sec capacity — throughput headroom
   badh gaya, par tumhe woh chahiye hi nahi tha. Aur ab har message ko
   **31 aur messages aane ka wait** karna padta batch bharne ke liye. Burst
   pe 200k/sec = ek msg har 5 µs → 32 msgs bharne mein ~160 µs wait +
   processing. **p99.9 latency 160 µs+ ho gayi** ek pehle 400 ns wale se.
   Disaster. Batching yahan galat — throughput already kaafi tha. Batching
   sirf tab jab tum throughput-bound ho (example 10). Best: "opportunistic
   batch" — jo msgs abhi buffer mein hain unhe saath process karo, par
   bharne ka wait kabhi mat karo.
   </details>

3. Ek function ka time **input pe depend** karta: chhota order book → 80 ns,
   deep book → 900 ns, aur book depth market conditions se badalti. p50 =
   200 ns par p99 = 850 ns. Yeh jitter hai — is jitter ko kaise kam karein
   (do approaches, trade-offs ke saath)?

   <details><summary>Answer</summary>

   Yeh **data-dependent work** jitter hai (35/06) — algorithm ka kaam hi
   variable hai, external interference nahi. Approaches:
   (1) **Bound the work** — book ke top N levels hi process karo (e.g. top
   10), baaki ignore. Ab har call ~constant time. Trade-off: agar tumhari
   strategy ko deep book chahiye to accuracy khoyi; par aksar sirf top-of-
   book maayne rakhta.
   (2) **Incremental maintenance** — poora book har tick re-process mat karo;
   sirf **delta** apply karo (jo levels badle). Ab per-tick kaam = number of
   changed levels, jo usually 1-2, chahe book kitni bhi deep ho. Trade-off:
   book data structure zyada complex (order-indexed + price-indexed),
   invariants sambhaalne padte.
   (3) **Precompute / cache** — derived quantities (mid, spread, weighted
   depth) ko incrementally update karo, har consumer ke liye re-derive mat
   karo.
   Sabme: worst case (deep book) ko **bound** karo, ya use **rare** banao
   (delta processing). p99 ko p50 ke kareeb laana = variable work ko
   constant/bounded banana.
   </details>

---

## Interview questions

1. Latency, throughput, jitter — teen alag cheezein, har ek ka metric.
2. Ek optimization jo throughput badhaati par latency bigaadti (batching).
3. HFT mein kaunsa metric primary, aur "average latency" kyun bekaar.
4. Latency budget kya hai, stage-wise budget ka faayda.
5. Data-dependent jitter vs interference jitter — dono ke examples aur fixes.

---

## Next
→ [`02-tail-latency.md`](02-tail-latency.md)
