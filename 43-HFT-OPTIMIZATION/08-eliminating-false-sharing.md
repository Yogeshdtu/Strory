# 08 — Eliminating false sharing (systematic hunt)

## Prerequisites
- `07-struct-layout-tuning.md`
- `27-ATOMICS-MEMORY-MODEL/*`, `36-LOW-LATENCY-CPP/11-false-sharing-elimination.md`
- `41-HFT-CONCURRENCY/*` (SPSC head/tail padding)

## Yeh topic abhi kyun

`07` single-threaded data layout tha. Jaise hi pipeline **multi-threaded**
hota (`41`: feed thread → SPSC → strategy thread → SPSC → gateway thread),
ek naya cache problem: do threads alag variables likh rahe jo **same cache
line** pe hain → line har baar core-to-core bounce → dono dheeme.

---

## False sharing — mechanism

Cache coherence (MESI) ki unit ek **cache line** (64 B), variable nahi.

```
Line L (64 bytes):  [ counter_A (8) ][ counter_B (8) ][ ... 48 more bytes ... ]

Core 0: counter_A++  (har microsecond)      Core 1: counter_B++  (har microsecond)
```

Core 0 `counter_A` likhta → poori line L uske L1 mein **Modified**, Core 1
ki copy **Invalid**. Core 1 ko `counter_B` chahiye → L wapas maangta
(Core 0 se, ~40-100+ cyc) → ab Core 1 ke paas Modified, Core 0 Invalid.
Ping-pong. Dono variables **logically independent** hain — sharing "false"
hai — par hardware ke liye ek hi line.

Effect: ek variable jise touch karna ~1 ns hona chahiye tha, ~50-100 ns le
raha, aur **doosre core ka throughput bhi girta**.

---

## Systematic hunt

### 1. Symptom pehchano

- Multi-threaded code jo core-count badhne pe **scale nahi** karta (ya ulta
  slow).
- `perf c2c record -- ./app && perf c2c report`:
  **HITM** (hit-modified — line dusre core se, modified state mein aayi)
  events high, ek specific cache line pe, do alag threads/functions se
  offsets.
- `perf stat`: high `mem_load_l3_hit_retired.xsnp_hitm` (Intel) ya
  equivalent.
- Linux `perf c2c` literally batata: "yeh line, ye offsets, ye code
  locations, itni HITM."

### 2. Kaunse variables — usual suspects

| Pattern | Fix |
|---|---|
| SPSC/MPMC queue ka `head_` aur `tail_` ek struct mein | `alignas(64)` har ek pe + padding (`41` `spsc_queue.hpp`) |
| Per-thread counters ek array mein: `long counts[NTHREADS]` | `struct alignas(64) { long v; } counts[NTHREADS]` |
| Ek struct jisme ek atomic flag + baaki hot data | atomic ko alag line pe |
| `std::vector<std::atomic<...>>` — elements adjacent | pad element type to 64 B |
| Global "stats" struct jise sab threads bump karte | per-thread shards, end pe merge |

### 3. Fix — do tareeke

**Padding / alignment** — variables ko alag lines pe force karo:
```cpp
struct Shared {
    alignas(64) std::atomic<std::size_t> head_;
    alignas(64) std::atomic<std::size_t> tail_;
    char pad_[64];                        // struct ke baad kuch aur na chipke
};
```
`41`'s `SpscQueue` exactly yeh + "cached opposite index" trick karta —
producer `tail_` ko rarely padhta (apna cached copy rakhta), to line
bounce hi nahi hota mostly.

**Sharding** — shared cheez ko per-thread copies mein todo:
```cpp
struct alignas(64) Stat { std::uint64_t trades = 0, volume = 0; char pad[48]; };
std::array<Stat, kMaxThreads> g_stats;        // thread t sirf g_stats[t] likhta
// reader end pe sab merge karta
```

---

## Is folder ke pipeline pe

`pipeline.hpp` ka pipeline **single-threaded** hai (clean measurement — `41`
lesson 13 ne dikhaya threading jitter optimization signal ko dhak deta).
Toh yahan false sharing **abhi nahi** hai.

Par jaise hi tum ise `41`'s 3-thread pipeline (`08_pipeline_demo.cpp`) mein
daaloge:
- SPSC queues ke `head_`/`tail_` — `41`'s `spsc_queue.hpp` already padded.
- Agar tum ek shared `struct { std::atomic<bool> done; Stats stats; }`
  banao — `done` (feed thread likhta) aur `stats` (strategy thread likhta)
  same line pe → false sharing. Alag karo.

> **HFT relevance:** ek market-data handler jo N symbols ko N threads pe
> baant ke process karta — agar per-symbol "last seq" counters ek array
> mein adjacent hain, har symbol update doosre symbols ke threads ki line
> invalidate karta. `alignas(64)` per counter → throughput 2-5× (measured
> in many real systems, `36/11`).

