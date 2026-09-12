# 02 — Arrays: two pointers, sliding window, prefix sums

## Prerequisites
- [`01-complexity-analysis.md`](01-complexity-analysis.md)
- Folder 09 (arrays), folder 19 file 02 (`std::vector`), folder 13 numeric (`partial_sum`)

## Yeh topic abhi kyun
Contiguous array algorithms interview ka sabse common bucket hain, **aur** HFT ka
bread and butter — kyunki array pe kaam cache-friendly hota hai. Teen patterns
90% problems cover karte: **two pointers**, **sliding window**, **prefix sums**.
Har ek ek naive `O(n²)` ko `O(n)` bana deta.

---

## Pattern 1 — Two pointers

Do indices jo array pe move karte hain, usually opposite ends se ya same
direction mein alag speeds pe. `O(n)` time, `O(1)` space.

### Opposite ends: pair with target sum (sorted array)

```cpp
// return the two indices whose values sum to target, or {-1,-1}
std::pair<int,int> twoSumSorted(const std::vector<int>& a, int target) {
    std::size_t lo = 0, hi = a.size() - 1;
    while (lo < hi) {
        int s = a[lo] + a[hi];
        if      (s == target) return {static_cast<int>(lo), static_cast<int>(hi)};
        else if (s < target)  ++lo;         // need a bigger sum
        else                  --hi;         // need a smaller sum
    }
    return {-1, -1};
}
```
Naive is `O(n²)` (all pairs). Two pointers: each step moves `lo` or `hi` toward
the middle → at most `n` steps → `O(n)`.

### Same direction: in-place partition / remove

```cpp
// move all zeros to the end, keep the order of non-zeros. O(n), O(1).
void moveZeros(std::vector<int>& a) {
    std::size_t w = 0;                       // write head
    for (std::size_t r = 0; r < a.size(); ++r)   // read head
        if (a[r] != 0) std::swap(a[w++], a[r]);
}
```
This is exactly `std::remove`'s mechanism (folder 19 file 10) — a read pointer
and a write pointer.

### Fast/slow: cycle detection, middle of a list

Floyd's tortoise-and-hare (`examples/03_linked_list.cpp`): `slow` moves 1,
`fast` moves 2. If they meet, there's a cycle. `fast` reaching the end → no
cycle. `O(n)` time, `O(1)` space (vs `O(n)` space for a visited-set).

---

## Pattern 2 — Sliding window

A contiguous sub-range `[l, r)` that grows on the right and shrinks on the left,
maintaining some running quantity. Turns "check every sub-array" (`O(n²)` or
`O(n³)`) into `O(n)`.

### Fixed window: max sum of `k` consecutive

```cpp
long long maxSumK(const std::vector<int>& a, std::size_t k) {
    long long sum = 0;
    for (std::size_t i = 0; i < k; ++i) sum += a[i];      // first window
    long long best = sum;
    for (std::size_t i = k; i < a.size(); ++i) {
        sum += a[i] - a[i - k];                            // slide: add new, drop old
        best = std::max(best, sum);
    }
    return best;
}
```

### Variable window: smallest sub-array with sum ≥ target (positive values)

```cpp
std::size_t minLenAtLeast(const std::vector<int>& a, long long target) {
    std::size_t l = 0, best = a.size() + 1;
    long long sum = 0;
    for (std::size_t r = 0; r < a.size(); ++r) {
        sum += a[r];
        while (sum >= target) {                            // shrink from the left while still valid
            best = std::min(best, r - l + 1);
            sum -= a[l++];
        }
    }
    return best == a.size() + 1 ? 0 : best;
}
```
Each index enters the window once (`r++`) and leaves once (`l++`) → `O(n)` total,
even with the inner `while`.

**Window works when** the quantity is monotone as the window grows/shrinks
(sum with non-negatives, count of distinct chars, etc.). It breaks with negative
numbers in a sum problem — use prefix sums + a data structure instead.

---

## Pattern 3 — Prefix sums

Precompute `pre[i] = a[0] + a[1] + … + a[i-1]` once (`O(n)`), then **any**
range sum is `O(1)`:

```cpp
std::vector<long long> pre(a.size() + 1, 0);
for (std::size_t i = 0; i < a.size(); ++i) pre[i + 1] = pre[i] + a[i];
// sum of a[l..r] inclusive  ==  pre[r + 1] - pre[l]
```

`std::partial_sum` / `std::inclusive_scan` do the build (folder 19 file 13).

Uses:
- **Range-sum queries**: `q` queries in `O(n + q)` instead of `O(n·q)`.
- **Count of sub-arrays with sum `k`** (works with negatives): as you scan, keep
  a hash map of `pre[]` values seen; `pre[r+1] - k` seen before → that many
  sub-arrays ending at `r`. `O(n)`.
- **2D prefix sums** (integral image): `O(1)` rectangle sums after `O(rows·cols)`
  precompute.
- **Difference array**: the inverse — add `v` to every element in `[l, r]` in
  `O(1)` by `diff[l] += v; diff[r+1] -= v;`, then one `partial_sum` at the end.

---

## Andar kya hota hai

- All three patterns are **single or double linear passes over contiguous
  memory** → the hardware prefetcher runs ahead, the loop body is branch-light,
  and the compiler often vectorizes the arithmetic. This is why they're not just
  asymptotically better but *constant-factor* excellent.
- Two pointers / sliding window are `O(1)` space — no allocation, nothing to
  invalidate, cache footprint is just the window.
