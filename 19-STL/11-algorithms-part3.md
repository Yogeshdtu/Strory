# 11 — Algorithms part 3: sorting family

## Prerequisites
- [`10-algorithms-part2.md`](10-algorithms-part2.md)
- [`08-iterators.md`](08-iterators.md) (random-access requirement), folder 20 preview (quicksort, heapsort)

## Yeh topic abhi kyun
Sorting itna common hai ki iske paas ek poora family hai — `sort`, `stable_sort`,
`partial_sort`, `nth_element`, `partition`. Har ek alag guarantee aur alag cost
deta. Galat waala chunne se ya to O(n log n) jahan O(n) chahiye tha, ya wrong
result. Sab **random-access iterators** maangte — `std::list` ke liye
`list::sort()` member hai.

---

## `std::sort` — the default

```cpp
std::sort(first, last);                                  // ascending, operator<
std::sort(first, last, std::greater<>{});                // descending
std::sort(first, last, [](const Order& a, const Order& b){ return a.price < b.price; });
std::ranges::sort(v);                                    // C++20 -- whole range
std::ranges::sort(v, {}, &Order::price);                 // C++20 projection: sort by member, default comparator
```

- **O(n log n)** — *guaranteed* worst case since C++11 (it's **introsort**:
  quicksort, switching to heapsort when recursion goes too deep, insertion sort
  for tiny sub-ranges). No more O(n²) adversarial input like old qsort.
- **Not stable** — equal elements may be reordered. Use `stable_sort` if relative
  order of equal keys matters.
- In place, O(log n) stack. Comparator must be a **strict weak ordering** (same
  rule as `map` — file 05; `<=` → UB, can crash or read out of bounds).

## `std::stable_sort` — keeps equal elements in original order

```cpp
std::stable_sort(first, last, byPrice);   // orders with equal price stay in insertion order
```

- **O(n log n)** if extra memory is available, **O(n log² n)** if not (it's a
  merge sort; needs a temp buffer of n/2).
- Slower and more memory than `std::sort`. Use only when stability is required —
  e.g. sort by secondary key, then `stable_sort` by primary key to get a
  lexicographic order without a compound comparator.

## `std::partial_sort` — top k, sorted

```cpp
std::partial_sort(first, first + k, last);   // the k smallest elements end up sorted in [first, first+k);
                                             // the rest [first+k, last) is unspecified order
```

- **O(n log k)** — builds a heap of the first k, then scans the rest swapping in
  anything smaller. Much cheaper than a full sort when `k << n` ("show the 10
  best bids of 100000").
- `std::partial_sort_copy(first, last, d_first, d_last)` — writes the sorted top
  `d_last - d_first` into a separate range (source untouched, source can be
  input-only).

## `std::nth_element` — the k-th element in place, O(n) average

```cpp
std::nth_element(first, first + k, last);
// after: *(first + k) is the element that WOULD be at index k if fully sorted.
// [first, first+k) are all <= it; [first+k+1, last) are all >= it. Neither part is sorted.
```

- **O(n) average** (quickselect — partition, recurse into one side only). The
  fastest way to get "the median" or "the 95th-percentile latency" without
  sorting everything.
- Combine: `nth_element` to isolate the top k region, then `sort` just that
  region if you also need it ordered → O(n) + O(k log k).

## `std::partition` / `std::stable_partition`

```cpp
auto mid = std::partition(first, last, pred);   // rearranges so [first, mid) satisfy pred, [mid, last) don't. Not stable. O(n)
auto mid2= std::stable_partition(first, last, pred);   // same but preserves relative order within each group. O(n) with buffer / O(n log n) without
```

Also: `std::is_sorted(first, last)`, `std::is_sorted_until(...)`,
`std::is_partitioned(...)`.

---

## Choosing

| You need | Use | Cost |
|---|---|---|
| Everything sorted | `std::sort` | O(n log n) |
| Everything sorted, equal keys keep order | `std::stable_sort` | O(n log n) + buffer |
| Only the k smallest, and sorted | `std::partial_sort` | O(n log k) |
| Only *which* element is at rank k (median / percentile) | `std::nth_element` | O(n) avg |
| Split into "matches / doesn't", order irrelevant | `std::partition` | O(n) |
| ...split, order preserved | `std::stable_partition` | O(n) + buffer |

Don't `std::sort` to find a max (`max_element`, O(n)), a median (`nth_element`,
O(n)), or the top 10 (`partial_sort`, O(n log 10)).

