# 09 — Algorithms part 1: non-modifying sequence ops

## Prerequisites
- [`08-iterators.md`](08-iterators.md), [`01-stl-architecture.md`](01-stl-architecture.md)
- Lambdas (folder 16 preview / folder 22)

## Yeh topic abhi kyun
`<algorithm>` mein ~110 functions hain. Ye lesson **non-modifying** waale cover
karta — jo range ko padhte hain par badalte nahi: search, count, compare,
predicate checks. Ye raw loops ki jagah lete hain — chhote, intent-revealing, aur
aksar zyada optimize hue (`std::find` on contiguous bytes → `memchr`).

`#include <algorithm>`. Sab C++20 `constexpr` (compile-time bhi chal sakte).
C++20 se `std::ranges::` versions bhi — `std::ranges::find(v, x)` (no
`.begin()/.end()`).

---

## Searching for a value / predicate

```cpp
auto it  = std::find(first, last, value);                 // first element == value, or last
auto it2 = std::find_if(first, last, pred);               // first where pred(x) is true
auto it3 = std::find_if_not(first, last, pred);           // first where pred(x) is false

// always check against last:
if (it != last) { /* found, *it is it */ } else { /* not found */ }

auto it4 = std::find_first_of(first, last, s_first, s_last);   // first element that equals ANY in [s_first, s_last)
auto it5 = std::adjacent_find(first, last);               // first position where x[i] == x[i+1]
auto it6 = std::adjacent_find(first, last, pred);         // ...where pred(x[i], x[i+1])
```

## Counting

```cpp
std::ptrdiff_t n  = std::count(first, last, value);       // how many == value
std::ptrdiff_t m  = std::count_if(first, last, pred);     // how many satisfy pred
```
(Return type is the iterator's `difference_type` — `long long` on Win64. Print
with `%lld` — see `examples/03`.)

## Predicate checks over the whole range

```cpp
bool a = std::all_of (first, last, pred);   // true for every element (vacuously true if empty)
bool o = std::any_of (first, last, pred);   // true for at least one
bool n = std::none_of(first, last, pred);   // true for none
```

## Comparing two ranges

```cpp
bool eq = std::equal(a_first, a_last, b_first);              // ⚠️ assumes b has >= as many elements
bool eq2= std::equal(a_first, a_last, b_first, b_last);      // ✅ safe 4-iterator form (checks lengths)

auto [pa, pb] = std::mismatch(a_first, a_last, b_first);     // first position where they differ (pair of iterators)

bool lex = std::lexicographical_compare(a_first, a_last, b_first, b_last);  // a < b, dictionary order
auto cmp = std::lexicographical_compare_three_way(/*...*/);  // C++20 -> std::strong_ordering
```

## Subrange search

```cpp
auto s  = std::search(first, last, pat_first, pat_last);    // first occurrence of the subsequence [pat_first, pat_last)
auto s2 = std::search(first, last, std::boyer_moore_searcher(pat_first, pat_last));  // faster for long patterns
auto e  = std::find_end(first, last, pat_first, pat_last);  // LAST occurrence
auto sn = std::search_n(first, last, count, value);         // first run of `count` consecutive `value`
```

## Min / max over a range

```cpp
auto mn  = std::min_element(first, last);                   // iterator to the smallest
auto mx  = std::max_element(first, last, cmp);
auto [lo, hi] = std::minmax_element(first, last);           // both in one pass (~1.5n comparisons, not 2n)

int  a = std::min(x, y);                                    // value versions (not iterators)
int  b = std::max({x, y, z, w});                            // initializer_list form
auto c = std::clamp(v, lo, hi);                             // C++17: max(lo, min(v, hi))
```

---

## Prefer these over hand loops — why

```cpp
// hand-written:
bool found = false;
for (std::size_t i = 0; i < v.size(); ++i) if (v[i] == target) { found = true; break; }

// algorithm:
bool found = std::find(v.begin(), v.end(), target) != v.end();
// or: bool found = std::ranges::contains(v, target);   // C++23
```

- **Intent** is in the name — a reader sees "find", not a loop to decode.
- **Fewer bugs** — no off-by-one, no forgotten `break`, correct empty-range
  behaviour baked in.
- **Optimized** — `std::find` on a contiguous range of bytes/ints becomes
  `memchr` / a SIMD scan in libstdc++. `std::count` auto-vectorizes. Hard to beat
  by hand.
- **Composable** with `std::ranges` and views (file 14).

Cost: none at `-O2` — these inline to the same (or better) code as the loop.

---

## Andar kya hota hai

- `std::find` / `std::count` / `std::all_of` are simple linear scans, but
  libstdc++ specializes: `std::find` for `T*` where `T` is 1-byte → `__memchr`;
  integer ranges → a hand-vectorized loop. `std::count` over `int` → SIMD
  add-reduce.
- `std::search` (default) is naive O(n·m); `std::boyer_moore_searcher` preprocesses
  the pattern into skip tables → sublinear on average for long patterns (used
  internally by some `string::find` paths).
- `std::minmax_element` does ~`3⌊n/2⌋` comparisons (process elements in pairs)
  vs `2n` for separate `min_element` + `max_element`.
- Everything takes iterators by value and the predicate by value — a lambda with
  no captures is an empty class, passed in a register, fully inlined.

