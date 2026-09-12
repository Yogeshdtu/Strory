# 10 — Algorithms part 2: modifying ops + the erase-remove idiom

## Prerequisites
- [`09-algorithms-part1.md`](09-algorithms-part1.md)
- [`02-vector-deep.md`](02-vector-deep.md) (size vs capacity), [`08-iterators.md`](08-iterators.md) (invalidation)

## Yeh topic abhi kyun
Ab woh algorithms jo range **badalte** hain — copy, move, transform, replace,
fill, reverse, rotate, aur sabse important: `remove` / `unique` aur unka
**erase-remove idiom**. Yeh idiom har C++ dev ko aana chahiye — kyunki `std::
remove` akele container ka size **nahi** badalta, aur yeh #1 confusion hai.

`examples/04_erase_remove.cpp` yeh sab live dikhata hai.

---

## Copy / move

```cpp
std::copy(first, last, d_first);                 // d_first must have room for last-first elements
std::copy_if(first, last, d_first, pred);        // copy only those satisfying pred
std::copy_n(first, n, d_first);
std::copy_backward(first, last, d_last);         // copies right-to-left -> safe when ranges overlap and dest > src
std::move(first, last, d_first);                 // like copy but move-assigns each element (source left "valid but unspecified")
std::move_backward(first, last, d_last);

// growing a destination -> back_inserter:
std::vector<int> out;
std::copy_if(v.begin(), v.end(), std::back_inserter(out), [](int x){ return x > 0; });
```

`std::copy` on trivially-copyable contiguous ranges → `memmove` in libstdc++.

## Transform

```cpp
std::transform(first, last, d_first, unaryOp);                 // d[i] = op(s[i])
std::transform(a_first, a_last, b_first, d_first, binaryOp);   // d[i] = op(a[i], b[i])

std::vector<int> lens;
std::transform(words.begin(), words.end(), std::back_inserter(lens),
               [](const std::string& s){ return (int)s.size(); });
```
`d_first` **may equal** `first` — in-place transform is allowed.

## Fill / generate / replace / iota

```cpp
std::fill(first, last, value);
std::fill_n(first, n, value);
std::generate(first, last, gen);            // x = gen()  each time (gen takes no args)
std::generate_n(first, n, gen);
std::iota(first, last, start);              // start, start+1, start+2, ...   (<numeric>)

std::replace(first, last, oldVal, newVal);
std::replace_if(first, last, pred, newVal);
```

## Reverse / rotate / shuffle / sample

```cpp
std::reverse(first, last);
std::rotate(first, middle, last);           // makes `middle` the new first; returns iterator to where the old first landed
std::shuffle(first, last, rng);             // needs a URBG (std::mt19937) -- file 18
std::sample(first, last, out, k, rng);      // k random elements without replacement (C++17)
```

`std::rotate` is O(n), does it in place with ≤ n swaps — useful for "move this
block to the front/back" without extra storage.

## Remove / unique — **these do NOT resize**

```cpp
// std::remove: shifts the kept elements to the front, returns the new logical end.
// The tail [newEnd, last) holds unspecified (moved-from) values. SIZE IS UNCHANGED.
auto newEnd = std::remove(v.begin(), v.end(), 42);        // "remove all 42s"
auto newEnd2= std::remove_if(v.begin(), v.end(), pred);

// std::unique: collapses CONSECUTIVE equal elements to one. Also returns a new end. Also no resize.
auto ue = std::unique(v.begin(), v.end());               // sort first if you want global dedup
```

Why can't they resize? Algorithms only have **iterators** — they can't call
`v.erase()` or shrink a container they know nothing about. They can only
rearrange within `[first, last)` and tell you where the "real" data now ends.

---

## The erase-remove idiom

```cpp
// remove all elements == 42:
v.erase(std::remove(v.begin(), v.end(), 42), v.end());

// remove all elements satisfying a predicate:
v.erase(std::remove_if(v.begin(), v.end(), [](int x){ return x < 0; }), v.end());

// dedup a sorted range:
std::sort(v.begin(), v.end());
v.erase(std::unique(v.begin(), v.end()), v.end());
```

Read it as: `remove` compacts and returns the new end → `erase(newEnd, end())`
actually drops the tail and updates `size()`.

**C++20 — just use the free functions:**
```cpp
std::erase(v, 42);                                        // one call, does the whole idiom
std::erase_if(v, [](int x){ return x < 0; });
// work on std::vector, deque, list, string, forward_list, and (erase_if) map/set/unordered_*
```
Prefer `std::erase` / `std::erase_if` in new code. Know the idiom because it's
everywhere in existing code and in interviews.

Complexity: `remove` is O(n) (one pass, moves kept elements). `erase(newEnd,
end())` is O(distance removed) destructor calls (O(1) for trivially destructible
+ just lowers `size_`). Total O(n), **one pass**, vs O(n²) for erase-in-a-loop.

---

## Andar kya hota hai

- `std::remove(f, l, val)`: scan for the first `*it == val` → that's the write
  head `w`. Continue with a read head `r`; whenever `*r != val`, `*w++ =
  std::move(*r)`. Return `w`. Elements `[w, l)` are moved-from husks. O(n) reads,
  ≤ n moves.
- `v.erase(w, v.end())`: runs `~T()` on `[w, end)` (skipped entirely for trivially
  destructible `T`), then sets `size_ = w - begin()`. **Capacity unchanged** —
  the buffer isn't freed or shrunk.
- The naive alternative `for (auto it = v.begin(); it != v.end();) if (*it ==
  val) it = v.erase(it); else ++it;` — each `v.erase(it)` shifts every later
  element down by one → O(n) per removal → **O(n²)** if you remove O(n) elements.
  The idiom is O(n) total.
