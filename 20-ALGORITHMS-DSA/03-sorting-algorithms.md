# 03 — Sorting algorithms

## Prerequisites
- [`01-complexity-analysis.md`](01-complexity-analysis.md)
- Folder 19 file 11 (the `std::sort` family), folder 07 (recursion)

## Yeh topic abhi kyun
Sorting sabse padha-jaane wala algorithm topic hai — aur ek achha teacher bhi,
kyunki har design idea (divide-and-conquer, in-place vs scratch, stable vs not,
comparison vs non-comparison) yahan dikhta hai. Aap `std::sort` use karoge, par
uske **internals** (file 04) samajhne ke liye pehle building blocks.

`examples/01_sorting_all.cpp` mein har ek implement + benchmarked hai.

---

## The O(n²) family — simple, in place, slow

### Bubble sort
Repeatedly swap adjacent out-of-order pairs; largest "bubbles" to the end each
pass. Early-exit if a pass makes no swaps (→ `O(n)` on already-sorted input).
Otherwise `O(n²)`. **Never use it** — it's here as the canonical bad example.

### Selection sort
Each pass, find the minimum of the unsorted suffix and swap it to the front.
Always `O(n²)` comparisons, but only `O(n)` **swaps** — useful when a swap is
very expensive (huge objects) and comparisons are cheap.

### Insertion sort
Take each element, shift the sorted prefix right until it fits, drop it in.
`O(n²)` worst, but **`O(n)` on nearly-sorted data** and tiny constant factor →
**this is what `std::sort` uses to finish small sub-ranges** (≤ ~16 elements).
Also **stable** and **online** (can sort as elements arrive).

Measured (`examples/01`, n=20,000 random, `-O2`): bubble ~730 ms, selection
~590 ms, insertion ~58 ms, `std::sort` ~1 ms.

---

## Merge sort — divide and conquer, stable, O(n) scratch

1. Split the array in half.
2. Recursively sort each half.
3. **Merge** the two sorted halves with a linear two-pointer pass into a scratch
   buffer, copy back.

```
[5 2 8 1 9 3 7 4]
   split -> [5 2 8 1] [9 3 7 4]
   ...recursively... -> [1 2 5 8] [3 4 7 9]
   merge -> [1 2 3 4 5 7 8 9]
```

- **`O(n log n)` guaranteed** — `log n` levels, `O(n)` merge work per level.
- **Stable** — the merge takes from the left half on ties (`a[j] < a[i]` is
  strict).
- **Not in place** — needs an `O(n)` scratch buffer.
- **Predictable** — no bad inputs. Good when worst-case matters or when data
  doesn't fit in memory (external merge sort) or is a linked list (`list::sort`
  is a merge sort — no random access needed).

Measured (`examples/01`, n=2,000,000): merge ~262 ms vs `std::sort` ~163 ms.

---

## Quicksort — divide and conquer, in place, pivot-sensitive

1. Pick a **pivot**.
2. **Partition**: rearrange so elements ≤ pivot are left, > pivot are right; the
   pivot lands in its final position `p`.
3. Recurse on `[lo, p)` and `[p+1, hi]`.

- **`O(n log n)` average**, `O(log n)` stack (if you recurse into the smaller
  half and loop on the larger).
- **`O(n²)` worst case** — a pivot that always splits off one element (e.g. first
  element as pivot on already-sorted data).
- **Not stable**. **In place**.
- Usually the **fastest** comparison sort in practice — great cache behaviour
  (partition is a linear scan), small constant.

### Pivot choice is everything
- First/last element → `O(n²)` on sorted/reverse-sorted input.
- **Median-of-three** (first, middle, last) → kills the common bad cases.
- **Random pivot** → expected `O(n log n)` on any input, but a call to the RNG
  per partition.

`examples/01_sorting_all.cpp` had a real bug here: after sorting the three
candidates it picked `a[hi]` (the **max** of the three, not the median) → `O(n²)`
on sorted-with-duplicates input, hung the benchmark. Fixed by swapping the median
into `a[hi]` before partitioning. **This is exactly why pivot logic gets tested.**

Measured (fixed, n=2,000,000): random input ~188 ms; already-sorted input ~50 ms
(median-of-3 keeps it `O(n log n)`).

---

## Heapsort — in place, O(n log n) guaranteed, cache-unfriendly

