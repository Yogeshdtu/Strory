# 04 — Project 3: Limit order book (v1 → v3)

## Prerequisites
- `03-project-feed-parser.md`
- `39-ORDER-BOOK` (poora — V1 `std::map` → V2 sorted vector regression → V3 flat array)
- `43-HFT-OPTIMIZATION/14-case-study-order-book.md`

## Yeh topic abhi kyun

Strategy ka "view of the market." Parse ne `MdMessage` diya; book use
apply karke ek queryable state banata: best bid/ask, per-level qty,
imbalance. 39 ne yeh 3 baar banaya aur measure kiya; yahan **V3 seedha
use** karte (39's story repeat nahi).

## `L2Book` — the design (`mh_order_book.hpp`)

39 V3 + 43 pipeline V3:

```cpp
std::array<std::int64_t, kLevels> bid_{}, ask_{};   // index = px_ticks - kBase, value = aggregate qty
std::vector<Loc>                  loc_;              // id -> {level_idx, qty, side}  (ids dense -> direct index)
std::size_t best_bid_ = kNoBid, best_ask_ = kLevels;   // cached top-of-book
```

- **apply(Add)**: `arr[idx] += qty` (1 write), `loc_[id] = {...}` (1 write,
  direct index), update `best_*` if improved. O(1), zero allocation.
- **apply(Cancel)**: `Loc l = loc_[id]` (1 read), `arr[l.idx] -= l.qty`
  (1 write), re-walk `best_*` **only if the touch level emptied**. O(1)
  amortized.
- **best_bid()/best_ask()/mid2()**: cached, O(1). `mid2()` returns
  `best_bid + best_ask` (= 2×mid) — division avoid (43/09).
- **imbalance(depth)**: `(bidQ − askQ) * 1000 / (bidQ + askQ + 1)` over
  the top `depth` levels — integer, no float.

### `resolve_cross()` — 39/16 invariant made executable
Ek pure L2 aggregator jise occasionally-crossing feed mile, transiently
`best_bid >= best_ask` dikha sakta. `resolve_cross`: jab tak `bid < ask`
nahi hota, dono touch levels mein se **chhote** wale ko drop karo (crossed
liquidity jo real venue pe trade ho jaati). Bounded (har step ek level
hataata). `apply()` ke end mein call hota.

Yahan sim non-crossing guarantee karta (project 1), to `resolve_cross`
almost kabhi fire nahi karta — par yeh defensive invariant hai, aur
`03_order_book.cpp` isko brute-force reference se verify karta.

## Measure (`03_order_book.cpp`)

### Correctness: BBO vs a brute-force `std::map` reference
```
invariant vs brute-force reference:
  118288 comparisons, 0 mismatches -> BBO always agrees
```
`RefBook` = `std::map` per side (O(log n), obviously-correct). Har message
ke baad, jab reference clean (non-crossed) ho, `L2Book`'s cached BBO usse
match kare. 0 mismatches → cached-BBO tracking correct hai.

### Speed
```
L2Book.apply() : ~17 ns/message  (flat array + cached BBO)
```
Folder 39 ne `std::map` version measure kiya — ~10–25× slower per op
(node malloc + tree walk + cache-cold pointers).

## HFT relevance

Yeh classic HFT interview question aur sabse hot data structure. Do use
cases, do designs (43/14):
- **Strategy's view** (yeh): aggregate qty + cached BBO + imbalance.
  Per-order FIFO nahi chahiye. Flat array jeetta.
- **Matching engine's book** (project 4): per-level FIFO (price-time
  priority) chahiye. Intrusive list per level (39 V3 / `FastVenue`).

> Real books: dynamic re-basing (price band shifts), a "far levels"
> fallback map beyond the flat window, book snapshots for recovery
> (38/03, 39/13), and hard invariant asserts in debug builds (39/16).

## ⚠️ Traps

### Trap 1 — flat array bina window bounds
`idx = px - kBase` jahan `px` window ke bahar → **OOB write** (silent
corruption). `L2Book::level_of` clamps. Real book: re-base or fallback.

### Trap 2 — cached BBO stale
`best_bid_` update bhoolna ek code path pe → strategy galat price pe
quote karti, risk galat collar check karta. `03_order_book.cpp`'s
reference comparison is exactly this guard.

### Trap 3 — dense-id assumption
`loc_[id]` direct index sirf jab ids compact. Exchange ids often 64-bit
sparse → `std::vector` sized to max = TB. Tab flat hash (39/09) ya
per-session remap. Sim ids dense by construction.

### Trap 4 — `std::map` "it's O(log n), fast enough"
Har op ek `malloc` + a tree walk over RAM-scattered nodes. "Big-O lies
when the constant is a cache miss" (20/DSA). Measure.

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Ek book design sab use cases ke liye | Strategy view (aggregate) ≠ matching book (per-order FIFO) |
| `std::map` fast enough | Node malloc + pointer chase; ~10–25× slower than flat (39) |
| Sorted vector always beats map (cache) | 39/06: churn-heavy pe O(n) memmove → overall worse |
| Crossed book impossible with a clean feed | Aggregator + stale orders → transient cross; `resolve_cross` |

## Exercises

1. `L2Book`'s `kLevels` 2048 se 128 kar do (sim px range ~9400–10600 =
   1200 ticks). Kya hota?
   <details><summary>Answer</summary>
   `idx = px - 9000` up to ~1600 > 128 → OOB write. `level_of` clamps to
   127 → sab high prices ek level pe collapse → BBO garbage. Window ko
   price range cover karna chahiye. ASan/bounds build se pakdo.
   </details>

2. Strategy ko "top 5 levels ka total qty" chahiye. `L2Book` pe cost?
   <details><summary>Answer</summary>
   `best_bid_` se 5 sequential `int64` reads (`bid_[best_bid_] +
   bid_[best_bid_-1] + ...`) — cache-line-local, few ns. Flat array isme
   bhi achha (contiguous). `std::map` mein 5 `--it` steps = 5 potential
   misses.
   </details>

3. `resolve_cross` ko remove kar do aur sim ko crossing allow karwao
   (`step()` ka clamp hataao). `03_order_book.cpp` kya dikhaayega?
   <details><summary>Answer</summary>
   Book crossed rahega (best_bid > best_ask), `L2Book`'s BBO reference se
   diverge karega (reference bhi cross karega par alag tarah), aur mid2
   stale ho jaayega. Yeh exactly 43-pipeline ka latent bug tha. Isliye
   dono: sim clamp + book resolve_cross.
   </details>

## Interview questions

1. `std::map` order book ki 3 concrete per-op costs?
2. Cached BBO — kab re-walk zaroori, aur re-walk O(kya)?
3. Aggregate-qty book price-time priority match kyun nahi kar sakti?
4. Dense id lookup: array vs hash — kab kaunsa?

## Next
→ [`05-project-matching-engine.md`](05-project-matching-engine.md)
