# 17 — Cache-aware data structures

## Prerequisites
- All prior files (you need to know the "textbook" structures first)
- Folder 07 (cache locality), folder 19 files 25–26 (container perf, STL in HFT)

## Yeh topic abhi kyun
Yeh folder ka **thesis lesson**. Ab tak har structure ka Big-O dekha; yahan
dekhte hain ki real hardware pe **memory layout Big-O ko kaise harata hai**, aur
HFT-grade structures kaise dikhte. `examples/08_flat_vs_pointer.cpp`: same `O(n)`
tree traversal, flat array **4–11× faster** than a pointer tree.

---

## The hardware reality

| access | latency (approx) |
|---|---|
| register | 0 (in the pipeline) |
| L1 cache hit | ~4 cycles |
| L2 hit | ~12 cycles |
| L3 hit | ~40 cycles |
| **DRAM (cache miss)** | **~200–300 cycles** |
| a branch mispredict | ~15–20 cycles |

A modern core retires ~4 instructions/cycle. So **one cache miss ≈ 800–1200
instructions of lost work**. Performance on data-structure-heavy code is
`≈ (number of cache misses)`, **not** `(number of operations)`.

Cache lines are **64 bytes**. A miss pulls a whole line. Two things exploit this:
- **Spatial locality** — if you'll touch nearby bytes soon, one miss covers them
  all (16 `int`s per line).
- **Prefetching** — the hardware detects sequential/strided access and pulls the
  next lines *before* you ask → sequential scans hide their misses.

Pointer chasing defeats both: the next address is unknown until the current load
completes (a **dependency chain**), and it's unpredictable → a miss per hop, no
overlap.

---

## Measured: layout beats Big-O

### `examples/03_linked_list.cpp` — `O(n)` vs `O(n)`
| structure | sum 5M elements |
|---|---|
| `std::vector<int>` | ~1.7 ms |
| singly linked list | ~70 ms — **~40×** |

### `examples/08_flat_vs_pointer.cpp` — `O(n)` tree traversal, two layouts
| layout | sum ~4.2M nodes |
|---|---|
| flat array (`2i+1`/`2i+2`), linear scan | ~2.7 ms |
| flat array, recursive | ~6.6 ms |
| pointer tree, recursive | ~30.4 ms — **4–11×** slower |

### `19-STL/examples/11` — same operation, five containers (N=1M, iterate)
`vector` 0.67 ms · `deque` 2.28 · `list` 18.0 · `set` 197 · `unordered_set` 66.6.

Identical Big-O in every row of every table. The difference is entirely cache
behaviour.

---

## The transforms: textbook → cache-aware

### 1. Pointers → indices into a contiguous arena
`Node* next` becomes `uint32_t next` indexing a `std::vector<Node>`. One
allocation instead of `N`; nodes are cache-adjacent; links are half the size
(more per line). Keeps `O(1)` splice. (File 06's arena linked list; file 09's
flattened BST.)

### 2. Node tree → implicit array tree
A complete binary tree needs no pointers: child of `i` is `2i+1`/`2i+2`. This is
what a **binary heap** already is (file 10) — and why `std::priority_queue`
beats a `std::set`-based PQ. Applies to segment trees, Fenwick trees, and
static search trees (**Eytzinger layout**: store a BST in BFS order in an array →
binary search with prefetch, ~2× a sorted-array `lower_bound` on large data).

### 3. Array-of-Structs → Struct-of-Arrays (SoA)
If a loop touches only 2 of a struct's 10 fields, AoS wastes ~80% of every cache
line it loads. SoA (`std::vector<float> px; std::vector<uint32_t> qty; …`) packs
each field densely → the loop streams exactly what it needs, and it vectorizes.
(Folders 09, 11, 32 — measured 1.6–4×.)

### 4. `std::map`/`std::set` → sorted `std::vector` + binary search
Same `O(log n)`, ~2 cache misses instead of `log n` scattered node misses → ~3×
(folder 19 file 25). `front()`/`back()` give min/max in `O(1)`. For a bounded
key range, go further: a **flat array indexed by the key** (price ticks from a
reference) → `O(1)`, zero misses beyond one load (folder 19 file 26).

