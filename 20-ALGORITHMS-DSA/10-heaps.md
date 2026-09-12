# 10 — Heaps and `priority_queue`

## Prerequisites
- [`09-trees-and-bst.md`](09-trees-and-bst.md), [`03-sorting-algorithms.md`](03-sorting-algorithms.md) (heapsort)
- Folder 19 file 07 (`std::priority_queue`), folder 19 file 12 (heap algorithms)

## Yeh topic abhi kyun
Heap = "mujhe baar-baar sabse chhota (ya bada) element chahiye" ka jawaab. `O(log n)`
insert/extract, `O(1)` peek. Event loops, Dijkstra, top-k, scheduling — sab heap
pe. Aur best part: **ek array hai** (`2i+1`, `2i+2`) — contiguous, cache-friendly,
no pointers.

---

## The structure

A **binary heap** is a **complete** binary tree (all levels full except the last,
filled left-to-right) with the **heap property**:

- **Max-heap**: every parent `≥` its children → the max is at the root.
- **Min-heap**: every parent `≤` its children → the min is at the root.

Stored **implicitly in an array** — no nodes, no pointers:

```
index:   0   1   2   3   4   5   6
value:  [9] [7] [8] [3] [6] [2] [1]

  parent(i) = (i - 1) / 2
  left(i)   = 2i + 1
  right(i)  = 2i + 2

         9(0)
        /    \
      7(1)   8(2)
     /  \    /  \
   3(3) 6(4) 2(5) 1(6)
```

It's **not sorted** — only the parent-child relation holds. The array
`[9,7,8,3,6,2,1]` is a valid max-heap; reading it left to right is meaningless
beyond `[0]`.

---

## The two operations

### `push` — insert, then sift up — `O(log n)`
Append at the end (keeps it complete), then swap with the parent while it
violates the heap property.

```cpp
void push(std::vector<int>& h, int x) {
    h.push_back(x);
    std::size_t i = h.size() - 1;
    while (i > 0) {
        std::size_t p = (i - 1) / 2;
        if (h[p] >= h[i]) break;          // max-heap: parent already >= child
        std::swap(h[p], h[i]);
        i = p;
    }
}
```

### `pop` — remove the root, then sift down — `O(log n)`
Move the last element to the root (keeps it complete), shrink, then swap with the
**larger** child while it violates.

```cpp
void pop(std::vector<int>& h) {
    h[0] = h.back();
    h.pop_back();
    std::size_t i = 0, n = h.size();
    for (;;) {
        std::size_t l = 2*i + 1, r = 2*i + 2, big = i;
        if (l < n && h[l] > h[big]) big = l;
        if (r < n && h[r] > h[big]) big = r;
        if (big == i) break;
        std::swap(h[i], h[big]);
        i = big;
    }
}
```

`examples/01_sorting_all.cpp`'s `siftDown` / `heapSort` are exactly this.

### `make_heap` — build from an array — **`O(n)`**, not `O(n log n)`
Sift-down from the last internal node (`n/2 - 1`) up to the root.

```cpp
for (std::size_t i = n / 2; i-- > 0; ) siftDown(h, i, n);
```

Why `O(n)`: a node at height `k` costs `O(k)` to sift down, and there are `≈ n /
2^(k+1)` such nodes. `Σ k · n / 2^(k+1)` converges to `2n` → `O(n)`. Building by
`n` pushes would be `O(n log n)`.

---

## `std::priority_queue`

```cpp
#include <queue>
std::priority_queue<int> maxpq;                                  // MAX-heap (top() = largest)
std::priority_queue<int, std::vector<int>, std::greater<>> minpq; // MIN-heap
```

- Adapter over a `std::vector` kept in heap order via `std::push_heap` /
  `std::pop_heap` (folder 19 files 07, 12).
- `push` `O(log n)`, `pop` `O(log n)`, `top` `O(1)`. **No iteration, no erase of
  a non-top element, no decrease-key.**
- Comparator convention is inverted-feeling: `std::less` → **max**-heap.

