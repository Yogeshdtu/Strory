# 05 — Binary search

## Prerequisites
- [`01-complexity-analysis.md`](01-complexity-analysis.md), [`03-sorting-algorithms.md`](03-sorting-algorithms.md)
- Folder 19 file 12 (`lower_bound`/`upper_bound`), folder 03 (integer overflow)

## Yeh topic abhi kyun
Binary search likhna aasan lagta hai aur galat likhna aur bhi aasan — "60% of
programmers can't write it correctly first try" wali classic. Ek **half-open
invariant** pakadne se saare off-by-one khatam ho jaate. Aur "binary search the
answer" pattern har DSA set mein aata.

`examples/02_binary_search.cpp` mein correct versions + traps.

---

## The canonical version — half-open `[lo, hi)`

**Invariant:** the answer (if it exists) is always in `[lo, hi)`. Start with the
whole array `[0, n)`. Each step halves it. Stop when `lo == hi` (empty).

```cpp
// membership
bool contains(const std::vector<int>& a, int x) {
    std::size_t lo = 0, hi = a.size();          // search space is [lo, hi)
    while (lo < hi) {
        std::size_t mid = lo + (hi - lo) / 2;   // NOT (lo + hi) / 2  -> can overflow
        if      (a[mid] == x) return true;
        else if (a[mid] <  x) lo = mid + 1;     // x is in [mid+1, hi)
        else                  hi = mid;         // x is in [lo, mid)
    }
    return false;
}
```

Three things that make it correct:
1. **`mid = lo + (hi - lo) / 2`** — never overflows (`lo + hi` can, for large
   indices / 32-bit).
2. **`hi = mid`, not `mid - 1`** — half-open, so `mid` is excluded by making it
   the new (exclusive) upper bound. `lo = mid + 1` because `mid` is checked and
   rejected.
3. **`while (lo < hi)`** — terminates because the interval strictly shrinks every
   step (`mid` is always in `[lo, hi)`, and we move past it).

`O(log n)` comparisons. For `n = 10⁹`, ~30 steps.

---

## `lower_bound` / `upper_bound` — the useful forms

Membership is rarely enough. What you usually want:

```cpp
// first index i with a[i] >= x   (== std::lower_bound)  -- the "insertion point"
std::size_t lowerBound(const std::vector<int>& a, int x) {
    std::size_t lo = 0, hi = a.size();
    while (lo < hi) {
        std::size_t mid = lo + (hi - lo) / 2;
        if (a[mid] < x) lo = mid + 1;           // a[mid] too small -> answer is right of mid
        else            hi = mid;               // a[mid] >= x -> answer is mid or left
    }
    return lo;                                   // in [0, n]
}

// first index i with a[i] > x    (== std::upper_bound)
// same, but the test is  a[mid] <= x
```

Derived queries (all `O(log n)`):
- **membership**: `auto i = lowerBound(a, x); return i < a.size() && a[i] == x;`
- **count of `x`**: `upperBound(a, x) - lowerBound(a, x)`
- **first element `> x`**: `upperBound(a, x)`
- **largest element `<= x`**: `upperBound(a, x) - 1` (if `> 0`)
- **sorted insert position**: `lowerBound(a, x)` — `v.insert(v.begin() + i, x)`
  keeps it sorted.

`examples/02` verifies these against `std::lower_bound`/`std::upper_bound` on
`{1, 3, 3, 3, 5, 8, 8, 13, 21}`.

---

## Binary search on the answer (predicate binary search)

If you can define a **monotone predicate** `p(k)` — `false, false, …, false,
true, true, …, true` — then binary search finds the **first `k` with `p(k) ==
true`**, even when there's no array.

```cpp
// smallest n with n*n >= target  (integer ceil-sqrt)
long long isqrtCeil(long long target) {
    long long lo = 0, hi = 3'000'000'000LL;     // hi*hi must not overflow
    while (lo < hi) {
        long long mid = lo + (hi - lo) / 2;
        if (mid * mid >= target) hi = mid;       // p(mid) true  -> answer <= mid
        else                     lo = mid + 1;   // p(mid) false -> answer >  mid
    }
    return lo;
}
```

