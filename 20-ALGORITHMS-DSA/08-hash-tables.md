# 08 — Hash tables (build your own)

## Prerequisites
- [`07-stacks-and-queues.md`](07-stacks-and-queues.md)
- Folder 19 file 06 (unordered containers), folder 05 file 05 (bitwise), folder 19 file 22 (`<bit>`)

## Yeh topic abhi kyun
Hash table `O(1)` average lookup deta — DSA aur systems dono ka workhorse. Par
`std::unordered_map` ka design (separate chaining) HFT-grade nahi. Apni table
likhne se samajh aata: **hash function**, **collision resolution**, **load
factor**, **resize**. `examples/04_hash_table.cpp` ek open-addressing table hai
jo `std::unordered_map` se ~2-4× tez chalti.

---

## The pieces

1. **Hash function** `h : key → size_t` — spreads keys uniformly.
2. **Bucket index** — `h(key) % capacity`, or `h(key) & (capacity-1)` for
   power-of-two capacity.
3. **Collision resolution** — two keys, same bucket. Two families:
   **separate chaining** and **open addressing**.
4. **Load factor** `α = size / capacity` — when it crosses a threshold, **resize**
   (rehash everything into a bigger table).

---

## Collision resolution

### Separate chaining (what `std::unordered_map` does)
Each bucket holds a **linked list** (or small vector) of entries.

```
buckets: [0] -> null
         [1] -> (cat,1) -> (dog,7) -> null
         [2] -> null
         [3] -> (ox,4) -> null
```

- Simple, tolerates `α > 1`, erase is trivial (unlink).
- **A heap node per element** → a cache miss per probe, memory bloat, allocator
  traffic. This is why `std::unordered_map` loses to flat maps (folder 19 file
  06).
- Mandated by `std::unordered_map`'s API (bucket iteration, `bucket_count()`,
  reference stability across rehash) → the standard map **can't** be flat.

### Open addressing (what fast hash maps do)
**All entries live in one contiguous array.** On collision, **probe** other slots
by a rule until an empty one is found.

- **Linear probing**: try `i, i+1, i+2, …` (mod capacity). Best cache behaviour
  (probes are adjacent → same cache line), but **primary clustering** (runs of
  full slots grow and merge).
- **Quadratic probing**: try `i, i+1, i+4, i+9, …`. Breaks up clustering, worse
  locality.
- **Robin Hood**: linear probing, but on insert, if the new key has probed
  farther than the key in the slot, swap them ("steal from the rich"). Evens out
  probe lengths → low variance, good for lookups.
- **Double hashing**: step size is a second hash of the key.

`examples/04_hash_table.cpp` uses **linear probing** with power-of-two capacity,
a splitmix64 hash finalizer, and **tombstones** for erase.

---

## `examples/04_hash_table.cpp` — walkthrough

```cpp
class HashMapU64 {
    enum State : uint8_t { EMPTY, FULL, TOMB };
    struct Slot { uint64_t key, value; State state; };
    std::vector<Slot> slots_;                  // ONE contiguous array
    ...
    static uint64_t mix(uint64_t x) {          // splitmix64 finalizer -- strong avalanche
        x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
        x ^= x >> 27; x *= 0x94d049bb133111ebULL;
        x ^= x >> 31; return x;
    }
    size_t mask() const { return slots_.size() - 1; }   // capacity is a power of two
```

- **Insert**: `i = mix(key) & mask()`; scan forward while the slot is `FULL`
  (probe); on `EMPTY` (or a reusable `TOMB`) put the entry; if the same key is
  found, overwrite. Resize when `(FULL + TOMB) / capacity ≥ 0.75`.
- **Find**: same probe; stop at `EMPTY` (definitely absent) or a matching `FULL`.
- **Erase**: mark the slot `TOMB` (not `EMPTY` — an `EMPTY` there would break the
  probe chain for keys inserted after it). Tombstones are cleared on the next
  resize.
- **Resize**: allocate `2×`, re-insert every `FULL` entry (tombstones dropped).

### Measured (`examples/04`, n = 1,000,000, `-O2`, this box)

| op | mine (open addressing) | `std::unordered_map` |
|---|---|---|
| build | **~66 ns/op** | ~295 ns/op |
| lookup (half present) | **~45 ns/op** | ~83 ns/op |

