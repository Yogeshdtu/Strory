# 08 — Iterators in depth

## Prerequisites
- [`01-stl-architecture.md`](01-stl-architecture.md), [`02-vector-deep.md`](02-vector-deep.md) … [`07-container-adapters.md`](07-container-adapters.md)
- Folder 12 (pointers), folder 21 preview (templates)

## Yeh topic abhi kyun
Iterators STL ka **glue** hain. Har algorithm iterators pe likha hai, har
container iterators deta hai. Ab tak humne inhe use kiya — yahan detail: kitni
**categories**, har ek kya kar sakti, kaunsa algorithm kya maangta, aur
**invalidation** ke poore rules ek jagah.

---

## The category hierarchy (refresher + detail)

Each category is a **superset** of the one above — a random-access iterator can do
everything a forward iterator can, and more.

```
   input ─┐                                    output
          ├─ forward ─ bidirectional ─ random-access ─ contiguous (C++20)
   (read) ┘                                    (write)
```

| Category | Operations | `*it` semantics | Example sources |
|---|---|---|---|
| **Input** | `++it`, `it++`, `*it` (read), `==`, `!=` | read-only, **single pass** (once you `++`, old copies are dead) | `std::istream_iterator`, `std::ranges::istream_view` |
| **Output** | `++it`, `*it = v` | write-only, single pass | `std::ostream_iterator`, `std::back_inserter` |
| **Forward** | input + multi-pass (copy an iterator, both stay valid), default-constructible | read (and write if mutable) | `std::forward_list`, `std::unordered_*` |
| **Bidirectional** | forward + `--it`, `it--` | | `std::list`, `std::map`, `std::set` |
| **Random access** | bidi + `it + n`, `it - n`, `it2 - it1`, `it[n]`, `<` `>` `<=` `>=` | O(1) jump | `std::deque`, raw pointers |
| **Contiguous** (C++20) | random access + `*(it + n) == *(std::to_address(it) + n)` (elements physically adjacent) | | `std::vector`, `std::array`, `std::string`, `std::span` |

**How to read an algorithm's requirement:** `std::find` takes `InputIt` → works on
everything. `std::reverse` takes `BidirIt` → not on `forward_list`. `std::sort`
takes `RandomAccessIt` → not on `list`/`set`. `std::ranges` versions state this
as concepts (`std::random_access_iterator auto`).

`it->member` is shorthand for `(*it).member`. `it++` returns the **old** value (a
copy) — prefer `++it` for non-trivial iterators.

---

## `<iterator>` utilities

```cpp
#include <iterator>

// --- movement ---
std::advance(it, n);          // it += n, but works for any category (O(n) if not random-access). Returns void, MODIFIES it
auto j = std::next(it, n);    // returns it advanced by n, doesn't touch it. n defaults to 1
auto k = std::prev(it, n);    // backward (needs bidirectional)
auto d = std::distance(a, b); // b - a for random-access (O(1)); else counts ++ steps (O(n))

// --- inserter adaptors (model OUTPUT iterator) ---
std::back_inserter(c)         // *out = x  ->  c.push_back(x)
std::front_inserter(c)        // -> c.push_front(x)   (deque/list, not vector)
std::inserter(c, pos)         // -> c.insert(pos, x)  (set/map: pos is a hint)

// --- stream iterators ---
std::istream_iterator<int>(std::cin)     // reads ints until failure; default-constructed = end sentinel
std::ostream_iterator<int>(std::cout, " ")  // *out = x -> cout << x << " "

// --- reverse ---
std::make_reverse_iterator(it)           // or container.rbegin()/rend()

// --- C++20 ---
std::ssize(c)                 // signed size (ptrdiff_t) -- avoids unsigned-underflow bugs
std::to_address(it)           // raw pointer from a (contiguous) iterator, even if it's a wrapper class
```

Example — read all ints from stdin into a vector, print doubled:
```cpp
std::vector<int> v{ std::istream_iterator<int>(std::cin), std::istream_iterator<int>() };
std::transform(v.begin(), v.end(),
               std::ostream_iterator<int>(std::cout, " "),
               [](int x){ return x * 2; });
```

---

## Invalidation — the complete table

"Invalidated" = using it is UB. **iter** = iterators, **ref** = pointers &
references to elements.

