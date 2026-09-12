# 16 — Throughput aur latency benchmarks

## Prerequisites
- `15-fuzzing.md`
- `35-*` ka benchmarking methodology (rdtsc harness, percentiles — agar
  padha ho), `39-ORDER-BOOK/04-measuring-v1.md` (SAME storage pattern
  ka tail-latency story, yahan REPEAT hoti)

## Yeh topic abhi kyun

**Correctness pehle prove ho chuki (14, 15) — ab REAL numbers.** CLAUDE.md
Rule 2: kabhi "yeh fast hai" mat likho bina chalaye — chalao, actual
number lo, wahi likho. Yeh sab is machine pe (AMD Ryzen 7 4700U, Zen 2,
~2.0 GHz, MinGW-w64 GCC 15.1.0) `-O2` pe **actually measured** hain.

---

## Numbers (`08_engine_bench.cpp`, N=150000 commands, seed=555)

```
                          p50      p99      p99.9      max        (ns)
ALL ops                   140.3    751.4    1763.4    508326.3
submit() only              150.3    781.5    1913.6    508326.3
  submit: Limit             160.3    721.4    6171.7    508326.3
  submit: Market            250.5   1082.1    2394.6     41869.6
  submit: IOC                50.1    490.9     921.8      1492.8
  submit: FOK                40.1    480.9     961.8      1983.8
cancel() only                60.1    300.6     911.7       9017.1

total commands = 150000, total trades generated = 33459
final: resting_count=39124, best_bid=9995, best_ask=10007
throughput (single-threaded, this workload mix) ~= 5.63 million ops/sec
```

---

## Har number ka MECHANISM -- kyun jaisa hai

### `cancel()` sabse FAST (p50 60.1 ns)
O(1) `index_.find()` (hash lookup) + O(1) `std::list::erase()` (iterator
already known — koi search nahi) — koi memory allocation NAHI (list-node
already exists, sirf unlink hota), koi map-rebalance NAHI (agar level
empty na ho). Sabse simple operation, sabse fast.

