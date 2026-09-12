# 08 — Ranges deep

## Prerequisites
- Folder 19 file 14 (ranges intro — views, `|`, projections)
- [`06-lambdas-deep.md`](06-lambdas-deep.md), folder 21 file 10 (concepts — ranges are concept-defined)

## Yeh topic abhi kyun
Folder 19 file 14 mein ranges ka intro tha. Yahan detail: **range concepts**
(kaunsa range kya support karta), **view semantics** (borrowed, common, sized,
owning), **lazy evaluation ke sookshm points** (`filter_view::begin()` O(n),
caching), aur **performance** — `examples/03` mein pipeline hand loop se **~1.5x
FASTER** (ek surprising, real result).

---

## Range concepts

```cpp
#include <ranges>
namespace rg = std::ranges;

rg::range<R>              // has begin()/end()  -> can be iterated
rg::sized_range<R>        // + size() in O(1)   (ranges::size works)
rg::borrowed_range<R>     // iterators stay valid even if the range is a temporary (lvalue ref, or a view marked borrowed)
rg::common_range<R>       // begin() and end() have the SAME type (needed for old iterator-pair APIs)
rg::viewable_range<R>     // can be turned into a view (an lvalue range, or a view/owning_view rvalue)

// iterator-capability tiers (mirror folder 19 file 08):
rg::input_range  <  rg::forward_range  <  rg::bidirectional_range
                 <  rg::random_access_range  <  rg::contiguous_range
```

`std::ranges::sort(x)` is constrained on `random_access_range<X> &&
sortable<...>` → pass a `std::list` and the error is "constraint
`random_access_range` not satisfied", one line, not a wall of iterator-arithmetic
noise.

---

## What a **view** is (precisely)

A **view** models `rg::view` — a range that is:
- **cheap to move/copy/destroy** (O(1) — it holds iterators/pointers/small state,
  not elements)
- **non-owning** (usually) — it refers to a range that must outlive it

```cpp
rg::view auto v = data | rv::filter(pred) | rv::transform(f);   // a nested view type, tiny sizeof
```

View categories you'll meet:
- **`ref_view`** — wraps an lvalue range by reference (what `|` does to a named
  container).
- **`owning_view`** — wraps an *rvalue* range by **moving it in** → the view
  owns the data (so it can't dangle). `views::all` on an rvalue gives this.
- **`filter_view` / `transform_view` / `take_view` / `drop_view` / ...** — the
  adaptors.

`sizeof` of a `filter|transform|take` pipeline is a handful of bytes (the source
reference + the two lambdas + a count).

---

## Lazy evaluation — the subtle points

```cpp
auto evens = v | rv::filter([](int x){ return x % 2 == 0; });

// 1. begin() is O(n), not O(1), for filter_view -- it must scan to the first match.
auto it = evens.begin();     // scans v until the first even element

// 2. begin() is CACHED after the first call (so a second begin() is O(1)) --
//    but that makes filter_view NOT const-iterable and NOT a borrowed_range.

// 3. re-iterating RE-RUNS the adaptors:
for (int x : evens) ...;     // runs the predicate for all of v
for (int x : evens) ...;     // runs it AGAIN

// 4. a view over a TEMPORARY dangles:
auto bad = makeVector() | rv::transform(f);   // the temp vector is gone -> UB when iterated
for (int x : makeVector() | rv::transform(f)) ...;   // ✅ the temp lives to the end of the loop
```

**Rules of thumb:**
- Don't store a `filter_view` you'll iterate many times — materialize it once.
- Don't build a view over an rvalue and keep it (unless it's `owning_view`).
- Cheap adaptors (`transform`, `take`, `drop`, `elements`, `iota`) compose
  freely; `filter`, `join`, `split` have gotchas (O(n) `begin`, single-pass-ish,
  not common/sized).

---

## The adaptor catalogue (C++20 + C++23)

| C++20 | C++23 |
|---|---|
| `filter`, `transform`, `take`, `take_while`, `drop`, `drop_while` | `zip`, `zip_transform`, `enumerate`, `adjacent`, `adjacent_transform`, `pairwise` |
| `reverse`, `keys`, `values`, `elements<N>` | `chunk`, `chunk_by`, `slide`, `stride` |
| `join`, `split`, `lazy_split`, `common`, `all`, `counted` | `join_with`, `cartesian_product`, `repeat`, `as_const`, `as_rvalue` |
| factories: `iota`, `single`, `empty`, `istream` | `std::ranges::to<Container>()` |

