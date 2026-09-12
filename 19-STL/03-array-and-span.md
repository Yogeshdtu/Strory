# 03 — `std::array` and `std::span`

## Prerequisites
- [`02-vector-deep.md`](02-vector-deep.md)
- Folder 09 (C arrays), folder 12 (pointers, array decay), folder 13 (references)

## Yeh topic abhi kyun
`std::vector` heap use karta hai aur resizable hai. Bahut baar aapko **fixed
compile-time size** chahiye (no heap, no growth) — wahan `std::array`. Aur jab
aapko kisi bhi contiguous block (C array / `std::array` / `std::vector` ka chunk)
ko **bina copy kiye** ek function ko dena ho — wahan `std::span` (C++20). Dono
zero-overhead hain, dono HFT mein bahut common.

---

## `std::array<T, N>` — a C array with a proper interface

```cpp
#include <array>

std::array<int, 4> a{10, 20, 30, 40};   // size is part of the TYPE. No heap. Lives where you put it

a.size();        // 4  -- constexpr, always
a[2];            // 30 -- no bounds check (like vector)
a.at(2);         // 30 -- bounds-checked
a.front(); a.back();
a.data();        // int*  -- contiguous
a.fill(0);
for (int x : a) { ... }
std::sort(a.begin(), a.end());          // works with every algorithm
```

- **`sizeof(std::array<int,4>)` == 16** — exactly `N * sizeof(T)`, no pointer, no
  size field. It *is* the storage.
- No dynamic allocation — a member `std::array` lives inside its object, a local
  one on the stack.
- Copyable by value (copies all `N` elements — cheap for small `N`, a `memcpy`).
- Can be returned by value, put in another `std::array`, used at compile time
  (`constexpr std::array`).
- `std::to_array({1,2,3})` (C++20) deduces `std::array<int,3>`.

### vs C array

| | C array `int a[4]` | `std::array<int,4>` |
|---|---|---|
| `.size()` | ❌ (need `sizeof a / sizeof a[0]`) | ✅ `constexpr` |
| decays to pointer | ✅ silently (loses size) | ❌ — stays a value |
| copy / assign / return by value | ❌ | ✅ |
| `.begin()/.end()`, works with algorithms directly | via `std::begin` | ✅ natively |
| bounds-checked access | ❌ | `.at()` |

`std::array` is a **zero-cost** wrapper — same machine code, better interface.
No reason to use a raw C array in new C++ except for interop.

---

## `std::span<T>` (C++20) — a non-owning view of contiguous elements

```cpp
#include <span>

void process(std::span<const int> xs) {          // takes a VIEW -- no copy, no allocation
    for (int x : xs) { ... }
    xs.size(); xs[0]; xs.front(); xs.back();
    xs.data();
    auto mid = xs.subspan(2, 3);                  // elements [2, 5)  -- another view
    auto first2 = xs.first(2);
    auto last2  = xs.last(2);
}

int c_arr[5]          = {1,2,3,4,5};
std::array<int,5> arr = {1,2,3,4,5};
std::vector<int> vec  = {1,2,3,4,5};

process(c_arr);          // all three convert to span implicitly --
process(arr);            //   span points at their storage, remembers the length
process(vec);
process({vec.data() + 1, 3});   // an explicit {ptr, length} sub-range
```

- **`std::span` = pointer + length** (a "fat pointer"). `sizeof(std::span<int>)`
  == 16 (dynamic extent). Copying a span copies those 2 words — **not** the
  elements.
- **Owns nothing.** The underlying storage must outlive the span. A span into a
  `std::vector` **dangles** if the vector reallocates or is destroyed.
- `std::span<const int>` — read-only view. `std::span<int>` — you can modify the
  elements through it (but not add/remove).
- Fixed extent: `std::span<int, 5>` — size in the type, `sizeof` == 8 (just the
  pointer). Dynamic extent (default): size is a runtime member.

### The parameter-type upgrade

```cpp
// ❌ old ways to take "a bunch of contiguous ints":
void f(const std::vector<int>& v);       // forces the caller to have a vector
void f(const int* p, std::size_t n);     // two args, easy to mismatch, no .begin()

// ✅ C++20:
void f(std::span<const int> xs);         // accepts vector, array, C array, sub-range; one arg; range-for; .size()
```

`std::string_view` is to `std::string` what `std::span` is to `std::vector` —
a non-owning `{ptr, len}` view, for read-only string params (folder 10).

---

## Andar kya hota hai

- `std::array` — a `struct { T _elems[N]; }`. Member functions are `constexpr`
  and inline. `a[i]` → `_elems[i]`. Zero overhead vs a raw array; the "wrapper"
  is compiled away entirely.
- `std::span` (dynamic) — a `struct { T* ptr; size_t len; }`. `xs[i]` →
  `ptr[i]`. `xs.subspan(o, c)` → `{ptr + o, c}` — no allocation, just pointer
  arithmetic. At `-O2` a function taking `std::span` and a loop over it is the
  same code as a hand-written `ptr`/`len` loop.
- Passing `std::span` by value (16 bytes, in registers) is the idiom — not
  `const std::span&`.

> **HFT relevance:** `std::array` is everywhere in HFT — fixed-size price-level
> arrays, ring-buffer storage, per-symbol stat blocks — because it's stack/inline
> (no allocation, no indirection) and its size is a compile-time constant the
> optimizer exploits (loop unrolling, no bounds branches). `std::span` is the
> standard way to hand a slice of a pre-allocated buffer to a parser or a
> serializer without copying — market-data decoders take `std::span<const
> std::byte>`. The dangling risk is real: never return a `span` into a local, and
> never hold one across a `vector` `push_back`.

