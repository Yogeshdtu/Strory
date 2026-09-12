# 25 — Container performance: the full table, cache behaviour, benchmarks

## Prerequisites
- [`02-vector-deep.md`](02-vector-deep.md)–[`07-container-adapters.md`](07-container-adapters.md) (every container)
- Folder 07 (cache locality), [`17-chrono.md`](17-chrono.md) (measuring), folder 33 preview (cache)

## Yeh topic abhi kyun
Ab tak har container alag-alag dekha. Ye lesson sabko **ek jagah** rakhta —
complexity table, aur usse zyada important, **measured** numbers is repo ke
`examples/11` aur `examples/02` se. Kyunki Big-O jhooth bolta hai jab constant
factor 30x ka ho: `std::list` aur `std::vector` dono O(n) iterate karte, par list
27x slower hai. Ye "cache" wali baat number ke saath samajhni hai.

---

## Asymptotic complexity (the theory)

| Operation | `vector` | `deque` | `list` | `map`/`set` | `unordered_map`/`set` |
|---|---|---|---|---|---|
| index `[i]` | O(1) | O(1) | — | — | — (by key: O(1) avg) |
| `push_back` | O(1)† | O(1) | O(1) | — | — |
| `push_front` | O(n) | O(1) | O(1) | — | — |
| insert/erase middle | O(n) | O(n) | O(1)‡ | O(log n) | O(1) avg |
| find by value/key | O(n) | O(n) | O(n) | O(log n) | O(1) avg |
| iterate all n | O(n) | O(n) | O(n) | O(n) | O(n) |
| memory / element | `sizeof(T)` | ~`sizeof(T)` | `sizeof(T)`+2ptr+hdr | `sizeof(T)`+3ptr+hdr | `sizeof(T)`+1ptr+hdr + bucket array |

† amortized (geometric growth). ‡ given the iterator; finding the position is O(n).

**This table is necessary but badly misleading on its own** — it counts
operations, not cache misses. Below is what actually runs.

---

## Measured — `examples/11_container_benchmark.cpp`, N = 1,000,000, `-O2`, this box