Range algorithms (`std::ranges::sort`, `find`, `count_if`, `for_each`, `copy`,
`min/max_element`, ...) all take a range + optional **projection**
(`&Struct::member` or a callable) so you don't write `[](auto& o){ return o.px;
}` lambdas everywhere (folder 19 file 14).

---

## Performance — measured

[`examples/03_ranges_pipelines.cpp`](examples/03_ranges_pipelines.cpp), `-O2`,
5,000,000 `int64`s, "sum `3·x` for even `x`":

| | time |
|---|---|
| `big \| filter(even) \| transform(*3)` then accumulate | **~6.5 ms** |
| `for (x : big) if (x % 2 == 0) s += x * 3;` (hand loop) | **~10 ms** |

The **pipeline is ~1.5× faster**. Reason: the hand loop's `if (even) s += ...` is
a **conditional accumulate** the compiler won't vectorize (it can't prove the
add is safe to do speculatively into `s`). The range pipeline's structure lets
GCC emit **branchless masked SIMD**. This is the "don't assume abstraction =
slower — measure" lesson made concrete.

Where views *do* cost:
- `filter_view::begin()` is O(n) — bad in a tight repeated loop.
- Some views break vectorization (a `filter` inside a complex pipeline).
- A view of a view of a view has a deep nested type — compile-time and,
  occasionally, missed optimizations.

Cheap adaptors (`transform`, `take`, `iota`) on contiguous data reliably fuse to
a clean loop.

---

## Andar kya hota hai

- `filter|transform|take` over a `vector<int>` builds the type
  `take_view<transform_view<filter_view<ref_view<vector<int>>, F>, G>>`. Iterating
  it calls `operator++`/`operator*` down the chain; at `-O2` the lambdas inline
  and the nest **collapses into one loop** with no materialization.
- `filter_view` caches its `begin()` in a member (`std::optional<iterator>` or
  similar) because recomputing "first element satisfying the predicate" every
  call would be O(n) each time — but a cached mutable member is why it isn't
  `const`-iterable or borrowed.
- `owning_view` move-constructs the source range into itself, so `auto v =
  std::vector<int>{1,2,3} | views::transform(f);` is safe *if* the pipe produces
  an `owning_view` — which it does for `views::all` on an rvalue, but a named
  rvalue via `|` on some adaptors doesn't → the dangling trap.
- Range algorithms + projections generate the same code as the hand-written
  comparator lambda — the projection is applied inline.

> **HFT relevance:** ranges give **readable, allocation-free** multi-stage data
> massaging — parse → filter by symbol → project a field → take N — and at `-O2`
> the cheap adaptors fuse to a loop that matches or (as `examples/03` shows)
> beats a hand loop. Practical rules for the hot path: use `transform` / `take` /
> `iota` / `elements` freely; keep `filter` / `join` / `split` out of the
> innermost loop (O(n) `begin`, vectorization gaps) or just write the loop;
> never hold a view over a temporary or re-iterate a `filter_view`. Range
> **algorithms + projections** are a clean win everywhere — same codegen as the
> lambda, far less boilerplate, and concept-checked errors instead of iterator
> sludge.

---

## Hands-on

```bash
./build.ps1 22-MODERN-CPP/examples/03_ranges_pipelines.cpp
./build.ps1 fast 22-MODERN-CPP/examples/03_ranges_pipelines.cpp
```

Write: `views::iota(1) | views::filter(isPrime) | views::take(10)` (infinite
source); a `views::zip(prices, quantities) | views::transform([](auto p){ return
get<0>(p) * get<1>(p); })` VWAP numerator; materialize a filtered view into a
`std::vector` (C++20 `push_back` loop, then note C++23 `ranges::to`).

---

## ⚠️ Traps

### Trap 1 — view over a temporary
```cpp
auto v = getVec() | std::views::reverse;   // ⚠️ getVec()'s result dies -> v dangles. Iterate inline, or store the vector first
```

### Trap 2 — re-iterating a filter_view
```cpp
auto primes = nums | std::views::filter(isPrime);
for (int p : primes) ...;   // runs isPrime for all of nums
for (int p : primes) ...;   // runs it AGAIN. Materialize if reused
```

### Trap 3 — `filter_view::begin()` in a hot loop
```cpp
for (int i = 0; i < N; ++i) use(*(data | std::views::filter(pred)).begin());   // ⚠️ O(n) scan each iteration
```

### Trap 4 — mutating through `transform`
```cpp
for (int& x : v | std::views::transform([](int x){ return x; })) x = 0;   // ❌ transform yields prvalues, not references
```

### Trap 5 — `std::ranges::sort` on a non-random-access range
```cpp
std::ranges::sort(myList);   // ❌ "constraint random_access_range not satisfied" -- list has list::sort()
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Ranges pipelines are slower than hand loops" | At `-O2` they fuse; `examples/03` shows the pipeline ~1.5× *faster* (hand loop's conditional accumulate doesn't vectorize) |
| "A view owns / copies the data" | Non-owning by default (`ref_view`); `owning_view` only when moving an rvalue in |
| "`filter_view::begin()` is O(1)" | O(n) the first time (scans to the first match), then cached |
| "Iterating a view twice is cheap" | Re-runs every adaptor; materialize if you reuse it |
| "All views are `const`-iterable / borrowed" | `filter_view` (cached begin) isn't; check `borrowed_range` before returning a view |

