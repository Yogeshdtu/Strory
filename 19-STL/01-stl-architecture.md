# 01 — STL architecture (containers + iterators + algorithms)

## Prerequisites
- Folders 09–11 (arrays, strings, structs), folder 12–13 (pointers, references)
- Folder 17–18 (RAII, move semantics — every container is RAII + move-aware)
- Folder 21 preview (templates — the STL is all templates)

## Yeh topic abhi kyun
STL (Standard Template Library) ka design ek elegant idea par khada hai:
**containers**, **iterators**, aur **algorithms** ko **alag** rakho, aur
iterators se unhe jodo. Isse M containers × N algorithms ka combinatorial
explosion **M + N** ban jaata hai. Yeh model samajhna zaroori — phir baaki poora
folder isi ka detail hai.

---

## The three pillars

```
   CONTAINERS  ──(provide)──►  ITERATORS  ──(consumed by)──►  ALGORITHMS
   vector, map,               begin()/end(),                 sort, find,
   list, set, ...             pointers-that-know-how          transform, ...
                              -to-advance
```

- **Containers** — data structures that own elements: `std::vector`,
  `std::map`, `std::unordered_map`, `std::deque`, `std::list`, ... Each manages
  storage, growth, and element lifetime (RAII).
- **Iterators** — a generalization of pointers. `it != end`, `++it`, `*it`,
  `it->m`. They *are* pointers for `std::vector`; for `std::map` they're
  tree-node walkers; for `std::list` they follow `next` links. Same syntax, hides
  the structure.
- **Algorithms** — functions that operate on **iterator ranges** `[first, last)`,
  not on containers: `std::sort(v.begin(), v.end())`, `std::find(m.begin(),
  m.end(), x)`. They know nothing about the container — just how to `++` and
  `*` the iterators.

**The decoupling:** `std::sort` is written once. It works on any container whose
iterators support random access. Add a new container → all existing algorithms
work on it. Add a new algorithm → it works on all existing containers. `M + N`,
not `M × N`.

---

## Half-open ranges `[first, last)`

```cpp
std::vector<int> v{10, 20, 30, 40};

v.begin()   // iterator to the first element (10)
v.end()     // iterator ONE PAST the last -- NOT a valid element, just a sentinel

std::sort(v.begin(), v.end());              // whole container
std::sort(v.begin(), v.begin() + 2);       // just the first 2
auto it = std::find(v.begin(), v.end(), 30);
if (it != v.end()) { /* found -- *it == 30 */ }
```

- `[first, last)` — includes `first`, **excludes** `last`.
- An empty range is `first == last`.
- `end()` is a sentinel; dereferencing it is UB.
- `last - first` = the number of elements (for random-access iterators).

This convention makes sub-ranges, "not found" (`== end()`), and loop termination
(`while (it != end)`) uniform.

---

## Iterator categories (file 08 — detail)

Iterators have **capabilities**, forming a hierarchy:

| Category | Can do | Containers |
|---|---|---|
| **Input** | `++`, `*` (read once), single pass | `istream_iterator` |
| **Output** | `++`, `*` (write once) | `back_inserter`, `ostream_iterator` |
| **Forward** | `++`, `*` (multi-pass) | `forward_list`, `unordered_*` |
| **Bidirectional** | `++`, `--` | `list`, `map`, `set` |
| **Random access** | `+ n`, `- n`, `[]`, `<`, `it2 - it1` | `vector`, `deque`, `array`, raw pointers |
| **Contiguous** (C++20) | random access + elements are adjacent in memory | `vector`, `array`, `string`, `span` |

An algorithm requires a minimum category. `std::sort` needs **random access**
(it does `first + n/2`). `std::find` needs only **input**. `std::list` can't be
`std::sort`ed with `std::algorithm` — it has `list::sort()` (a member).

---

## Common utilities that glue it together