> **HFT relevance:** for **small, contiguous** ranges (a few price levels, a
> fixed-size symbol table) `std::find`/`std::count`/`std::any_of` compile to tight
> vectorized scans that beat a branchy hand loop and a hash lookup both — linear
> over 8–32 contiguous elements is often the fastest possible. The win fades once
> the range is large enough to blow the cache — then you need the right data
> structure, not a faster scan. `std::min_element`/`std::max_element` over a
> sorted book are O(1) at the ends instead (`front()`/`back()`); use the
> algorithms on unsorted scratch data.

---

## Hands-on

```bash
./build.ps1 19-STL/examples/03_algorithms_tour.cpp
```

30+ algorithms on one vector. Add: `std::adjacent_find` to detect the first
duplicate-in-a-row; `std::mismatch` to find where two `std::string`s first
differ; `std::minmax_element` and confirm it's one pass.

---

## ⚠️ Traps

### Trap 1 — not comparing against `last`
```cpp
auto it = std::find(v.begin(), v.end(), x);  use(*it);   // ⚠️ UB if not found. if (it != v.end())
```

### Trap 2 — 3-iterator `std::equal` with a shorter second range
```cpp
std::equal(a.begin(), a.end(), b.begin());   // ⚠️ reads b past its end if b.size() < a.size(). Use the 4-iterator form
```

### Trap 3 — `%ld` for `std::count`'s return on Win64
```cpp
std::printf("%ld\n", std::count(v.begin(), v.end(), x));   // ⚠️ difference_type is long long here -> %lld
```

### Trap 4 — `std::min`/`std::max` with mixed types
```cpp
std::max(3, 4u);      // ❌ int vs unsigned -> no matching overload (template arg deduction fails). std::max(3, 4) or cast
std::max(a, b + 1);   // fine if same type
```

### Trap 5 — `std::max_element` returns an iterator, not a value
```cpp
int m = std::max_element(v.begin(), v.end());   // ❌ it's an iterator. int m = *std::max_element(...); (guard empty!)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`std::find` returns `-1` / `nullptr` when absent" | Returns `last` — always compare to `end()` |
| "`std::count` returns `int`" | Returns `difference_type` (`long long` on LP64/LLP64) — print `%lld` |
| "`std::all_of` on an empty range is false" | It's `true` (vacuous); `any_of` is `false`, `none_of` is `true` |
| "`std::equal(a,a,b)` is safe" | Only if `b` has at least as many elements — use the 4-iterator overload |
| "These are slower than a raw loop" | At `-O2` they match or beat it (memchr/SIMD specializations) |

---

## Exercises

1. **Any/all/none:** given `std::vector<int> v`, one expression each for: "all
   positive", "some even", "no zeros".

   <details><summary>Answer</summary>

   `std::all_of(v.begin(), v.end(), [](int x){ return x > 0; })`;
   `std::any_of(v.begin(), v.end(), [](int x){ return x % 2 == 0; })`;
   `std::none_of(v.begin(), v.end(), [](int x){ return x == 0; })`.
   </details>

2. **First difference:** two `std::string`s `a`, `b`. Print the index where they
   first differ, or "equal prefix" if one is a prefix of the other.

   <details><summary>Answer</summary>

   `auto [pa, pb] = std::mismatch(a.begin(), a.end(), b.begin(), b.end()); if (pa
   == a.end() || pb == b.end()) puts("equal prefix"); else printf("%td\n", pa -
   a.begin());`
   </details>

3. **minmax one pass:** why is `std::minmax_element` preferable to calling
   `min_element` and `max_element` separately? How many comparisons for n
   elements?

   <details><summary>Answer</summary>

   One pass instead of two (better cache use), and ~`3⌊n/2⌋` comparisons vs `2n`
   — it processes elements in pairs, comparing the pair first then each to the
   running min/max.
   </details>

4. **contains, pre-C++23:** write a `contains(range, value)` helper in terms of
   `std::find`, and a `count_between(v, lo, hi)` using `std::count_if`.

   <details><summary>Answer</summary>

   `bool contains(const auto& r, const auto& x){ return std::find(std::begin(r),
   std::end(r), x) != std::end(r); }`. `auto count_between(const std::vector<int>&
   v, int lo, int hi){ return std::count_if(v.begin(), v.end(), [&](int x){ return
   lo <= x && x <= hi; }); }`
   </details>

5. **search:** find whether the byte pattern `{0x7E, 0x00, 0x7E}` appears in a
   `std::vector<std::uint8_t> frame`, and at what offset.

   <details><summary>Answer</summary>

   `std::array<std::uint8_t,3> pat{0x7E,0x00,0x7E}; auto it =
   std::search(frame.begin(), frame.end(), pat.begin(), pat.end()); if (it !=
   frame.end()) printf("at %td\n", it - frame.begin());`
   </details>

---

## Interview questions

1. `std::find` "not found" kaise batata? Return type?
2. `std::all_of` empty range pe kya deta, kyun?
3. `std::equal` ka 3-iterator vs 4-iterator form — kaunsa safe?
4. `std::count` ka return type kya, Win64 pe kaise print karein?
5. `std::minmax_element` ek pass mein kaise, kitni comparisons?
6. Raw loop ke bajaye algorithm kyun — 3 wajah?

---

## Next
→ [`10-algorithms-part2.md`](10-algorithms-part2.md)
