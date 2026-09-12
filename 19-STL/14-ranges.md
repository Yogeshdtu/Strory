# 14 — `<ranges>` (C++20): views, pipelines, lazy evaluation

## Prerequisites
- [`09-algorithms-part1.md`](09-algorithms-part1.md)–[`13-numeric-algorithms.md`](13-numeric-algorithms.md)
- [`08-iterators.md`](08-iterators.md), lambdas, `auto` return types

## Yeh topic abhi kyun
C++20 ranges STL ke 20 saal purane rough edges theek karta:
1. `v.begin(), v.end()` har call pe likhna — ab `std::ranges::sort(v)`.
2. Multi-step data transforms mein intermediate vectors — ab **lazy views** jo
   `|` se compose hote, zero allocation.

HFT mein views kaam ke hain kyunki woh **non-owning** hain aur `-O2` pe aksar ek
hi fused loop ban jaate — abstraction bina cost ke.

`examples/05_ranges_demo.cpp` ismein sab live hai.

---

## Range algorithms — `std::ranges::`

```cpp
#include <ranges>
#include <algorithm>
namespace rg = std::ranges;

rg::sort(v);                                   // no .begin()/.end()
rg::sort(v, std::greater<>{});
auto it = rg::find(v, 42);
bool ok = rg::all_of(v, [](int x){ return x > 0; });
auto n  = rg::count_if(v, isEven);
rg::for_each(v, [](int& x){ x *= 2; });
rg::copy(v, std::back_inserter(out));
```

Plus two things the old algorithms couldn't do:

### Projections — "operate on this member/derived value"

```cpp
struct Order { double px; std::uint32_t qty; std::string sym; };
std::vector<Order> book;

rg::sort(book, {}, &Order::px);                       // sort by px, default (<) comparator
rg::sort(book, std::greater<>{}, &Order::qty);        // by qty descending
auto it = rg::find(book, "AAPL", &Order::sym);        // find where o.sym == "AAPL"
auto mx = rg::max_element(book, {}, &Order::px);      // level with the highest price
```

The projection (last arg) is applied to each element before the comparator /
predicate sees it — no more one-line lambdas just to reach into a field.

### They return more info

`rg::copy` returns `{in, out}` (both end iterators); `rg::for_each` returns
`{in, fun}`; subrange-returning algorithms return `rg::subrange`. Structured
bindings pick out what you need.

---

## Views — lazy, non-owning adaptors

A **view** is a lightweight object (cheap to copy, owns nothing) that presents a
*transformed* look at an underlying range. Nothing is computed until you iterate.

```cpp
namespace rv = std::ranges::views;   // or std::views

std::vector<int> v{1,2,3,4,5,6,7,8,9,10,11,12};

auto pipeline = v
              | rv::filter([](int x){ return x % 2 == 0; })   // 2 4 6 8 10 12
              | rv::transform([](int x){ return x * x; })     // 4 16 36 64 100 144
              | rv::take(3);                                   // 4 16 36

for (int x : pipeline) std::printf("%d ", x);   // <-- computation happens HERE, element by element
// no intermediate vector was allocated for "the evens" or "the squares"
```

`|` is the composition operator (like a Unix pipe). `range | adaptor` → a new
view.

### The common adaptors

| Adaptor | What it yields |
|---|---|
| `views::filter(pred)` | elements where `pred` is true |
| `views::transform(f)` | `f(x)` for each `x` |
| `views::take(n)` / `views::take_while(pred)` | first `n` / leading run |
| `views::drop(n)` / `views::drop_while(pred)` | all but first `n` / after the leading run |
| `views::reverse` | back-to-front (needs bidirectional) |
| `views::keys` / `views::values` | `.first` / `.second` of a range of pairs (e.g. a `map`) |
| `views::elements<N>` | the `N`th element of each tuple |
| `views::join` | flatten a range of ranges |
| `views::split(delim)` / `views::chunk(n)` / `views::slide(n)` (C++23) | sub-ranges |
| `views::enumerate` (C++23) | `{index, element}` pairs |
| `views::zip` (C++23) | tuples across several ranges in lockstep |

### Factories (not adaptors — they *make* a range)

