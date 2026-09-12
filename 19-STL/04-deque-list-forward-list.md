# 04 — `deque`, `list`, `forward_list`

## Prerequisites
- [`02-vector-deep.md`](02-vector-deep.md), [`03-array-and-span.md`](03-array-and-span.md)
- Folder 14 (heap, allocation cost), folder 07 file on cache locality

## Yeh topic abhi kyun
`std::vector` ke baad ye teen sequence containers aate hain. Har ek ki ek
**specific** wajah hai — aur woh wajah aksar utni strong nahi hoti jitni log
sochte hain. Ye lesson batata hai kab inme se koi genuinely better hai, aur kab
`std::vector` phir bhi jeet jaata (spoiler: zyaadatar baar).

---

## `std::deque<T>` — double-ended queue

```cpp
#include <deque>
std::deque<int> d{1, 2, 3};
d.push_back(4);      // O(1)
d.push_front(0);     // O(1)  -- vector can't do this cheaply
d[2];                // O(1) random access -- but NOT one contiguous block
d.pop_front();       // O(1)
```

**Layout:** a deque is **not contiguous**. It's an array of pointers ("map") to
fixed-size **chunks** (libstdc++: 512 bytes worth of `T`, so `512/sizeof(T)`
elements per chunk):

```
   map:   [ ptr | ptr | ptr | ptr ]
             │     │     │     │
             ▼     ▼     ▼     ▼
          [chunk][chunk][chunk][chunk]      each chunk contiguous; chunks are not adjacent
```

