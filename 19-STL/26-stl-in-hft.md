# 26 — The STL in HFT: what's used, what's replaced, and why "nothing" is common

## Prerequisites
- All of folder 19 (this is the synthesis lesson)
- Folder 17–18 (RAII, move), [`25-container-performance.md`](25-container-performance.md) especially

## Yeh topic abhi kyun
Poore folder mein har container/algorithm ke saath ek "HFT relevance" callout
tha. Ye lesson unhe **ek picture** mein jodta: kya STL se as-is use hota, kya
sirf idea use hota (implementation replace), aur kyun hot path pe aksar "koi
STL container nahi" hota. Ye folder 36–44 (low-latency + HFT track) ka bridge hai.

---

## The three tiers of an HFT system

```
   ┌─────────────────────────────────────────────────────────────┐
   │  CONTROL PLANE   (startup, config, admin, logging setup)     │  <- STL freely, allocation fine
   ├─────────────────────────────────────────────────────────────┤
   │  WARM PATH       (per-second housekeeping, stats, non-critical)│  <- STL with care: reserve, no per-op alloc
   ├─────────────────────────────────────────────────────────────┤
   │  HOT PATH        (packet in -> decision -> order out, ~1 µs)  │  <- almost no STL containers; flat, preallocated
   └─────────────────────────────────────────────────────────────┘
```

The rule of thumb: **an allocation, a cache miss per element, or an unbounded
worst case is fine in the control plane, risky in the warm path, and banned on
the hot path.**

---

## Used as-is (all tiers)

| Component | Why it survives the hot path |
|---|---|
| `std::vector` | Contiguous, `reserve`d once at startup → `push_back`/`clear`/refill with **zero** reallocation; `data()` handed to parsers/syscalls; best cache behaviour of anything |
| `std::array` | Compile-time size, stack/inline, no allocation, size is a constant the optimizer exploits |
| `std::span` / `std::string_view` | Non-owning `{ptr,len}` views — pass slices of a preallocated buffer with no copy |
| `<algorithm>` on contiguous ranges | `sort` (introsort — bounded worst case), `lower_bound`, `nth_element`, `partition`, `min/max_element` compile to tight vectorized loops |
| `<numeric>` — `transform_reduce`, `inner_product`, `partial_sum`, `adjacent_difference` | FMA-vectorized; VWAP, cumulative depth, inter-arrival deltas |
| `<chrono>` `steady_clock` | Interval timing; `rdtscp` for the very hottest |
| `<type_traits>`, `<bit>` | Compile-time — `is_trivially_copyable` gates memcpy fast paths; `popcount`/`countr_zero` for bitmask slot scanning; `bit_cast` for aliasing-safe reinterpret |
| `std::optional` / `std::variant` + `std::visit` | Allocation-free, contiguous; `variant` of message types + jump-table dispatch instead of a vtable |
| `std::unique_ptr` | Zero-overhead ownership for the objects that *are* heap-allocated (at startup) |

## Idea used, implementation replaced (hot path)

| STL thing | Replaced by | Reason |
|---|---|---|
| `std::map<Price, Level>` (order book) | **sorted `std::vector<Level>` + `lower_bound`**, or a **flat array indexed by ticks-from-reference** | tree = O(log n) cache misses; `front()`/`back()` give best bid/ask O(1); array indexing = 0 misses, 0 search |
| `std::unordered_map<Id, T>` | **fixed-capacity open-addressed table** (contiguous, no rehash, no per-node alloc), or **direct index** on dense id bits | `std::unordered_map` = mandated chaining → node alloc + rehash spikes + ~2 misses; flat table ~1 miss, deterministic |
| `std::list` (intrusive chains, LRU, free lists) | **intrusive linked list over a `std::vector`/pool arena**; "pointers" are indices | keeps O(1) splice/erase **and** cache locality; no per-node `malloc` |
| `std::queue` / ring buffer needs | **preallocated SPSC ring buffer** (`std::array` + head/tail atomics) | `std::queue` allocates deque chunks; the ring never touches the allocator |
| `std::function` callbacks | **template parameter**, **function pointer**, small **tag struct**, or `variant`+`visit` | `std::function` = indirect non-inlined call + possible `operator new` on assignment |
| `operator new` / default allocator | **arena** (bump + `reset()` per event) and **fixed-block pool** allocators, all reserved at startup; or `std::pmr` + `monotonic_buffer_resource` + `null_memory_resource()` upstream | default `new` is shared, locked, syscall-prone, tail-latency-spiky |
| `std::regex` | hand-written fixed-field parsing (`memchr`, `std::from_chars`) | regex = per-match allocation + backtracking + µs construction |
| `std::string` (in messages) | `std::string_view` into the receive buffer; fixed `char[N]` for owned short strings | avoid the heap + the copy |

