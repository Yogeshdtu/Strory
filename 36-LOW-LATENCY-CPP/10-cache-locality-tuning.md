# 10 — Cache locality tuning: hot/cold, packing, alignment

## Prerequisites
- **`32-CACHE-MEMORY-PERFORMANCE`** (the full treatment — this lesson is a
  hot-path recap + practice)
- `11-STRUCTS` (padding/alignment), `25-OBJECT-MODEL/09` (`alignas`)

## Yeh topic abhi kyun
Folder 32 mein cache ki poori theory + measured benchmarks hain (sequential
vs random ~7×, row vs column ~10×, AoS vs SoA ~2–3×, false sharing ~6–44×,
pointer chase ~95 ns/hop). Yahan hot-path checklist: apne data structures ko
cache ke liye kaise shape karo.

---

## Rule 0: latency comes from memory, not instructions

Ek hot path pe, agar tum stalled ho, ~hamesha memory pe stalled ho, ALU pe
nahi (folder 31 IPC, folder 32). Isliye locality tuning = latency tuning.

Latency ladder (folder 32, is class of box):
```
  L1     ~1 ns      (4 cyc)
  L2     ~4 ns      (12-14 cyc)
  L3     ~15 ns     (40-50 cyc, shared)
  DRAM   ~90-100 ns (200+ cyc)
  remote DRAM (NUMA) ~150 ns
  page walk (TLB miss) up to 4 more dependent loads
```

Har DRAM miss ~200 cycles = ~100 ns. Ek hot path with a 1 µs budget can
afford ~10 such misses total. Count them.

---

## The checklist

### 1. Hot/cold field split
Ek struct ke fields ko **access frequency** se separate karo:
```cpp
// BEFORE: one 256-byte struct, hot path reads 3 fields
struct Order { uint64_t id; int64_t px; uint32_t qty; Side side;
               char client[64]; char tag[64]; Fill fills[8]; ... };

// AFTER: hot fields dense (one cache line for many orders), cold elsewhere
struct OrderHot  { uint64_t id; int64_t px; uint32_t qty; uint8_t side; };  // 24 B
struct OrderCold { char client[64]; char tag[64]; };                        // by id
Fill* fills_by_id(uint64_t);                                                // arena, lazy
```
Now iterating hot orders touches ~2.6 orders/cache-line instead of straddling.
`03` exercise 1 and `07` exercise 1 both hinge on this.

### 2. AoS → SoA where you scan
Iterate one field across many records → **struct of arrays**:
```cpp
// scan all prices: AoS touches 24-256 B/record; SoA touches 8 B/record + vectorizes
std::vector<int64_t> px;  std::vector<uint32_t> qty;  std::vector<uint8_t> side;
```
Measured (32/05): scan-few-fields **SoA ~2×**, and SoA **vectorizes** where
AoS's stride doesn't. Random *whole-record* access → AoS wins (~3×) — SoA
only for scans. AoSoA (blocks of N) is the hybrid.

### 3. Flat, not pointer-based
Linked list / tree of `new`-d nodes → each hop a likely DRAM miss (~95 ns,
32). Replace with:
- an **array + indices** (`next` is an `int32` index, not a pointer) —
  arena-allocated, contiguous.
- a **flat sorted array** + binary search instead of a `std::map`.
- **open addressing** (`flat_hash_map`-style) instead of chained buckets —
  ~1 miss/lookup vs ~20 for `std::map` (32/08, 20/04).

### 4. Alignment
- `alignas(64)` any struct written by one thread and read by another →
  own cache line, no false sharing (lesson 11).
- Align big buffers to 64 (or a page) so a hot region doesn't straddle
  lines / 4 KB boundaries unnecessarily.
- ⚠️ don't `alignas(64)` *every* small object — it wastes space and TLB/cache
  (a 24-byte `OrderHot` at 64-byte alignment = 62% waste).

### 5. Pack, but mind the trade-off
Reorder members largest-to-smallest to cut padding (11). But: `#pragma pack`
/ `[[gnu::packed]]` can create **misaligned** members → slower access (or a
fault on strict targets). Pack for *wire structs* (fixed layout), align for
*working structs*.

### 6. Keep the working set in cache
Blocking / tiling so you reuse data while it's hot (32/06-07). ⚠️ measured
(32/07): naive blocking can be *slower* than a good loop order — the loop
order + a tuned microkernel is the real win.