### `submit(): FOK` sabse FAST submit (p50 40.1 ns)
Yeh COUNTER-INTUITIVE lag sakta ("FOK to sabse COMPLEX hai, precheck
wala!") — par is workload mix mein zyaadatar FOK orders **precheck mein
hi REJECT ho jaate** (available liquidity target se kam), jo ek CHEAP,
early-exit path hai — poora matching loop kabhi CHALTA hi nahi. Jab FOK
paas hota (poora fill hone wala), tab woh EXPENSIVE ho sakta (poora match
karna padta) — yeh p99.9 (961.8 ns, IOC se thoda zyaada) mein dikhta hai.

**Yeh EXACTLY CLAUDE.md's Rule 2 ka spirit hai** — number counter-
intuitive lagta jab tak MECHANISM samjho, phir bilkul makes sense.

### `submit(): IOC` bhi fast (p50 50.1 ns)
Similar reasoning — is workload mix mein kaafi IOC orders price-cross
NAHI karte (turant `match_against`'s `while` loop `break` ho jaata), koi
allocation nahi hoti.

### `submit(): Market` sabse SLOW average (p50 250.5 ns)
Market order **price check hi nahi karta** (04) — jab tak book khaali na
ho ya qty poori fill na ho, HAMESHA kai levels sweep karta, matlab
zyaadatar Market orders **MULTIPLE trades** generate karte (ek submit()
call mein kai Trade objects push_back hote, kai list-erase() calls) —
IOC/FOK ke "aksar zero-trade" pattern se ULTA.

### `submit(): Limit` ka HUGE tail (p50 160.3, PAR p99.9 6171.7!)
Yeh **39-ORDER-BOOK's V1 ka EXACT SAME pattern hai** (`04-measuring-v1.md`
mein already dekha): jab ek Limit order ki price ek **NAYA price level**
banati (koi resting order pehle se us price pe nahi tha), `bids_[price]`
(`std::map::operator[]`) ek NAYA tree-node ALLOCATE karta, poori tarah
REBALANCE ho sakta (red-black tree insertion), aur us level ke `std::list`
mein PEHLA node bhi ALLOCATE hota. Yeh dono allocations OS/allocator ki
availability pe depend karte — MOSTLY fast (cached free-list se), par
KABHI-KABHI slow (fresh page fault, allocator ka internal bookkeeping) —
isi wajah se p50 kam par p99.9 bahut zyaada.

**Yeh REPEAT SEEKHA gaya lesson hai, is folder mein SPECIFICALLY dobara
measure kiya gaya taaki verify ho ki 39's finding sirf 39-specific NAHI
thi — yeh `std::map`+`std::list` combination ka GENERIC property hai,
jahan bhi use ho.**

---

## Throughput number -- kya woh MEANS karta

`~5.63 million ops/sec` — yeh **poore run ka average** hai (sab commands
ki total time / N). Yeh HONEST hai (Rule 2), par iski limitations bhi
explicitly note karni chahiye:

- Yeh **single-threaded, in-process** number hai — koi network I/O,
  koi serialization, koi real market-data-feed burstiness (36-LOW-
  LATENCY-CPP ka poora domain) shamil nahi.
- **p99.9 tail ko yeh number nahi dikhata** — average throughput HIGH
  ho sakta chahe kuch operations 1000x SLOWER hon (jaisa Limit ka
  p99.9 upar dikhaya) — average sirf "bulk" story batata, "worst-realistic-
  case" nahi (35/05's principle).
- Yeh EK specific workload MIX (engine_workload.hpp's ratios) pe measured
  hai — different mix (zyaada Market orders, ya zyaada Limit-with-new-
  levels) DIFFERENT throughput dega.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — average latency (p50) ko "typical performance" maan lena
p50 sirf MEDIAN batata — Limit order ka p50 (160.3) uski real WORST-CASE
story (p99.9 = 6171.7, **38x zyaada**) ko chhupata. Production system
design karte waqt tail matter karta (35/05).

### Trap 2 — is number ko "matching engines generically itni fast hoti"
generalize kar dena
Yeh number IS engine (map+list, single-threaded, is machine pe) ka hai —
39's V1 vs V3 story ne already dikhaya ki storage-choice se numbers 2-4x
badal sakte. Agar production-grade throughput chahiye, 39's V3-style
(arena + intrusive list + flat hash) ka RESTING-storage approach yahan
bhi laga sakte (exercise 17 mein).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| FOK sabse slow hoga (sabse complex logic) | Is workload mix mein SABSE fast (early-reject path dominant) |
| Throughput number = worst-case guarantee | Sirf AVERAGE batata; p99.9 (Limit ka 6171.7 ns) alag story batata |
| Limit order ki tail-latency ek NAYI cheez hai is folder ki | 39-ORDER-BOOK's V1 mein EXACT SAME mechanism, dobara verify hui |

---

## Hands-on

```bash
./build.ps1 fast 40-MATCHING-ENGINE/examples/08_engine_bench.cpp
```

Apni machine pe chalao, apne NUMBERS compare karo — CLAUDE.md ka rule:
absolute numbers machine-specific hote, RATIOS/SHAPES (Limit ka tail
IOC se zyaada bada, Market ka average sabse zyaada) generalize hote.

---

## Exercises

1. Kyun FOK orders (average) IOC se bhi thoda FASTER measure hue is run
   mein, jabki FOK "logically zyaada complex" hai (precheck extra step)?
   <details><summary>Answer</summary>
   Precheck khud CHEAP hai jab woh REJECT karta (early exit, poori
   matching loop skip ho jaati) -- aur is workload mix mein zyaadatar
   FOK orders reject ho rahe the. "Extra logical step" hona zaroori
   nahi "slower" ka matlab rakhta -- depends kaunsa PATH zyaada baar
   liya jaata (Rule 2 ka essence).
   </details>

2. Agar tum production-grade throughput chahte (39's V3 jaisa), kya
   badlega?
   <details><summary>Answer</summary>
   Resting-order storage `std::map`+`std::list` se 39's V3-style
   (tick-indexed flat array + intrusive arena-linked-list + tombstone
   flat hash) mein badalna hoga -- `index_` ka O(1) DIRECT access (list
   iterator ki jagah arena-slot index) allocation-free hot path deta.
   Yeh 17's ek EXTENSION exercise hai.
   </details>

---

## Interview questions

1. Har order type ka latency-profile (p50) aur uska MECHANISM explain
   karo.
2. Limit order ka p99.9 itna zyaada kyun hai apne p50 se? Yeh kahan
   pehle bhi dekha hai (course mein)?
3. Throughput ka single average-number kis important cheez ko CHHUPATA
   hai?

---

## Next
→ [`17-exercises.md`](17-exercises.md)
