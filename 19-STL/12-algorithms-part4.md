# 12 — Algorithms part 4: binary search, set ops, heap ops, permutations

## Prerequisites
- [`11-algorithms-part3.md`](11-algorithms-part3.md) (sorting — all of these need **sorted** input)
- [`05-map-and-set.md`](05-map-and-set.md)

## Yeh topic abhi kyun
Ye saare algorithms **ek sorted range** maangte hain (ya heap ke case mein heap
order). Iska matlab: agar aapka data pehle se sorted `std::vector` mein hai, to
aapko `std::set`/`std::map` ki zaroorat hi nahi — `lower_bound` + merge ops sab
kuch de dete, contiguous speed ke saath. HFT order books isi pattern pe bane hote.

---

## Binary search on a sorted range — O(log n)

```cpp
// PRECONDITION: [first, last) is sorted by the same comparator.

bool present = std::binary_search(first, last, value);          // just yes/no

auto lo = std::lower_bound(first, last, value);   // first element >= value  (insertion point to keep sorted)
auto hi = std::upper_bound(first, last, value);   // first element  > value
auto [a, b] = std::equal_range(first, last, value);             // {lower_bound, upper_bound} -- the run of == value

// with a comparator / projection:
auto it = std::lower_bound(v.begin(), v.end(), key,
                           [](const Level& lvl, double k){ return lvl.price < k; });   // note: (element, value)
```

Uses:
- **Membership**: `binary_search` (or `lower_bound` then check `it != last && *it
  == value`).
- **Count of a value**: `std::distance(lower_bound, upper_bound)` (or
  `equal_range`).
- **Sorted insert**: `v.insert(std::lower_bound(v.begin(), v.end(), x), x)` —
  keeps the vector sorted. The `lower_bound` is O(log n); the `insert` shift is
  O(n) (still often beats `std::set` for n up to ~10⁴ — file 25).
- **Range scan**: everything in `[keyLo, keyHi]` = `[lower_bound(keyLo),
  upper_bound(keyHi))`.