## Often just "nothing"

A lot of hot-path "data structures" aren't containers at all:

- **Order book** — two `std::array<Level, MAX_LEVELS>` (bids, asks) indexed by
  `price_ticks - reference_ticks`, plus a `best_bid_idx` / `best_ask_idx`. Update
  = one array write. No search, no nodes, no allocation.
- **Working orders** — a `std::array<Order, MAX_ORDERS>` slab + a free-list
  bitmask; `allocate` = `countr_zero(free_mask)`, `free` = set the bit. O(1),
  contiguous.
- **Inbound queue** — a fixed ring buffer over a `std::array`, cache-line-aligned
  head/tail.
- **Symbol table** — perfect hash or a sorted `std::array` computed at startup;
  `lower_bound` at runtime.

Because everything is **fixed-capacity and preallocated**, steady-state
processing does no allocation, no rehash, no reallocation, no tree rebalance —
the latency distribution has almost no tail.

---

## When STL *is* the right call

- **Control plane / startup**: parsing config, building the symbol universe,
  wiring strategies, setting up logging, reading reference data — use
  `std::map`, `std::unordered_map`, `std::string`, `<filesystem>`, `std::regex`
  freely. It runs once; clarity beats nanoseconds.
- **Warm path** (per-second stats, position reconciliation, risk snapshots):
  STL with discipline — `reserve` up front, no per-iteration allocation, prefer
  `vector` + algorithms.
- **Backtesting / research / sim**: full STL + ranges + `std::pmr`. Throughput
  matters, single-digit-µs latency doesn't.
- **Everywhere**: the STL **iterator + algorithm model** and the **vocabulary
  types** (`optional`, `variant`, `span`, `string_view`) — those are kept even in
  hot code. It's the *node-based default containers* and *hidden allocations*
  that get swapped out.

---

## Andar kya hota hai — the pattern behind all of it

Every replacement above is the same move: **trade flexibility for a flat,
preallocated, contiguous layout with a bounded worst case.**

- Dynamic size → fixed capacity chosen from the known worst case.
- Scattered nodes → one contiguous array; links become indices.
- Runtime polymorphism (`std::function`, virtual) → compile-time
  (templates) or a closed `variant` with jump-table dispatch.
- General allocator → arena/pool reserved at startup, `reset()` per cycle.
- "Handle any input" (`std::regex`, `std::map` with any key) → "handle *our*
  input" (fixed parser, tick-indexed array).

The STL is built for the *general* case with good *average* behaviour. HFT needs
the *specific* case with good *worst-case* behaviour. That's the whole story.

> **HFT relevance:** this entire lesson is the HFT relevance. Practical takeaways
> for writing such code: (1) reach for `std::vector` + `<algorithm>` + `std::span`
> first, always. (2) Before any `std::map` / `std::unordered_map` / `std::list` /
> `std::function` on a measured hot path, have a benchmark that says it's fine, or
> replace it with the flat equivalent. (3) Preallocate everything at startup;
> assert no allocation in steady state (`null_memory_resource()` upstream, or a
> hooked `operator new` that aborts). (4) Keep the STL's *interfaces* (iterators,
> algorithms, vocabulary types) — they're free. See folders 36 (low latency) and
> 37–44 (order book, matching engine, market data, lock-free, memory pool, event
> pipeline).

---

## Hands-on

```bash
./build.ps1 fast 19-STL/examples/11_container_benchmark.cpp   # why node containers lose
./build.ps1 fast 19-STL/examples/02_map_vs_unordered.cpp      # sorted vector vs map vs unordered
./build.ps1 fast 19-STL/examples/10_pmr_demo.cpp              # zero-heap containers on a stack buffer
./build.ps1 fast 19-STL/examples/08_std_function_cost.cpp     # why not std::function on the hot path
```

Exercise: sketch (types only) a fixed-capacity order book with `std::array` bid/
ask level arrays indexed by tick offset, a `best_bid_idx`, and an update function
that does no allocation and no search.

---

## ⚠️ Traps

### Trap 1 — "the STL is slow" as a blanket statement
`std::vector` + `<algorithm>` on contiguous data is as fast as hand code. Only
**node containers, `std::function`, `std::regex`, and hidden allocations** are the
problem. Don't throw out the iterator/algorithm model.

### Trap 2 — replacing STL prematurely (in the control plane)
A hand-rolled hash map in config-loading code is wasted effort and more bugs.
Flat/custom structures earn their keep only on the measured hot path.