| Container | Insert | Erase |
|---|---|---|
| **`vector`** | realloc → **all iter+ref**; no realloc → iter+ref **at/after** the point | iter+ref **at/after** the erased element |
| **`deque`** | at either end → **all iter**, but **ref stay valid**; middle → **all iter+ref** | at either end → only the erased iter+ref; middle → **all iter+ref** |
| **`list` / `forward_list`** | **nothing** invalidated | only the erased element's iter+ref |
| **`set` / `map` / `multi*`** | **nothing** invalidated | only the erased element's iter+ref |
| **`unordered_*`** | rehash → **all iter**; **ref stay valid**. No rehash → nothing | only the erased element's iter+ref |
| **`std::array`** | n/a (fixed) | n/a |

Mnemonics:
- **Node-based (`list`, `map`, `set`)** — maximally stable; only erase kills the
  erased node.
- **`vector`** — realloc kills everything; otherwise everything from the edit
  point onward.
- **`deque`** — end operations spare *references* but kill *iterators*; middle
  operations kill both.
- **`unordered_*`** — rehash kills *iterators* but spares *references*.
- `end()` / `past-the-end` iterators are invalidated by anything that changes
  `size()` for `vector`/`deque`.

`erase()` returns the iterator **past** the removed element — the safe way to
delete while looping:
```cpp
for (auto it = c.begin(); it != c.end(); )
    if (pred(*it)) it = c.erase(it);      // erase returns the next valid iterator
    else           ++it;
```

---

## Writing your own iterator (minimal forward iterator)

```cpp
struct IntRange {
    int lo, hi;
    struct iterator {
        int v;
        using value_type        = int;
        using difference_type   = std::ptrdiff_t;
        using iterator_category = std::forward_iterator_tag;
        using pointer           = const int*;
        using reference         = int;

        int  operator*()  const { return v; }
        iterator& operator++()  { ++v; return *this; }
        iterator  operator++(int){ auto t = *this; ++v; return t; }
        bool operator==(const iterator& o) const { return v == o.v; }
    };
    iterator begin() const { return {lo}; }
    iterator end()   const { return {hi}; }
};

for (int x : IntRange{3, 7}) std::printf("%d ", x);   // 3 4 5 6
long s = std::accumulate(IntRange{1,101}.begin(), IntRange{1,101}.end(), 0L);  // 5050
```

The 5 `using` typedefs (or a `std::iterator_traits` specialization) are how
algorithms introspect your iterator. For random-access you'd also add `+`, `-`,
`[]`, `<`, and `difference_type operator-(iterator)`.

---

## Andar kya hota hai

- `std::iterator_traits<It>` pulls `value_type`, `difference_type`,
  `iterator_category` etc. from the iterator (or from `T*` for raw pointers).
  Algorithms **dispatch on the category tag**: `std::advance` has overloads for
  `input_iterator_tag` (loop `++`), `bidirectional_iterator_tag` (loop `++`/`--`),
  `random_access_iterator_tag` (`it += n`) — picked at compile time, zero runtime
  cost.
- `std::distance` similarly: O(1) `b - a` for random-access, O(n) count otherwise.
  Calling it on a `std::list` in a hot loop is a hidden O(n).
- For `std::vector`, libstdc++'s `iterator` is `__gnu_cxx::__normal_iterator<T*,
  vector>` — a class wrapping a `T*`, adding debug bounds checks under
  `_GLIBCXX_DEBUG`. At `-O2` with no debug mode it's a zero-overhead wrapper; the
  generated code is identical to using `T*`.
- Contiguous-iterator concept lets algorithms recover the pointer
  (`std::to_address`) and call `memcpy`/`memmove`/SIMD paths.

> **HFT relevance:** the tag-dispatch machinery is all compile-time → iterators
> cost nothing at runtime; a `std::vector` iterator loop is a raw-pointer loop.
> The traps that matter: (1) `std::distance` / `std::advance` on non-random-access
> iterators is O(n) — easy to hide an O(n²) in a `list`/`map` loop. (2)
> Invalidation bugs: caching an iterator/pointer across a `push_back` or a rehash
> → UB that may "work" in testing and corrupt in production. Hot code that must
> hold references across mutation uses node-based containers, `std::deque` (end
> ops), pre-`reserve`d vectors, or indices into a stable arena.

---

## Hands-on

```bash
./build.ps1 19-STL/examples/03_algorithms_tour.cpp
./build.ps1 19-STL/examples/01_vector_deep.cpp
```

Write: the `IntRange` iterator above and feed it to `std::accumulate` and
`std::ranges::for_each`. Then trigger an invalidation bug on purpose — hold
`&v[0]`, `push_back` past capacity, print `*p` under `./build.ps1 san`.

---

## ⚠️ Traps

### Trap 1 — `std::distance` on a `list`/`map` in a loop
```cpp
for (auto it = m.begin(); it != m.end(); ++it)
    if (std::distance(m.begin(), it) == target) ...   // ⚠️ O(n) each iteration -> O(n²). Track an index yourself