Classic uses:
- "Minimum capacity / speed / time to finish within a limit" (ship packages in D
  days, Koko eats bananas, minimum largest sub-array sum).
- "Maximum value such that a feasibility check passes."
- The feasibility check is often `O(n)`, giving `O(n log(range))` overall.

**Requirement:** `p` must actually be monotone. If `p` is `F T F T`, the result
is meaningless. Prove monotonicity before you code.

---

## The classic bugs

| Bug | Symptom | Fix |
|---|---|---|
| `mid = (lo + hi) / 2` | overflow → negative `mid` → OOB / wrong | `lo + (hi - lo) / 2` |
| `while (lo <= hi)` with `hi = n` | reads `a[n]` (OOB) | half-open: `while (lo < hi)`, `hi = n` |
| `hi = mid - 1` in a half-open loop | skips the answer | `hi = mid` |
| `lo = mid` when discarding the low half | infinite loop when `hi - lo == 1` | `lo = mid + 1` |
| search on **unsorted** data | silently wrong (not a crash) | sort first, or it's a bug |
| non-monotone predicate | meaningless result | prove `F…FT…T` first |

The "closed interval" `[lo, hi]` version (`while (lo <= hi)`, `hi = mid - 1`,
`lo = mid + 1`) is *also* correct if written carefully — but it has more
edge cases (empty range, `hi` underflow when `hi` is unsigned and `mid == 0`).
**Pick the half-open form and use it everywhere.**

---

## Andar kya hota hai

- Binary search does `⌈log₂(n+1)⌉` comparisons. The accessed indices are `n/2`,
  `n/4` or `3n/4`, … — for the **last several steps** they fall within one or two
  cache lines, so a search over a contiguous `std::vector` touches ~2–3 cache
  lines total. Over a `std::map` (folder 19 file 05) the same `log n` comparisons
  hit `log n` scattered heap nodes → `log n` cache misses → ~3× slower measured.
- The branch in the loop (`a[mid] < x`) is **unpredictable** by design (50/50
  each step) → ~`log n` branch mispredicts per search (~15–20 cycles each). This
  is why *branchless* binary search (below) can win for hot lookups.
- **Branchless variant**: `lo += (a[lo + half] < x) * half` style, or
  `std::lower_bound`-with-`__builtin_expect`-free code; replaces the
  mispredicting branch with a `cmov` / arithmetic. Combined with prefetching both
  candidates, it can be ~2× faster on large arrays. libstdc++ `std::lower_bound`
  is a plain branchy loop; hand-rolled branchless search is a real HFT technique.
- For small `n` (≤ ~64), a **linear SIMD scan** beats binary search — no
  mispredicts, fully prefetched, vectorized compare. Binary search wins once
  `log n` cache-miss savings outweigh the mispredict cost.

> **HFT relevance:** binary search over a **sorted `std::vector`** is the standard
> order-book price-level lookup — contiguous, `O(log n)`, ~2 cache misses, and
> `front()`/`back()` give best bid/ask in `O(1)` (folder 19 files 12, 25, 26).
> For the hottest lookups, a **branchless** binary search (arithmetic index
> update + prefetch both halves) removes the `log n` branch mispredicts; and for
> a handful of levels, a plain **linear scan** is faster still. "Binary search
> the answer" shows up in calibration/sizing code (smallest buffer, largest safe
> rate) run at startup, not on the tick path.

---

## Hands-on

```bash
./build.ps1 20-ALGORITHMS-DSA/examples/02_binary_search.cpp
```

Write `contains`, `lowerBound`, `upperBound`, and `isqrtCeil` from the invariant,
without looking. Test `lowerBound` on: empty array, all-equal array, `x` smaller
than everything, `x` larger than everything, `x` between two elements. Then a
"binary search the answer": minimum days to ship all packages given a daily
weight cap.

---

## ⚠️ Traps

### Trap 1 — `mid = (lo + hi) / 2` overflow
```cpp
int mid = (lo + hi) / 2;   // ⚠️ lo + hi overflows int for large arrays -> UB. lo + (hi - lo) / 2
```

### Trap 2 — closed interval + `hi = mid - 1` with unsigned `hi`
```cpp
std::size_t hi = a.size() - 1;
// ... hi = mid - 1;   when mid == 0 -> hi = SIZE_MAX -> reads garbage. Use half-open [lo, hi)
```