- `std::copy` / `std::move` / `std::fill` on trivially-copyable contiguous ranges
  lower to `memmove` / `memset`.

> **HFT relevance:** the idiom's guarantee — **one linear pass, no per-element
> allocation, no repeated shifting** — is exactly the shape you want for pruning a
> working set each cycle (expired orders, filled levels). Removing N items from a
> `std::vector` in the naive loop is O(N²) and can blow a latency budget under
> load; `std::erase_if` is O(size). Because capacity is retained, the vector is
> immediately ready to refill without reallocating (file 02). For a book you
> often don't remove at all — you overwrite a slot and keep a count, or swap the
> victim with the last element and `pop_back()` (O(1), reorders — fine when order
> doesn't matter).

---

## Hands-on

```bash
./build.ps1 19-STL/examples/04_erase_remove.cpp
```

It shows: `std::remove` alone (size stays the same, tail is garbage), the
one-liner idiom, `remove_if`, `std::erase`/`std::erase_if` (C++20), `std::unique`
(consecutive only → sort first), and the O(n²) erase-in-loop for contrast.

---

## ⚠️ Traps

### Trap 1 — `std::remove` without `erase`
```cpp
std::remove(v.begin(), v.end(), 42);   // ⚠️ does nothing visible: size unchanged, 42s may still be readable in the tail
```

### Trap 2 — reading the tail after `remove`
```cpp
auto e = std::remove(v.begin(), v.end(), 0);
for (auto it = e; it != v.end(); ++it) use(*it);   // ⚠️ [e, end) is moved-from junk
```

### Trap 3 — `std::unique` expecting global dedup
```cpp
std::vector<int> v{1,2,1,2,1};
v.erase(std::unique(v.begin(), v.end()), v.end());   // ⚠️ nothing removed -- no ADJACENT dups. Sort first
```

### Trap 4 — `std::copy` into an empty container
```cpp
std::vector<int> out;
std::copy(v.begin(), v.end(), out.begin());   // ❌ out has no room -> UB. std::back_inserter(out)
```

### Trap 5 — erase-in-loop O(n²)
```cpp
for (auto it = v.begin(); it != v.end(); )
    if (bad(*it)) it = v.erase(it); else ++it;   // ⚠️ O(n²) for a vector. std::erase_if(v, bad) is O(n)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`std::remove` deletes elements" | It compacts and returns a new end — `size()` is unchanged. Pair with `erase` |
| "`v.erase(remove(...))`" | Must be `v.erase(remove(...), v.end())` — the 2-arg range form |
| "`std::unique` removes all duplicates" | Only consecutive runs — `sort` first for a full dedup |
| "erase-remove frees memory" | Lowers `size()`; capacity (the buffer) is retained |
| "erase-in-a-loop and the idiom cost the same" | Loop is O(n²); the idiom / `erase_if` is O(n) |

---

## Exercises

1. **Trace `remove`:** `std::vector<int> v{3,1,3,2,3,4};` then `auto e =
   std::remove(v.begin(), v.end(), 3);`. What is `[v.begin(), e)`? What is
   `v.size()`? What might `[e, v.end())` contain?

   <details><summary>Answer</summary>

   `[begin, e)` = `{1, 2, 4}`, `e - v.begin() == 3`. `v.size()` is still **6**.
   `[e, end)` = 3 unspecified (moved-from) `int`s — often leftovers like
   `{3,3,4}` but you must not rely on that.
   </details>

2. **Full dedup:** remove all duplicate ints from `std::vector<int> v`, order of
   first appearance not required.

   <details><summary>Answer</summary>

   `std::sort(v.begin(), v.end()); v.erase(std::unique(v.begin(), v.end()),
   v.end());` — O(n log n). (Order-preserving dedup needs a seen-set + `erase_if`
   or `copy_if`.)
   </details>

3. **erase_if on a map:** drop every entry from `std::map<int, Order>` whose value
   is filled (`o.remaining == 0`). One call?

   <details><summary>Answer</summary>

   `std::erase_if(book, [](const auto& kv){ return kv.second.remaining == 0; });`
   (C++20) — works on `map`/`set`/`unordered_*` too, and is O(n).
   </details>

4. **swap-pop:** remove the element at index `i` from a `std::vector<T> v` in O(1)
   when order doesn't matter. Caveat?

   <details><summary>Answer</summary>

   `std::swap(v[i], v.back()); v.pop_back();`. O(1). Caveat: it reorders (the last
   element takes slot `i`), and if `i == v.size()-1` it's just `pop_back`. Also
   invalidates `v.back()`'s and `v[i]`'s references.
   </details>

5. **Why O(n²):** explain precisely why `for (...) if (bad) it = v.erase(it);` is
   O(n²) for a vector but O(n) for a `std::list`.

   <details><summary>Answer</summary>

   `vector::erase(it)` shifts every element after `it` left by one → O(n) work,
   done up to O(n) times → O(n²). `list::erase(it)` unlinks one node → O(1), done
   O(n) times → O(n). (But even the list version does n node deallocations and
   walks cache-cold memory.)
   </details>

---

## Interview questions

1. `std::remove` container ka size kyun nahi badalta?
2. Erase-remove idiom — poori line likho aur padho.
3. `std::unique` kya karta — global dedup ke liye kya chahiye?
4. C++20 mein iska replacement kya (`std::erase`/`std::erase_if`)?
5. Erase-in-loop O(n²) kyun (vector), idiom O(n) kyun?
6. `std::copy` ko destination mein jagah kaise deni — `back_inserter` kyun?

---

## Next
→ [`11-algorithms-part3.md`](11-algorithms-part3.md)