`lower_bound` on a `std::list`/`std::set` iterator range **compiles** but is O(n)
(no random access → can't halve). Use `set::lower_bound` (the member) — it walks
the tree in O(log n). `map`/`set` members `find`, `count`, `lower_bound`,
`upper_bound`, `equal_range`, `contains` mirror these.

## Merge & set operations — O(n + m), sorted inputs → sorted output

```cpp
// both inputs sorted; output is sorted:
std::merge(a, ae, b, be, out);                    // all elements of both, merged (keeps duplicates)
std::set_union(a, ae, b, be, out);               // in A or B (dups collapsed to max multiplicity)
std::set_intersection(a, ae, b, be, out);        // in both
std::set_difference(a, ae, b, be, out);          // in A but not B
std::set_symmetric_difference(a, ae, b, be, out);// in exactly one
bool sub = std::includes(a, ae, b, be);          // is B a subset of A?

std::inplace_merge(first, middle, last);          // merge two consecutive sorted sub-ranges in place (used by stable_sort)
```

`out` is usually `std::back_inserter(result)`. These are the array-based
equivalent of `std::set` operations — if your sets are sorted vectors, these are
**much** faster than building `std::set`s and iterating.

## Heap operations — `<algorithm>`, operate on a `std::vector`

```cpp
std::make_heap(first, last);              // O(n) -- rearrange into a max-heap ([first] is the max)
std::push_heap(first, last);              // element just appended at last-1 is sifted up   -- O(log n)
std::pop_heap(first, last);               // moves the max to last-1, restores heap on [first, last-1) -- O(log n)
                                         //   (then you call container.pop_back())
std::sort_heap(first, last);              // turns a heap into a sorted range -- O(n log n)  (this is heapsort)
bool h = std::is_heap(first, last);
auto e = std::is_heap_until(first, last);
```

This is exactly what `std::priority_queue` (file 07) uses internally — use these
directly when you want heap behaviour **plus** occasional full access to the
underlying vector (which `priority_queue` hides).

## Permutations

```cpp
std::next_permutation(first, last);      // rearranges to the next lexicographic permutation; returns false + wraps to sorted when it was the last
std::prev_permutation(first, last);
// enumerate all n! permutations:
std::sort(v.begin(), v.end());
do { use(v); } while (std::next_permutation(v.begin(), v.end()));

bool p = std::is_permutation(a, ae, b);  // is b a reordering of a? O(n²) worst
```

---

## The "sorted vector as a set/map" pattern

```cpp
struct Level { double price; std::uint64_t qty; };
std::vector<Level> book;   // kept sorted by price

// lookup:
auto it = std::lower_bound(book.begin(), book.end(), px,
                           [](const Level& l, double p){ return l.price < p; });
bool found = (it != book.end() && it->price == px);

// insert new price level, staying sorted:
book.insert(it, Level{px, q});          // O(n) shift, but contiguous memcpy -- fast for small books

// best bid / best ask: book.back() / book.front()   -- O(1)
// range [pLo, pHi]:  [lower_bound(pLo) .. upper_bound(pHi))
```

Beats `std::map<double, Level>` for realistic book sizes: contiguous (few cache
misses), `back()`/`front()` are O(1), and merge ops are available. Downside: mid
insert/erase is O(n) — fine when books are small (tens–hundreds of levels) or
when you mostly update existing levels in place.

---

## Andar kya hota hai

- `lower_bound`: classic binary search — `count = last - first`; repeatedly halve
  (`step = count/2`, `mid = first + step`, `if (*mid < value) first = mid+1,
  count -= step+1; else count = step`). O(log n) comparisons. On contiguous data
  the touched elements are within a few cache lines near the end → far fewer
  misses than a tree's O(log n) scattered nodes (file 05 measured 357 ns vs 1049
  ns).
- `set_intersection` etc.: a single linear merge walk — advance whichever
  iterator points at the smaller element; emit on the condition. O(n + m), one
  pass each, cache-friendly.
- `make_heap`: Floyd's build — sift-down from the last internal node up to the
  root. `∑ (nodes at height h)·h` telescopes to **O(n)**, not O(n log n).
- `next_permutation`: find the longest non-increasing suffix; if the whole array,
  reverse to sorted and return false; else find the pivot just before it, swap
  with the smallest suffix element greater than it, reverse the suffix. O(n)
  worst, O(1) amortized over a full enumeration.

> **HFT relevance:** the sorted-`std::vector` + `lower_bound` pattern **is** the
> standard order-book representation in latency-sensitive code — contiguous,
> `front()`/`back()` give best bid/ask in O(1), `lower_bound` finds a price level
> in O(log n) with minimal cache misses, and price-range scans are a contiguous
> walk. `std::merge` / `std::set_*` combine sorted level vectors without any tree.
> An even flatter variant indexes a plain array by *ticks from a reference price*
> → O(1), zero search. `std::map` is reserved for cold metadata. Heap algorithms
> back time-ordered event queues.

---

## Hands-on

```bash
./build.ps1 19-STL/examples/03_algorithms_tour.cpp
./build.ps1 fast 19-STL/examples/02_map_vs_unordered.cpp
```

`examples/02` includes the sorted-vector + `lower_bound` row (357 ns/lookup) next
to `std::map` (1049) and `std::unordered_map` (113). Add a `std::set_intersection`
of two sorted vectors and a `std::map`-based intersection; time both.

---

## ⚠️ Traps

### Trap 1 — binary search on an unsorted range
```cpp
std::binary_search(v.begin(), v.end(), x);   // ⚠️ UB (wrong answer) if v isn't sorted by the same order
```

### Trap 2 — `lower_bound` comparator argument order
```cpp
std::lower_bound(v.begin(), v.end(), key, [](double k, const Level& l){ ... });   // ❌ it's (element, value): (const Level&, double)
```