### 7. Prefetch — last resort, measured
`__builtin_prefetch(next)` ahead of a pointer chase. Measured (32/06):
**~1.1× on a light gather** (MLP already overlaps ~10 misses), **3× slower**
on a memory-saturated loop. "Prefetch is not free money."

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `alignas(64)` everywhere
Space + cache + TLB waste. Only for cross-thread-shared structs, and big
buffers.

### Trap 2 — SoA for random access
SoA is for *scans*. Random whole-record access → AoS (all fields on ~one
line) beats SoA (one line per field). Measure (32/05).

### Trap 3 — packing a working struct
`[[gnu::packed]]` → misaligned members → slower / fault. Pack wire structs
only.

### Trap 4 — prefetch by reflex
Add it, measure, keep only if it helped *this* loop on *this* box. Often
neutral or harmful.

### Trap 5 — micro-benchmarking locality in isolation
The bench's working set fits L2; production's doesn't → 3-10× different
behaviour (35/01, 35/09). Confirm with `perf` on the real binary.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "align everything to 64" | only cross-thread structs + big buffers |
| "SoA is always faster" | scans yes; random whole-record → AoS |
| "packed structs are smaller so faster" | misalignment can be slower; pack wire structs only |
| "prefetch = free speedup" | ~1.1× or 3× *slower*; measure per loop |
| "linked list is fine, it's O(1) insert" | each hop ~95 ns DRAM; array+indices |

---

## Exercises

1. Ek `std::unordered_map<uint64_t, Book*>` (symbol id → book pointer) hot
   path pe lookup hota, ~2000 symbols. `perf` dikhata is lookup pe har call
   ~3 cache misses (hash bucket load, node load, then `*Book`). Redesign for
   ~0-1 misses.

   <details><summary>Answer</summary>

   2000 symbols, ids assigned at startup → make them **dense**: `symbol_id`
   in `[0, 2000)`. Then `Book` lookup is a **direct array index**:
   `std::vector<Book> books_;  Book& b = books_[symbol_id];` — one load, and
   if `books_` is iterated/accessed in id order it's prefetcher-friendly.
   No hash (no bucket load, no string/int hashing), no node (books stored
   inline, contiguous), no pointer indirection (`Book` by value, not
   `Book*`). If `Book` is large and you don't want 2000 × `sizeof(Book)`
   resident, store `books_` as a `std::deque` or a `std::vector` of
   fixed-size chunks — still 1 load for the outer index + 1 for the book,
   vs 3 for the hash map. The symbol→id mapping itself (a `string` →
   `int` map) is used only at **subscription time** (cold path), so a
   `std::unordered_map` there is fine.
   </details>

2. Ek 40-byte `MarketEvent` struct hai jismein 8 hot bytes (px, qty) aur
   32 cold bytes (venue, flags, timestamps). Ek hot loop **1M events** ke
   px sum karta. AoS mein yeh kitni cache lines touch karta, aur SoA se
   kya badlega? (line = 64 B)

   <details><summary>Answer</summary>

   **AoS**: 1M events × 40 B = 40 MB. Iterating touches every byte of every
   event (the loop reads `event[i].px`, but the hardware loads the whole
   64-byte line, and consecutive 40-byte events straddle lines). Lines
   touched ≈ 40 MB / 64 B = **625,000 lines**, ≈ 40 MB of memory traffic
   for an 8 MB-of-actual-data job. At ~100 ns/miss beyond L3 (40 MB >> L3),
   this is bandwidth-bound and slow.
   **SoA**: a separate `std::vector<int64_t> px` (8 MB). The loop reads
   8 contiguous bytes per event; lines touched ≈ 8 MB / 64 = **125,000
   lines** — **5× less traffic**, fully sequential (prefetcher nails it),
   and it **vectorizes** (`paddq`/`vpaddq`, 4-8 lanes) since the stride is
   `sizeof(int64_t)`. Expect ~4-8× faster for this scan (measured shape,
   32/05). The cold 32 bytes/event are never loaded. Trade-off: if another
   part of the code needs *all* fields of one event at once (random access),
   SoA makes that a gather across 40 MB of separate arrays — keep an AoS
   copy for that path, or use AoSoA.
   </details>

---

## Interview questions

1. Latency ladder (L1→DRAM→page walk); "10 misses" in a 1 µs budget.
2. Hot/cold field split — kya, kaise, faayda.
3. AoS vs SoA — kab kaunsa (scan vs random), aur SoA + vectorization.
4. `alignas(64)` — kab lagao, kab NAHI.
5. Prefetch — measured reality (~1.1× or 3× slower); kab try karo.

---

## Next
→ [`11-false-sharing-elimination.md`](11-false-sharing-elimination.md)
