# 02 — `std::vector` deep dive

## Prerequisites
- [`01-stl-architecture.md`](01-stl-architecture.md)
- Folder 09 (arrays), folder 14 file 08 (allocation cost), folder 18 file 13 (`noexcept` move)

## Yeh topic abhi kyun
`std::vector` **default container** hai — 95% cases mein yahi use karo. Contiguous
memory (cache-friendly), O(1) amortized `push_back`, O(1) indexing, works with
every algorithm. Par uske growth model, `reserve`, `capacity`, aur **iterator/
reference invalidation** — inhe theek se samajhna zaroori (ye bugs aur perf
issues ka #1 source hain).

---

## Layout

```
   std::vector<int> v;

   v:  [ data_ptr | size | capacity ]      (3 pointers/sizes -- sizeof(vector) == 24 on 64-bit)
          │
          ▼
       [ elem0 | elem1 | ... | elem(size-1) | ...unused... ]   (one contiguous heap buffer)
       └──────────── capacity elements worth of space ─────────┘
```

- **`data()`** — pointer to the first element (contiguous, so `&v[0]`, `&v[1]`
  are adjacent).
- **`size()`** — number of constructed elements.
- **`capacity()`** — allocated slots (`>= size()`). `capacity - size` = free
  slots before the next reallocation.
- `sizeof(std::vector<T>)` is **24** regardless of `T` or element count (3 words).

---

## Growth — geometric (usually 2x)

```cpp
std::vector<int> v;
// libstdc++: capacity goes 0 -> 1 -> 2 -> 4 -> 8 -> 16 -> 32 ...  (doubling)
// libc++ / MSVC: often 1.5x
```

When `push_back` would exceed `capacity`:
1. Allocate a new buffer of `~2 * capacity` (at least `size + 1`).
2. **Move** (if element move is `noexcept` — folder 18 file 13) or **copy** each
   existing element to the new buffer.
3. Destroy the old elements, free the old buffer.
4. Append the new element.

Geometric growth → `push_back` is **O(1) amortized**: N pushes cause ~log₂(N)
reallocations, moving a total of ~2N elements → O(N) total → O(1) each.

`examples/01_vector_deep.cpp` prints the capacity jumps and measures reserved vs
unreserved fill.

---

## `reserve` — the single most important vector optimization

```cpp
std::vector<Order> book;
book.reserve(expectedMaxOrders);        // ONE allocation up front
for (const auto& o : incoming) book.push_back(o);   // zero reallocations, zero element moves
```

- `reserve(n)` — ensure `capacity >= n`. If `n <= capacity`, no-op. Otherwise
  **one** reallocation now.
- **Does not change `size()`** — the vector is still empty/whatever it was.
- After `reserve(n)`, `push_back` up to `n` elements causes **no reallocation** →
  no element moves, no iterator invalidation during that phase.

**If you know (or can bound) the final size, `reserve` it.** Measured
(`examples/01`): reserved fill vs unreserved for 2M ints — the unreserved version
does ~20 reallocations moving a total of ~4M ints.

`resize(n)` is different: it **changes `size()`** — adds `n - size` value-
initialized (or specified) elements, or truncates.

---

## Iterator / pointer / reference invalidation

**Any operation that reallocates invalidates ALL iterators, pointers, and
references into the vector.**

```cpp
std::vector<int> v{10, 20, 30};
int& first = v[0];
int* p     = &v[1];
auto it    = v.begin();

v.push_back(99);          // MIGHT reallocate -> first, p, it all potentially DANGLING now
first = 5;                // ⚠️ if reallocated -> use-after-free (folder 13/14)
```

| Operation | Invalidates |
|---|---|
| `push_back`, `emplace_back`, `insert`, `resize`, `reserve` **that reallocates** | **everything** |
| `insert` / `erase` in the middle (no realloc) | iterators/refs **at and after** the point |
| `pop_back` | the iterator/ref to the removed element only |
| `clear` | all (but capacity unchanged) |
| indexing, `at`, `front`, `back`, iteration — read-only | nothing |

**Rules:**
- Don't hold a pointer/reference/iterator into a vector across a `push_back`
  (unless you `reserve`d and stay within capacity).
- In a loop that may `push_back` to the same vector: use **indices**, not
  iterators, and re-check `size()`.
- After `reserve(final)`, references are stable until you exceed capacity.

---

## `capacity` management

```cpp
v.clear();                    // size -> 0, capacity UNCHANGED (memory retained for reuse)
v.shrink_to_fit();            // non-binding request to release excess capacity (usually honored)
std::vector<T>().swap(v);     // GUARANTEED release: swap with an empty temporary -> v now empty + capacity 0
std::vector<T>{}.swap(v);     // same

// keep capacity for reuse (common in hot loops):
v.clear();                    // ready to refill without reallocating
```

`clear()` keeping capacity is a **feature** — reuse a vector as a scratch buffer
across iterations without reallocating each time (`v.clear()` then refill).
`shrink_to_fit` / swap-with-empty only when you genuinely want the memory back.

---

## `push_back` vs `emplace_back`

```cpp
v.push_back(Widget(1, 2));       // construct temporary, then MOVE into the vector
v.emplace_back(1, 2);            // construct IN PLACE from the args -- one fewer move
v.emplace_back(existingWidget);  // == push_back(existingWidget) -- a copy
```

`emplace_back(args...)` forwards `args` to `T`'s constructor, building the element
directly in the vector's storage — saves a move vs `push_back(T(args...))`. For a
trivial/small `T` the difference is nil; for `T` with an expensive move it
matters. `push_back(std::move(x))` for a named object.

---

## `operator[]` vs `at()`

```cpp
v[i]        // no bounds check -- UB if i >= size(). Fast (a load). Use when i is known-valid
v.at(i)     // bounds-checked -- throws std::out_of_range. Slower (a compare + branch)
v.front()   // v[0]         -- UB if empty
v.back()    // v[size()-1]  -- UB if empty
v.data()    // T* to the buffer -- for C-API interop, or v.data() + i
```

Hot code: `v[i]` after validating `i` once. `at()` at boundaries / with untrusted
indices. (GCC's `_GLIBCXX_ASSERTIONS` makes `v[i]` OOB abort — on GCC 16.2 it is on by default
**only at `-O0`**; at `-O2` add `-D_GLIBCXX_ASSERTIONS` yourself — folder 09 file 11.)

---

## `std::vector<bool>` — the gotcha

`std::vector<bool>` is a **specialization** that packs bits (1 bit per element)
→ it's **not a real container of `bool`**:
- `v[i]` returns a **proxy object**, not `bool&`. `bool* p = &v[i];` doesn't
  compile.
- Can't use it with algorithms expecting `T&`.
- Not contiguous in the `bool` sense.

For a resizable bitset use `std::vector<char>` / `std::vector<std::uint8_t>` (a
byte each, but a real container), or `std::bitset<N>` (fixed size), or
`boost::dynamic_bitset`.

---

## Andar kya hota hai

- `push_back` fast path (capacity available): construct one element at `data_ +
  size_`, `++size_`. ~a few instructions, inlined.
- Reallocation: `operator new` for `capacity * 2 * sizeof(T)`, a loop of
  `std::move_if_noexcept` per element, destroy old, `operator delete`. The
  allocation + the O(n) move is the cost `reserve` avoids.
- `v[i]` → `*(data_ + i)` — one scaled load. `at(i)` → `if (i >= size_) throw;`
  then the same load.
- Contiguity → the CPU prefetcher streams through a vector; `std::accumulate`
  over a `std::vector<int>` auto-vectorizes (SIMD). This is why vector beats
  `list`/`deque` for iteration by 10-30x (`examples/11`).
- `sizeof(vector<T>)` is 24 — three words — so passing `const vector<T>&` (8
  bytes) vs by value (24 + a deep copy) matters.

> **HFT relevance:** `std::vector` is the workhorse — pre-`reserve`d to the max
> expected size at startup, then `push_back`/`clear`/refill in the hot loop with
> **zero reallocations** (so references stay valid and there's no allocation).
> `data()` + a length is handed to parsers and syscalls. The invalidation rules
> are a common bug source: caching `&book[i]` across an `insert` → dangling. Hot
> code either pre-sizes and uses indices, or uses `std::span` views into a stable
> buffer. `std::vector<bool>` is banned (proxy surprises). For order books,
> sorted `std::vector<PriceLevel>` + `lower_bound` often beats `std::map` (file
> 25, folder 39).

---

## Hands-on

```bash
./build.ps1 19-STL/examples/01_vector_deep.cpp
./build.ps1 fast 19-STL/examples/01_vector_deep.cpp
```

Capacity doubling, reserve vs no-reserve reallocation count, the invalidation
demo (`&v[0]` changes after growth), `clear` keeping capacity, swap-with-empty
guaranteed release.

---

## ⚠️ Traps

### Trap 1 — pointer/reference held across `push_back`
```cpp
int& x = v[0];  v.push_back(1);  x = 5;   // ⚠️ realloc -> x dangling. reserve, or use v[0] again
```

### Trap 2 — iterator loop that `push_back`s to the same vector
```cpp
for (auto it = v.begin(); it != v.end(); ++it) if (cond(*it)) v.push_back(...);   // ⚠️ realloc -> it invalid. Index-based
```

### Trap 3 — `reserve` then expecting `size()` to change
```cpp
v.reserve(10);  v[5] = 1;   // ⚠️ size is still 0 -> v[5] is UB. resize(10), or push_back
```

### Trap 4 — `std::vector<bool>` as a real container
```cpp
std::vector<bool> flags(8);  bool* p = &flags[0];   // ❌ proxy, not bool&. Use vector<char>
```

### Trap 5 — forgetting `emplace_back`/`push_back(std::move)` for expensive elements
```cpp
v.push_back(BigThing(args));   // ⚠️ builds a temp then moves. v.emplace_back(args) -> in place
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`reserve(n)` sets the size to n" | Only capacity; size unchanged. `resize` changes size |
| "`clear()` frees the memory" | Size → 0, capacity retained (feature — reuse). swap-with-empty to free |
| "`push_back` never invalidates iterators" | It does if it reallocates — everything is invalidated |
| "`std::vector<bool>` is `vector` of `bool`" | Bit-packed proxy specialization — not a normal container |
| "`v[i]` and `v.at(i)` perform the same" | `at` adds a bounds check + branch |

---

## Exercises

1. **Reallocation count:** for `std::vector<int> v;` and 1000 `push_back`s (2x
   growth), how many reallocations? Total element moves? With `v.reserve(1000)`
   first?

   <details><summary>Answer</summary>

   ~10 reallocations (1,2,4,...,1024). Total moves ≈ 1+2+4+...+512 ≈ 1023 ≈ N.
   With `reserve(1000)`: **1** allocation, **0** moves.
   </details>

2. **Invalidation:** `std::vector<int> v{1,2,3}; auto* p = &v[1]; v.reserve(100);
   v.push_back(4); *p;` — is `*p` valid? What if the `reserve(100)` line is
   removed?

   <details><summary>Answer</summary>

   With `reserve(100)` first: `push_back(4)` doesn't reallocate (capacity 100) →
   `p` still valid. Without it: `push_back` on a size-3 capacity-3 vector
   reallocates → `p` dangles → `*p` is use-after-free.
   </details>

3. **Scratch reuse:** rewrite a hot loop that does `std::vector<int> tmp; ...
   use tmp ...;` **per iteration** so it reuses one vector without reallocating
   each time.

   <details><summary>Answer</summary>

   Hoist the vector out: `std::vector<int> tmp; for (...) { tmp.clear(); /* refill
   tmp */; use(tmp); }`. `clear()` keeps the capacity → after the first few
   iterations, no reallocation.
   </details>

4. **emplace savings:** `std::vector<std::string> v; v.push_back(std::string(100,
   'x'));` vs `v.emplace_back(100, 'x');` — how many `std::string` constructions/
   moves each?

   <details><summary>Answer</summary>

   `push_back(std::string(100,'x'))` — 1 construction of the temporary + 1 move
   into the vector. `emplace_back(100,'x')` — 1 construction, in place, no move.
   </details>

5. **sizeof:** `sizeof(std::vector<int>)` and `sizeof(std::vector<std::string>)`
   — same? Why? What's the cost of passing one by value vs `const&`?

   <details><summary>Answer</summary>

   Both 24 (3 words: data ptr, size, capacity) regardless of `T`. By value → copy
   the 3 words **and** deep-copy every element (alloc + memcpy). `const&` → pass
   8 bytes, no copy. Always `const vector<T>&` for read-only params.
   </details>

---

## Interview questions

1. `std::vector` ka layout, `sizeof`, growth model?
2. `push_back` O(1) amortized kaise (geometric growth)?
3. `reserve` vs `resize` — kya fark, `reserve` kab?
4. Reallocation kya invalidate karta? Middle `insert`/`erase`?
5. `clear()` capacity ka kya karta — feature kyun?
6. `std::vector<bool>` ka gotcha?

---

## Next
→ [`03-array-and-span.md`](03-array-and-span.md)