---

## Exercises

1. **Why faster:** `examples/03` — the range pipeline beats the hand loop `if
   (x%2==0) s += x*3;`. Explain in terms of vectorization.

   <details><summary>Answer</summary>

   The hand loop conditionally accumulates into `s` — the compiler can't
   speculatively do the add for odd `x` (it would corrupt `s`), so it keeps a
   scalar branchy loop. The pipeline's `filter`+`transform`+reduce structure lets
   GCC generate a **branchless masked SIMD** loop (compute `x*3` for a lane,
   add it into the accumulator only where the mask says "even"). ~1.5×.
   </details>

2. **Dangling:** which is safe? (a) `auto v = vec | views::take(3);` used later,
   `vec` alive. (b) `return vec | views::take(3);` from a function where `vec` is
   a local. (c) `for (int x : makeVec() | views::take(3)) ...`

   <details><summary>Answer</summary>

   (a) safe — refers to `vec` which outlives `v`. (b) **dangling** — `vec` (the
   local) is destroyed at return; the returned view refers to freed memory. (c)
   safe — the temporary from `makeVec()` lives until the end of the range-`for`.
   </details>

3. **Projection:** sort `std::vector<Order>` by `sym` then by `px` descending
   with `std::ranges::sort` — one call, or two?

   <details><summary>Answer</summary>

   Two keys in opposite directions → either a comparator lambda, or two stable
   sorts (least significant first): `rg::stable_sort(v, std::greater<>{},
   &Order::px); rg::stable_sort(v, {}, &Order::sym);`. A single projection can't
   express "asc then desc".
   </details>

4. **Infinite range:** produce the first 10 Fibonacci numbers with ranges. Why
   doesn't it hang?

   <details><summary>Answer</summary>

   There's no stdlib Fibonacci factory — you'd `views::iota(0) | views::transform(
   fib_of)` (with a cached `fib_of`) `| views::take(10)`, or use a coroutine
   generator. It doesn't hang because `take(10)` stops pulling after 10 elements
   → the lazy source is only advanced that far.
   </details>

5. **borrowed_range:** why can't a function safely `return v | std::views::filter
   (pred);` even when `v` is a parameter passed by `const&`?

   <details><summary>Answer</summary>

   `filter_view` caches its `begin()` in a mutable member → it's **not** a
   `borrowed_range`, and more importantly it holds a reference to `v`. If `v` is
   a `const&` parameter bound to a caller's temporary, the returned view outlives
   that temporary → dangling. Return a materialized `std::vector`, or take `v` by
   value / require an lvalue.
   </details>

---

## Interview questions

1. Range concepts — `range` / `sized_range` / `borrowed_range` / `common_range` / `viewable_range`?
2. View kya model karta — cheap to copy, non-owning; `owning_view` kab?
3. `filter_view::begin()` O(n) kyun, caching ka side effect (not const/borrowed)?
4. View over a temporary — kab dangle, kab safe?
5. `examples/03` mein pipeline hand loop se fast kyun (vectorization)?
6. Range algorithm + projection — boilerplate kaise kam, codegen same kyun?

---

## Next
→ [`09-coroutines.md`](09-coroutines.md)
