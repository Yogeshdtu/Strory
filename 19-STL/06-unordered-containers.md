# 06 — Unordered containers (hash tables)

## Prerequisites
- [`05-map-and-set.md`](05-map-and-set.md)
- Folder 05 (bitwise, modulo), folder 07 (cache), folder 15–16 (`operator==`, custom types)

## Yeh topic abhi kyun
Jab aapko sirf **membership** ya **key → value lookup** chahiye, aur order se
farak nahi padta — `std::unordered_map` / `std::unordered_set`. Average **O(1)**
lookup. Ye course ke sabse zyada use hone waale associative containers hain. Par
inka standard-mandated design (bucket + separate chaining) inhe HFT-grade nahi
banata — woh bhi samjhenge.

---

## The four unordered containers

`std::unordered_set<K>`, `std::unordered_map<K,V>`,
`std::unordered_multiset<K>`, `std::unordered_multimap<K,V>` — same key/value/
duplicate structure as the ordered four, but:

- **No ordering.** Iteration yields elements in an unspecified, hash-dependent
  order that can change on rehash.
- **Average O(1)** insert / erase / find; **worst case O(n)** (all keys collide).
- Need `std::hash<K>` **and** `operator==` for `K` (equality resolves
  collisions). Built-in for all scalar types, `std::string`, pointers, etc.

```cpp
#include <unordered_map>

std::unordered_map<std::string, int> m;
m.reserve(10'000);                       // ⬅ do this if you know the size -- avoids repeated rehashing
m["apple"] = 3;
m.emplace("banana", 5);
if (auto it = m.find("apple"); it != m.end()) use(it->second);
m.contains("apple");                     // C++20
m.erase("apple");
```

---

## How the standard container works: buckets + separate chaining

```
   hash(key) --%--> bucket index

   buckets:  [0]-> nullptr
             [1]-> node("cat",1) -> node("dog",7) -> nullptr      (a linked list -- a "chain")
             [2]-> nullptr
             [3]-> node("ox",4) -> nullptr
             ...
```

1. `h = std::hash<K>{}(key)` → a `size_t`.
2. `bucket = h % bucket_count()` (libstdc++ uses a **prime** bucket count and
   real `%`; some impls use power-of-two + mask).
3. Walk that bucket's **singly-linked list**, comparing keys with `operator==`,
   until found or end.
4. Insert: append a node to the bucket's chain. If `size() + 1 > max_load_factor()
   * bucket_count()` → **rehash**: allocate more buckets, recompute every
   element's bucket, relink. O(n), and it invalidates **all iterators** (but not
   references — the nodes themselves don't move in libstdc++).

**`load_factor()` = `size() / bucket_count()`.** `max_load_factor()` defaults to
`1.0`. Cross it → rehash (roughly doubling buckets).

### Consequences

- **Every element is a separately heap-allocated node** (libstdc++: nodes hold
  `next` + the hash + the `pair`). Same cache-miss-per-probe problem as
  `std::list` / `std::map`, just usually only **1–2** nodes per lookup instead of
  `log n`.
- The bucket array is one contiguous allocation of pointers; the nodes are
  scattered.
- `reserve(n)` / `rehash(n)` up front → one big bucket allocation, no rehash
  spikes during fill. **Always `reserve` when you can bound the size.**
- Pointer/reference stability: **references survive rehash**; **iterators do
  not**.

Measured (`examples/02`, n = 200,000, reserved): **113 ns/lookup** vs `std::map`
1049 ns — ~9x faster, because it's ~1 cache miss instead of ~18.

---

## Custom key types — you must provide `hash` + `==`

```cpp
struct OrderId { std::uint32_t venue; std::uint64_t seq; };

// 1. equality
bool operator==(const OrderId& a, const OrderId& b) {
    return a.venue == b.venue && a.seq == b.seq;
}

// 2. hash -- specialise std::hash, OR pass a hasher as the 3rd template arg
template <>
struct std::hash<OrderId> {
    std::size_t operator()(const OrderId& o) const noexcept {
        std::size_t h = std::hash<std::uint32_t>{}(o.venue);
        h ^= std::hash<std::uint64_t>{}(o.seq) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);  // boost::hash_combine
        return h;
    }
};

