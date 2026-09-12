# 08 — Cache-friendly data structures: flat vs pointer-based

## Prerequisites
- `05-locality.md`, `04-cache-misses.md` (MLP, dependent misses)
- `19-STL/` aur `20-ALGORITHMS-DSA/` (containers, trees, hashing)

## Yeh topic abhi kyun
Data structure ka asli cost woh nahi jo textbook big-O batata — woh **kitne
cache misses** woh generate karta hai, aur woh misses **dependent** hain ya
nahi. `std::list` aur `std::vector` dono O(n) traverse — par list har node
ek dependent DRAM miss, vector ~1 miss / 16 elements. Yeh lesson containers
ko "miss count + dependency" ke lens se dekhna sikhata.

---

## Golden rule: pointer chase = serial DRAM misses

```cpp
for (Node* n = head; n; n = n->next)  sum += n->val;
```

`n = n->next` **dependent** hai — agla address abhi wale load ke result se
aata. Zero MLP (lesson 04). Har node jo cache mein nahi = full DRAM latency
(~90 ns is box, example `08` curve B). 1M nodes = ~90 ms.

Same 1M ints ek `std::vector` mein: contiguous, prefetcher-fed, ~0.3 ns/int
= ~0.3 ms. **~300x.** Same big-O.

**Har data structure ko poochho: traversal/lookup mein kitne pointer hops,
aur kya woh hops serial hain?**

---

## Container-by-container

### `std::vector<T>` — the default, and usually right
- Contiguous. Sequential scan: ~1 miss / (64/sizeof(T)) elements, prefetcher
  helps. Random index: 1 miss, but **independent** across iterations → MLP.
- `push_back` amortized O(1), occasional realloc+copy.
- **Yehi use karo** jab tak koi specific reason na ho.

### `std::deque<T>`
- Chunks of elements (libstdc++: 512 B chunks) + a map of chunk pointers.
  Sequential scan mostly contiguous (within chunk), chunk boundary pe ek
  indirection. Decent locality, worse than vector.

### `std::list<T>` / `std::forward_list<T>`
- Har node alag allocation. `next`/`prev` (8-16 B) + `T` + malloc header
  (~16 B). Traversal = dependent misses. **Hot path se hataao.**
- Sirf tab jab: (a) huge elements jinhe move karna mehnga, (b) stable
  addresses / iterators chahiye across insert/erase, (c) O(1) splice.

### `std::map` / `std::set` (red-black tree)
- Har node alag alloc, 3 pointers + color + `T`. Lookup = ~log₂(N) **dependent**
  hops (har hop ek possible miss). N=1M → ~20 hops → potentially ~20 misses
  serial. Ordered iteration = pointer chase.
- Alternative: **sorted `std::vector` + `std::lower_bound`** — same O(log N)
  comparisons, par contiguous memory, ~log₂(N/16) misses, prefetcher-friendly
  binary search steps. 2-10x faster lookups, way faster iteration. Cost:
  insert O(N) (shift). Read-heavy → sorted vector jeetta.

### `std::unordered_map` / `std::unordered_set` (chaining)
- Bucket array (contiguous) + **linked list per bucket** (nodes alag alloc).
  Lookup: bucket index (1 miss) → first node (1 miss, random alloc) →
  collisions → more dependent misses. ~2-3 misses/lookup typical.
- Alternative: **open-addressing hash** (`absl::flat_hash_map`, `boost::
  unordered_flat_map`, robin-hood). Ek contiguous slot array. SIMD control-
  byte probing (Swiss table) → ~1 miss/lookup, no per-node alloc, better
  iteration. 2-4x faster, less memory. Cost: erase thoda complex, iterator
  invalidation on rehash.

### B-tree / B+-tree (`absl::btree_map`)
- Har node mein **kai** keys (ek ya do cache lines bharke) → fanout ~16-64 →
  tree depth ~log_fanout(N) = N=1M → ~3-4 levels → ~3-4 misses/lookup vs
  RB-tree's ~20. Ordered, cache-conscious. Insert/erase O(log N) with small
  shifts within a node.
- Best "ordered map" for large N when you also mutate.

---

## Node count → miss count (N = 1,000,000)

