# 04 — `std::sort` internals: introsort

## Prerequisites
- [`03-sorting-algorithms.md`](03-sorting-algorithms.md) (quicksort, heapsort, insertion sort)
- Folder 19 file 11 (the algorithm's contract), folder 33 preview (optimization)

## Yeh topic abhi kyun
`std::sort` "ek quicksort hai" — yeh adhoora jawaab interview mein pakda jaata.
Actual answer: **introsort** = introspective sort = quicksort + heapsort fallback
+ insertion sort finish. Har piece ek specific weakness fix karta. Yeh samajhna
= yeh samajhna ki library-grade code kaise likha jaata.

---

## The contract (what libstdc++ promises)

- **`O(n log n)` comparisons, always** — not "average". Since C++11, worst case
  is guaranteed.
- **Not stable** (equal elements may reorder — use `std::stable_sort`).
- **Random-access iterators** required (it does `first + n/2`).
- The comparator must be a **strict weak ordering** — a broken one (`<=`) is UB
  and can read out of bounds inside the partition.

Plain quicksort is `O(n log n)` *average* but `O(n²)` worst — that violates the
contract. Introsort fixes it while keeping quicksort's speed on normal input.

---

## The three parts

```
introsort(first, last):
    depth_limit = 2 * floor(log2(last - first))
    introsort_loop(first, last, depth_limit)          // quicksort, with a depth cap
    final_insertion_sort(first, last)                  // one pass over everything

introsort_loop(first, last, depth_limit):
    while (last - first > THRESHOLD):                  // THRESHOLD = 16 in libstdc++
        if depth_limit == 0:
            heap_sort(first, last)                     // <-- the "introspective" bailout
            return
        --depth_limit
        pivot = median_of_3(first, mid, last - 1)
        cut = partition(first, last, pivot)
        introsort_loop(cut, last, depth_limit)         // recurse on the RIGHT part
        last = cut                                     // loop on the LEFT part (tail-call elimination)
    // sub-ranges <= 16 are left UNSORTED here -- the final insertion sort handles them
```

### Part 1 — quicksort (the fast common path)

- **Median-of-three pivot**: `median(a[first], a[mid], a[last-1])`. Kills the
  `O(n²)` cases on sorted / reverse-sorted / organ-pipe input that a
  first-element pivot would hit.
- **Hoare-style partition** in libstdc++ (two pointers moving inward), which does
  fewer swaps than Lomuto and handles duplicates better.
- **Recurse into one side, loop on the other** → recursion depth stays `O(log
  n)` even though it's a divide-and-conquer.

### Part 2 — heapsort (the safety net)

`depth_limit = 2·⌊log₂ n⌋`. If quicksort recurses deeper than that on some
sub-range, the pivots must be pathologically bad → **switch that sub-range to
heapsort**, which is `O(n log n)` guaranteed and `O(1)` space. This is the
"introspective" idea: watch your own recursion depth and bail to a safe
algorithm.

For n = 1,000,000: `depth_limit = 2·20 = 40`. Random input never comes close;
only crafted adversarial input triggers the fallback.

### Part 3 — insertion sort (the finisher)

`introsort_loop` **stops recursing** on ranges ≤ 16 elements, leaving them
unsorted-but-nearly-in-place (each within 16 of its final spot). One final
`insertion_sort` pass over the *whole* array then fixes all of them cheaply —
insertion sort is `O(n·k)` when every element is within `k` of home, and here
`k ≤ 16` → effectively `O(n)` with a tiny constant. Cheaper than recursing down
to size-1 ranges (recursion overhead dominates for tiny inputs).

libstdc++ actually splits this: `__final_insertion_sort` does an
`__insertion_sort` on the first 16 elements and an `__unguarded_insertion_sort`
on the rest (no bounds check on the inner loop because a smaller element is
guaranteed to exist to the left).

---

## Why each piece

| Weakness of plain quicksort | Introsort's fix |
|---|---|
| `O(n²)` on adversarial pivots | depth counter → heapsort fallback |
| `O(n²)` on sorted/reverse input with naive pivot | median-of-three |
| `O(log n)` → `O(n)` stack on bad splits | recurse smaller half, loop larger |
| Slow (recursion overhead) on tiny sub-ranges | stop at 16, finish with insertion sort |

Result: quicksort's cache-friendly speed on ~all real inputs, with a hard
`O(n log n)` ceiling.

---

## `std::stable_sort` is different

- **Merge sort**, not introsort. `O(n log n)` **if** it can allocate an `O(n/2)`
  buffer; `O(n log² n)` if allocation fails (in-place merge).
- **Stable** — equal elements keep their relative order.
- Slower and uses more memory than `std::sort`. Use only when stability is
  required.

`std::sort_heap`, `std::partial_sort` (heap-based, `O(n log k)`),
`std::nth_element` (introselect — quickselect + median-of-medians fallback,
`O(n)`) are the other members (folder 19 file 11).

---

## Andar kya hota hai

- The `THRESHOLD` of 16 and `depth_limit` of `2·log₂ n` are libstdc++ tuning
  constants (`_S_threshold`). libc++ and MSVC use similar but not identical
  values.
- `__median_of_3` reorders `a[first]`, `a[mid]`, `a[last-1]` and returns the
  middle by value; the partition then uses it as a sentinel so the inner scan
  loops need no bounds check (`while (*++i < pivot)` — pivot itself stops it).
- The comparator is a template parameter → a capture-less lambda inlines
  completely, so `std::sort(v.begin(), v.end(), [](int a, int b){ return a < b;
  })` is the same machine code as the default `<`.
- libstdc++ has vectorized small-range paths for arithmetic types on recent
  versions; the generated inner loops use SIMD compares/moves.
- Measured (`examples/01_sorting_all.cpp`, n=2,000,000, `-O2`): `std::sort`
  ~163 ms vs a decent hand-rolled quicksort ~188 ms and heapsort ~390 ms. The
  library version wins on tuning, not magic.

> **HFT relevance:** knowing `std::sort` is introsort tells you it's **safe to
> use on the hot path with untrusted data** — no `O(n²)` blowup, so no
> tail-latency surprise from a crafted feed. You still (a) `reserve` and reuse
> the buffer to avoid allocation, (b) pass a lightweight comparator or a
> projection, and (c) for fixed-width integer keys, consider a `radix sort`
> (`O(n)`, streaming) which can beat introsort by 2–3×. For "sort the book's
> few dozen levels after an update", introsort's overhead is fine but insertion
> sort (nearly-sorted → `O(n)`) is even cheaper. Never hand-roll a general sort
> for production — you'll reimplement introsort worse.

---

## Hands-on

```bash
./build.ps1 fast 20-ALGORITHMS-DSA/examples/01_sorting_all.cpp
./build.ps1 asm 20-ALGORITHMS-DSA/examples/01_sorting_all.cpp     # see std::sort inlined
```

Try to make `std::sort` go quadratic — you can't with random or sorted data
(median-of-3 + heapsort fallback). Compare your hand quicksort against
`std::sort` on: random, sorted, reverse-sorted, all-equal, organ-pipe
(`1 2 3 … n/2 … 3 2 1`). The library version should be flat across all five.

---

## ⚠️ Traps

### Trap 1 — "`std::sort` is quicksort"
```cpp
// Incomplete. It's introsort: quicksort + heapsort fallback + insertion-sort finish.
```

### Trap 2 — expecting stability from `std::sort`
```cpp
std::sort(people.begin(), people.end(), byAge);   // ⚠️ equal ages reorder. std::stable_sort
```

### Trap 3 — a non-strict-weak comparator
```cpp
std::sort(v.begin(), v.end(), [](int a, int b){ return a <= b; });   // ❌ UB -- can crash inside partition
```

### Trap 4 — assuming `std::stable_sort` is as fast / lean as `std::sort`
```cpp
// stable_sort = merge sort + O(n/2) buffer. Slower, allocates. Use only if you need stability.
```

### Trap 5 — hand-rolling a "faster" general sort
```cpp
// You'll miss median-of-3, the depth guard, or the insertion-sort finish -> slower and/or O(n^2). Use std::sort.
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`std::sort` = quicksort" | Introsort — quicksort + heapsort fallback + insertion-sort finish |
| "`std::sort` can be `O(n²)`" | Guaranteed `O(n log n)` since C++11 (the heapsort bailout) |
| "`std::stable_sort` is just `std::sort` + stability" | Different algorithm (merge sort), extra `O(n)` memory, slower |
| "The recursion goes down to size 1" | It stops at ~16; a final insertion-sort pass finishes those |
| "A lambda comparator is slower than `<`" | Inlines to identical code (template parameter) |

---

## Exercises

1. **Depth limit:** for `n = 1,048,576`, what is introsort's `depth_limit`?
   What triggers the heapsort fallback?

   <details><summary>Answer</summary>

   `2·⌊log₂(2²⁰)⌋ = 2·20 = 40`. If quicksort recurses more than 40 deep on any
   sub-range (i.e. pivots keep splitting off tiny slices), that sub-range
   switches to heapsort.
   </details>

2. **Why 16:** why does introsort stop recursing at ~16 elements instead of
   sorting them recursively?

   <details><summary>Answer</summary>

   For tiny ranges, recursion/partition overhead (pivot selection, function
   calls, stack) exceeds the cost of insertion sort, which has a minimal
   constant and is `O(n)` when elements are near their final positions (which
   they are after the quicksort passes).
   </details>

3. **Adversary:** can you construct an input that makes `std::sort` (libstdc++)
   go `O(n²)`? Why or why not?

   <details><summary>Answer</summary>

   No. Median-of-3 defeats the classic sorted/organ-pipe attacks, and the
   depth-limit → heapsort caps any remaining bad case at `O(n log n)`. (Plain
   quicksort with a fixed pivot *is* attackable.)
   </details>

4. **Comparator:** does `std::sort(v.begin(), v.end(), std::greater<>{})`
   generate different/slower code than the default?

   <details><summary>Answer</summary>

   Different (it sorts descending) but not slower — `std::greater<>` is an empty
   type, its `operator()` inlines exactly like `<`. Same instruction count.
   </details>

5. **stable_sort cost:** you sort 10M `Trade` structs by symbol and stability
   matters. What does `std::stable_sort` cost beyond `std::sort` here?

   <details><summary>Answer</summary>

   An `O(n/2)` temporary buffer allocation (~`5M · sizeof(Trade)` bytes) and
   ~1.5–2× the time (merge sort's extra memory traffic + the copy-back). If
   allocation fails it degrades to `O(n log² n)` in-place merges.
   </details>

---

## Interview questions

1. `std::sort` andar kya hai — teen parts, har ek kyun?
2. Introspective "introspection" kya hai (depth counter → heapsort)?
3. `depth_limit` kaise compute hota, kya trigger karta fallback?
4. Recursion 16 pe kyun rukti, phir kaise finish hoti?
5. `std::sort` vs `std::stable_sort` — algorithm, memory, speed?
6. `std::sort` untrusted input pe safe kyun (no `O(n²)`)?

---

## Next
→ [`05-binary-search.md`](05-binary-search.md)
