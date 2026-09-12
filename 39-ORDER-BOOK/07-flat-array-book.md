# 07 — Version 3: flat array-of-price-levels

## Prerequisites
- [`06-measuring-v2.md`](06-measuring-v2.md)
- `examples/orderbook_v3_flat.hpp`

## Yeh topic abhi kyun
06 ne do problems dikhaye: V1's tree/list allocation overhead, aur V2's
naya linear-scan regression. V3 dono ko fix karta — **price levels ke
liye** yeh lesson, order-within-level ke liye 08, order-id lookup ke
liye 09.

---

## Core idea: price ko seedha ARRAY INDEX bana do

```
Price ek TICK hai (37/08 -- integer). Agar price ek bounded range mein
hai (center ke +-N ticks), to price -> array-index ek O(1) ARITHMETIC
operation hai -- koi search, koi tree traversal, kuch nahi.
```

```cpp
static constexpr int NUM_LEVELS = 256;

// Bid: index 0 = center-1 (closest, potentially best), index badhne se
// price GHATTI hai (center se door).
int bid_idx = static_cast<int>(center_ - 1 - price);

// Ask: index 0 = center+1 (closest, potentially best), index badhne se
// price BADHTI hai (center se door).
int ask_idx = static_cast<int>(price - center_ - 1);

std::array<LevelV3, NUM_LEVELS> bid_levels_;
std::array<LevelV3, NUM_LEVELS> ask_levels_;
```

**`add(price)` ab O(1) hai** — na binary search (V2), na tree lookup (V1),
sirf ek subtraction + array-index. Level EXISTS karta hai HAMESHA (array
mein woh slot pehle se allocated hai) — "naya level create karna" jaisa
concept hi khatam ho gaya (V1's Add-tail ka #1 source).

---

## Trade-off: BOUNDED range (02 mein already flagged)

```cpp
if (offset < 0 || offset >= NUM_LEVELS) return false;   // range se bahar -> reject
```

Yeh V1/V2 se ek **fundamental fark** hai — V1/V2 KOI bhi `int64_t` price
handle kar sakte the (tree/vector dynamically grow hote). V3 ek fixed
`NUM_LEVELS` array rakhta — price agar range se bahar hai, `add()` seedha
`false` return karta.

**Real systems isse kaise handle karte:**
- Price range **generously size karo** (tick size, 37/08, aur symbol ki
  typical daily-move se estimate karke).
- **Re-centering** — periodically (off hot-path, jab price drift kare)
  poora array ek naye `center_` ke around REBUILD karo.
- Extreme moves (circuit-breaker-level, 37/08) ke liye ek **fallback**
  (dynamic structure) rakho jo rarely trigger ho.

Yeh trade-off **explicit** hai, chhupa hua nahi — 24-tradeoffs-and-when-
not-to (36) ka principle yahan bhi.

---

## Correctness verified — V1/V2 se IDENTICAL

```
=== BookV3 (flat array + intrusive list + flat hash) -- correctness check ===
ops applied: 20000 / 20000  (0 failed)
live orders in book: 9118

best bid = 9999 x30136
best ask = 10001 x25547
best_bid < best_ask? haan (sahi)
```

Teeno versions (V1, V2, V3) **exact same numbers** dete same workload pe —
strong correctness evidence (14/16 mein aur rigorous verification).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `NUM_LEVELS` ko bahut chhota rakhna
Agar tumhara actual price range `NUM_LEVELS` se badi hai, orders SILENTLY
reject honge (`add()` false return karta) — jo tumhare book ko real
exchange state se **diverge** kar dega, bilkul 38/03 ka "silent drift"
jaisa. Return value **hamesha check karo.**

### Trap 2 — array ko "free" samajhna
`std::array<LevelV3, 256>` **poora allocate hota** construction pe (dono
sides, dono ~256 elements) — chahe kitne bhi levels abhi "khaali" hon.
Yeh memory cost hai jo V1/V2 mein nahi tha (jo sirf ACTUALLY-USED levels
ke liye allocate karte). Trade-off: predictable memory vs actual-usage
memory.

### Trap 3 — center_ ko kabhi update na karna
Agar price permanently drift kare (naya range) aur `center_` kabhi
re-center na ho, sab NAYE orders reject honge forever. Re-centering ek
zaroori maintenance operation hai, "optional" nahi.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| V3 koi bhi price handle kar sakta | Bounded range — `NUM_LEVELS` se bahar reject |
| Array = free memory | Poora array allocate hota, chahe empty levels ho |
| Bounded range = worse design | Explicit, documented trade-off — O(1) access ke against |
| Range-check skip kar sakte "usually fine hai" soch ke | Silent drift ka risk — hamesha check karo |

---

## Exercises

1. `NUM_LEVELS = 256` hai, workload ka `price_range = 100` (default). Kya
   headroom hai, aur kyun zaroori hai?
   <details><summary>Answer</summary>
   156 levels ka headroom (256 - 100). Zaroori hai kyunki: (a) safety
   margin agar prices thoda expected se zyada spread hon, (b) `Replace`
   operations naye prices generate karte jo thoda alag range mein ho
   sakte. Zero-headroom design fragile hota — chhoti si workload-shift bhi
   silent rejections start kar deti.
   </details>

2. Ek symbol jiski daily price-range bahut wide hai (jaise ek volatile
   crypto pair) — V3 ka fixed `NUM_LEVELS` approach use karne ke liye kya
   badalna padega?
   <details><summary>Answer</summary>
   `NUM_LEVELS` ko is symbol ke worst-realistic-case range ke hisaab se
   size karo (bada array — memory trade-off), YA periodic re-centering
   implement karo (jab price ek threshold se drift kare, array ko naye
   center ke around off-hot-path rebuild karo), YA is specific symbol ke
   liye V2-style (bounded-range-agnostic) design use karo agar range
   truly unpredictable hai.
   </details>

---

## Interview questions

1. Price-to-array-index mapping ka formula batao (bid aur ask dono).
2. V3 ka bounded-range trade-off kya hai, aur real systems ise kaise
   handle karte?
3. Kyun `NUM_LEVELS` headroom rakhna zaroori hai?
4. Array ki "poori allocation upfront" ka memory trade-off kya hai V1/V2
   ke against?

---

## Next
→ [`08-intrusive-order-lists.md`](08-intrusive-order-lists.md)