1. Build a max-heap from the array in `O(n)` (sift-down from the last internal
   node up).
2. Repeatedly swap the root (max) with the last element, shrink the heap by 1,
   sift the new root down.

- **`O(n log n)` guaranteed**, **`O(1)` space**, **in place**.
- **Not stable**.
- **Slower than quicksort in practice** (~2× in `examples/01`: heap ~390 ms vs
  quick ~188 ms) — sift-down jumps around the array (`2i+1`, `2i+2`) → poor
  locality, and it does more comparisons.
- Its value: the `O(n log n)` **worst-case guarantee** with `O(1)` space → it's
  the fallback introsort switches to when quicksort recursion goes too deep.

---

## Non-comparison sorts — O(n) when they apply

Beat the `Ω(n log n)` comparison lower bound by **not comparing**:

- **Counting sort**: values in a small range `[0, K)`. Count occurrences, then
  emit. `O(n + K)` time, `O(K)` space. Stable variant via prefix sums over the
  counts.
- **Radix sort**: sort by digit/byte, least-significant first, using a stable
  counting sort per pass. `O(d·(n + b))` for `d` digits of base `b`. Great for
  fixed-width integers.
- **Bucket sort**: distribute into buckets by value range, sort each, concatenate.
  `O(n)` expected for uniform data.

These need structure in the keys (bounded range, fixed width). For 32-bit ints,
a 4-pass byte radix sort can beat `std::sort`. Not comparison-based → the
`n log n` bound doesn't apply.

---

## Choosing