### Trap 3 — `std::vector` on the hot path without `reserve`
An unreserved `push_back` can reallocate → allocation + O(n) move + invalidated
references, mid-tick. Size it at startup.

### Trap 4 — assuming "contiguous" = "fast" for big elements
`std::vector<BigStruct>` where `BigStruct` is 1 KB: only a few per cache line.
Use indices/SoA.

### Trap 5 — keeping `std::map`/`std::unordered_map` "because it works in tests"
Tests don't show the tail latency from a rehash or the cache misses under a real
working set. Benchmark with production-shaped data.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "HFT doesn't use the STL" | It uses `vector`, `array`, `span`, `<algorithm>`, `<numeric>`, `<chrono>`, vocabulary types heavily — it replaces node containers, `std::function`, `std::regex`, and the default allocator on the hot path |
| "Replace all STL containers with custom ones" | Only on the measured hot path; control plane uses STL for clarity |
| "`std::unordered_map` is fine, it's O(1)" | Mandated chaining → node alloc + rehash spikes + cache misses; hot path uses a flat/open-addressed table |
| "The order book is a `std::map<price, level>`" | Conceptually; in practice a sorted vector or a tick-indexed flat array |
| "Preallocation is a micro-optimization" | It's the design — steady state must not call the allocator at all |

---

## Exercises

1. **Classify:** for each, say control-plane / warm / hot and what you'd use:
   (a) load `symbols.json` at boot, (b) update the book on every market-data
   tick, (c) compute per-minute VWAP, (d) match an incoming order against the
   book.

   <details><summary>Answer</summary>

   (a) control plane — `std::unordered_map`/`std::map` + `<filesystem>` + a JSON
   lib, allocation fine. (b) hot — tick-indexed `std::array` levels, no alloc, no
   search. (c) warm — `std::vector` of the minute's trades (reserved) +
   `std::transform_reduce`. (d) hot — walk the sorted/flat level array, O(1)
   `front()`, decrement quantities in place.
   </details>

2. **Replace a `std::map`:** an order book keyed by integer price ticks, prices
   within ±2000 ticks of a reference. Design the flat replacement and give the
   lookup/update cost.

   <details><summary>Answer</summary>

   `std::array<Level, 4001> levels;` indexed by `tick - reference + 2000`. Lookup
   / update = one bounds check + one array access → **O(1), 0 cache misses**
   (hot region stays in L1), no allocation, no rebalance. Track `best_bid_idx` /
   `best_ask_idx` incrementally. Re-center (shift) rarely, off the hot path.
   </details>

3. **Why not `std::function` for a per-tick strategy callback?** and what to use
   for exactly-4 strategies chosen at runtime.

   <details><summary>Answer</summary>

   `std::function` = an indirect, non-inlined call every tick, plus a possible
   `operator new` whenever the callback is reassigned. Use `std::variant<StratA,
   StratB, StratC, StratD>` selected once + `std::visit` (or a `switch` on
   `.index()`) — no allocation, jump-table dispatch, each `operator()` inlinable
   in the visitor.
   </details>

4. **The common pattern:** state the single trade-off that every hot-path STL
   replacement in this lesson makes.

   <details><summary>Answer</summary>

   Give up dynamic flexibility (arbitrary size, arbitrary key types, runtime
   polymorphism, general allocation) in exchange for a **fixed-capacity,
   preallocated, contiguous layout with a bounded worst case** — so steady state
   has no allocation, no rehash/realloc/rebalance, and minimal cache misses.
   </details>

5. **Assert no allocation:** how do you *enforce* that a hot-path function does no
   heap allocation, in a test?

   <details><summary>Answer</summary>

   Back its containers with `std::pmr` + a `monotonic_buffer_resource` whose
   upstream is `std::pmr::null_memory_resource()` (throws on overflow), or install
   a global `operator new` that `std::abort()`s (or sets a flag the test checks)
   for the duration of the hot-path call. Run it under load in CI.
   </details>

---

## Interview questions

1. HFT system ke 3 tiers — kaunse tier pe STL free, kaunse pe banned?
2. Kaunse STL cheezein hot path pe as-is chalti hain (5+)?
3. `std::map` order book ke liye kya replace karta, kyun?
4. `std::unordered_map` ki jagah flat/open-addressed table kyun?
5. Har hot-path replacement ka common trade-off kya (ek line)?
6. STL ka kaunsa hissa hot code bhi rakhta hai (model + vocabulary types)?

---

## Next
→ [`27-exercises.md`](27-exercises.md)