### Trap 3 — `std::lower_bound` on `std::set`'s iterators
```cpp
std::lower_bound(s.begin(), s.end(), x);   // ⚠️ compiles, but O(n) (bidirectional iterators). Use s.lower_bound(x)
```

### Trap 4 — forgetting to `pop_back` after `pop_heap`
```cpp
std::pop_heap(v.begin(), v.end());   // moves max to v.back() but does NOT remove it. v.pop_back() next
```

### Trap 5 — `next_permutation` without sorting first
```cpp
do { ... } while (std::next_permutation(v.begin(), v.end()));   // ⚠️ starts mid-sequence -> misses permutations before the start. sort() first
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`binary_search` works on any range" | Only a range sorted by the comparator you pass |
| "`lower_bound` returns end() when not found" | Returns the **insertion point** (first `>=`) — could be any position incl. `end()` |
| "`std::lower_bound` on a `set` is O(log n)" | O(n) — use the member `set::lower_bound` |
| "`set_intersection` needs `std::set`" | Needs **sorted ranges** — sorted vectors work and are faster |
| "`make_heap` is O(n log n)" | O(n) (Floyd build-heap) |

---

## Exercises

1. **Count occurrences:** sorted `std::vector<int> v`. How many times does `7`
   appear, in O(log n)?

   <details><summary>Answer</summary>

   `auto [lo, hi] = std::equal_range(v.begin(), v.end(), 7); auto n = hi - lo;`
   (or `std::upper_bound(...) - std::lower_bound(...)`).
   </details>

2. **Sorted insert:** keep `std::vector<int> v` sorted while inserting `x`. Cost
   breakdown?

   <details><summary>Answer</summary>

   `v.insert(std::lower_bound(v.begin(), v.end(), x), x);` — `lower_bound` O(log
   n) comparisons, `insert` O(n) element shift (a `memmove` for trivial types).
   </details>

3. **Intersection two ways:** given sorted `std::vector<int> a, b`, produce their
   intersection. Then say why this beats `std::set_intersection` on two
   `std::set`s.

   <details><summary>Answer</summary>

   `std::vector<int> out; std::set_intersection(a.begin(), a.end(), b.begin(),
   b.end(), std::back_inserter(out));` — O(n+m), one contiguous pass each. Two
   `std::set`s cost tree builds (O(n log n) + allocations) and the walk chases
   scattered nodes.
   </details>

4. **k largest via heap:** use `<algorithm>` heap ops on a `std::vector<int> v`
   to extract the 5 largest in descending order.

   <details><summary>Answer</summary>

   `std::make_heap(v.begin(), v.end());` then 5×: `std::pop_heap(v.begin(),
   v.end()); int x = v.back(); v.pop_back();` → yields the max each time,
   descending.
   </details>

5. **All arrangements:** print every ordering of `{1,2,3,4}`. How many, and why
   must you sort first?

   <details><summary>Answer</summary>

   `std::sort(v.begin(), v.end()); do { print(v); } while
   (std::next_permutation(v.begin(), v.end()));` → 4! = 24. Starting unsorted,
   `next_permutation` only walks forward from the current arrangement to the last,
   then stops — you'd miss every earlier permutation.
   </details>

---

## Interview questions

1. `lower_bound` vs `upper_bound` vs `equal_range` — kya return karte?
2. `std::binary_search` ka precondition, na ho to kya?
3. Sorted `std::vector` + `lower_bound` `std::map` ko kaise replace karta — trade-offs?
4. `std::set_intersection` ko `std::set` chahiye ya sorted range?
5. `make_heap` O(n) kaise (O(n log n) nahi)?
6. `next_permutation` se saari permutations — pehle sort kyun?

---

## Next
→ [`13-numeric-algorithms.md`](13-numeric-algorithms.md)
