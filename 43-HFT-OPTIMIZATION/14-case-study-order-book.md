# 14 — Case study: order book (data structure)

## Prerequisites
- `13-case-study-feed-handler.md`
- `39-ORDER-BOOK` (poora — V1/V2/V3 measured progression)
- `07-struct-layout-tuning.md`, `10-avoiding-division.md`

## Yeh topic abhi kyun

`01`'s profile: book stage ~48% (rdtsc-inflated ~1.1–1.4 µs/tick) — pipeline
ka **sabse bada** slice. `39-ORDER-BOOK` ne isko standalone 3 baar banaya
aur measure kiya; yahan hum us result ko **pipeline ke andar** dekhte hain,
aur pipeline-specific simplification bhi.

---

## v0 book — `std::map<double, std::list> + std::map<id→...>`

`pipeline.hpp` `PipelineV0`:

```cpp
std::map<double, std::list<std::pair<u32,int>>, std::greater<double>> bids_;
std::map<double, std::list<std::pair<u32,int>>>                        asks_;
std::map<u32, double> id_px_;      // cancel: id -> price
std::map<u32, char>   id_side_;    // cancel: id -> side
```

Har add:
- `std::map` insert → **red-black tree**: `log n` node visits, har node ek
  **separate heap allocation** (`std::map` node = key + value + 3 pointers +
  color, `malloc`'d), RAM mein bikhre → har traversal step ~cache miss.
- `std::list::push_back` → **another** heap allocation per order.
- `id_px_[id] = ...`, `id_side_[id] = ...` → **two more** tree inserts +
  allocations.

Har cancel:
- 2 tree lookups (`id_px_`, `id_side_`), 1 more (`bids_`/`asks_.find`),
  **linear scan** `std::list` to find the id, `erase` (free), maybe erase
  map node (free).

Har signal tick: `bids_.begin()` / `asks_.begin()` — tree leftmost, cheap-ish,
par node cache-cold.

`39` ne yahi V1 banaya. Measured (39): correct, simple, **slow** — har op
`malloc` + pointer-chase.

**Pipeline rdtsc attribution:** book ~1065–1400 ns/tick.

---

## The progression (folder 39 ne measure kiya)

| Version | Design | 39's measured verdict |
|---|---|---|
| **V1** | `std::map` + `std::list` + `std::map` index | baseline; every op allocates + pointer-chases |
| **V2** | **sorted `std::vector`** of levels + index | **overall WORSE than V1** (genuine Rule-2 regression) — mid-book insert/erase = O(n) memmove; wins on top-of-book scan, loses on churn. Root-caused in `39/06`. |
| **V3** | **flat array** of price levels (index = price−base) + **intrusive** FIFO list per level + **flat hash** id→node | **wins every metric**: p99.9 ~2–3× vs V1, ~4× vs V2. O(1) add/cancel/top. |

**Sabak (39 se carry):** "obvious" optimization (contiguous vector) ne
measure pe ulta kiya. Sorted-vector ka O(n) mid insert HFT churn pattern
(constant add/cancel near touch) ke liye galat. **Measure, assume mat.**

---

## v3 book in `pipeline.hpp` — aur bhi aage (pipeline-specific)

Pipeline ko signal ke liye sirf **top-of-book** aur **per-level aggregate
qty** chahiye — har level pe per-order FIFO **nahi** (woh matching engine
ka concern, `40`). Toh V3 book aur simple + faster:

```cpp
std::array<std::int64_t, 4096> bid_qty_{};   // index = px_ticks - kBase; aggregate qty
std::array<std::int64_t, 4096> ask_qty_{};
std::vector<Loc> id_loc_;                     // id -> {level_idx, qty, side}; ids DENSE -> direct index
std::size_t best_bid_ = kNoBid, best_ask_ = kLevels;   // cached top-of-book
```

- **add**: `arr[idx] += qty` (one write), `id_loc_[id] = {...}` (one write,
  direct index — ids sequential from feed), update `best_*` if improved
  (one compare). **O(1), zero allocation.**
- **cancel**: `Loc l = id_loc_[id]` (one read, direct), `arr[l.idx] -= l.qty`
  (one write), re-walk `best_*` **only if** the touch level emptied. **O(1)
  amortized.**
- **top-of-book**: `best_bid_`, `best_ask_` — already cached, O(1). Re-walk
  is a bounded backward/forward scan over `int64` array (cache-friendly,
  sequential) only on touch-level depletion.

Design choices mapped to lessons:
- flat array index = `px - base` → `07` (no node chase), `10` (subtract not
  divide)
- dense-id direct-index vector → `39/09` (id lookup: array beats hash when
  ids dense) + `07` (SoA-ish: `Loc` is 12 bytes, tight)
- cached `best_*` → `39/11` (top-of-book fast path)
- aggregate-qty-per-level (no per-order list) → scope: signal doesn't need
  FIFO; matching does (`40`)

---

## Result — measured endpoints (`04_before_after.cpp`, is box)

```
                 book ns/tick (rdtsc attribution)     ratio
v0               ~1065 – 1400
v3               ~43 – 48                             ~24 – 29x
```

> v3 ~45 ns mein ~35 ns rdtsc-probe (`03`). True v3 book work < 10 ns/tick.

Combined with parse (`13`), poore pipeline (A): ~1.9–2.7 µs → ~25–45 ns/tick,
**~60–80×**.

---

## Explain — har change ne kya kiya

| Change | Mechanism | Effect |
|---|---|---|
| `std::map` node tree → flat `std::array` indexed by `px−base` | no `log n` traversal, no per-node cache miss, no `malloc` per level | biggest single win |
| `std::list` per level → aggregate `int64` (or intrusive list in `39` V3) | no `malloc` per order; add/cancel = one arithmetic op | removes allocator from hot path |
| `std::map<id→...>` index → dense `std::vector<Loc>` direct index | tree lookup (log n + misses) → one array load | cancel O(1), cache-hot |
| `begin()` scan → cached `best_bid_/best_ask_` | top-of-book O(1), re-walk only on depletion | signal stage cheap |
| `double` price key → `int64` tick index | exact, no FP compare, enables array indexing | correctness + speed |

---

## ⚠️ Traps / Common mistakes

### Trap 1 — flat array bina bounds / window
`idx = px_ticks - kBase` jahan `px` window (`kBase .. kBase+kLevels`) ke
bahar → OOB. `pipeline.hpp` ka feed clamp karta hai (`9000..10999` ticks,
window `4096`). Real book: dynamic re-basing (price band shift) ya a
"far levels" fallback map. `39/07`.

### Trap 2 — `std::vector` (V2) ko "obviously better than map" maan lena
`39` ne measure pe ulta paya. Contiguous scan wins, mid-insert `memmove`
loses on churn. **Workload's access pattern se decide.**

### Trap 3 — cached best-bid stale
`best_bid_` update karna bhoole ek code path pe (e.g. modify that moves a
level) → signal galat price pe. Invariant test: `best_bid_ == actual max
non-empty level` har op ke baad (debug build). `39/16`.

### Trap 4 — dense-id assumption toot gaya
Direct `id_loc_[id]` sirf tab jab ids compact (0..max). Exchange ids often
64-bit sparse → `std::vector` sized to max = TB. Tab flat hash (`39/09`) ya
per-session remap. Pipeline feed ids dense by construction.

### Trap 5 — aggregate-qty book se matching try karna
Pipeline V3 book per-level FIFO nahi rakhta → price-time priority match
**nahi** kar sakta. Woh `40`'s engine ka kaam. Sahi tool per sahi job.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| `std::map` "balanced tree, O(log n), fast" | Har node `malloc`'d, RAM-scattered → every step a miss |
| Sorted vector hamesha map se tez (cache) | `39/06`: churn-heavy pe O(n) memmove → overall worse |
| id→order ke liye hash map hi option | Dense ids → direct array index, ~5× (`39/09`) |
| Full per-order book hi "real" book | Signal ko aggregate + top kaafi; per-order = matching's need |

---

## Hands-on

```bash
./build.ps1 fast 39-ORDER-BOOK/examples/07_comparison_suite.cpp    # V1 vs V2 vs V3 measured
./build.ps1 fast 43-HFT-OPTIMIZATION/examples/04_before_after.cpp   # book ratio in-pipeline
```

---

## Exercises

1. `pipeline.hpp` V3 book ka `kLevels` 4096 se 256 kar do. Feed clamp
   `9000..10999` (2000 ticks). Kya hoga?
   <details><summary>Answer</summary>
   `idx = px_ticks - 9000` up to 1999 > 256 → **OOB write** — `bid_qty_`
   ke baad jo memory hai woh corrupt. UB. Window ko price range cover
   karna chahiye. (ASan / bounds-check build se pakdo.)
   </details>

2. `39` ne V2 (sorted vector) ko V1 se **worse** paya. Kaunsa operation
   aur kis workload pattern pe?
   <details><summary>Answer</summary>
   Mid-book **insert/erase** (naya price level touch ke paas, ya level
   empty hone pe erase) → `O(n)` `memmove` of all levels beyond it. HFT
   churn = constant add/cancel near touch → constant memmoves. V2 sirf tab
   jeetta jab book static + full scans common. `39/06`.
   </details>

3. Signal ko ab "top 5 levels ka total qty" chahiye (depth imbalance), sirf
   best nahi. V3 book pe cost?
   <details><summary>Answer</summary>
   `best_bid_` se 5 steps neeche walk: `bid_qty_[best_bid_] + bid_qty_
   [best_bid_-1] + ...` — 5 sequential `int64` reads from a hot array,
   ~cache-line-local, few ns. Flat array isme bhi achha (contiguous). `std::map`
   V0 mein yeh 5 tree-`--it` steps = 5 potential misses.
   </details>

---

## Interview questions

1. `std::map` order book ki 3 concrete costs (per op)?
2. `39` ne sorted-vector ko regression kyun paya? Root cause?
3. Flat-array book kab OOB? Kaise handle (re-base / fallback)?
4. id→order: hash map vs direct array — trade-off, kab kaunsa?
5. Pipeline ka aggregate-qty book matching kyun nahi kar sakta?

---

## Next
→ [`15-case-study-full-pipeline.md`](15-case-study-full-pipeline.md)