```cpp
for (int i : std::views::iota(0, 5)) ...          // 0 1 2 3 4
for (int i : std::views::iota(1))    ...          // 1 2 3 ...  infinite -- pair with take
std::views::repeat(x, n)                           // C++23
std::views::single(x)  /  std::views::empty<T>
```

Idiomatic index loop: `for (auto i : std::views::iota(0uz, v.size()))`.

---

## Lazy evaluation — only what you consume

```cpp
int calls = 0;
auto expensive = [&](int x){ ++calls; return x * 10; };

auto view = v | rv::transform(expensive) | rv::take(3);
// calls == 0 here -- nothing has run

int sum = 0;
for (int x : view) sum += x;
// calls == 3 -- transform ran exactly for the 3 elements take() pulled
```

Consequences:
- **No intermediate containers.** `filter | transform | take` over a million
  elements that yields 3 does ~3 elements of work, 0 allocations.
- **Composes with infinite ranges**: `views::iota(1) | views::filter(isPrime) |
  views::take(10)`.
- **Re-evaluates on each pass.** Iterating a `filter_view` twice runs the
  predicate twice. `filter_view::begin()` is O(n) (it must find the first
  match) and is cached after the first call — so *the first* `begin()` isn't
  O(1).

---

## Materializing a view into a container

```cpp
// C++20 -- manual:
std::vector<int> out;
for (int x : v | rv::filter(pred) | rv::transform(f)) out.push_back(x);
// or: std::ranges::copy(view, std::back_inserter(out));

// C++23 -- std::ranges::to:
auto out = v | rv::filter(pred) | rv::transform(f) | std::ranges::to<std::vector>();
auto s   = v | rv::transform([](int x){ return std::to_string(x); })
             | std::ranges::to<std::set<std::string>>();
```

(This repo builds `-std=c++20`, so `examples/05` uses the manual `push_back`
form and notes `ranges::to` as C++23.)

---

## Andar kya hota hai