```cpp
#include <iterator>

std::back_inserter(v)      // an output iterator: *it = x -> v.push_back(x)
std::front_inserter(dq)    // -> dq.push_front(x)
std::inserter(s, s.end())  // -> s.insert(pos, x)  (for set/map)

std::distance(first, last) // number of elements (O(1) random-access, O(n) otherwise)
std::advance(it, n)        // move it forward n (O(1) or O(n))
std::next(it, n)           // returns it advanced by n (doesn't modify it)
std::prev(it, n)

std::begin(c), std::end(c) // work on C arrays too: std::begin(rawArray)
```

`std::back_inserter` is how algorithms that "produce output" (`std::transform`,
`std::copy`, `std::set_union`) grow a destination container:

```cpp
std::vector<int> out;
std::transform(v.begin(), v.end(), std::back_inserter(out), [](int x){ return x * 2; });
// out now has v's elements doubled
```

---

## Non-member `begin`/`end` and range-`for`

```cpp
for (const auto& x : container) { ... }
// desugars to (roughly):
{
    auto&& __r = container;
    auto __b = std::begin(__r);          // c.begin(), or begin(c) via ADL, or pointer for arrays
    auto __e = std::end(__r);
    for (; __b != __e; ++__b) {
        const auto& x = *__b;
        ...
    }
}
```

Any type with `begin()`/`end()` (or free `begin(x)`/`end(x)`) works with
range-`for` and with C++20 ranges algorithms — including your own types and
`std::span` / views.

---

## Andar kya hota hai

- Templates: every container and algorithm is a template. `std::vector<int>` and
  `std::vector<std::string>` are separate instantiations. The compiler generates
  the code per type used → binary size cost, but full optimization (no type
  erasure, everything inlinable).