| Structure | misses / lookup (approx) | dependent? | iteration |
|---|---|---|---|
| sorted `vector` + `lower_bound` | ~log₂(1M) − log₂(16) ≈ **16** | mostly (binary search) | sequential ✅ |
| `std::map` (RB tree) | ~**20** | fully serial | pointer chase ❌ |
| `absl::btree_map` | ~**3-4** | serial but few | semi-sequential |
| `std::unordered_map` (chaining) | ~**2-3** | serial | scattered ❌ |
| `absl::flat_hash_map` (open addr) | ~**1** | — | contiguous-ish ✅ |
| `std::vector` scan (find) | N/16 total, **independent** | no (MLP) | sequential ✅ |

Binary search on a sorted vector still touches scattered lines (first probe
middle, then quarter…) — for very hot small-to-medium sets, a **linear scan**
of a tiny sorted array (≤ a few cache lines) beats binary search: fully
sequential, branch-predictable, SIMD-able. "log N" loses to "n with perfect
locality" for small n.

---

## Arena / pool allocation — fixing node-based structures

Agar aapko ek tree/list chahiye hi (ordering + cheap insert), nodes ko ek
**contiguous arena** se allocate karo:

```cpp
std::vector<Node> arena;          // reserve upfront
Node* alloc() { return &arena.emplace_back(); }
// "pointers" ki jagah uint32 indices into arena -> 4 B not 8, aur
// arena mein nodes ~insertion order mein -> traversal locality
```

Isse: (a) nodes paas-paas → traversal semi-sequential, (b) index = 4 B (half
of pointer) → node chhota → zyada per line, (c) no malloc per node. Yeh
"structure-of-arrays for a tree" hai — game engines aur DB indexes yahi karte.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `std::list` "kyunki insert/erase O(1)"
O(1) *agar aapke paas already iterator ho*. Use paane ke liye traverse =
O(n) of dependent misses. Aur real workload mein woh O(1) splice kabhi use
hi nahi hota. Default `vector` + swap-and-pop.

### Trap 2 — `std::map` for a hot lookup
~20 serial misses/lookup. Agar ordered chahiye → `absl::btree_map` ya sorted
vector. Agar nahi → `flat_hash_map`.

### Trap 3 — `std::unordered_map` "O(1) to fast hai"
O(1) *amortized comparisons*, par ~2-3 misses/lookup + per-node malloc +
poor iteration. Open-addressing alternative ~1 miss.

### Trap 4 — pointers jahan indices chalte
`Node* children[8]` → 64 B. `uint32 children[8]` → 32 B (half a line saved,
2x fanout per line). Agar aapka arena < 4 billion nodes, index use karo.

### Trap 5 — `std::vector<std::vector<T>>` for a matrix / grid
Har inner vector alag heap alloc, rows memory mein bikhri. `std::vector<T>`
of size `rows*cols` + manual indexing (`m[i*cols + j]`) — ek allocation,
contiguous, prefetcher-friendly.

### Trap 6 — small-buffer types bhoolna
`std::string` (SSO), `boost::small_vector`, `absl::InlinedVector` — chhote
sizes ke liye inline storage, no heap, no pointer chase. Hot path mein
frequently-small collections ke liye bada win.

---

## > **HFT relevance**

> - **Order book = flat arrays, indexed by price level.** `std::array<Level,
>   MAX_LEVELS>` ya ek ring — tree/map nahi. Price → index arithmetic. Top
>   of book ke levels ek-do cache lines mein.
> - **Order lookup by ID = open-addressing hash** (`flat_hash_map` ya custom).
>   ~1 miss. Ya agar IDs dense hain → direct array index.
> - **No `std::list`, no `std::map` on the hot path.** Ordered-and-mutable →
>   `absl::btree_map` ya sorted vector + gap buffer.
> - **Pool-allocate everything.** Orders, events, nodes — ek pre-reserved
>   arena se, indices se link. Zero malloc in steady state (`29/16`).
> - **Small collections inline.** Per-symbol subscriber lists, pending child
>   orders — `InlinedVector<T, 4>` type, no heap for the common small case.
> - **Measure lookups in misses, not just ns.** `perf stat -e
>   mem_load_retired.l3_miss` around your book/order-map ops.

---

## Hands-on

```bash
# pointer-chase (dependent misses) vs sequential -- the core cost model:
./build.ps1 fast 32-CACHE-MEMORY-PERFORMANCE/examples/08_tlb_hugepages.cpp
#   curve B ~ list/tree traversal cost as working set grows (95 ns/hop @ 8MB+)

# AoS vs SoA random access (T3): 1 line vs 8 lines per object:
./build.ps1 fast 32-CACHE-MEMORY-PERFORMANCE/examples/05_aos_vs_soa.cpp
```

