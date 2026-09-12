# 16 — BUILD: poora feed handler (simple → measured → optimized)

## Prerequisites
- Poora folder 38 (`01`–`15`)
- `examples/03` se `10` tak (poore process ka code)

## Yeh topic abhi kyun
Yeh folder ka **capstone** hai — CLAUDE.md ka HFT engineering process
(build simple → measure → find bottleneck → optimize → re-benchmark →
explain kya badla) is folder mein pehli baar **poora, end-to-end** apply
hua. Yeh lesson poori journey ko ek jagah jodta hai.

---

## Poora process, ek nazar mein

```
STEP 1  build simple, CORRECT       -> 03_simple_parser.cpp
STEP 2  MEASURE it                  -> 04_parser_benchmark.cpp
STEP 3  find bottleneck, OPTIMIZE   -> 05_zero_copy_parser.cpp
STEP 4  RE-BENCHMARK, before/after  -> 06_parser_comparison.cpp
STEP 5  poore pipeline mein jodo    -> 10_feed_handler.cpp (+ 07, 08, 09)
STEP 6  EXPLAIN kya badla, kyun     -> (yahan)
```

---

## STEP 1 — Simple, correct (03)

```cpp
struct ParsedEvent { uint8_t type; ... };   // owned struct
static ParsedEvent parse_one(...) { ... }   // memcpy + swap

std::vector<ParsedEvent> events;             // NO reserve -- naive default
while (offset < buf.size()) {
    events.push_back(parse_one(...));
    offset += hdr.length;
}
```

Correctness verify ki gayi (03 ka output):
```
messages generated = 10000
messages parsed    = 10000
Add=5532 Execute=1832 Cancel=1119 Delete=901 Replace=616  (sum=10000)
sequence numbers strictly +1 monotonic? haan
```

**Yeh step OPTIMIZE nahi karta.** Iska poora point hai: pehle ek version
banao jispe tum BHAROSA kar sako. Bina iske, tumhe pata hi nahi chalega
agar baad ka "optimized" version SAHI bhi hai ya bas fast-aur-galat.

---

## STEP 2 — Measure (04)

```
=== Naive parser (owned ParsedEvent per message, growing vector) ===
  simple_parser (03)           p50   30.1   p99     50.1   p99.9     771.5   max  1728075.1  ns
```

**p50 chhota hai (30.1 ns)** — typical case fine hai. **p99.9 bahut zyada
hai (771.5 ns, ~25x p50)** — yeh tail hi problem hai (35/05 ka core
lesson yahan phir se). Guess nahi kiya ki "vector push_back slow hoga" —
**measure kiya, tail dekha, tabhi pata chala.**

---

## STEP 3 — Find bottleneck, optimize (05)

**Bottleneck kya tha?** `events.push_back(...)` — bina `reserve()` ke,
vector periodically **grow** karta (realloc + poore existing elements ki
copy) — yeh 36/04 ka allocation-tail pattern hai, exactly wahi shape.

**Fix:** owned-copy-into-growing-container ki jagah, **overlay-read
seedha buffer se, fixed pre-allocated sink mein likho**:

```cpp
std::array<SymbolState, NUM_SYMBOLS> book{};   // PRE-allocated, fixed
...
const auto* m = reinterpret_cast<const AddOrderMsg*>(buf + offset);  // overlay
book[sym].last_price = net_to_host_i64(m->price_ticks);               // direct write
```

Koi `push_back`, koi growing container, koi per-message owned struct.

---

## STEP 4 — Re-benchmark, before/after (06)

```
same feed (200000 messages), same box, same run:

  naive (03/04)                p50   30.1   p99    110.2   p99.9     891.7   max  1269363.2  ns
  zero-copy (05)               p50   30.1   p99     30.1   p99.9      40.1   max    11511.9  ns

p99.9 ratio (naive / zero-copy) = 22.2x
```

**Kya badla:**
- p50 barabar hai — typical-case cost same rahi (dono utna hi memory
  touch karte).
- **p99.9 mein 22.2x** — allocation tail poori tarah gayab, kyunki
  allocation khud gayab hai.
- **max mein bhi bada fark** (1.27ms vs 11.5µs) — chahe max ka ek
  hissa OS-noise ho sakta (35/06), naive path ka occasional-bada-realloc
  bhi is number mein contribute karta.

**Kyun badla — mechanism, na ki magic:** `std::vector::push_back` amortized
O(1) hai *average* mein, par **worst case O(n)** (realloc + copy) — yeh
worst case hi p99.9/max mein dikhta hai. Zero-copy path mein aisa koi
"worst case" hai hi nahi (fixed array, koi growth kabhi nahi hoti).

---

## STEP 5 — Poore pipeline mein jodo (07, 08, 09, 10)

Individual pieces ready the (parsing, gap-detection — 08, A/B arbitration
— 09) — `10_feed_handler.cpp` sabko **ek class** mein jodta:

```cpp
class FeedHandler {
    std::size_t on_bytes(const std::byte* buf, std::size_t remaining) {
        // 1. framing (11) -- peek_header, partial-check
        // 2. gap detection (04) -- expected_seq tracking
        // 3. zero-copy dispatch + book update (09) -- overlay-read
    }
};
```