- `std::vector<T>::iterator` is often just `T*` (libstdc++ wraps it in
  `__normal_iterator` for debug checks; at `-O2` it's a raw pointer). `std::sort`
  on a `vector` compiles to introsort over raw pointers — as fast as a
  hand-written qsort, and type-safe.
- Node containers (`list`, `map`) — iterators are small structs holding a node
  pointer; `++it` follows `next` / tree-successor. Each `*it` may be a cache
  miss (nodes scattered on the heap — file 25, `examples/11_container_benchmark.cpp`).
- Algorithms are `constexpr` (C++20) where possible and heavily specialized
  (e.g. `std::copy` on trivially-copyable contiguous ranges → `memmove`).

> **HFT relevance:** the STL's zero-overhead-abstraction promise mostly holds for
> `std::vector` + `<algorithm>` on contiguous data — `std::sort`, `std::lower_bound`,
> `std::accumulate` compile to tight, vectorizable loops. It breaks down for
> node-based containers (`std::map`, `std::list`, `std::unordered_map` default) —
> a pointer chase and likely cache miss per element (file 25). HFT hot paths use
> `std::vector` + algorithms + `std::span` views, and hand-rolled flat structures
> (sorted arrays, open-addressing hash maps) where `std::map`/`std::unordered_map`
> would be. The iterator/algorithm model itself is kept — it's the *default
> containers* that get replaced.

---

## Hands-on

```bash
./build.ps1 19-STL/examples/03_algorithms_tour.cpp        # 30+ algorithms on a vector
./build.ps1 19-STL/examples/01_vector_deep.cpp
```

Write: a `std::transform` + `std::back_inserter` that turns a `std::vector<int>`
into a `std::vector<std::string>`; a `std::find` that reports "not found" via
`== end()`.

---

## ⚠️ Traps

### Trap 1 — dereferencing `end()`
```cpp
auto it = std::find(v.begin(), v.end(), x);  int y = *it;   // ⚠️ UB if not found. Check `it != v.end()` first
```

### Trap 2 — passing a container to an algorithm (pre-C++20)
```cpp
std::sort(v);   // ❌ pre-C++20. std::sort(v.begin(), v.end()); or std::ranges::sort(v) (C++20)
```

### Trap 3 — `std::sort` on a `std::list`
```cpp
std::sort(lst.begin(), lst.end());   // ❌ list iterators aren't random-access. lst.sort()
```

### Trap 4 — output iterator without a sink
```cpp
std::vector<int> out;
std::transform(v.begin(), v.end(), out.begin(), f);   // ⚠️ out is empty -> writes past end -> UB. std::back_inserter(out)
```

### Trap 5 — `std::distance` on non-random-access iterators is O(n)
```cpp
std::distance(lst.begin(), it);   // O(n) walk for a list. For vector it's O(1)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Algorithms operate on containers" | On iterator ranges `[first, last)` — container-agnostic |
| "`end()` is the last element" | One-past-the-last — a sentinel, not dereferenceable |
| "Any algorithm works on any container" | Depends on iterator category (`std::sort` needs random access) |
| "`std::vector::iterator` is slow (a class)" | At `-O2` it's a raw pointer; algorithms are as fast as hand-written |
| "The STL is uniformly zero-overhead" | Contiguous + algorithms: yes. Node containers: pointer-chase per element |

---

## Exercises

1. **M+N:** you have 5 container types and 20 algorithms. Without the iterator
   abstraction, how many "sort for container X" functions would you write? With
   it?

   <details><summary>Answer</summary>

   Without: up to 5 × 20 = 100 (each algorithm reimplemented per container).
   With: 5 (containers) + 20 (algorithms) = 25 — each algorithm written once
   against iterators.
   </details>

2. **Half-open:** `std::vector<int> v{1,2,3,4,5};` — write a loop over just the
   middle 3 using iterators, and one using indices. What's `v.end() - v.begin()`?

   <details><summary>Answer</summary>

   Iterators: `for (auto it = v.begin()+1; it != v.begin()+4; ++it)`. Indices:
   `for (size_t i = 1; i < 4; ++i)`. `v.end() - v.begin() == 5` (the size).
   </details>

3. **Category:** which of these compile? `std::sort(list.begin(), list.end())`,
   `std::find(list.begin(), list.end(), x)`, `std::sort(vec.begin(),
   vec.end())`, `std::binary_search(forwardList.begin(), forwardList.end(), x)`.

   <details><summary>Answer</summary>

   `std::find` on list — OK (needs input iterator). `std::sort` on vector — OK
   (random access). `std::sort` on list — **compile error** (bidirectional, not
   random access) → use `list.sort()`. `binary_search` on `forward_list` — compiles
   but is O(n) (no random access → can't halve), so pointless.
   </details>

4. **back_inserter:** copy the even elements of `v` into a new `std::vector<int>
   evens;` using `std::copy_if` + `std::back_inserter`.

   <details><summary>Answer</summary>

   `std::vector<int> evens; std::copy_if(v.begin(), v.end(), std::back_inserter(evens),
   [](int x){ return x % 2 == 0; });`
   </details>

5. **Custom range:** give a `struct Ring { int buf[8]; int* begin(); int* end();
   };` a `begin()`/`end()` so `for (int x : ring)` and `std::accumulate(ring.begin(),
   ring.end(), 0)` work.

   <details><summary>Answer</summary>

   `int* begin() { return buf; } int* end() { return buf + 8; }` (+ `const`
   overloads). Now range-`for`, `std::accumulate`, `std::ranges::sort(ring)` all
   work — raw pointers are contiguous iterators.
   </details>

---

## Interview questions

1. STL ke 3 pillars — kaise decoupled, M+N vs M×N?
2. Half-open range `[first, last)` — kyun, `end()` kya?
3. Iterator categories — `std::sort` ko kaunsa chahiye, `std::find` ko?
4. `std::back_inserter` — kya karta, kab chahiye?
5. `std::vector::iterator` `-O2` pe kya hai? Algorithm ki speed?
6. STL "zero overhead" — kahan hold karta, kahan nahi?

---

## Next
→ [`02-vector-deep.md`](02-vector-deep.md)