---

## Sorting your own types

```cpp
struct Quote { double px; std::uint32_t qty; std::int64_t ts; };

// option A: comparator lambda
std::sort(v.begin(), v.end(), [](const Quote& a, const Quote& b){
    if (a.px != b.px) return a.px > b.px;      // price desc
    return a.ts < b.ts;                         // then oldest first
});

// option B (C++20): projection
std::ranges::sort(v, std::greater<>{}, &Quote::px);   // by px descending

// option C: give the type operator<=>  (folder 22) -> std::sort(v) just works
```

For sorting a `std::vector<BigObject>`, sort a `std::vector<int> indices` by a
comparator that indexes into the objects, or sort `std::vector<std::pair<Key,
Handle>>` — avoids moving big objects around (though `std::sort` uses moves, not
copies, so it's often fine).

---

## Andar kya hota hai

- **Introsort** (`std::sort`): median-of-3 pivot quicksort; if depth exceeds
  `2·⌊log₂ n⌋`, the current sub-range switches to **heapsort** (guarantees O(n log
  n) worst case); sub-ranges below ~16 elements are left for a final **insertion
  sort** pass (insertion sort is faster than quicksort's overhead on tiny,
  nearly-sorted data). This is why `std::sort` beats a textbook quicksort.
- **`nth_element`**: quickselect — partition around a pivot, then recurse **only**
  into the side containing rank k. Average work `n + n/2 + n/4 + … ≈ 2n` = O(n).
  Falls back to median-of-medians-ish safeguards to avoid O(n²).
- **`partial_sort`**: `make_heap` on `[first, first+k)` (a max-heap), then for
  each remaining element, if it's `< heap top`, replace the top and sift down.
  End with `sort_heap`. O(n log k).
- All are templated on the comparator; a lambda comparator with no captures
  inlines completely — `std::sort` on `std::vector<int>` is as fast as a
  hand-tuned integer sort, and libstdc++ has vectorized small-range paths.

> **HFT relevance:** picking the minimal-cost member matters when it's per-tick.
> "Best N levels for the UI" → `partial_sort` / `nth_element`, not `sort`.
> Latency-percentile reporting (p50/p99) over a batch of samples → `nth_element`
> (O(n)), never a full sort. Keeping a book sorted → you don't re-`sort` each
> update; you **insert in order** (`lower_bound` + `insert`, or a structure that
> stays sorted). `std::sort`'s guaranteed O(n log n) worst case (introsort)
> removes the old qsort DoS risk on adversarial input. Comparator must be a
> strict weak ordering — a bad one is UB that has crashed production sorts.

---

## Hands-on

```bash
./build.ps1 19-STL/examples/03_algorithms_tour.cpp
```

It demos `sort`, `nth_element`, `partial_sort`, `partition`, `binary_search`.
Add: time `std::sort` vs `std::partial_sort(…, +10, …)` vs `std::nth_element(…,
+n/2, …)` on a shuffled `std::vector<int>` of 1,000,000 with `-O2` — see the
O(n log n) vs O(n log k) vs O(n) gap.

---

## ⚠️ Traps

### Trap 1 — `std::sort` on a `std::list` / `std::set`
```cpp
std::sort(lst.begin(), lst.end());   // ❌ not random-access. lst.sort(). (set is always sorted anyway)
```

### Trap 2 — non-strict-weak comparator
```cpp
std::sort(v.begin(), v.end(), [](int a, int b){ return a <= b; });   // ❌ <= -> UB (may read out of bounds / crash)
```

### Trap 3 — assuming `std::sort` is stable
```cpp
std::sort(records.begin(), records.end(), byLastName);   // ⚠️ equal last names get arbitrary order. stable_sort if that matters
```

### Trap 4 — `nth_element` / `partial_sort` and expecting the whole range sorted
```cpp
std::nth_element(v.begin(), v.begin() + k, v.end());
// v is NOT sorted -- only v[k] is in its final place; the two sides are just partitioned
```

### Trap 5 — full `sort` where a cheaper op exists
```cpp
std::sort(v.begin(), v.end()); int med = v[v.size()/2];   // ⚠️ O(n log n) for a median. nth_element -> O(n)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`std::sort` can be O(n²) on bad input" | Introsort → guaranteed O(n log n) worst case since C++11 |
| "`std::sort` is stable" | Not stable — use `std::stable_sort` |
| "`nth_element` sorts the range" | Only places the k-th element; the rest is merely partitioned |
| "To get the top 10 I sort then take 10" | `partial_sort(…, first+10, …)` → O(n log 10) |
| "Comparator just needs to return a bool" | Must be a strict weak ordering — `<`, never `<=` |

---

## Exercises

1. **Which algorithm:** (a) p99 of 100k latency samples, (b) the 20 cheapest
   offers of 5000, sorted, (c) sort trades by symbol keeping same-symbol trades
   in arrival order, (d) move all "urgent" orders to the front, order among them
   irrelevant.

   <details><summary>Answer</summary>

   (a) `std::nth_element(v.begin(), v.begin()+ (size*99/100), v.end())` then read
   that element — O(n). (b) `std::partial_sort(v.begin(), v.begin()+20, v.end())`
   — O(n log 20). (c) `std::stable_sort(v.begin(), v.end(), bySymbol)`. (d)
   `std::partition(v.begin(), v.end(), isUrgent)` — O(n).
   </details>

2. **Introsort:** what two things does `std::sort` do that a plain recursive
   quicksort doesn't, and why each?

   <details><summary>Answer</summary>

   (1) Switches to **heapsort** once recursion depth exceeds ~2 log n → caps
   worst case at O(n log n) (defeats adversarial / already-sorted-with-bad-pivot
   inputs). (2) Stops recursing on small sub-ranges and finishes with one
   **insertion sort** pass → less overhead on tiny/nearly-sorted data.
   </details>

3. **nth_element then sort:** you need the 50 smallest of 1,000,000, sorted.
   Two-step plan and total complexity?

   <details><summary>Answer</summary>

   `std::nth_element(b, b+50, e);` (O(n) — now the 50 smallest occupy `[b, b+50)`
   unsorted) then `std::sort(b, b+50);` (O(50 log 50)). Total ≈ O(n). Beats
   `partial_sort` slightly and a full `sort` massively.
   </details>

4. **Stability trick:** you want records sorted by `(dept asc, salary desc)`
   without writing a compound comparator. How, using stability?

   <details><summary>Answer</summary>

   `std::stable_sort(v.begin(), v.end(), bySalaryDesc);` then
   `std::stable_sort(v.begin(), v.end(), byDeptAsc);` — the second sort preserves
   the salary order within each dept. (Sort by the *least* significant key first.)
   </details>

5. **Index sort:** sort a `std::vector<HugeStruct> data` by `data[i].key` without
   moving `HugeStruct`s. Produce `std::vector<std::size_t> order`.

   <details><summary>Answer</summary>

   `std::vector<std::size_t> order(data.size()); std::iota(order.begin(),
   order.end(), 0); std::sort(order.begin(), order.end(), [&](size_t a, size_t b){
   return data[a].key < data[b].key; });` — then iterate `data[order[i]]`.
   </details>

---

## Interview questions

1. `std::sort` andar kya hai (introsort) — worst case guarantee kya?
2. `std::sort` stable hai? Nahi to kya use karein?
3. `nth_element` kya karta, complexity, kab use?
4. `partial_sort` vs full `sort` — top-k ke liye kaunsa, kyun?
5. Comparator "strict weak ordering" na ho to (`<=`) kya hota?
6. `std::list` ko `std::sort` se kyun nahi kar sakte?

---

## Next
→ [`12-algorithms-part4.md`](12-algorithms-part4.md)
