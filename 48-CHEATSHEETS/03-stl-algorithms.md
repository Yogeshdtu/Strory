# 03 — `<algorithm>` / `<numeric>` cheatsheet

`#include <algorithm>` `<numeric>` `<ranges>`. Deep: folder `19-STL`, `20-DSA`.
C++20: har algo ka `std::ranges::` version leta hai `(container)` seedha aur
projections. Neeche iterator form.

Convention: `[first, last)` half-open. Predicates take by value/`const&`, return `bool`.
Comparators = **strict weak ordering** (`comp(a,a)` must be `false`). (folder 05/B14)

---

## Non-modifying scan

| Call | Does |
|---|---|
| `all_of / any_of / none_of(f,l,pred)` | quantifiers |
| `count(f,l,val)` / `count_if(f,l,pred)` | how many |
| `find(f,l,val)` / `find_if` / `find_if_not` | first match → iterator (or `last`) |
| `find_first_of(f,l, s,e)` | first elem also in `[s,e)` |
| `adjacent_find(f,l)` | first equal neighbour pair |
| `search(f,l, s,e)` | subrange match (naive; `boyer_moore_searcher` for big) |
| `mismatch(f,l, f2)` | first differing position of two ranges |
| `equal(f,l, f2)` | ranges equal? |
| `lexicographical_compare(...)` | `<` on ranges |
| `for_each(f,l,fn)` | run `fn` on each (has `std::execution::par` overload) |

---

## Binary search — range MUST be sorted

| Call | Returns |
|---|---|
| `lower_bound(f,l,x)` | first `>= x` |
| `upper_bound(f,l,x)` | first `> x` |
| `equal_range(f,l,x)` | `{lower, upper}` = the run of `x` |
| `binary_search(f,l,x)` | `bool` only |

`count(lo,hi)` of `x` = `upper_bound - lower_bound`. All `O(log n)` on random-access
(else `O(n)` walk).

---

## Modifying / moving

| Call | Does |
|---|---|
| `copy / copy_if / copy_n / copy_backward` | copy out (use `back_inserter` or `reserve`) |
| `move(f,l,out)` | move-out (source = valid-but-unspecified) |
| `fill / fill_n` | write a value |
| `generate / generate_n` | write `fn()` |
| `transform(f,l, out, fn)` | map; 2-range overload for zip |
| `replace / replace_if` | in-place substitute |
| `remove / remove_if` | **shift kept elems forward, return new end** — doesn't shrink! |
| `unique(f,l)` | drop *consecutive* dups → new end |
| `reverse` / `rotate(f, mid, l)` | in place |
| `shuffle(f,l, rng)` | random permute (needs a `<random>` engine) |
| `sample(f,l, out, n, rng)` | pick `n` (C++17) |

**Erase–remove idiom:** `v.erase(std::remove_if(b, e, pred), e);` — or C++20
`std::erase_if(v, pred)`.

---

## Sorting & friends

| Call | Cost | Note |
|---|---|---|
| `sort(f,l[,cmp])` | `O(n log n)` | introsort; **not stable** |
| `stable_sort` | `O(n log n)` w/ buffer, else `O(n log²n)` | keeps equal order |
| `partial_sort(f, mid, l)` | `O(n log k)` | top-`k` sorted at the front |
| `nth_element(f, nth, l)` | `O(n)` avg | just `*nth` in place (median, k-th) |
| `is_sorted / is_sorted_until` | `O(n)` | |
| `partition(f,l,pred)` | `O(n)` | true-group first (unstable) |
| `stable_partition` | `O(n log n)` / `O(n)` w/ buffer | order kept |
| `merge(a..,b.., out)` | `O(n+m)` | two sorted → one |
| `inplace_merge(f, mid, l)` | `O(n log n)` | halves already sorted |

Don't full-`sort` when you need less: k-th → `nth_element`; top-k → `partial_sort`
/ a size-k heap.

---

## Heap (on a random-access range)

`make_heap` `O(n)` · `push_heap` (after `push_back`) `O(log n)` · `pop_heap`
(then `pop_back`) `O(log n)` · `sort_heap` `O(n log n)` · `is_heap`.

---

## Set ops — sorted ranges → sorted out

`set_union` · `set_intersection` · `set_difference` · `set_symmetric_difference`
· `includes` (subset test). All `O(n+m)`.

---

## `<numeric>`

| Call | Does |
|---|---|
| `accumulate(f,l, init[, op])` | fold **left**; result type = `init` type (`0` → int overflow, pass `0LL`) |
| `reduce(f,l[, init, op])` | like accumulate but **unordered** / parallelizable; float → non-deterministic |
| `transform_reduce(...)` | fused map+reduce (dot product) |
| `inner_product(...)` | `Σ aᵢ·bᵢ` |
| `partial_sum` / `inclusive_scan` / `exclusive_scan` | prefix sums |
| `adjacent_difference` | `aᵢ − aᵢ₋₁` |
| `iota(f,l, start)` | fill `start, start+1, …` |
| `gcd` / `lcm` / `midpoint` / `lerp` | scalar helpers |

---

## Ranges (C++20) — the short version

```cpp
namespace rv = std::views;
auto r = data | rv::filter(pred) | rv::transform(fn) | rv::take(10);  // lazy
std::ranges::sort(data);                        // no .begin()/.end()
std::ranges::for_each(data, fn);
auto it = std::ranges::find(data, x);
// projections: std::ranges::sort(people, {}, &Person::age);
```

Views are lazy + non-owning → don't pipe a temporary you then store; watch dangling.

## Next
→ [`04-complexity-tables.md`](04-complexity-tables.md)