```

### Trap 2 — erase in a range-for
```cpp
for (auto& x : v) if (pred(x)) v.erase(&x);   // ❌ range-for caches end(); erase invalidates. Use the erase-return loop or std::erase_if
```

### Trap 3 — `it++` where `++it` would do
```cpp
for (auto it = big.begin(); it != big.end(); it++)   // ⚠️ it++ copies the iterator each step. ++it for non-trivial iterators
```

### Trap 4 — dereferencing a stale iterator after `push_back`
```cpp
auto it = v.begin();  v.push_back(1);  *it;   // ⚠️ realloc -> it dangles
```

### Trap 5 — mixing iterators from two containers
```cpp
std::find(a.begin(), b.end(), x);   // ❌ different containers -> UB. Both must be from the same range
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "All iterators support `it + n`" | Only random-access/contiguous. `list`/`map` iterators don't |
| "`std::distance` is O(1)" | O(1) for random-access; O(n) otherwise |
| "`std::advance(it, n)` returns the advanced iterator" | Returns `void`, modifies `it` in place. `std::next` returns a new one |
| "`vector` iterators are slow because they're a class" | At `-O2` (no debug mode) they compile to raw pointers |
| "Erase invalidates only for `vector`" | `deque` middle-erase and any `vector` erase invalidate from the point on; nodes/unordered are gentler |

---

## Exercises

1. **Category quiz:** for each, name the minimum iterator category and whether it
   compiles on `std::list`: `std::sort`, `std::reverse`, `std::find_if`,
   `std::nth_element`, `std::lower_bound`.

   <details><summary>Answer</summary>

   `sort` — random-access — ❌ on list. `reverse` — bidirectional — ✅ on list.
   `find_if` — input — ✅. `nth_element` — random-access — ❌. `lower_bound` —
   forward (but O(log n) only with random-access; on list it *compiles* but is
   O(n)) — ✅ compiles, not useful.
   </details>

2. **Safe erase:** remove every negative from a `std::list<int>` in one pass
   using the erase-return idiom. Then do it in one line for a `std::vector<int>`.

   <details><summary>Answer</summary>

   list: `for (auto it = l.begin(); it != l.end(); ) { if (*it < 0) it =
   l.erase(it); else ++it; }`. vector: `std::erase_if(v, [](int x){ return x < 0;
   });` (C++20).
   </details>

3. **Invalidation:** `std::deque<int> d(100); int& r = d[50]; auto it = d.begin();
   d.push_front(0);` — is `r` valid? Is `it` valid?

   <details><summary>Answer</summary>

   `r` — valid (deque end-insertion keeps element references valid). `it` —
   invalid (deque insertion at either end invalidates all iterators).
   </details>

4. **Custom iterator:** extend `IntRange::iterator` to random-access (add what's
   needed for `std::sort` to accept it — though sorting a range of consecutive
   ints is pointless, the compiler check is the exercise).

   <details><summary>Answer</summary>

   Add `iterator_category = std::random_access_iterator_tag`; `operator+(n)`,
   `operator-(n)`, `operator+=`, `operator-=`, `difference_type operator-(iterator
   o) const { return v - o.v; }`, `operator[](n)`, and `<`,`>`,`<=`,`>=`. Now it
   models random-access and `std::sort` compiles.
   </details>

5. **stream iterators:** in one `std::copy`, read all whitespace-separated
   `double`s from `std::cin` and print each on its own line to `std::cout`.

   <details><summary>Answer</summary>

   `std::copy(std::istream_iterator<double>(std::cin),
   std::istream_iterator<double>(), std::ostream_iterator<double>(std::cout,
   "\n"));`
   </details>

---

## Interview questions

1. Iterator categories — 6 batao, har ek kya add karti hai?
2. `std::sort` `std::list` pe kyun nahi chalta?
3. `std::advance` vs `std::next` — return, mutation ka fark?
4. `std::distance` kab O(n) hai — usse kaise bug banta hai?
5. `vector` iterator `-O2` pe kya hai? Debug mode mein?
6. Tag dispatch se `std::advance` overloads kaise chunte hain (compile-time)?

---

## Next
→ [`09-algorithms-part1.md`](09-algorithms-part1.md)