### Trap 3 — `lo = mid` (not `mid + 1`)
```cpp
if (a[mid] < x) lo = mid;   // ⚠️ hi - lo == 1 -> mid == lo -> no progress -> infinite loop
```

### Trap 4 — searching unsorted data
```cpp
std::binary_search(v.begin(), v.end(), x);   // ⚠️ v not sorted -> wrong answer, no error
```

### Trap 5 — non-monotone predicate in "search the answer"
```cpp
// p(k) must be F...FT...T. If feasibility isn't monotone in k, the result is meaningless.
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`lower_bound` returns `end()` when x is absent" | Returns the **insertion point** (first `>= x`), which can be any index incl. `n` |
| "`(lo + hi) / 2` is fine" | Overflows for large indices — `lo + (hi - lo) / 2` |
| "Closed and half-open are equally easy" | Half-open has fewer edge cases — prefer it |
| "Binary search needs an array" | Only a monotone predicate over an index range |
| "Binary search is always faster than linear" | For n ≤ ~64 a SIMD linear scan wins (no branch mispredicts) |

---

## Exercises

1. **lower_bound by hand:** implement it, then use it for membership, count-of-x,
   and "largest element ≤ x" on `{2,4,4,4,6,9}`.

   <details><summary>Answer</summary>

   `lb(x)` as in the lesson. Membership: `i = lb(x); i < n && a[i] == x`.
   Count: `lb(x+1) - lb(x)` (or `ub - lb`). Largest ≤ x: `ub(x) - 1` if `> 0`
   else "none". For x=4: lb=1, ub=4, count 3, largest ≤ 4 is index 3 (value 4).
   </details>

2. **Rotated array:** search `x` in a sorted array rotated at an unknown pivot
   (`[4,5,6,7,0,1,2]`), `O(log n)`.

   <details><summary>Answer</summary>

   Standard binary search, but at each `mid` decide which half is sorted: if
   `a[lo] <= a[mid]`, the left half is sorted → if `a[lo] <= x < a[mid]` go
   left else right; symmetric if the right half is sorted.
   </details>

3. **Search the answer:** minimum ship capacity to deliver weights `w[]` in `D`
   days (each day, load in order until adding the next would exceed capacity).

   <details><summary>Answer</summary>

   `p(cap)` = "can finish in ≤ D days with this capacity" — monotone (bigger cap
   → fewer days). `lo = max(w)`, `hi = sum(w)`. Binary search for the smallest
   `cap` with `p(cap)`. `p` is an `O(n)` simulation → `O(n log(sum))`.
   </details>

4. **Off-by-one hunt:** `lo=0, hi=n; while(lo<=hi){ mid=lo+(hi-lo)/2; if(a[mid]<x)
   lo=mid+1; else hi=mid; }` — what's wrong?

   <details><summary>Answer</summary>

   `while (lo <= hi)` with `hi = n` (exclusive) → when `lo == hi == n` it computes
   `mid = n` and reads `a[n]` — out of bounds. Use `while (lo < hi)`.
   </details>

5. **Float binary search:** find `x` such that `f(x) = 0` for a monotone
   continuous `f` on `[a, b]`, to tolerance `1e-9`. How does the loop condition
   change?

   <details><summary>Answer</summary>

   No integer `mid + 1`; loop `while (hi - lo > 1e-9)` (or a fixed ~100
   iterations), `mid = (lo + hi) / 2`, move `lo` or `hi` to `mid` based on
   `sign(f(mid))`. Return `(lo + hi) / 2`.
   </details>

---

## Interview questions

1. Half-open `[lo, hi)` invariant — teen cheezein jo correctness dete?
2. `mid = (lo + hi) / 2` ka bug, fix?
3. `lower_bound` vs `upper_bound` vs `binary_search` — kya return?
4. "Binary search the answer" — kab laga sakte, kya prove karna?
5. `map` vs sorted-`vector` binary search — dono `O(log n)`, ek 3× fast kyun?
6. Chhote `n` (≤64) pe linear scan binary search se kyun jeet sakta?

---

## Next
→ [`06-linked-lists.md`](06-linked-lists.md)
