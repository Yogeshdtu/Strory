# 04 — SPSC queue between stages: build + benchmark

## Prerequisites
- `03-shared-nothing-design.md`
- `28-LOCK-FREE/02-spsc-optimized.md` (padding + cached-index design ka
  full derivation — is folder isko REUSE karta, reinvent nahi)

## Yeh topic abhi kyun

01-03 ne architecture-level principles diye (pipeline, single-writer,
shared-nothing). Yeh lesson **actual queue** hai jo do pipeline stages
ke beech baithta — `spsc_queue.hpp`.

---

## `SpscQueue<T, Capacity>` — 28's design, packaged

28-LOCK-FREE ne teen versions banaye the (naive -> padded -> cached-
index), measure karke dikhaya cached-index hi asli jeet hai
(~1.3-1.6x over naive, padding akele se noise-level fayda). Yeh folder
us FINAL (V2/cached-index) design ko ek reusable template class mein
package karta, taaki 07 aur 08 ise SEEDHA use kar sakein, copy-paste
nahi:

```cpp
template <class T, std::size_t Capacity>
class SpscQueue {
    // ... padded head_/tail_, cached_tail_/cached_head_ ...
    bool try_push(const T& v);   // producer-only
    bool try_pop(T& out);        // consumer-only
};
```

`Capacity` **compile-time, power-of-two** honi CHAHIYE — `%` (modulo)
ki jagah `& kMask` use hota (bitwise AND), jo `%` se kaafi sasta hai
(03-VARIABLES-DATA-TYPES/05, 05-OPERATORS mein power-of-two-modulo-via-
AND trick already dekha).

---

## `01_spsc_hft.cpp` — correctness pehle

2 million messages, `SpscQueue<Tick, 1024>` se guzarte — order aur
content DONO exactly preserved (`mismatches=0`). Backpressure bhi dikha
(`try_push()` false deta jab full, caller `while (!q.try_push(...))`
se spin karta) — **isliye Capacity choose karna ek real design decision
hai**: bahut chhoti to producer baar-baar backpressure hit karega
(measured yahan: 1024-capacity ring pe 2M messages mein **~1.5 million
baar** full-queue backpressure hit hua — matlab producer consumer se
FASTER hai, jo pipeline design mein normal hai, consumer ko catch-up
karne ka time chahiye).

---

## `02_spsc_benchmark.cpp` — rigorous latency + throughput

rdtsc-based measurement (35/36/38/39/40's SAME harness) — **throughput
pass** (blast, no pacing) aur **latency pass** (paced, queue shallow
rehti, PURE hand-off cost measure hota) alag rakhe gaye — 39's
benchmark-design lesson yahan bhi laagu: agar dono ek saath measure
karte, latency numbers throughput-pass ke SATURATED-queue delays se
contaminate ho jaate.

**Measured (this run, N=20M throughput / 500K paced latency):**

```
Throughput: 16.1 M msg/s
Hand-off latency: p50 400.8   p99 171856.7   p99.9 1880224.6   max 2866479.7  ns
```

p50 400.8ns EXPECTED range mein hai (28/06 ne SAME design ke liye
~0.4us measure kiya tha). **p99/p99.9 ka HUGE tail** yeh box **unpinned
desktop** hai — OS scheduler kabhi-kabhi consumer thread ko preempt kar
deta, aur woh 100+ microseconds ke liye wapas nahi aata. Yeh 28/06's
EXACT observation hai ("Windows desktop, no core pinning, teenon mein
100us+ spikes"), yahan phir se confirm hua. **08-core-pinning-strategy.md**
isi jitter ko address karta.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — Capacity power-of-two na rakhna
`static_assert` isse compile-time hi pakad leta — `& kMask` trick sirf
power-of-two capacity ke saath sahi wraparound deta.

### Trap 2 — dono strategies (blast + paced) ek hi pass mein measure karna
Blast pass ke throughput-optimal state mein queue LAGATAAR bharti rehti
(producer consumer se fast) — is state mein "latency" measure karna
GALAT signal deta (queue-depth-delay, hand-off-cost nahi). Alag passes
zaroori.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| SPSC queue "kabhi full nahi hoti" agar consumer fast hai | Producer bursts mein aksar consumer se fast hota — backpressure NORMAL hai, bug nahi |
| p99 tail queue design ki galti hai | Yahan (unpinned desktop) OS scheduler jitter hai, queue mechanism ki nahi (04 vs 08 se compare) |
| Throughput aur latency EK number se pata chal jaate | Alag measurement PASSES chahiye (39/40 ka established pattern) |

---

## Hands-on

```bash
./build.ps1 fast 41-HFT-CONCURRENCY/examples/01_spsc_hft.cpp
./build.ps1 fast 41-HFT-CONCURRENCY/examples/02_spsc_benchmark.cpp
```

---

## Exercises

1. Ek producer consumer se 2x FASTER hai (sustained). `Capacity=1024` ki
   ring ke saath kya hoga long-term?
   <details><summary>Answer</summary>
   Ring jaldi FULL ho jaayegi aur producer zyaadatar time `try_push()`
   false pa ke SPIN karega (backpressure) -- throughput ULTIMATELY
   consumer ki speed se BOUND hoga (queue sirf ek chhota buffer hai, sustained
   rate-mismatch ko capacity nahi FIX kar sakti). Bada capacity sirf
   BURSTS absorb karta, sustained mismatch ka solution nahi.
   </details>

---

## Interview questions

1. `SpscQueue`'s cached-index optimization kaise kaam karta (28 se recall)?
2. Backpressure kya hai, aur ek producer ko kya karna chahiye jab
   `try_push()` false de?
3. Throughput-pass aur latency-pass ko alag kyun rakha jaata benchmarking mein?

---

## Next
→ [`05-lmax-disruptor.md`](05-lmax-disruptor.md)