### 5. `std::unordered_map` → open-addressed flat table
All entries in one array, linear probing → ~1 cache line per lookup, no per-node
alloc, no rehash spike (file 08, folder 19 file 06). Or a **direct array index**
when the key is a dense small integer.

### 6. `std::list` → intrusive list over a slab / a ring buffer
Order queues, LRU chains, free lists → intrusive links inside preallocated slots
(file 06). Bounded FIFOs → power-of-two ring buffer (file 07).

### 7. B-tree-style high fan-out
When you genuinely need a balanced tree, pack **many keys per node** (a
cache-line's worth) so each miss does more work → `log_B n` misses instead of
`log₂ n`. Database indexes, `absl::btree_map`.

---

## The cost of the transform

Cache-aware structures trade **flexibility** for **layout**:
- Fixed / bounded capacity (chosen from the worst case) instead of unbounded
  growth.
- Reordering/compaction cost on some operations (swap-and-pop, tombstone
  cleanup, arena re-center).
- Index links instead of pointers → less type safety, manual lifetime.
- More code, more invariants to test.

Worth it on a measured hot path; **not** worth it in control-plane code where a
`std::map` reads clearly and runs once.

---

## Andar kya hota hai

- **Dependency chains**: `p = p->next; use(*p);` — the CPU can't compute the next
  address until the current load retires, so out-of-order execution can't hide
  the ~200-cycle miss. A `v[i++]` loop knows every address immediately → the
  load-store unit runs ahead, the prefetcher streams, multiple iterations overlap.
- **Prefetcher**: recognizes `+64`, `+128`, … (and simple strides). Contiguous
  structures ride it for free. Random / pointer access gets nothing;
  `__builtin_prefetch` can help *if* you know the next address early enough
  (e.g. prefetch both children before recursing).
- **TLB**: each 4 KB page needs a TLB entry; a structure scattered over many
  pages also thrashes the TLB (~10–100 cycles per miss). Contiguous + huge pages
  minimizes this (folder 29).
- **Vectorization**: SIMD needs contiguous, aliasing-free data. A `std::vector`
  sum auto-vectorizes to `paddd`; a `list` sum can't — the loads aren't a
  packable pattern.
- Node size matters: `std::list<int>` node ≈ 24–32 B for 4 B of payload → 6–8×
  the memory traffic and cache pressure of `std::vector<int>`.

> **HFT relevance:** this lesson *is* the HFT data-structure philosophy (folder
> 19 file 26, folders 32, 39). The hot path is **flat**: `std::vector` +
> `<algorithm>` + `std::span`; sorted arrays / tick-indexed arrays instead of
> `std::map`; open-addressed or direct-index tables instead of
> `std::unordered_map`; intrusive lists over an arena instead of `std::list`;
> ring buffers instead of `std::queue`. Everything **preallocated at startup** so
> steady state never allocates, never rehashes, never rebalances — the latency
> distribution has almost no tail. Big-O rules out the disasters; **cache
> behaviour, branch predictability, and bounded worst case decide the winner.**

---

## Hands-on

```bash
./build.ps1 fast 20-ALGORITHMS-DSA/examples/08_flat_vs_pointer.cpp
./build.ps1 fast 20-ALGORITHMS-DSA/examples/03_linked_list.cpp
./build.ps1 fast 19-STL/examples/11_container_benchmark.cpp
```

Then rebuild file 06's linked-list sum benchmark as an **index-linked arena list**
and measure how much of the 40× gap closes. Build a sorted-vector "map" and time
`lower_bound` against `std::map::find` for N = 200,000 (expect ~3×).

---

## ⚠️ Traps

### Trap 1 — choosing a structure by Big-O alone
```cpp
// "list insert is O(1), vector insert is O(n) -> use list". For n up to thousands, vector wins on cache + no alloc.
```

### Trap 2 — AoS when the hot loop uses 2 of 12 fields
```cpp
struct Q { double px; uint32_t qty; /* +10 more fields */ };
for (auto& q : v) total += q.px * q.qty;   // ⚠️ loads whole 100+ B struct per element. SoA -> stream px and qty only
```

### Trap 3 — flattening but keeping random access patterns
```cpp
// A flat array you index randomly still misses per access -- flattening helps SEQUENTIAL / predictable traversal most.
```

### Trap 4 — over-engineering the control plane
```cpp
// A hand-rolled open-addressed map for config loading = wasted effort + bugs. std::map/unordered_map there.
```

### Trap 5 — ignoring the worst case after optimizing the average
```cpp
// A flat structure with an occasional O(n) compaction (tombstone cleanup, re-center) can still spike p99.9. Bound it.
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Same Big-O ⇒ same speed" | `list` vs `vector` iterate: both `O(n)`, ~40× apart (cache) |
| "Cache-aware means micro-optimizing loops" | It's a **data-layout** decision — the 10–100×, made before the loop |
| "Flattening always helps" | Helps most for sequential/predictable traversal; random access still misses |
| "A `std::map` is fine, it's `O(log n)`" | `log n` scattered node misses; a sorted vector is ~3× for the same `O(log n)` |
| "Replace every STL container" | Only on the measured hot path; control plane stays STL for clarity |

---

## Exercises

1. **Miss budget:** a lookup does 20 pointer hops (a balanced tree, `n ≈ 10⁶`),
   each a cache miss at ~250 cycles. How many "wasted" instruction-slots is that
   on a 4-IPC core? Compare to a sorted-array binary search (~2 misses).

   <details><summary>Answer</summary>

   `20 × 250 = 5000` cycles ≈ `20,000` instruction-slots lost. Sorted array:
   `~2 × 250 = 500` cycles ≈ 2,000 slots. Same 20 comparisons, ~10× the stall for
   the tree.
   </details>

2. **AoS → SoA:** a loop computes `Σ px[i]·qty[i]` over 1M `Quote` structs where
   `sizeof(Quote) == 96` but `px`+`qty` are 12 bytes. Bytes of memory traffic
   AoS vs SoA?

   <details><summary>Answer</summary>

   AoS: `1M × 96 = 96 MB` streamed (whole struct per element). SoA: `1M × 8`
   (px) `+ 1M × 4` (qty) `= 12 MB`. ~8× less traffic → ~8× fewer misses, and SoA
   vectorizes.
   </details>

3. **Eytzinger:** why does storing a static search tree in BFS order in an array
   speed up binary search vs a plain sorted array?

   <details><summary>Answer</summary>

   In BFS (Eytzinger) layout, the children of the element you just compared are at
   `2i` and `2i+1` — adjacent in memory and **prefetchable before you know which
   branch you'll take**. A plain sorted array's next probe is `n/4` away —
   unpredictable, no prefetch. Fewer effective misses on large arrays.
   </details>

4. **When NOT to:** you're writing the config loader that reads `symbols.json`
   once at startup into a name→id map. Flat open-addressed table or
   `std::unordered_map`?

   <details><summary>Answer</summary>

   `std::unordered_map` (or `std::map`). It runs once, latency is irrelevant, and
   the standard container is clearer and less bug-prone. Cache-aware structures
   are for the measured hot path.
   </details>

5. **Arena list gap:** file 06's benchmark: pointer list sum ~70 ms, vector sum
   ~1.7 ms. Predict where an *index-linked arena list* sum lands and why.

   <details><summary>Answer</summary>

   Somewhere in between — closer to the vector. The slots are contiguous (one
   allocation, cache-adjacent, often prefetchable) so most of the per-node miss
   goes away; but you still chase `pool[i].next` indices, so if the list order
   doesn't match memory order you lose some prefetch. Typically a few ms, not 70.
   </details>

---

## Interview questions

1. "Performance ≈ cache misses, not operations" — ek cache miss kitne
   instruction-slots ka nuksaan?
2. Pointer chasing prefetch aur out-of-order execution ko kyun defeat karta?
3. `list` vs `vector` iterate — dono `O(n)`, 40× kyun?
4. Pointer tree → implicit array tree — kya milta, kya kho jaata?
5. AoS → SoA kab, kitna fark (memory traffic)?
6. `std::map` → sorted-`vector` + binary search — same `O(log n)`, 3× kyun?

---

## Next
→ [`18-problem-sets.md`](18-problem-sets.md)