- Prefix sums trade `O(n)` space for `O(1)` queries. The prefix array is itself
  contiguous, so building and querying it are both cache-friendly.
- The `while` inside a variable window looks like it could be `O(n²)`, but the
  left pointer only ever moves forward across the whole array once → amortized
  `O(1)` per outer step.

> **HFT relevance:** these are the shapes real market-data code uses — a **fixed
> sliding window** over the last N ticks for a moving average / rolling
> volatility (add the new tick, subtract the one that fell off — `O(1)` per
> update, no re-scan); **prefix sums** for cumulative volume / cumulative depth
> down an order book (`std::inclusive_scan`, folder 19 file 13); **two pointers**
> for merging two sorted level vectors or walking bid/ask sides inward. All
> operate on a preallocated contiguous buffer, `O(1)` extra space, no allocation
> in steady state — exactly the constraints the hot path imposes.

---

## Hands-on

```bash
./build.ps1 20-ALGORITHMS-DSA/examples/02_binary_search.cpp
```

Write yourself: `twoSumSorted`, `moveZeros`, a fixed-window moving average over
`std::vector<double>`, and "count sub-arrays summing to k" with a prefix-sum hash
map. Verify each against a brute-force `O(n²)` on small random inputs.

---

## ⚠️ Traps

### Trap 1 — sliding window with negative numbers
```cpp
// "smallest subarray with sum >= target" via shrink-from-left ASSUMES non-negative values.
// With negatives, shrinking can make the sum go back up -> wrong. Use prefix sums + a set/map.
```

### Trap 2 — off-by-one in the prefix array
```cpp
// pre has size n+1; sum of a[l..r] inclusive is pre[r+1] - pre[l], NOT pre[r] - pre[l].
```

### Trap 3 — `size() - 1` underflow on an empty vector
```cpp
std::size_t hi = a.size() - 1;   // ⚠️ a.empty() -> hi = SIZE_MAX. Guard, or use half-open [0, size())
```

### Trap 4 — integer overflow in a sum
```cpp
int sum = 0; for (int x : a) sum += x;   // ⚠️ n large + values large -> overflow. long long accumulator
```

### Trap 5 — two-pointer on an UNSORTED array for a sum problem
```cpp
// twoSumSorted needs sorted input. On unsorted data use a hash set (O(n)) or sort first (O(n log n)).
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Sliding window is always O(n)" | Only when the metric is monotone under grow/shrink; else it's not applicable |
| "Prefix sums help with updates too" | Point updates cost `O(n)` to rebuild; use a Fenwick/segment tree for update + query |
| "Two pointers needs sorted data" | Opposite-ends sum problems do; same-direction (partition, fast/slow) don't |
| "The inner `while` in a variable window makes it O(n²)" | The left pointer advances ≤ n times total → amortized O(1) |
| "Range sum needs a loop" | With a prefix array it's one subtraction, O(1) |

---

## Exercises

1. **Two-pointer:** given a sorted `std::vector<int>`, remove duplicates in place
   and return the new length. `O(n)`, `O(1)`.

   <details><summary>Answer</summary>

   `std::size_t w = 0; for (std::size_t r = 0; r < a.size(); ++r) if (r == 0 ||
   a[r] != a[w-1]) a[w++] = a[r]; return w;` — write head `w` keeps the deduped
   prefix. (This is `std::unique`.)
   </details>

2. **Fixed window:** moving average of window `k` over `std::vector<double> px`,
   output a `std::vector<double>` of length `px.size() - k + 1`.

   <details><summary>Answer</summary>

   Sum the first `k`, push `sum/k`. Then for `i` from `k`: `sum += px[i] -
   px[i-k]; push(sum/k);`. `O(n)`, one add + one subtract per step.
   </details>

3. **Prefix sum with negatives:** count sub-arrays of `a` whose sum equals `k`
   (values may be negative).

   <details><summary>Answer</summary>

   `std::unordered_map<long long,int> seen{{0,1}}; long long pre = 0, count = 0;
   for (int x : a) { pre += x; count += seen[pre - k]; ++seen[pre]; } return
   count;` — `pre - k` seen `m` times ⇒ `m` sub-arrays ending here sum to `k`.
   </details>

4. **Variable window:** longest sub-array with at most `K` distinct integers.

   <details><summary>Answer</summary>

   Grow `r`, track counts in a hash map; while `map.size() > K`, shrink `l`
   (decrement `a[l]`'s count, erase at 0), `++l`. Record `r - l + 1`. `O(n)`.
   </details>

5. **Difference array:** apply `m` range-add updates `(l, r, v)` to a zero array
   of length `n`, then answer the final array. Total complexity?

   <details><summary>Answer</summary>

   `diff[l] += v; diff[r+1] -= v;` per update — `O(1)` each, `O(m)` total. Then
   one `std::partial_sum` over `diff` — `O(n)`. Total `O(n + m)` vs `O(n·m)`
   naive.
   </details>

---

## Interview questions

1. Two-pointer sum problem — kyun `O(n)`, kyun sorted chahiye?
2. Sliding window kab valid hai (monotonicity), kab break hota?
3. Variable window ka inner `while` `O(n²)` kyun nahi?
4. Prefix sum se range-sum query `O(1)` kaise? Off-by-one kya?
5. Negatives ke saath sub-array-sum-k — window kyun nahi, kya use?
6. Difference array kya solve karta, complexity?

---

## Next
→ [`03-sorting-algorithms.md`](03-sorting-algorithms.md)