- `push_back` / `push_front` — O(1), allocate a new chunk when the end one fills.
  Only the **map** may reallocate (cheap — it's just pointers).
- `d[i]` — O(1) but **two indirections**: find the chunk (`map[i / chunkLen]`),
  then the offset (`chunk[i % chunkLen]`). Slower than `vec[i]`.
- **`push_back`/`push_front` do NOT invalidate references** to existing elements
  (unlike `vector`) — the elements don't move. Iterators *are* invalidated.
  `insert`/`erase` in the **middle** invalidates everything.
- No `reserve()`, no `capacity()`, no `data()` (not contiguous → can't hand to a
  C API as one block).
- Iteration is slower than `vector` (~3x in `examples/11` — chunk boundaries
  break the prefetcher) but far faster than `list`.

**Use `std::deque` when:** you genuinely need cheap `push_front` **and**
`push_back` (a work queue, a sliding window), and you don't need contiguity.
It's also the default backing store for `std::stack` and `std::queue` (file 07).

---

## `std::list<T>` — doubly-linked list

```cpp
#include <list>
std::list<int> l{1, 2, 3};
l.push_back(4); l.push_front(0);          // O(1)
auto it = std::next(l.begin(), 2);
l.insert(it, 99);                         // O(1) GIVEN the iterator -- no shifting
l.erase(it);                             // O(1) given the iterator
l.splice(l.begin(), other);             // O(1) -- move a whole sublist by relinking, no element moves
l.sort();                               // member (can't use std::sort -- not random access)
```

**Layout:** every element is a **separately heap-allocated node** `{ prev, next,
T value }`. Nodes are scattered across the heap.

```
   [node]<->[node]<->[node]<->[node]        each an independent malloc; ~2 pointers overhead per element
```

- `insert` / `erase` / `splice` — O(1) **if you already hold the iterator**. No
  element is moved or copied — just pointer relinking.
- **Every iterator/pointer/reference stays valid** across `insert`/`push`/`splice`
  — only `erase` invalidates (just the erased node's iterator). This *stability*
  is `list`'s one real advantage.
- **No random access.** `l[5]` doesn't exist. `std::next(it, 5)` is O(5).
- **Iteration is terrible** — ~18ms vs vector's 0.67ms for 1M ints in
  `examples/11` (~27x). Each `++it` is a pointer dereference to a random heap
  address → a cache miss almost every step. Plus 16+ bytes overhead per element.
- Node allocation per `push_back` → slow builds (~30x vector in `examples/11`).

**Use `std::list` when:** you need **iterator/reference stability** across
insertions/erasures anywhere, **and** frequent O(1) splicing, **and** you rarely
iterate the whole thing. That combination is rare. "I insert/erase in the middle
a lot" is usually **still faster with `std::vector`** for N up to thousands,
because the `memmove` is cache-friendly and there's no per-node allocation.

---

## `std::forward_list<T>` — singly-linked list

```cpp
#include <forward_list>
std::forward_list<int> fl{1, 2, 3};
fl.push_front(0);                         // O(1) -- the ONLY push
fl.insert_after(fl.before_begin(), 99);  // insert/erase are "_after" -- you can only look forward
fl.erase_after(fl.begin());
```

- Only a `next` pointer per node → **smallest per-element overhead** of any node
  container (one pointer, not two).
- **No `size()`** (would cost a word per list or an O(n) walk — the committee
  chose neither). No `push_back` (no tail pointer). Forward-only iterators.
- Same cache-miss iteration problem as `list`, slightly less memory.

**Use `std::forward_list` when:** you need a linked list, only ever prepend /
insert-after a known position, never need `size()`, and memory per node is
critical. Extremely niche.

---

## The decision, in one table

| Need | Container |
|---|---|
| Default. Contiguous, cache-friendly, works with everything | **`std::vector`** |
| Cheap `push_front` AND `push_back`, don't need contiguity | `std::deque` |
| Iterator/reference stability across mid-container insert/erase + O(1) splice, rare full iteration | `std::list` |
| Linked list, prepend-only, `size()` never needed, min overhead | `std::forward_list` |
| Fixed compile-time size, no heap | `std::array` |

**In practice:** `std::vector` for ~everything. `std::deque` occasionally for
queues. `std::list` / `std::forward_list` almost never in performance code — the
cache behaviour kills them.

---

## Andar kya hota hai

- `deque[i]`: `map[base + i / N][ (i) % N ]` — a load of a pointer, then a load of
  the element. Branchless but two dependent loads. Iteration crosses a chunk
  boundary every `N` elements → a pointer chase + likely a cache miss there.
- `list` `++it`: `it = it->next` — load a pointer from a node, follow it. The next
  node is at an unpredictable heap address → the prefetcher can't help → ~100–300
  cycle stall on an L2/L3/DRAM miss, *per element*. This is why `examples/11`
  shows list iteration ~27x slower than vector.
- `list::splice` — literally 6 pointer assignments, moves zero elements, O(1)
  regardless of sublist length. This is the operation `list` exists for.
- Per-element memory: `vector<int>` = 4 bytes/elem. `list<int>` = 4 + 2 pointers
  + allocator header ≈ 24–32 bytes/elem. 6–8x the footprint → 6–8x the cache
  pressure.

> **HFT relevance:** `std::list` and `std::forward_list` are effectively **banned**
> in hot paths — a cache miss per element is the opposite of what low latency
> needs. Where you'd reach for a linked list (an intrusive free list, an LRU
> chain), HFT uses an **intrusive linked list over a contiguous arena**: nodes
> live in a `std::vector`/pool, "pointers" are indices, so you get O(1) splice
> *and* cache locality. `std::deque` shows up as a bounded SPSC-ish work queue,
> but a preallocated ring buffer (`std::array` + head/tail) is preferred because
> it never allocates. `std::vector` + indices beats all three for the vast
> majority of cases.

---

## Hands-on

```bash
./build.ps1 fast 19-STL/examples/11_container_benchmark.cpp
```

Measured on this box (N = 1,000,000):

| container | iterate (ms) | build (ms) |
|---|---|---|
| `vector` | **0.67** | **2.86** |
| `deque` | 2.28 | 4.09 |
| `list` | 18.02 | 86.43 |

list iterate is **~27x** slower than vector; list build **~30x** slower. Run it
yourself and watch the linked-list rows.

---

## ⚠️ Traps

### Trap 1 — `std::sort` on a `list`
```cpp
std::sort(l.begin(), l.end());   // ❌ not random-access. Use l.sort()
```

### Trap 2 — `deque` and a C API
```cpp
read(fd, d.data(), n);   // ❌ deque has no data() -- not contiguous. Use vector
```

### Trap 3 — expecting `forward_list::size()`
```cpp
auto n = fl.size();   // ❌ no such member. std::distance(fl.begin(), fl.end()) -- O(n)
```

### Trap 4 — "list is faster for middle insert" without measuring
```cpp
// For N up to a few thousand, vector.insert (a memmove) beats list.insert
// (a node malloc + cache-cold relink). Measure before choosing list
```

### Trap 5 — invalidation confusion for `deque`
```cpp
int& x = d[0];  d.push_back(9);  x = 1;   // OK -- deque push_back keeps element refs valid...
auto it = d.begin();  d.push_back(9);  ++it;  // ⚠️ ...but ITERATORS are invalidated
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`std::deque` is contiguous like `vector`" | Array of chunks — `d[i]` is 2 indirections, no `data()` |
| "`std::list` is faster for lots of middle inserts" | Only if N is large AND you hold the iterator AND rarely iterate — usually `vector` wins |
| "Linked lists are cache-friendly" | The opposite — a likely cache miss per `++it` |
| "`forward_list` has `size()`" | It doesn't — by design (O(n) or a wasted word) |
| "`std::list::sort()` is `std::sort`" | Separate member — `std::sort` needs random-access iterators |

---

## Exercises

1. **deque layout:** `std::deque<double>` with libstdc++'s 512-byte chunk — how
   many `double`s per chunk? After 1000 `push_back`s from empty, roughly how many
   chunk allocations?

   <details><summary>Answer</summary>

   512 / 8 = 64 doubles per chunk. 1000 / 64 ≈ 16 chunk allocations (+ a few map
   reallocations, which are just pointer arrays).
   </details>

2. **Stability:** you have a container of `Session` objects and hold raw
   `Session*` to some of them elsewhere. You frequently erase arbitrary sessions.
   `vector`, `deque`, or `list`?

   <details><summary>Answer</summary>

   `std::list` — erasing a node doesn't move or invalidate other elements, so the
   held `Session*` stay valid. (`vector` erase shifts everything after; `deque`
   middle-erase invalidates all.) Or: `std::vector<std::unique_ptr<Session>>` and
   hold `Session*` — the pointees are stable, only the pointer slots move.
   </details>

3. **Iteration cost:** why is `std::list<int>` iteration ~27x slower than
   `std::vector<int>` for 1M elements even though both are "walk N elements"?

   <details><summary>Answer</summary>

   `vector` — elements are contiguous; the hardware prefetcher streams them, ~1
   element per few cycles, and it auto-vectorizes. `list` — each node is at a
   random heap address; `++it` dereferences an unpredictable pointer → a cache
   miss (~100–300 cycles) almost every step, no prefetch, no SIMD.
   </details>

4. **splice:** move the last 3 elements of `std::list<int> a` to the front of
   `std::list<int> b` in O(1). Why can't `vector` do this in O(1)?

   <details><summary>Answer</summary>

   `b.splice(b.begin(), a, std::prev(a.end(), 3), a.end());` — pure relinking,
   no element moves. `vector` would have to `memmove` b to make room and copy the
   3 elements over — O(size of b + 3).
   </details>

5. **forward_list use case:** name one scenario where `forward_list` is the right
   choice over `list`.

   <details><summary>Answer</summary>

   A large number of small lists where the extra `prev` pointer per node (and per
   node × millions of nodes) is a meaningful memory cost, and you only ever
   traverse forward and prepend / insert-after — e.g. hash-table separate-chaining
   buckets in a memory-constrained setting.
   </details>

---

## Interview questions

1. `std::deque` ka internal layout — `d[i]` kitni indirections?
2. `deque` `push_back` element references invalidate karta? Iterators?
3. `std::list` iteration itni slow kyun (cache)?
4. `std::list` ka ek genuine advantage kya (stability + splice)?
5. `forward_list` mein `size()` kyun nahi?
6. "Middle insert ke liye list use karo" — kab galat hai?

---

## Next
→ [`05-map-and-set.md`](05-map-and-set.md)