For "smallest pending event by timestamp" use `std::priority_queue<Event,
vector<Event>, cmp>` with `cmp` returning `a.ts > b.ts`
(`examples/06_graph_algorithms.cpp`'s Dijkstra does this).

---

## What heaps are for

- **Dijkstra / Prim** — repeatedly extract the min-distance frontier node
  (`examples/06`). `O((V+E) log V)`.
- **Top-k** — a **min-heap of size k**: push each element, pop if `size > k`; the
  heap ends holding the k largest. `O(n log k)`, `O(k)` space (better than
  sorting when `k ≪ n`; `std::partial_sort` / `std::nth_element` also do this,
  folder 19 file 11).
- **Merge k sorted lists** — a heap of the k current heads. `O(N log k)`.
- **Running median** — two heaps (a max-heap of the lower half, a min-heap of the
  upper half), rebalanced so their sizes differ by ≤ 1. Median in `O(1)`, insert
  `O(log n)`.
- **Event-driven simulation / schedulers** — the timeline is a min-heap by time.

---

## When `priority_queue` isn't enough

- **Need to cancel / update a pending item** (decrease-key): `std::priority_queue`
  can't. Options: (a) **lazy deletion** — mark it cancelled in a set, skip on
  `pop`; (b) an **indexed heap** — an array heap plus `pos[id] → heap index`,
  giving `O(log n)` erase and decrease-key.
- **Need to iterate** all pending items: use a different structure or keep a
  parallel container.
- **d-ary heap** (4 or 8 children per node) — shallower (`log_d n` levels) →
  fewer sift-down steps and better cache use per level; common in
  performance-tuned Dijkstra.

---

## Andar kya hota hai

- The heap array is one contiguous `std::vector` → `push`/`pop` walk parent↔child
  index pairs that, **for the top few levels, share cache lines**. Only the
  deepest sift-down steps miss. Far better locality than a pointer-linked tree or
  `std::multiset` for the same "get min repeatedly" job — this is why
  `std::priority_queue` beats a `std::set`-based priority queue.
- `push` sift-up touches ≤ `log n` elements but usually stops early (a random new
  element rarely bubbles far). `pop` sift-down almost always goes the full `log
  n` (the element moved to the root came from a leaf → it's small → it sinks).
- `make_heap`'s `O(n)`: most nodes are near the bottom (cheap sift-downs); only
  `O(1)` nodes near the root cost `O(log n)`.
- A `d`-ary heap with `d = 4` cuts levels to `log₄ n = ½ log₂ n` → half the
  sift-down depth; each level compares `d` children (more work per level, but
  they're contiguous). Net win when comparisons are cheap and memory latency
  dominates.

> **HFT relevance:** `std::priority_queue<Event, vector, cmp>` is the standard
> **event-loop / discrete-event-simulation** timeline — pop the next event,
> process it, push any it spawns. Contiguous array storage keeps it fast. Its
> limits bite: no cancel (an order pulled before its timer fires), no peek past
> `top()`. Production engines use **lazy deletion** (a `cancelled` set, skip on
> pop) or a hand-rolled **indexed binary/4-ary heap** with a `pos[id]` map for
> `O(log n)` erase and decrease-key. Everything preallocated (`reserve` the
> vector to the max event count). Top-k / running-median heaps show up in signal
> and risk aggregation.

---

## Hands-on

```bash
./build.ps1 20-ALGORITHMS-DSA/examples/06_graph_algorithms.cpp   # Dijkstra with std::priority_queue
./build.ps1 fast 20-ALGORITHMS-DSA/examples/01_sorting_all.cpp    # heapSort / siftDown
```

Implement: `push`/`pop`/`makeHeap` for a max-heap over `std::vector<int>`; top-k
with a size-k min-heap; running median with two heaps. Then add lazy deletion to
a `priority_queue`-based event loop.

---

## ⚠️ Traps

### Trap 1 — `std::priority_queue<int>` is a min-heap
```cpp
std::priority_queue<int> pq;   // ⚠️ MAX-heap. top() is the LARGEST. Use std::greater<> for a min-heap
```

### Trap 2 — building a heap with n pushes
```cpp
for (int x : v) pq.push(x);   // O(n log n). std::make_heap(v.begin(), v.end()) is O(n)
```

### Trap 3 — expecting the underlying array to be sorted
```cpp
// A heap is NOT sorted -- only heap-ordered. Draining it with pop() repeatedly IS heapsort (O(n log n)).
```

### Trap 4 — trying to erase / update a non-top element
```cpp
// std::priority_queue has no such operation. Lazy-delete, or use an indexed heap.
```

### Trap 5 — top-k with a max-heap of all n
```cpp
// O(n) build + k pops = O(n + k log n). A size-k MIN-heap is O(n log k) and O(k) space -- better for k << n.
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "A heap is a sorted array" | Heap-ordered only — parent vs children; siblings unordered |
| "`priority_queue<int>` gives the smallest" | It gives the largest (max-heap default) |
| "Building a heap is `O(n log n)`" | `make_heap` is `O(n)` (Floyd's bottom-up build) |
| "`priority_queue` supports decrease-key" | It doesn't — lazy-delete or use an indexed heap |
| "Heap = tree with pointers" | Implicit array (`2i+1`/`2i+2`) — no nodes, cache-friendly |

---

## Exercises

1. **Sift trace:** insert `5, 3, 8, 1, 9, 2` into an empty max-heap one at a
   time. Final array?

   <details><summary>Answer</summary>

   5 → [5]. 3 → [5,3]. 8 → [8,3,5] (8 sifts past 5). 1 → [8,3,5,1]. 9 →
   [9,8,5,1,3] (9 sifts up past 3 then 8). 2 → [9,8,5,1,3,2]. Root = 9. ✓
   </details>

2. **Top-k:** 100M numbers streaming, keep the 10 largest. Structure, complexity,
   memory?

   <details><summary>Answer</summary>

   A size-10 **min-heap**. For each number: if `heap.size() < 10` push; else if
   `x > heap.top()` pop and push `x`. `O(n log 10)` time, `O(10)` memory. The
   heap's top is the 10th-largest so far.
   </details>

3. **Running median:** you get numbers one at a time and must report the median
   after each. Design.

   <details><summary>Answer</summary>

   `maxHeap` (lower half) + `minHeap` (upper half). Insert into `maxHeap`, then
   move its top to `minHeap`; if `minHeap` bigger, move its top back. Keep sizes
   equal or `maxHeap` one bigger. Median = `maxHeap.top()` (odd count) or the
   average of both tops (even). Insert `O(log n)`, query `O(1)`.
   </details>

4. **Cancel an event:** your `priority_queue` timeline must cancel event `id`
   before it fires. Two approaches, with trade-offs.

   <details><summary>Answer</summary>

   (a) **Lazy**: `unordered_set<int> cancelled`; on `pop`, skip if present.
   Simple; wastes heap space until the event would have fired; `top()` may be a
   ghost. (b) **Indexed heap**: array heap + `pos[id] → index`; erase = swap with
   last, pop, sift. `O(log n)`, exact, more code.
   </details>

5. **d-ary:** why might a 4-ary heap beat a binary heap for Dijkstra on a large
   graph?

   <details><summary>Answer</summary>

   Levels drop to `log₄ n = ½ log₂ n` → sift-down (the dominant cost, done on
   every `pop`) is half as deep. Each level compares 4 contiguous children
   instead of 2 — more comparisons but they're in the same cache line. When
   memory latency dominates and comparisons are cheap (ints), the shallower tree
   wins.
   </details>

---

## Interview questions

1. Binary heap array mein kaise store hota (`2i+1`, `2i+2`), sorted kyun nahi?
2. `push` / `pop` `O(log n)` kaise (sift up / down)?
3. `make_heap` `O(n)` kyun, `n` pushes `O(n log n)` kyun?
4. `std::priority_queue<int>` max ya min? Min-heap kaise?
5. Top-k ke liye size-k min-heap vs sab ka max-heap — kaunsa, kyun?
6. `priority_queue` mein decrease-key / cancel — kya karte (lazy / indexed heap)?

---

## Next
→ [`11-tries.md`](11-tries.md)