std::unordered_set<OrderId> live;   // now works
```

**A good hash** spreads keys uniformly across `size_t`. A bad one (e.g. returning
`o.seq` alone when `seq` is always small, or `a ^ b` which collides on swaps)
piles keys into few buckets → chains grow → O(n) behaviour. Never just `return
0;` (legal, catastrophic).

`std::hash<std::string>` is decent but not cryptographic; for adversarial input
(untrusted keys → deliberate collisions = "hash flooding" DoS) use a seeded hash
(absl, wyhash) or a different structure.

---

## `unordered_map` vs `map` — when which

| Want | Use |
|---|---|
| Fastest membership / key→value lookup, order irrelevant | **`unordered_map` / `unordered_set`** (and `reserve`) |
| Sorted iteration, `lower_bound`/range queries, predecessor/successor | `map` / `set` |
| Iterator stability across insert/erase | `map` (unordered: refs stable, iterators not) |
| No latency spikes ever (bounded worst case) | `map` (O(log n) always) — or a custom flat hash |
| Small N (≤ ~30), keys cheap to compare | a **sorted `std::vector` + `lower_bound`**, or even linear scan — beats both |

---

## The standard hash map is not the fast hash map

`std::unordered_map`'s API **mandates** separate chaining semantics (it exposes
`bucket()`, `begin(n)` for bucket-local iteration, node handles, reference
stability), which forces node-per-element. High-performance hash maps use **open
addressing** (all entries in one contiguous array, probe on collision):

- `absl::flat_hash_map`, `boost::unordered_flat_map`, `ankerl::unordered_dense`,
  `tsl::robin_map` — typically **2–5x faster** than `std::unordered_map`, far
  fewer cache misses, no per-element allocation.
- Trade-offs: references are **not** stable across rehash (entries move); erase
  may leave tombstones; iteration order shifts.

For HFT, if a hash map is on the hot path at all, it's one of these (or a
hand-rolled fixed-capacity open-addressed table), never `std::unordered_map`.

---

## Andar kya hota hai

- `find`: hash the key (for `int`, `std::hash` is often the identity — a few
  cycles; for `std::string`, a loop over the bytes), `% bucket_count`, load the
  bucket head pointer, then 1+ node dereferences comparing keys. The bucket array
  load + the first node load are typically the only cache misses → ~2 misses,
  ~100 ns cold.
- `insert` past the load factor: `operator new` for `next_prime(2 * buckets)`
  pointers, then for each existing node recompute `hash % new_count` and relink.
  O(n) — a visible spike. `reserve` up front sizes the bucket array once.
- libstdc++ caches each node's full hash in the node → rehash doesn't re-hash
  keys, just re-mods; and `==` is skipped when cached hashes differ.
- Power-of-two implementations replace `% bucket_count` with `& (bucket_count -
  1)` (1 cycle vs ~20 for `%`) but need a good hash's low bits — libstdc++ chose
  primes + `%` for robustness against weak hashes.

> **HFT relevance:** `std::unordered_map` with `reserve()` is acceptable in
> warm-but-not-hottest paths (symbol → config, orderId → order in a
> non-latency-critical component). On the actual hot path it's replaced by a
> **fixed-capacity open-addressed table** sized at startup (no allocation, no
> rehash, entries contiguous → ~1 cache line per probe), or the key is turned
> into a **dense small integer** used as a direct array index (orderId low bits,
> symbol id) → O(1), zero misses, zero hashing. Rule: hashing + a pointer chase
> is still slower than an array index you can compute.

---

## Hands-on

```bash
./build.ps1 fast 19-STL/examples/02_map_vs_unordered.cpp
./build.ps1 fast 19-STL/examples/11_container_benchmark.cpp
```

`examples/02`: `unordered_map` 113 ns vs `map` 1049 ns vs sorted-vector 357 ns
per lookup (n=200k). `examples/11`: `unordered_set` membership over 1M queries
49 ms vs `set` 1066 ms (~22x).

---

## ⚠️ Traps

### Trap 1 — no `reserve`, then bulk insert
```cpp
std::unordered_map<int,int> m;
for (int i = 0; i < 1'000'000; ++i) m[i] = i;   // ⚠️ ~20 rehashes, each O(size). m.reserve(1'000'000) first
```

### Trap 2 — custom key without a hash
```cpp
std::unordered_set<MyStruct> s;   // ❌ compile error: no std::hash<MyStruct>. Provide one + operator==
```

### Trap 3 — a weak hash
```cpp
struct H { size_t operator()(Point p) const { return p.x ^ p.y; } };   // ⚠️ (1,2) and (2,1) collide; (0,k) all hit bucket k. Use hash_combine
```

### Trap 4 — relying on iteration order
```cpp
for (auto& [k,v] : um) log(k);   // ⚠️ order is unspecified and changes on rehash / across runs / impls
```

### Trap 5 — holding an iterator across an insert
```cpp
auto it = um.find(k);  um.emplace(other, v);  ++it;   // ⚠️ the emplace may rehash -> it invalid. (references would survive)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`unordered_map` is always O(1)" | Average O(1); worst case O(n) (collisions / hash flooding) |
| "It's contiguous / cache-friendly" | Bucket array is; the **nodes** are scattered (1 heap alloc each) |
| "`std::unordered_map` is the fast hash map" | Chaining is mandated by its API — `absl`/`boost` flat maps are 2–5x faster |
| "Iteration order is insertion order" | Unspecified; can differ per run, per impl, after a rehash |
| "`reserve` doesn't matter much" | It turns ~log(n) rehashes into one allocation — big win on bulk load |