~4× faster build (no per-node allocation), ~2× faster lookup (~1 cache line per
probe vs a bucket-array load + a scattered node load).

---

## Hash function quality

A good hash **avalanches** — flipping one input bit flips ~half the output bits.

- **Integers**: `std::hash<int>` is often the **identity** — fine for prime
  capacity + `%`, terrible for power-of-two + `&` (low bits = the number itself →
  clustering). Apply a finalizer (splitmix64, `murmur3` fmix, `wyhash`) before
  masking.
- **Strings**: FNV-1a, `murmur`, `xxHash`, `wyhash`. `std::hash<std::string>` is
  decent but not adversary-resistant.
- **Structs**: combine field hashes — `boost::hash_combine`:
  `h ^= hash(field) + 0x9e3779b97f4a7c15 + (h<<6) + (h>>2)`. Never `a ^ b` (swaps
  collide) or `a + b` (poor mixing).
- **Bad hash** → chains / probe runs grow → `O(n)` behaviour. `return 0;` is
  legal and catastrophic.
- **Hash flooding**: with untrusted keys, an attacker who knows your hash can
  force all keys into one bucket → DoS. Use a **seeded** hash (`wyhash` with a
  per-process random seed, SipHash) for untrusted input.

---

## Load factor and resize

- Chaining: `max_load_factor` defaults to `1.0`; crossing it roughly doubles
  buckets and rehashes (`O(n)` spike).
- Open addressing: keep `α ≤ 0.7–0.85`. Above that, probe lengths explode
  (linear probing: expected probes ≈ `1/(1-α)` for lookups → `α=0.9` → ~10
  probes).
- **Resize is `O(n)`** and invalidates iterators (chaining) or moves every entry
  (open addressing). `reserve(n)` up front turns ~`log(n)` resizes into one
  allocation — always do it when you can bound the size (folder 19 file 06).

---

## Andar kya hota hai

- Open-addressing `find`: hash (a few cycles for an int finalizer, a byte loop
  for a string), `& mask` (1 cycle), load `slots_[i]` (1 cache miss cold), compare
  key, maybe probe `i+1` (same cache line → no extra miss for the first few).
  ~1–2 cache misses total regardless of `n` → the flat ~45 ns.