- A view stores its source (by reference or by move if it's an rvalue) plus the
  adaptor's state (the lambda). `sizeof` is tiny. `filter | transform | take`
  builds a nested type
  `take_view<transform_view<filter_view<ref_view<vector<int>>, F>, G>>`.
- Iterating calls `operator++` / `operator*` down the chain: `take`'s `++`
  advances `transform`'s iterator, whose `*` calls `g(*filter_it)`, whose `++`
  scans the underlying vector for the next element satisfying `f`. At `-O2` the
  lambdas inline and the whole nest **collapses into one loop** with no
  materialization — often the same assembly as a hand-written combined loop.
- `filter_view` is only a *forward* range (not random-access) — `take`/`drop` on
  it can't do O(1) index math. Ordering the pipeline as `take | filter` vs
  `filter | take` changes both semantics and cost.
- Views are **not** `regular` in the old sense — many are move-only or have
  non-O(1) `begin()`. They're meant to be created, consumed, and dropped in the
  same expression.

> **HFT relevance:** views give you readable multi-stage data massaging (parse →
> filter by symbol → project a field → take N) with **zero heap allocation** and,
> at `-O2`, a fused loop — the abstraction is genuinely free for the simple
> adaptors (`transform`, `take`, `iota`, `elements`). Caveats for hot paths:
> `filter_view`'s first `begin()` is O(n); dangling is easy (`auto v = getVec() |
> views::transform(f);` — the temporary vector is gone); some views are
> single-pass. Teams tend to use the cheap adaptors freely and avoid `filter` /
> `join` / `split` in the innermost loop, or just write the loop. Range
> *algorithms* + projections, though, are a clear win everywhere — less
> boilerplate, same codegen.

---

## Hands-on

```bash
./build.ps1 19-STL/examples/05_ranges_demo.cpp
```

Shows: `filter|transform|take`, the other adaptors, the lazy call-counter proof,
`ranges::sort`/`find`/`count_if`, and materializing into a vector. Add a
`views::iota(1) | views::transform(sq) | views::take(10)` (infinite source) and a
`map | views::values | ... ` pipeline.

---

## ⚠️ Traps

### Trap 1 — view over a temporary → dangling
```cpp
auto bad = makeVector() | std::views::transform(f);   // ⚠️ the temporary vector dies; iterating `bad` is UB
for (int x : makeVector() | std::views::transform(f)) ...   // ✅ fine -- temporary lives to the end of the loop
```

### Trap 2 — expecting a view to cache its results
```cpp
auto ev = v | std::views::filter(isPrime);
for (int x : ev) ...;   // runs isPrime for all of v
for (int x : ev) ...;   // runs isPrime AGAIN. Materialize if you'll reuse it
```

### Trap 3 — `filter_view::begin()` is not O(1)
```cpp
auto f = v | std::views::filter(rarePred);
f.begin();   // ⚠️ O(n) -- scans for the first match (cached afterward). Costly in a tight loop
```

### Trap 4 — mutating through `transform`
```cpp
for (int& x : v | std::views::transform([](int x){ return x; })) x = 0;   // ❌ transform yields prvalues, not references
```

### Trap 5 — `std::ranges::to` on C++20
```cpp
auto out = view | std::ranges::to<std::vector>();   // ❌ C++23. On C++20: ranges::copy(view, back_inserter(out))
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "A view holds a copy of the data" | Non-owning — it refers to the source (which must outlive it) |
| "The pipeline computes each stage into a temp array" | Lazy — nothing runs until iteration; no intermediate containers |
| "Iterating a view is cheap to repeat" | Each pass re-runs the adaptors (predicate, transform) |
| "`filter | take` and `take | filter` are the same" | Different results *and* different iterator strength / cost |
| "`ranges::sort(v)` needs `<ranges>` only" | Also `<algorithm>`; and `v` must be a random-access range (still not `list`) |

---

## Exercises

1. **Rewrite with a pipeline:** given `std::vector<int> v`, build a vector of the
   squares of the first 5 odd numbers in `v`.

   <details><summary>Answer</summary>

   `std::vector<int> out; for (int x : v | std::views::filter([](int x){ return
   x % 2; }) | std::views::transform([](int x){ return x*x; }) |
   std::views::take(5)) out.push_back(x);`
   </details>

2. **Lazy proof:** with an infinite `std::views::iota(1)`, produce the first 10
   multiples of 3. Why doesn't this hang?

   <details><summary>Answer</summary>

   `std::views::iota(1) | std::views::filter([](int x){ return x % 3 == 0; }) |
   std::views::take(10)`. `take(10)` stops pulling after 10 elements, so `iota`
   is only advanced far enough to yield them — laziness bounds the infinite
   source.
   </details>

3. **Projection:** sort `std::vector<Order>` by `sym` ascending then `px`
   descending, using `std::ranges::sort` — can a single projection do it?

   <details><summary>Answer</summary>

   Not with one projection (it's two keys, opposite directions). Either a
   comparator lambda, or two stable sorts: `rg::stable_sort(v, std::greater<>{},
   &Order::px); rg::stable_sort(v, {}, &Order::sym);` (least-significant key
   first).
   </details>

4. **Dangling:** which is safe? (a) `auto view = vec | views::reverse;` then use
   `view` later (with `vec` still alive). (b) `auto view = std::vector<int>{1,2,3}
   | views::reverse;` then use `view`.

   <details><summary>Answer</summary>

   (a) safe — `view` refers to `vec`, which outlives it. (b) dangling — the
   temporary `vector` is destroyed at the end of the statement; `view` refers to
   freed memory. (`owning_view` would be needed, which `|` on a *named* rvalue
   doesn't give you here.)
   </details>

5. **map values:** given `std::map<std::string,int> m`, sum its values with a
   range pipeline.

   <details><summary>Answer</summary>

   `int total = 0; for (int x : m | std::views::values) total += x;` — or
   `std::ranges::fold_left(m | std::views::values, 0, std::plus<>{})` (C++23).
   </details>

---

## Interview questions

1. Range algorithm + projection — kya boilerplate hatata hai?
2. View kya hai — owning? lazy? `sizeof` chhota kyun?
3. Lazy evaluation ke 2 fayde (no temp containers, infinite ranges)?
4. View ko dobara iterate karne pe kya hota (re-runs)?
5. `filter_view::begin()` O(1) kyun nahi?
6. View over a temporary — kab dangling, kab safe?

---

## Next
→ [`15-utility-types.md`](15-utility-types.md)