---

## Hands-on

```cpp
// span_demo.cpp
#include <array>
#include <span>
#include <cstdio>
#include <numeric>

int sum(std::span<const int> xs) {                    // one function...
    int s = 0; for (int x : xs) s += x; return s;
}

int main() {
    int c[4] = {1, 2, 3, 4};
    std::array<int, 4> a{10, 20, 30, 40};

    std::printf("sizeof array<int,4> = %zu\n", sizeof(a));        // 16
    std::printf("sizeof span<int>    = %zu\n", sizeof(std::span<int>{a}));  // 16

    std::printf("sum(c)            = %d\n", sum(c));              // 10   -- C array
    std::printf("sum(a)            = %d\n", sum(a));              // 100  -- std::array
    std::printf("sum(a.first(2))   = %d\n", sum(std::span<int>{a}.first(2)));  // 30
}
```
```bash
./build.ps1 19-STL/examples/01_vector_deep.cpp
```

---

## ⚠️ Traps

### Trap 1 — returning a `span` into a local
```cpp
std::span<int> f() { std::array<int,4> a{}; return a; }   // ⚠️ a dies -> dangling span
```

### Trap 2 — `span` held across a `vector` reallocation
```cpp
std::span<int> s = vec;  vec.push_back(1);  s[0];   // ⚠️ realloc -> s dangles (same rule as iterators)
```

### Trap 3 — `std::array` with wrong element count
```cpp
std::array<int, 3> a{1, 2, 3, 4};   // ❌ too many initializers -> compile error (good — C array wouldn't warn as loudly)
std::array<int, 3> b{1};            // OK -> {1, 0, 0}
```

### Trap 4 — expecting `std::array` to be resizable
```cpp
std::array<int,4> a;  a.push_back(5);   // ❌ no such member. Size is fixed in the type
```

### Trap 5 — `std::span<int>` from a `const` container
```cpp
const std::vector<int> v{1,2,3};
std::span<int> s = v;   // ❌ would allow writing through a const view. std::span<const int>
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`std::array` has overhead vs `int[N]`" | Zero — same layout, `sizeof == N*sizeof(T)`, wrapper compiled away |
| "`std::array` uses the heap" | Never — it *is* the storage; stack or inline in the parent object |
| "`std::span` copies the elements" | Copies `{ptr, len}` (16 bytes) — the elements are shared |
| "`std::span` keeps the data alive" | Owns nothing; underlying storage must outlive it |
| "Take `const std::vector<int>&` for read-only int params" | `std::span<const int>` — also accepts array, C array, sub-ranges |

---

## Exercises

1. **sizeof:** `std::array<double, 8>`, `std::span<double>` (dynamic extent),
   `std::span<double, 8>` (fixed extent) — give each `sizeof` on a 64-bit box.

   <details><summary>Answer</summary>

   `array<double,8>` = 64 (8 × 8, pure storage). `span<double>` = 16 (ptr + len).
   `span<double,8>` = 8 (ptr only — length is in the type).
   </details>

2. **One function, three callers:** write `double avg(std::span<const double>)` and
   call it with a `double[3]`, a `std::array<double,5>`, and the middle 4 elements
   of a `std::vector<double>` of size 10.

   <details><summary>Answer</summary>

   `double avg(std::span<const double> xs){ double s=0; for(double x:xs) s+=x;
   return xs.empty()?0:s/xs.size(); }` — call `avg(carr)`, `avg(arr)`,
   `avg(std::span<const double>{v}.subspan(3, 4))` (or `{v.data()+3, 4}`).
   </details>

3. **Dangling:** which of these return a valid span? (a) `span<int> f(std::array<int,4>& a){ return a; }` (b)
   `span<int> g(){ static int s[4]{}; return s; }` (c) `span<int> h(){ std::vector<int> v(4); return v; }`

   <details><summary>Answer</summary>

   (a) valid — the caller's array outlives the call. (b) valid — `static`
   storage lives forever. (c) dangling — `v` is destroyed at return.
   </details>

4. **array vs vector:** you need 32 counters, count known at compile time, reset
   every loop iteration, no allocation allowed. Which, and why?

   <details><summary>Answer</summary>

   `std::array<int, 32>` — compile-time size, no heap, lives on the stack /
   inline, `a.fill(0)` to reset. `std::vector` would allocate.
   </details>

5. **subspan:** given `std::span<const std::byte> packet`, extract a 4-byte
   header view and a payload view (rest).

   <details><summary>Answer</summary>

   `auto header = packet.first(4); auto payload = packet.subspan(4);` — both are
   views into the same buffer, no copy.
   </details>

---

## Interview questions

1. `std::array` vs C array — kya milta hai, overhead kitna?
2. `std::array` heap use karta? `sizeof` kya?
3. `std::span` kya hai — layout, kya own karta?
4. Read-only contiguous-int parameter ke liye best type kaunsa, kyun?
5. `std::span` kab dangle karta? (2 cases)
6. Fixed-extent vs dynamic-extent `std::span` — `sizeof` ka fark?

---

## Next
→ [`04-deque-list-forward-list.md`](04-deque-list-forward-list.md)