**End-to-end measured:**
```
=== End-to-end feed handler (framing + gap-detect + zero-copy + book update) ===
  on_bytes()                   p50   30.1   p99     30.1   p99.9      40.1   max     3596.8  ns

messages processed: 199795
gap events: 205  (total 205 sequence numbers missing)
duplicates/out-of-order: 0
```

**Yeh number bilkul optimized-parser-ALONE (05) jaisa hai** (p50 30.1,
p99.9 40.1) — matlab **framing + gap-detection logic ne apni khud ki koi
extra tail add nahi ki**. Woh dono O(1), branch-predictable, allocation-
free operations hain — poori pipeline ka cost sirf parsing/book-update ka
hai, jo hum already optimize kar chuke the.

---

## Poori kahani — ek table mein

| Step | File | p50 | p99.9 | Kya sikhaya |
|---|---|---|---|---|
| 1. Build correct | 03 | — | — | Bina correctness-baseline ke, optimization ka koi meaning nahi |
| 2. Measure | 04 | 30.1 | 771.5 | Tail (p99.9) typical-case (p50) se 25x zyada — allocation ka signature |
| 3. Optimize | 05 | 30.1 | 40.1 | Zero-copy: owned-struct-into-vector → overlay-read-into-fixed-sink |
| 4. Compare | 06 | 30.1 vs 30.1 | 891.7 vs 40.1 | 22.2x tail improvement, ZERO p50 regression |
| 5. Integrate | 10 | 30.1 | 40.1 | Poori pipeline (framing+gap+parse) = akele parser jitna hi tight |

---

## ⚠️ Traps / Common mistakes

### Trap 1 — Step 3 (optimize) seedha Step 1 se karna, Step 2 (measure)
skip karke
Bina baseline measurement ke, tumhe pata hi nahi chalega optimization ne
kitna faayda diya (ya diya bhi ki nahi) — "measure first" (36/24 ka
Rule 0) yahan bhi lagta.

### Trap 2 — sirf p50 dekh ke "optimization ki zaroorat nahi" bolna
04 ka p50 (30.1) already "theek" lag raha tha — agar sirf p50 dekhte,
tail problem (771.5 ns, 25x) miss ho jaata. **Hamesha p99.9 (ya zyada)
bhi dekho.**

### Trap 3 — end-to-end integration ke baad re-measure na karna
Individual pieces optimize karne ke baad bhi, **poore pipeline ko ek
saath measure karna zaroori hai** (Step 5) — kabhi-kabhi integration
apna khud ka overhead add kar sakta (jo yahan nahi hua, par yeh verify
karna hi Step 5/6 ka point hai, assume nahi karna).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Optimization seedha shuru se karna chahiye | Pehle correct-simple baseline, phir measure, phir optimize |
| p50 dekhna kaafi hai | p99.9/tail dikhata jo p50 chhupa deta |
| Individual pieces optimize karne se end-to-end automatically optimal hota | Integration ko bhi re-measure karo (yahan match hua, guaranteed nahi tha) |
| "Zero-copy" kehna kaafi hai claim karne ke liye | Measure karke PROVE karo (06 jaisa before/after) |

---

## Exercises

1. Agar `10_feed_handler.cpp` ka end-to-end p99.9 (40.1 ns) `05`'s
   standalone parser (40.1 ns) se **bahut zyada** hota (jaise 500 ns),
   iska kya matlab hota, aur agla step kya hota?
   <details><summary>Answer</summary>
   Iska matlab hota ki gap-detection ya framing logic (jo `05` mein nahi
   thi, sirf `10` mein hai) apna khud ka tail-cost add kar rahi. Agla
   step: profile karo (35's tools — perf/`perf annotate` on Linux) yeh
   pata karne ke liye kaunsa specific piece (framing check, gap-tracking
   state update) yeh extra tail add kar raha, phir usi ko optimize karo
   (same process, ek level deeper).
   </details>

2. Poora process (03→04→05→06→10) mein "explain kya badla" step
   (CLAUDE.md Rule 12) is lesson mein kahan hai?
   <details><summary>Answer</summary>
   STEP 4 ke baad ka "Kya badla — mechanism, na ki magic" section —
   sirf numbers dikhana kaafi nahi tha, WHY (vector's worst-case O(n)
   realloc vs fixed-array's O(1) guaranteed) explicitly explain kiya
   gaya. Yeh CLAUDE.md ka explicit requirement hai: "re-benchmark ->
   explain kya badla," sirf numbers table nahi.
   </details>

---

## Interview questions

1. Poora HFT performance-engineering process (steps) batao, is folder
   ke concrete example ke saath.
2. p50 same rehte hue p99.9 mein 22x improvement — yeh kya batata hai
   allocation ke behavior ke baare mein?
3. Kyun end-to-end integration ko separately measure karna zaroori hai,
   individual pieces optimize karne ke baad bhi?
4. "Measure first" principle ko break karne se kya risk hai?

---

## Next
→ [`17-exercises.md`](17-exercises.md)