---

## Exercises

1. **Rehash count:** `std::unordered_map<int,int> m;` (max_load_factor 1.0,
   bucket counts roughly doubling), insert 1,000,000 keys with no `reserve`.
   Roughly how many rehashes? What does `m.reserve(1'000'000)` change?

   <details><summary>Answer</summary>

   ~20 rehashes (bucket count climbs 1→3→7→17→... past 1e6, ~log₂(1e6)≈20 steps),
   each rehash O(current size). `reserve(1e6)` → one bucket allocation big enough
   from the start → **0** rehashes during the fill.
   </details>

2. **Hash quality:** `struct H { size_t operator()(std::pair<int,int> p) const {
   return p.first; } };` — what's wrong, and when does it bite?

   <details><summary>Answer</summary>

   Ignores `p.second` entirely → every pair with the same `first` lands in one
   bucket. If your data has few distinct `first` values (e.g. `first` is a small
   category id), chains become O(n) and lookups degrade to linear scan. Combine
   both fields.
   </details>

3. **Choose:** (a) orderId → Order\*, ~50k live, need fastest lookup, no ordering.
   (b) price → level, need "best bid" = max price and range scans. (c) 12 config
   flags by name, read at startup.

   <details><summary>Answer</summary>

   (a) `unordered_map` with `reserve(64k)` — or a flat open-addressed map / direct
   index on orderId bits. (b) `map` (ordered — `rbegin()` is best bid,
   `lower_bound` for ranges), or a sorted vector. (c) anything — `map` or even a
   `std::array` of `{name,value}` with linear search; N=12, perf irrelevant.
   </details>

4. **Stability:** after `m.insert(x)` triggers a rehash, which of these that you
   grabbed *before* the insert are still valid: `int& v = m[k];`, `auto it =
   m.find(k);`, `int* p = &m.at(k);`?

   <details><summary>Answer</summary>

   `v` and `p` (references/pointers to elements) — **valid**, libstdc++ doesn't
   move nodes on rehash. `it` (iterator) — **invalid**, rehash invalidates all
   iterators.
   </details>

5. **`%` vs `&`:** why might a power-of-two bucket count with `& (n-1)` be faster
   per lookup than a prime count with `%`, and why did libstdc++ pick primes
   anyway?

   <details><summary>Answer</summary>

   `& (n-1)` is 1 cycle; `%` by a runtime value is ~20–40 cycles. But `&` only
   uses the **low bits** of the hash, so a hash with poor low-bit distribution
   (e.g. `std::hash<int>` = identity → low bits are just the number) clusters
   badly. Primes + `%` mix all bits, so libstdc++ is robust to weak hashes at the
   cost of the `%`.
   </details>

---

## Interview questions

1. `std::unordered_map` andar — buckets, chaining, load factor, rehash?
2. Average O(1) kaise, worst case O(n) kab?
3. Custom key ke liye kya chahiye (`hash` + `==`), acha hash kya hota?
4. Rehash pe iterators aur references ka kya hota (libstdc++)?
5. `std::unordered_map` "fast hash map" kyun nahi — open addressing kya deta?
6. `reserve` kyun important, kya bachata?

---

## Next
→ [`07-container-adapters.md`](07-container-adapters.md)
