# 13 — Event ordering across threads, timestamps

## Prerequisites
- `12-numa-thread-placement.md`
- `40-MATCHING-ENGINE/09-determinism.md`, `10-event-sourcing.md`

## Yeh topic abhi kyun

40 mein "determinism" ek SINGLE-THREADED engine ke andar tha — ek shared
`seq_` counter, ek thread, koi ambiguity. Ab jab pipeline **MULTIPLE
threads** mein phaila hai (01), ek naya sawaal aata: **"event X, event
Y se PEHLE hua ya BAAD mein" yeh kaise pata chalega jab X aur Y ALAG
threads pe generate hue?**

---

## Timestamp-at-source, propagate through

`08_pipeline_demo.cpp`'s approach: **Stage 1** (jahan order arrive
hota) EK BAAR `tsc()` leta, us timestamp ko poore pipeline ke through
**carry** karta (`PipelineOrder::t_tsc`, phir `PipelineTrade::t_tsc`
mein forward hota):

```cpp
struct PipelineOrder { std::uint64_t t_tsc; Order order; };   // Stage1 pe stamp
struct PipelineTrade { std::uint64_t t_tsc; Trade trade; };   // SAME stamp forward
```

Stage 3 `tsc() - pt.t_tsc` compute karta — yeh **end-to-end latency**
hai, "order Stage 1 mein kab aaya" se "trade Stage 3 mein kab pahuncha"
tak. Yeh single-machine, single-TSC-domain approach hai — **saare
threads SAME physical CPU ki `rdtsc` clock use karte** (invariant TSC,
35's foundational assumption), isliye timestamps DIRECTLY comparable
hain, chahe alag cores pe liye gaye hon.

---

## Multi-machine hota to kya badalta

Agar pipeline stages ALAG MACHINES pe hote (network ke through connected
— 30-NETWORKING, 42-HFT-NETWORKING), `rdtsc()` KAAM NAHI karta (har
machine ki apni TSC, ALAG epoch, drift possible) — tab **hardware
timestamping** (30/14, PTP-synced clocks) chahiye hota — yeh already
30-NETWORKING mein cover ho chuka hai. **Single-machine pipeline mein
`rdtsc()` kaafi hai; multi-machine mein PTP-sync zaroori hai.**

---

## Ek real measurement finding — pacing aur tail latency

`08_pipeline_demo.cpp` banate waqt ek experiment kiya gaya: feed-pace
(Stage 1 kitni fast orders bhejta) BADALNE se tail latency (p99+) pe
kya asar padta.

| Pace | p50 | p99 | p99.9 | Total runtime |
|---|---|---|---|---|
| ~1us/order | 2334.4 ns | 30,022,977 ns | 34,425,406 ns | ~0.5s |
| ~10us/order | 1011.9 ns | 43,853.4 ns | 1,646,649.3 ns | ~5s |
| ~20us/order | 950.3 ns | 750,436.5 ns | 3,593,200.4 ns | ~10s |

**Counter-intuitive result:** SLOWER pacing (jo backlog/queueing-delay
KAM karna chahiye tha) ne p99 ko actually WORSE bana diya (750K ns @
20us pace vs 43.8K ns @ 10us pace)! Agar yeh **backlog** ki wajah se
hota, SLOWER pace se p99 **BEHTAR** hona chahiye tha (queue kabhi bharti
hi nahi), ULTA hua.

**Asli wajah:** SLOWER pace ka matlab test ZYAADA DER chalta (0.5s se
20s tak) — **zyaada wall-clock exposure = OS scheduler jitter ke zyaada
CHANCES** (04/05/06 ka SAME finding, is unpinned desktop pe). `p50`
teeno cases mein STABLE raha (~950-2334ns) — yeh dikhata hai ki pipeline
ka ACTUAL per-hop cost consistent hai; sirf TAIL test-duration ke saath
badhta hai kyunki lambi duration mein rare scheduler-hiccups zyaada baar
hone ka mauka milta hai.

> **Yeh CLAUDE.md ka poora Rule 2 hai:** naive intuition ("slower feed =
> kam backlog = behtar tail") galat nikla — asli mechanism (test-duration
> vs scheduler-jitter-exposure) alag tha. Number chhupaya nahi gaya,
> teach kiya gaya.

**Final chosen pace (~10us/order):** best balance — kaafi FAST test
(5s) rakhte hue Stage 2 (matching engine, jo is order-mix mein bahut
crossing-heavy hai) ko OVERLOAD na kare.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — cross-machine events ko `rdtsc()` se compare karna
Har machine ki apni TSC hai (alag epoch, potential drift) — 2 alag
machines ke `rdtsc()` values ko SEEDHA subtract karna MEANINGLESS hai.
PTP-synced wall-clock (30/14) chahiye.

### Trap 2 — p99 dekh ke "system degrade ho gaya" jump-to-conclusion karna
Jaisa upar dikha — HAMESHA pehle root-cause karo (backlog? OS jitter?
GC pause? — yahan koi GC nahi, par concept generalize hota) SIRF number
dekh ke assume mat karo.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Slower feed rate hamesha tail latency behtar karta | Test-duration bhi badhti — longer exposure = zyaada scheduler-jitter chance |
| `rdtsc()` kisi bhi do timestamps compare karne ke liye theek hai | Sirf SAME machine ke andar (invariant TSC) — cross-machine PTP chahiye |
| p50 aur p99 hamesha SAME direction mein move karte | Yahan p50 STABLE raha, p99 pace ke saath BADHA -- alag mechanisms |

---

## Hands-on

```bash
./build.ps1 fast 41-HFT-CONCURRENCY/examples/08_pipeline_demo.cpp
```

---

## Exercises

1. Agar tum is pipeline ko 2 ALAG machines pe split karo (Stage 1 machine
   A pe, Stage 2/3 machine B pe), `t_tsc` field ka kya hoga?
   <details><summary>Answer</summary>
   Ab kaam nahi karega jaisa hai -- Machine A ki `rdtsc()` aur Machine B
   ki `rdtsc()` ALAG, unrelated clocks hain. Timestamp ko PTP-synced
   wall-clock (`CLOCK_REALTIME` ya hardware NIC timestamp, 30/14) se
   lena padega, aur network transit time bhi latency budget mein add
   hoga.
   </details>

---

## Interview questions

1. Timestamp-at-source, propagate-through pattern explain karo.
2. Single-machine vs multi-machine timing mein kya fark aata?
3. Pacing-experiment ka counter-intuitive result aur uska root-cause
   batao.

---

## Next
→ [`14-exercises.md`](14-exercises.md)