| container | iterate (ms) | build (ms) | membership lookup |
|---|---|---|---|
| `vector` | **0.67** | **2.86** (reserved) | `std::find` linear: ~0.28 ms **per lookup** (O(n) — don't) |
| `deque` | 2.28 | 4.09 | — |
| `list` | 18.02 | 86.43 | — |
| `set` (RB-tree) | 197.02 | 1616.95 | 1065.74 ms for all N (`count`) |
| `unordered_set` | 66.61 | 230.48 (reserved) | **49.30 ms** for all N (`count`) |

Ratios that matter:
- **iterate**: `list` is **27x** slower than `vector`; `set` **~290x**; even
  `deque` **3.4x**. Same O(n), wildly different constants.
- **build**: `list` **30x**, `set` **560x**, `unordered_set` **80x** slower than
  a reserved `vector` — node allocation + tree rebalancing / hashing.
- **membership**: `unordered_set` **~22x** faster than `set` for the same N
  lookups — hash O(1) + ~1 cache miss vs tree O(log n) + ~log n misses.

## Measured — `examples/02_map_vs_unordered.cpp`, N = 200,000, `-O2`, this box

| structure | ns / lookup |
|---|---|
| `std::map` | **1049** |
| sorted `std::vector` + `std::lower_bound` | **357** |
| `std::unordered_map` (reserved) | **113** |

Both `map` and the sorted vector are O(log n); the vector is **3x faster** purely
because binary search over contiguous memory touches ~2–3 cache lines while the
tree touches ~18 scattered nodes.

---

## Why: the cache is the whole story

A modern core does ~4 instructions/cycle but stalls ~**200–300 cycles** on a DRAM
miss. So performance ≈ (number of cache misses), not (number of operations).

| Access pattern | Misses | Containers |
|---|---|---|
| Sequential over contiguous memory | ~1 per **cache line** (64 B → 16 ints), hardware prefetcher hides even that; auto-vectorizes | `vector`, `array`, sorted-vector scan |
| Chunked contiguous | ~1 per chunk boundary + prefetch restart | `deque` |
| Pointer chase to random heap addresses | ~1 **per element** (prefetcher can't predict) | `list`, `forward_list`, `map`, `set`, `unordered_*` nodes |
| Random index into a big array | ~1 per access, but no per-element overhead | `vector` with random `[i]` |

`list`/`map`/`set` nodes are separate `malloc`s scattered across the heap →
`++it` dereferences an unpredictable address → a miss almost every step. That's
the 27x–290x. `unordered_map` is "only" ~1–2 misses per lookup (bucket array +
first node) which is why it's the least-bad node container.

Per-element **memory** compounds it: `list<int>` uses ~24–32 B/elem vs 4 B for
`vector<int>` → 6–8x the footprint → 6–8x the cache pressure → fewer of your
*other* working-set lines survive.

---

## The decision procedure

1. **Default to `std::vector`.** Contiguous, minimal overhead, every algorithm,
   best cache behaviour. `reserve()` if you can bound the size.
2. Need **key → value, order irrelevant, fastest lookup**? →
   `std::unordered_map` **with `reserve`** (or a flat hash map — file 06).
3. Need **sorted iteration / range queries / predecessor**? → sorted
   `std::vector` + `lower_bound` first; `std::map`/`std::set` only if you also
   need cheap arbitrary insert/erase **and** iterator stability.
4. Need **cheap push_front + push_back**? → `std::deque`.
5. Need **iterator/reference stability across arbitrary insert/erase + O(1)
   splice**, rare iteration? → `std::list`. (Rare.)
6. **Fixed compile-time size, no heap**? → `std::array`.
7. **Small N (≤ ~64)?** A linear scan over a `std::vector` beats a hash map and a
   tree — no hashing, no pointer chase, fully prefetched/vectorized. Measure.

Then **measure with real data and access patterns** (`examples/11` style:
warm-up, `-O2`, min of reps, sink). The right answer shifts with `sizeof(T)`, N,
read/write ratio, and how sorted/clustered the keys are.

---

## Andar kya hota hai

- `vector` iterate: `for (int x : v) s += x;` → a unit-stride load loop; libstdc++
  + `-O2` emits SIMD (`paddd` over 4–8 ints) and the prefetcher runs ahead. ~1
  element per <1 cycle amortized → 1M ints in ~0.5 ms.
- `list` iterate: `it = it->next; s += it->value;` — a load of `next` (miss), a
  load of `value` (often same line as `next` if the node is small, so ~1 miss/
  node). 1M misses × ~200 cycles ≈ tens of ms → the measured 18 ms.
- `set` iterate: in-order successor walk — `it->right` then leftmost, or up via
  `parent`. More pointer loads per step than a list, nodes bigger and more
  scattered → ~290x.
- `map` lookup vs sorted-vector `lower_bound`: identical `⌈log2 n⌉` comparisons.
  Vector: the compared elements are at `base + n/2`, `base + n/4`, … — the last
  ~4 steps land in one or two cache lines. Tree: each compared node is a distinct
  heap allocation → ~`log n` independent misses.
- `unordered_map` lookup: hash (a few cycles for int; a byte loop for string),
  `% bucket_count` (or `& mask`), load bucket head (miss #1), load first node
  (miss #2), compare. ~2 misses regardless of n → flat ~113 ns.

> **HFT relevance:** this lesson *is* the HFT container philosophy. Latency is
> dominated by cache misses, so the hot path is **flat**: `std::vector` +
> algorithms + `std::span`, sorted arrays instead of `std::map`, open-addressed
> or direct-indexed tables instead of `std::unordered_map`, intrusive lists over
> an arena instead of `std::list`. Everything sized and `reserve`d at startup so
> steady state never allocates. `std::map`/`std::unordered_map`/`std::list` live
> in the control plane (config, session state) where a few hundred ns and a
> `malloc` don't matter. When someone proposes a node container on the hot path,
> the answer is a measured benchmark like `examples/11`. See folder 26 and folder
> 39.

---

## Hands-on

```bash
./build.ps1 fast 19-STL/examples/11_container_benchmark.cpp
./build.ps1 fast 19-STL/examples/02_map_vs_unordered.cpp
```

Reproduce the tables above. Then change `N`, change the element from `int` to a
32-byte struct, and re-run — watch the vector's lead shrink (bigger elements =
fewer per line) but the node containers stay bad. Try `unordered_map` **without**
`reserve` and see the build time jump.

---

## ⚠️ Traps

### Trap 1 — choosing by Big-O alone
```cpp
// "insert/erase middle is O(1) for list, O(n) for vector -> use list"
// ⚠️ for N up to thousands, vector's O(n) memmove beats list's O(1)-but-cache-cold + node alloc. Measure
```

### Trap 2 — `std::map` because "I need fast lookups"
```cpp
std::map<int, V> m;   // ⚠️ O(log n) with ~log n cache misses. unordered_map (reserved) or sorted vector is several x faster for pure lookup
```

### Trap 3 — benchmarking at `-O0`
```cpp
// vector's advantage (SIMD, prefetch, inlined iterators) largely vanishes at -O0. Always ./build.ps1 fast
```

### Trap 4 — `unordered_map` without `reserve` in a bulk load
```cpp
for (...) m[k] = v;   // ⚠️ ~log(n) rehashes, each O(size). m.reserve(n) -> one allocation (build time ~halved here)
```

### Trap 5 — ignoring `sizeof(T)` and element count
```cpp
// A vector of 1 KB structs isn't "cache friendly" just because it's contiguous -- only ~64 fit per 64 KB of L1.
// Store indices / pointers to big objects, or a struct-of-arrays layout
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Same Big-O ⇒ same speed" | `list` and `vector` both iterate O(n); `list` is ~27x slower (cache) |
| "`std::map` is the fast lookup container" | Tree → O(log n) + ~log n misses; `unordered_map`/sorted-vector beat it several x |
| "Linked lists are good when you insert a lot" | Node alloc + cache-cold relink; `vector` wins up to thousands of elements |
| "`unordered_map` is contiguous" | Bucket array is; nodes are scattered `malloc`s (~1–2 misses/lookup) |
| "Micro-optimize the loop" | Pick the data structure / layout first — that's the 10–100x, not the loop body |

---

## Exercises

1. **Explain the 27x:** `list` and `vector` both "walk N elements". Give the
   per-element cost of each in terms of cache misses and instruction-level
   parallelism.

   <details><summary>Answer</summary>

   `vector`: contiguous → hardware prefetch + SIMD → ~<1 cycle/element amortized,
   ~1 miss per 16 ints. `list`: `++it` loads `next` from an unpredictable heap
   address → a cache miss (~200+ cycles) almost every element, no prefetch, no
   vectorization, and the dependent load serializes iterations. → ~27x.
   </details>

2. **map vs sorted vector:** both O(log n) for lookup; the vector measured 3x
   faster (`examples/02`). Why, precisely?

   <details><summary>Answer</summary>

   Same number of comparisons (`⌈log2 n⌉`). Binary search over a contiguous array
   touches elements that, for the last several steps, share one or two cache
   lines → ~2–3 misses total. The tree's compared nodes are separate heap
   allocations → ~`log2 n` (≈18 for n=200k) independent cache misses.
   </details>

3. **Pick and justify:** (a) 10M price updates/sec, need best bid/ask + price
   range scans. (b) orderId → order object, ~100k live, pure lookup. (c) 8
   feature flags read once per message.

   <details><summary>Answer</summary>

   (a) sorted `std::vector<Level>` (or a tick-indexed array) — `front()`/`back()`
   O(1), `lower_bound` for ranges, contiguous. (b) `unordered_map` with
   `reserve(128k)`, or a flat/open-addressed map, or direct index on orderId
   bits. (c) `std::array<bool,8>` or a bitmask — linear/bit test, no container
   machinery.
   </details>

4. **When does vector lose?** name two situations where `std::vector` is the
   wrong default despite the cache advantage.

   <details><summary>Answer</summary>

   (1) You need **iterator/reference stability** while inserting/erasing
   elsewhere (holding pointers to elements) — a realloc or shift invalidates
   them; use a node container or `vector<unique_ptr<T>>` / indices. (2) Frequent
   `push_front` / middle insert at **large** N where the O(n) shift genuinely
   dominates — `deque` or a different structure.
   </details>

5. **Design the benchmark:** you must decide `std::unordered_map` vs a flat hash
   map for a real workload. What do you measure and how (list the methodology
   points)?

   <details><summary>Answer</summary>

   Use the **real** key type and distribution, real N, real read/write mix and
   order. `-O2`. Warm up (caches, branch predictor). Time only the operation
   (exclude setup / key generation). Repeat, report the **min** (and p99 for
   tail). Sink results so the optimizer can't delete them. Measure memory too.
   Test with and without `reserve`.
   </details>

---

## Interview questions

1. Big-O same hone par bhi `list` `vector` se 27x slow kyun (iterate)?
2. Cache miss ka cost (~cycles), aur "performance ≈ misses" kaise?
3. `map` vs sorted-vector+`lower_bound` — dono O(log n), vector 3x fast kyun?
4. Node containers (list/map/set) ka per-element memory overhead — kya asar?
5. Container chunne ka decision order — default kya, kab deviate?
6. Small N (≤ ~64) pe linear `vector` scan hash-map ko kyun harata?

---

## Next
→ [`26-stl-in-hft.md`](26-stl-in-hft.md)