Experiment: 1M ints ko (a) `std::vector`, (b) `std::list`, (c) `std::vector<
Node>` arena with `uint32 next` in shuffled order — teeno ko sum karo. b >> a,
aur c a ke kareeb (arena locality) agar insertion-order traversal ho.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "same big-O = same speed" | miss count + dependency alag → 10-300x |
| "`list` insert O(1) so it's fast" | traverse-to-iterator = O(n) dependent misses |
| "`map` O(log n) is fine" | ~20 serial misses/lookup; btree/sorted-vec/hash better |
| "`unordered_map` O(1) = fastest" | ~2-3 misses + per-node malloc; open-addr ~1 |
| "pointers, indices — same thing" | index = half the bytes, 2x fanout/line, no chase |
| "`vector<vector<T>>` for grids" | scattered rows; flat `vector<T>` + index math |

---

## Exercises

1. Ek hot lookup: given order ID, find the order. 500k live orders, IDs are
   64-bit random. `std::map<uint64,Order*>` vs `absl::flat_hash_map<uint64,
   uint32>` (index into an order pool). Compare misses/lookup aur why.

   <details><summary>Answer</summary>

   `std::map`: RB-tree, ~log₂(500k) ≈ **19 dependent hops**, har hop ek node
   (alag alloc) → up to ~19 serial misses. Plus `Order*` deref = ek aur miss.
   `flat_hash_map<uint64,uint32>`: open addressing, ~**1 miss** to the slot
   group (SIMD probe 16 control bytes at once), then the pool `Order` at
   `pool[idx]` = 1 more miss. ~2 misses total, and the hash probe is not a
   deep dependent chain. ~10x fewer misses → typically 5-15x faster lookups,
   half the memory (uint32 index vs 8-byte pointer + tree overhead).
   </details>

2. Aapke paas ek sorted array of 12 `int` keys hai jise aap har tick ~1000
   baar search karte ho. Binary search vs linear scan — kaunsa tez aur kyun?

   <details><summary>Answer</summary>

   12 ints = 48 bytes = **ek cache line** (barely). Linear scan: ek line
   load (ya already hot), 12 sequential compares — fully predictable branches
   (`key > arr[i]` monotonic), auto-vectorizable (compare 8 ints at once with
   AVX2). ~1-2 ns. Binary search: same one line, par ~4 iterations with
   **data-dependent branches** (mispredict-prone, `31/07`) → ~4 mispredicts ×
   ~15 cyc. For n ≤ ~64 (a few lines) linear scan on contiguous data beats
   binary search — "O(n) with perfect locality + no mispredicts" < "O(log n)
   with branchy hops". Crossover depends on machine; measure.
   </details>

3. Ek graph (adjacency list) jise aap BFS karte ho. `std::vector<std::list<
   int>>` vs CSR (compressed sparse row: `vector<int> edges` + `vector<int>
   offsets`). Cache behaviour compare karo.

   <details><summary>Answer</summary>

   `vector<list<int>>`: per-vertex a list, har neighbour ek alag node (alag
   alloc, scattered) → visiting a vertex's neighbours = dependent pointer
   chase, ~1 miss/neighbour. Plus the outer vector of list-heads. **CSR**:
   `offsets[v]..offsets[v+1]` gives a **contiguous slice** of `edges[]` for
   vertex v's neighbours → sequential scan, prefetcher-fed, ~1 miss / 16
   neighbours. And CSR is ~half the memory (no next-pointers, no malloc
   headers). BFS on CSR is typically 5-20x faster and is what every real
   graph library (and GPU) uses. Cost: building CSR needs the graph upfront
   (or a rebuild on mutation).
   </details>

---

## Interview questions

1. `std::list` vs `std::vector` traversal — miss count aur dependency.
2. `std::map` lookup — kitne misses, aur woh serial kyun.
3. Chaining hash vs open-addressing hash — cache behaviour.
4. B-tree node-based tree se kyun cache-friendly (fanout → depth).
5. Arena/pool allocation node-based structure ko kaise bachati.
6. Pointer vs index (uint32) — 3 concrete benefits.
7. "log n beats n" kab galat hota (small, contiguous, branchy binary search).

---

## Next
→ [`09-aos-vs-soa-deep.md`](09-aos-vs-soa-deep.md)