---

## Real sharing vs false sharing

- **False**: variables independent, bas line share karte → padding fixes.
- **Real**: threads sach mein **same** variable pe contend (ek shared
  counter jo sabko chahiye) → padding se kuch nahi. Fix: algorithm —
  per-thread accumulate + merge, ya atomic-free design, ya sharding.

Dono ka symptom `perf c2c` mein similar dikhta (HITM) — code padhkar
decide karo variables same hain ya bas padosi.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `alignas` laga diya par struct array mein phir bhi chipke
`alignas(64)` element ko align karta, par agar tum `std::vector<T>` use
karo aur `T` ka size 64 se kam hai bina trailing pad ke — nahi, actually
`alignas(64)` `sizeof` ko bhi 64 ka multiple bana deta. Par ek **plain
`T[]`** mein raw `alignas` member wale struct — check `sizeof`. Explicit
`char pad[...]` sabse safe.

### Trap 2 — false sharing ko guess se "fix" karna
Bina `perf c2c` ke padding chaaro taraf laga dena → memory bloat, cache
pressure, aur shayad asli problem (real contention ya D-cache miss) untouched.
**Measure pehle.**

### Trap 3 — read-only sharing ko problem samajhna
Agar dono threads ek line ko sirf **padhte** hain (koi likhta nahi) → koi
false sharing nahi (Shared state mein reh sakti, sab happy). Problem tab
jab **koi likhta** hai.

### Trap 4 — 128-byte "adjacent line prefetch"
Kuch Intel CPUs adjacent cache line prefetch karte (effective 128 B unit).
Ultra-sensitive code `alignas(128)` use karta critical vars pe. Measure —
zyadatar 64 kaafi.

### Trap 5 — padding se `sizeof` explode
Har per-thread stat 64 B → 128 threads → 8 KB stats struct. Theek hai
agar merge rare hai. Par 64 counters × 64 B = 4 KB jo poore L1 D-cache ka
1/8 — agar reader sab scan karta har baar, woh slow. Trade-off.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Cache coherence variable-level hai | **Line-level** (64 B) — isliye padosi variables interfere |
| False sharing = slow reads | Slow tab jab **likhna** hota; read-only sharing fine |
| Padding hamesha lagao (safe) | Bina measure ke = memory bloat + cache pressure |
| `alignas(64)` false sharing khatam | Sirf agar `sizeof` bhi 64 ka multiple aur array/layout cooperate kare |

---

## Hands-on

```bash
# 41 ka SPSC (already padded) -- padding hata ke dekho:
./build.ps1 fast 41-HFT-CONCURRENCY/examples/02_spsc_benchmark.cpp
# spsc_queue.hpp mein alignas(64) ko alignas(8) karke re-run -> throughput girta
# Linux: perf c2c record -- ./bin ; perf c2c report   (HITM lines dekho)
```

---

## Exercises

1. Ye struct do threads use karte (T0 `push_count` bump karta, T1
   `pop_count`). False sharing hai?
   ```cpp
   struct Q { std::atomic<long> push_count; std::atomic<long> pop_count; };
   ```
   <details><summary>Answer</summary>
   Haan — dono atomics 16 bytes, same 64-B line. T0 har push pe line ko
   Modified, T1 invalidate. Fix: `alignas(64)` dono pe + trailing `char
   pad[64]`. (Ya `41`'s cached-index trick se contention hi kam karo.)
   </details>

2. `perf c2c` bina, kaise shak karoge ki false sharing hai?
   <details><summary>Answer</summary>
   Multi-thread version single-thread se dheema ya barabar; core add karne
   pe throughput badhta nahi ya girta; `perf stat` mein high L2/L3 traffic
   aur `mem_load...xsnp_hitm` (ya AMD equivalent); code review mein do
   threads ka do adjacent variables pe write.
   </details>

3. Ek "global trade counter" jise 8 strategy threads har trade pe `++`
   karte. Padding se fix hoga?
   <details><summary>Answer</summary>
   Nahi — yeh **real sharing** hai (sab **same** variable). Padding sirf
   false sharing ke liye. Fix: per-thread `alignas(64)` counter, reader/
   end pe sum. Ya `std::atomic` fetch_add accept karo agar rate low hai
   (par 8-way contention pe woh bhi ~100 ns/op).
   </details>

---

## Interview questions

1. False sharing exactly kya? MESI terms mein ping-pong describe karo.
2. False vs true sharing — symptom same, fix alag. Kaise distinguish?
3. `alignas(64)` + `char pad[64]` — dono kyun (sirf alignas kaafi kyun
   nahi kabhi)?
4. `perf c2c` kya measure karta? "HITM" kya hai?
5. Per-thread sharding kab padding se behtar?

---

## Next
→ [`09-fixed-point-arithmetic.md`](09-fixed-point-arithmetic.md)
