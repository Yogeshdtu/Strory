# 15 — Case study: end-to-end tick-to-trade

## Prerequisites
- `13-case-study-feed-handler.md`, `14-case-study-order-book.md`
- `42-HFT-NETWORKING/14-wire-to-wire-measurement.md`, `37-HFT-FUNDAMENTALS/14-latency-budget.md`

## Yeh topic abhi kyun

`13` ne parse, `14` ne book optimize kiya. Ab **poora** stitch karke dekhte
hain — cumulative speedup, per-stage contribution before/after, aur tail
(p99/p99.9), kyunki HFT mein tail hi sab kuch (`36/02`).

---

## The pipeline (single-threaded, clean measurement)

```
market-data byte blob
      │
      ▼  parse    (13)   -> Msg {type, side, px_ticks, qty, id}
      ▼  book     (14)   -> apply add/cancel; update top-of-book
      ▼  signal          -> mid vs SMA; threshold cross?
      ▼  encode          -> order record (POD, no alloc)
      │
      ▼  (would go to gateway -> wire; here: recorded)
```

Threading jaan-boojh kar nahi (`41/13`: unpinned box pe thread jitter
optimization signal ko dhak deta — p50 stable rehta par p99 100× jhoolta).
Single-thread = har change ka effect saaf dikhta. Threading ka apna
treatment `41` mein.

---

## v0 → v3 — measured (`04_before_after.cpp`, is box, 120k msgs)

### Per-tick total (true average, clean loop — the honest number)

```
v0 (naive)     :  ~1.9 – 2.7  µs/tick     (run-to-run varies, unpinned)
v3 (optimized) :  ~25  – 45   ns/tick
speedup        :  ~60 – 80x
```

### Per-stage (rdtsc attribution — SHAPE, not absolutes)

```
stage     v0 ns/tick      v3 ns/tick*     ratio
parse     ~1100–1330      ~45  (probe)    ~25x
book      ~1065–1400      ~44  (probe)    ~27x
signal    ~125–170        ~37  (probe)    ~4x
                          * v3 per-stage ≈ 4 rdtsc probes (~80 ns) + <10 ns real work
```

### Correctness gate

```
v0 fired 110 orders,  v3 fired 110 orders
positional matches: 110 / 110   -> IDENTICAL
```

Fixed-point signal (`09`, `10`) ne float signal ko **exactly** reproduce
kiya is workload pe. Speedup **and** same output.

---

## Amdahl check — kya cumulative speedup expect tha

v0 breakdown ≈ parse 45%, book 48%, signal 5%, (encode folded into signal).

- Sirf parse ∞-fast: `1/(1-0.45)` = 1.8× max
- Sirf book ∞-fast: `1/(1-0.48)` = 1.9× max
- **Dono** (parse+book = 93%) ∞-fast: `1/(1-0.93)` = **14× max**

Measured **~60–80×** — Amdahl ke "14× max" se **zyada**?! Kyun:

Amdahl assume karta har slice ka baaki-sab **constant** rehta. Yahan
`std::string`/`std::map`/`malloc` hatane se sirf woh stage tez nahi hui —
**cache behaviour poore process ka** badal gaya: v0 har tick ~5 mallocs +
tree nodes RAM mein bikhere → D-cache constantly thrashed, har stage ki
"baseline" cost bhi high thi. v3 ka working set chhota + contiguous → sab
kuch L1/L2 → signal stage bhi (jo "unchanged" thi) tez chalti.

**Sabak:** big structural changes non-linear hote — ek stage saaf karne se
padosi stages bhi faayda paate (shared resource: cache, allocator,
TLB). Amdahl ek **lower bound** intuition deta jab changes local hon; global
changes usse tod dete.

---

## The tail — p99 / p99.9 (jo HFT mein matters)

`04` mean deta; tail ke liye per-tick distribution chahiye. v0 ke tail
spikes ka source:
- `std::vector<std::string> out_` **realloc** jab signal fires cluster mein
  (capacity double → `malloc` + `memcpy` of all prior) — ek tick 50–200 µs
- `std::map` node `malloc` jab allocator ko OS se page maangna pade
- freed-block fragmentation → later allocs slow

v3 ke tail:
- sab preallocated (`id_loc_` sized upfront, `out_` ek POD reused) → **no
  alloc in steady state** → tail collapse
- `rewalk_bid_/ask_` on level depletion = bounded sequential scan, worst
  case ~few hundred ns (still << v0 p50)

`36/02` (tail latency), `36/04-05` (preallocation) ne yeh isolate karke
dikhaya. Pipeline mein: **v0 ka p99.9 v3 ke p50 se hazaaron guna** — mean
speedup 60–80× hai, tail speedup usse bahut zyada.

---

## Where the time goes NOW (v3) — the inversion

v3 mein parse/book itne chhote ho gaye ki:
- rdtsc probe cost (~80 ns) > real work (~15–25 ns) — measurement ab
  bottleneck (`02`, `03`)