| Situation | Sort |
|---|---|
| Default, general data | `std::sort` (introsort) |
| Need stability | `std::stable_sort` (merge sort) |
| Nearly-sorted, or n ≤ ~32 | insertion sort (`std::sort` does this internally) |
| Worst-case `O(n log n)` + `O(1)` space required | heapsort |
| Keys are small-range integers / fixed-width | counting / radix sort — `O(n)` |
| External (doesn't fit in RAM) / linked list | merge sort |
| Only the top-k / the median needed | `partial_sort` / `nth_element` — not a full sort |

---

## Andar kya hota hai

- **Insertion sort's small constant**: the inner loop is a shift (`a[j] =
  a[j-1]`) — one load, one store, one compare, fully in cache for a tiny range,
  branch-predicted well on sorted-ish data. For n ≤ ~16 it beats the
  bookkeeping overhead of recursion/heapify.
- **Quicksort's cache win**: partition is a single forward scan with two write
  positions — streaming access, prefetcher-friendly. Merge sort's merge is also
  streaming but writes to a *separate* buffer (double the memory traffic) and
  copies back.
- **Heapsort's cache loss**: `siftDown` follows `2i+1`/`2i+2` — for a large heap
  the children are far from the parent in memory → cache misses, and the number
  grows with `log n` per element.
- **`O(n)` build-heap**: sift-down from height `h` costs `O(h)`; summing `(nodes
  at height h) × h` telescopes to `O(n)`, not `O(n log n)`.

> **HFT relevance:** you almost always call `std::sort` / `std::stable_sort` /
> `nth_element` — they're introsort-backed (bounded worst case) and vectorized.
> Where you *do* hand-roll: **radix sort** on fixed-width integer keys (order
> ids, timestamps, price ticks) can beat `std::sort` by 2–3× because it's `O(n)`
> and touches memory in a streaming pattern; **insertion sort** for keeping a
> tiny already-mostly-sorted structure ordered (a handful of price levels after
> one update — `O(n)` on nearly-sorted, no allocation). The lesson from the
> quicksort pivot bug: any hand-rolled sort needs adversarial-input tests, or a
> bad day becomes an `O(n²)` latency spike.

---

## Hands-on

```bash
./build.ps1 fast 20-ALGORITHMS-DSA/examples/01_sorting_all.cpp
```

Implement each yourself before reading the example. Then: add a counting sort for
values in `[0, 1000)` and time it against `std::sort` for n=1,000,000 — it should
win. Feed your quicksort an all-equal array and a sorted array; if either is
slow, your partition/pivot is wrong.

---

## ⚠️ Traps

### Trap 1 — quicksort pivot = first/last element
```cpp
int pivot = a[hi];   // ⚠️ O(n^2) on sorted / reverse-sorted input. median-of-3 or random
```

### Trap 2 — median-of-3 picking the max instead of the median
```cpp
// sort a[lo],a[mid],a[hi], then use a[hi] as pivot -> that's the MAX -> O(n^2) on sorted input.
// swap the median into the pivot slot first (examples/01 had exactly this bug).
```

### Trap 3 — merge sort losing stability
```cpp
buf[k++] = (a[i] <= a[j]) ? a[i++] : a[j++];   // ⚠️ take from RIGHT on ties -> unstable.
buf[k++] = (a[j] <  a[i]) ? a[j++] : a[i++];   // ✅ take from LEFT on ties -> stable
```

### Trap 4 — recursing into both halves without tail-looping
```cpp
quickSort(lo, p-1); quickSort(p+1, hi);   // ⚠️ O(n) stack worst case. recurse smaller, loop larger -> O(log n)
```

### Trap 5 — using bubble/selection sort in real code
```cpp
// n=20000 -> ~600-730 ms vs std::sort ~1 ms (examples/01). Just call std::sort.
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Quicksort is always `O(n log n)`" | Average yes; `O(n²)` worst case with bad pivots |
| "Merge sort is in place" | It needs `O(n)` scratch; `stable_sort` uses a buffer |
| "Heapsort is fast" | `O(n log n)` guaranteed but ~2× quicksort in practice (cache) |
| "You can't sort faster than `n log n`" | Comparison sorts can't; counting/radix are `O(n)` for bounded/fixed-width keys |
| "insertion sort is useless" | It's the tail of `std::sort` — best for tiny / nearly-sorted ranges |

---

## Exercises

1. **Stability:** sort `[(3,'a'), (1,'b'), (3,'c'), (1,'d')]` by the number.
   What does a stable sort give? An unstable one?

   <details><summary>Answer</summary>

   Stable: `(1,'b') (1,'d') (3,'a') (3,'c')` — equal keys keep input order.
   Unstable: same keys, but `'b'/'d'` and `'a'/'c'` may be in either order.
   </details>

2. **Pivot disaster:** your quicksort uses `a[lo]` as pivot. Give an input of
   size `n` that makes it `O(n²)`, and explain.

   <details><summary>Answer</summary>

   An already-sorted array. `a[lo]` is the minimum → partition puts 0 elements
   left, `n-1` right → recursion depth `n`, each level `O(n)` → `O(n²)`.
   Reverse-sorted does the same.
   </details>

3. **Counting sort:** write it for `std::vector<int>` with values in `[0, K)`.
   Complexity and when it beats `std::sort`.

   <details><summary>Answer</summary>

   `std::vector<int> cnt(K,0); for (int x : a) ++cnt[x]; std::size_t w=0; for
   (int v=0;v<K;++v) while (cnt[v]--) a[w++]=v;` — `O(n + K)`. Beats `std::sort`
   when `K = O(n)` (or smaller), e.g. ages, small enums, byte values.
   </details>

4. **Merge vs quick:** you must sort 10 GB of records that don't fit in RAM.
   Which algorithm's structure do you use, and why?

   <details><summary>Answer</summary>

   External **merge sort**: sort chunks that fit in RAM, write sorted runs to
   disk, then k-way merge the runs with sequential reads. Merge only needs
   sequential access and bounded memory; quicksort needs random access to the
   whole array.
   </details>

5. **Introsort sketch:** how does `std::sort` combine quicksort, heapsort, and
   insertion sort?

   <details><summary>Answer</summary>

   Quicksort with median-of-3; if recursion depth exceeds ~`2·log₂ n`, switch
   that sub-range to **heapsort** (caps worst case at `O(n log n)`); stop
   recursing on ranges ≤ ~16 elements and finish with a single **insertion
   sort** pass over the whole array (cheap on nearly-sorted data). Detail in
   file 04.
   </details>

---

## Interview questions

1. Merge sort vs quicksort — stability, space, worst case, practical speed?
2. Quicksort `O(n²)` kab, kaise avoid (pivot strategies)?
3. Heapsort ka guarantee kya, practically slow kyun?
4. `Ω(n log n)` comparison-sort bound — counting/radix kaise bypass karte?
5. `std::sort` chhoti sub-ranges insertion sort se kyun finish karta?
6. Stable sort kab chahiye — ek real example?

---

## Next
→ [`04-std-sort-internals.md`](04-std-sort-internals.md)