- Chaining `find`: hash, `%` (~20 cycles) or `&`, load the bucket-array pointer
  (miss #1), load the first node (miss #2, scattered), compare, maybe follow
  `next` (miss #3). ~2–3 misses → ~83 ns, and worse under churn.
- `% prime` vs `& (pow2-1)`: libstdc++ uses prime capacities + real `%` so it's
  robust to weak hashes (mixes all bits); fast flat maps use power-of-two + mask
  (1 cycle) and *require* a good finalizer.
- Tombstones: on a heavy insert/erase workload they accumulate and lengthen
  probes; the resize (triggered by `FULL + TOMB`) clears them. A table that only
  grows never needs to worry.

> **HFT relevance:** if a hash map is on a measured hot path, it's an
> **open-addressed, fixed-capacity** table sized at startup (`load factor` chosen
> so it never resizes) — contiguous, ~1 cache line per lookup, zero allocation,
> no rehash spike. Often the key is turned into a **dense small integer** (order
> id low bits, symbol id) used as a **direct array index** → `O(1)`, zero
> hashing, zero misses beyond the one array load. `std::unordered_map` (with
> `reserve`) is acceptable only in warm/control-plane code. `absl::flat_hash_map`
> / `boost::unordered_flat_map` are the off-the-shelf open-addressed options
> (folder 19 file 06).

---

## Hands-on

```bash
./build.ps1 fast 20-ALGORITHMS-DSA/examples/04_hash_table.cpp
```

Extend it: add Robin Hood insertion (track each slot's probe distance, swap on
insert) and compare lookup-time *variance* against plain linear probing under a
90% load factor. Then try `std::hash<uint64_t>` (identity) **without** the
`mix()` finalizer and watch clustering wreck it.

---

## ⚠️ Traps

### Trap 1 — power-of-two capacity + identity hash + `& mask`
```cpp
size_t i = key & (cap - 1);   // ⚠️ low bits of `key` only -> sequential keys cluster. Finalize the hash first
```

### Trap 2 — erasing by setting the slot to EMPTY
```cpp
slots_[i].state = EMPTY;   // ⚠️ breaks the probe chain -> later keys become unfindable. Use a TOMB state
```

### Trap 3 — no resize / load factor too high
```cpp
// linear probing at alpha = 0.95 -> expected ~20 probes per lookup. Keep alpha <= ~0.8, resize past it
```

### Trap 4 — weak struct hash
```cpp
size_t operator()(Point p) const { return p.x ^ p.y; }   // ⚠️ (1,2) and (2,1) collide. hash_combine
```

### Trap 5 — no `reserve` before a bulk load
```cpp
for (...) m[k] = v;   // ⚠️ ~log(n) resizes, each O(size). m.reserve(n) first -> one allocation
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Hash tables are always `O(1)`" | Average `O(1)`; worst case `O(n)` (collisions / flooding / high load factor) |
| "`std::unordered_map` is the fast hash map" | Chaining is API-mandated → node-per-entry; open-addressed flat maps are 2–5× faster |
| "Any hash that compiles is fine" | It must avalanche; identity + power-of-two + `&` clusters badly |
| "Erase = clear the slot" | Open addressing needs tombstones or backward-shift deletion |
| "Load factor doesn't matter much" | Linear-probe cost is ~`1/(1-α)` — it blows up past ~0.85 |

---

## Exercises

1. **Probe count:** linear probing, `α = 0.5` vs `α = 0.9`. Expected probes for a
   successful lookup (`≈ ½(1 + 1/(1-α))`)?

   <details><summary>Answer</summary>

   `α=0.5`: `½(1 + 2) = 1.5`. `α=0.9`: `½(1 + 10) = 5.5`. (Unsuccessful lookups
   are worse: `≈ ½(1 + 1/(1-α)²)` → 2.5 vs 50.5.) Keep `α` well below 0.9.
   </details>

2. **Tombstone necessity:** table `[_, A, B, _]` where `A` and `B` both hash to
   index 1. Erase `A` by setting slot 1 to EMPTY. Now `find(B)`?

   <details><summary>Answer</summary>

   `find(B)` hashes to 1, sees EMPTY, concludes "absent" — but `B` is at slot 2.
   Setting slot 1 to TOMB instead keeps the probe going to slot 2. (Or
   backward-shift: move `B` into slot 1 on erase.)
   </details>

3. **Direct index vs hash:** you have order ids that are dense 32-bit counters,
   ≤ 4M live at once. Best structure for id → order lookup?

   <details><summary>Answer</summary>

   A plain array indexed by `id & (CAP-1)` where `CAP` covers the live range
   (e.g. `std::vector<Order*>` of 8M, or `id % maxLive`) — `O(1)`, no hashing,
   one array load, zero collisions if the id space is dense and bounded. Falls
   back to a real hash map only if ids are sparse.
   </details>

4. **hash_combine:** combine `uint32 venue` and `uint64 seq` into one hash.

   <details><summary>Answer</summary>

   `size_t h = splitmix64(venue); h ^= splitmix64(seq) + 0x9e3779b97f4a7c15ULL +
   (h << 6) + (h >> 2); return h;` — each field finalized, then mixed so field
   order and swaps don't collide.
   </details>

5. **Resize trigger:** your open-addressed table triggers resize at
   `FULL/cap ≥ 0.75` but never counts tombstones. What goes wrong on a
   churny workload?

   <details><summary>Answer</summary>

   Erases create tombstones that count as "occupied" for probing but not for the
   resize trigger. `FULL` stays low, no resize fires, tombstones pile up, probe
   lengths grow toward `O(n)`. Trigger on `(FULL + TOMB)/cap` instead.
   </details>

---

## Interview questions

1. Separate chaining vs open addressing — cache, erase, load factor?
2. `std::unordered_map` flat kyun nahi ho sakti (API constraints)?
3. Linear probing ka clustering problem — Robin Hood kaise theek karta?
4. Tombstone kyun chahiye open addressing mein?
5. Acha hash function kya karta (avalanche); identity hash + power-of-two ka bug?
6. HFT mein id→object lookup — hash map ki jagah kya (direct index)?

---

## Next
→ [`09-trees-and-bst.md`](09-trees-and-bst.md)