- agla real bottleneck: `42/14` wala point — **network hops** (jo yahan
  0 hain, recorded feed) aur **strategy logic** (yahan trivial SMA). Real
  system mein processing (matching, risk, decision) SABSE BADA aur SABSE
  CONTROLLABLE hissa ban jaata jab I/O bypass ho jaata (`42`).

Optimization iterative: parse+book saaf → ab agar aur chahiye to (a) better
measurement (batch timing / `perf`), (b) SIMD parse (multiple messages at
once), (c) strategy logic (jo yahan ~0 hai). **Diminishing returns** — `16`.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — sirf mean report karna
"60× faster" — par v0 ka p99.9 kya tha? Tail speedup mean se alag (yahan
bahut bada). HFT mein p99.9 quote karo.

### Trap 2 — Amdahl ko literal lena
"93% optimize kiya, max 14×" — measured 70×. Global changes (cache,
allocator) Amdahl's locality assumption todte. Amdahl = intuition, measure
= truth.

### Trap 3 — v3 ke per-stage rdtsc numbers ko trust karna
~45 ns/stage mein ~35 ns probe hai. v3 profile karne ke liye alag method
(`03`).

### Trap 4 — pipeline ko threaded banake "aur tez" expect karna
`41/13`: unpinned box pe threading ne p99 ko 100× jhula diya, p50 same.
Threading throughput ke liye hai (parallel symbols), single-stream latency
ke liye nahi (jab tak core-pinned + isolated).

### Trap 5 — "done" jab pipeline tez ho gaya
Real tick-to-trade = wire-in → parse → book → signal → risk → encode →
wire-out. Yah humne bs middle 4 optimize kiye. Network (`42`), risk,
serialization abhi. `37/14` latency budget.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Amdahl kehta max 14×, toh 70× galat measurement | Global changes (cache/alloc) Amdahl locality assumption todte |
| 60× mean speedup = 60× everywhere | Tail (p99.9) speedup mean se bahut zyada (alloc spikes gaye) |
| Pipeline tez → tick-to-trade done | Wire I/O, risk, serialization abhi baaki (`42`, `37/14`) |
| Threading = automatic speedup | Latency ke liye pinned+isolated chahiye; warna jitter (`41/13`) |

---

## Hands-on

```bash
./build.ps1 fast 43-HFT-OPTIMIZATION/examples/04_before_after.cpp
# distribution ke liye: 01/03 ka (A) ko ek per-tick vector mein collect karke
# sort + p50/p99/p99.9 (like 40/08_engine_bench.cpp)
```

---

## Exercises

1. v0 breakdown 45/48/5. v3 mein parse+book ~24–27× hue, signal ~unchanged.
   Naya per-tick total (rough)?
   <details><summary>Answer</summary>
   v0 ~2000 ns → parse ~900, book ~960, signal ~100 (rough, of the *true*
   not rdtsc split). v3: parse ~900/25 ≈ 36, book ~960/26 ≈ 37, signal ~100
   (unchanged?) → ~173 ns. Par measured v3 ~30 ns! Signal bhi gira
   (integer, ring — `09`/`10`) AUR cache effect ne sab ko aur neeche kheecha
   → non-linear (upar wala inversion point).
   </details>

2. `out_` (v0 `std::vector<std::string>`) ko `reserve(200)` upfront kar do.
   Mean pe farak? Tail pe?
   <details><summary>Answer</summary>
   Mean: chhota (signal 5% hai, aur alloc har tick nahi hoti). **Tail:**
   bada — realloc spikes (jab fires cluster karte, capacity double +
   memcpy) gaye. p99.9 meaningfully girega. Yeh `36/04-05` ka point:
   preallocation mean se zyada tail ko theek karta.
   </details>

3. Pipeline ko `41`'s 3-thread version mein daal do (parse thread → SPSC →
   book+signal thread → SPSC → encode thread), unpinned. p50 aur p99 ka
   kya hoga (`41/13` se)?
   <details><summary>Answer</summary>
   p50: ~same ya thoda behtar (parallel stages). p99/p99.9: **bahut worse**
   — unpinned threads OS scheduler ke reham pe, ek stage ka thread
   descheduled → poori chain stall. `41/13`: paced pipeline pe p99 30ms tak
   dekha. Fix: core-pin + isolate (`41/06`), warna single-thread behtar
   latency deta.
   </details>

---

## Interview questions

1. Cumulative speedup Amdahl ke "max" se zyada kaise ho sakta?
2. Mean speedup 60×, tail speedup usse bahut zyada — kyun (allocation
   terms)?
3. v3 mein "where does the time go now" — do naye bottlenecks?
4. Yeh pipeline single-threaded kyun rakha (measurement ke liye)?
5. Real tick-to-trade mein is pipeline ke aage/peeche kya-kya hai?

---

## Next
→ [`16-when-to-stop.md`](16-when-to-stop.md)
