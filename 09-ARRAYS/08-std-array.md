# 08 — `std::array<T, N>`

## Prerequisites
- [`05-array-decay.md`](05-array-decay.md), [`07-arrays-as-parameters.md`](07-arrays-as-parameters.md)
- `08-FUNCTIONS/04-return-values.md` (value semantics, RVO)

## Yeh topic abhi kyun
`std::array` C array ke saare fixed-size + stack + zero-overhead fayde deta hai,
bina decay ki dikkat ke. Modern C++ mein **fixed-size array ka default choice**.

---

## C array vs `std::array`

```cpp
int             c[5] = {1, 2, 3, 4, 5};
std::array<int, 5> a = {1, 2, 3, 4, 5};   // #include <array>
```

| | C array `int[5]` | `std::array<int, 5>` |
|---|---|---|
| Memory layout | 20 bytes contiguous | **identical** (zero overhead) |
| `.size()` | ❌ (`std::size` before decay) | ✅ always |
| Decay to pointer | ✅ (loses size) | ❌ (`.data()` explicit) |
| Copy (`b = a`) | ❌ | ✅ (element-wise) |
| Compare (`a == b`) | ❌ (compares addresses) | ✅ (element-wise) |
| Return from function | ❌ (decays) | ✅ (RVO, free) |
| `.at()` bounds-check | ❌ | ✅ (throws) |
| STL algorithms | via `std::begin/end` | `.begin()/.end()` |
| Pass to function | decays | **copies** by value (use `const&` / `span`) |

**`sizeof(std::array<int,5>) == sizeof(int[5]) == 20`.** No hidden overhead.

---

## API

```cpp
std::array<int, 5> a = {10, 20, 30, 40, 50};

a.size()            // 5
a.empty()           // false
a[2]                // 30  (unchecked)
a.at(2)             // 30  (checked -- throws std::out_of_range)
a.front()           // 10
a.back()            // 50
a.data()            // int*  (explicit decay -- for C APIs)
a.fill(7)           // sab 7
a.begin(), a.end()  // iterators
a.rbegin(), a.rend()// reverse

std::array<int, 5> b = a;              // copy
b == a                                 // element-wise compare
std::swap(a, b);                       // O(N) swap

auto [p, q, r, s, t] = a;              // structured binding
```

---

## No decay — but by-value COPIES

```cpp
void f(std::array<int, 5> a);          // ⚠️ 20-byte copy per call
void f(const std::array<int, 5>& a);   // ✅ no copy, but locked to size 5
void f(std::span<const int> a);        // ✅ no copy, any size (file 09)
```

Rule: read-only param → `std::span<const T>` (flexible) or `const std::array<T,
N>&` (when size is part of the contract).

---

## CTAD (C++17)

```cpp
std::array a = {1, 2, 3};              // -> std::array<int, 3>
std::array b = {1.0, 2.0};            // -> std::array<double, 2>
std::to_array({1, 2, 3});             // C++20 -- from a braced list or C array
```

---

## Common uses

```cpp
// Fixed lookup table (constexpr -- folder 08 lesson 11)
constexpr std::array<int, 7> daysInMonth = {31, 28, 31, 30, 31, 30, 31};

// Return multiple related values
std::array<double, 3> rgbToHsv(double r, double g, double b);

// Fixed-size buffer -- no heap
std::array<char, 64> lineBuffer;

// Small stack of known max depth
std::array<Node*, 32> dfsStack;
std::size_t top = 0;
```

---

## `std::array` vs `std::vector`

| | `std::array<T, N>` | `std::vector<T>` |
|---|---|---|
| Size | compile-time, fixed | runtime, growable |
| Storage | inline (stack/wherever the array is) | heap |
| Allocation | **none** | `new` on construct / grow |
| `push_back` | ❌ | ✅ |
| Cost of copy | fixed memcpy | heap alloc + copy |

**Max size compile-time pe pata hai → `std::array`.** Warna `std::vector` (with
`reserve()` if you know an upper bound).

---

## Andar kya hota hai

- `std::array` = `struct { T _elems[N]; }` — literally a C array in a struct.
  Aggregate type, trivially copyable (for trivial `T`).
- `[]` → same scaled load as C array. `.at()` → adds `cmp`/`jae`.
- `.size()` → returns the compile-time `N` — folds to a constant, often
  disappears.
- Pass by value → `memcpy` of `N*sizeof(T)`. `-O2` may elide for small `N`.
- `constexpr std::array` → data baked into `.rodata` (folder 08 lesson 11).

> **HFT relevance:** `std::array` is everywhere in HFT: fixed message buffers,
> price-level arrays (`std::array<Level, kDepth>`), small object pools,
> ring-buffer storage, `constexpr` protocol tables. **Zero heap, cache-resident,
> size-safe.** Pass as `std::span` to avoid the by-value copy. When the size is
> genuinely runtime but bounded, a `std::array` + a `size_` counter (a "static
> vector" / `boost::static_vector` / `std::inplace_vector` C++26) beats
> `std::vector` for latency (folder 36).

---

## Hands-on

`examples/04_std_array.cpp` — API, `.at()` throw, value semantics, no-decay,
STL algos, zero overhead:

```bash
./build.ps1 09-ARRAYS/examples/04_std_array.cpp
```

---

## ⚠️ Traps

### Trap 1 — pass by value
```cpp
void process(std::array<Big, 100> a);   // ⚠️ huge copy. const& or span
```

### Trap 2 — `.data()` outliving the array
```cpp
int* p;
{ std::array<int, 3> a = {1,2,3}; p = a.data(); }   // ⚠️ dangling
```

### Trap 3 — `std::array<int>` (missing N)
```cpp
std::array<int> a;      // ❌ N is required
std::array<int, 5> a;   // ✅
```

### Trap 4 — uninitialized `std::array` (same as C array for trivial T)
```cpp
std::array<int, 5> a;   // ⚠️ elements uninitialized (garbage) if local
std::array<int, 5> a{}; // ✅ all zero
```

### Trap 5 — `a == b` with different `N`
```cpp
std::array<int, 3> a;  std::array<int, 4> b;
a == b;                 // ❌ compile error (different types) -- not "false"
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`std::array` has overhead vs C array" | Identical layout & speed |
| "`std::array` doesn't decay, so pass by value is fine" | Copies — use `const&` / `span` |
| "`std::array<int, 5> a;` zeroes elements" | Garbage (local, trivial T) — `a{}` |
| "`std::array` can grow" | Fixed — `std::vector` for growable |
| "`std::array` lives on the heap" | Wherever the object lives (usually stack) |

---

## Exercises

1. **API tour:** `std::array<int, 6> a = {5,2,8,1,9,3}` — `size`, `front`, `back`,
   `at(2)`, `at(10)` (catch), `fill(0)`, sort, `max_element`.

2. **Value semantics:** `auto b = a; b[0] = 99;` — `a[0]` badla? `a == b`?

3. **Copy cost:** `void f(std::array<int, 1000> a)` vs `const&` — 100000 calls,
   `-O2`, time.

4. **`constexpr` table:** `constexpr std::array<int, 13> fib()` — fib(0..12).
   `static_assert(fib()[10] == 55);` `-S` se dekho data rodata mein.

5. **Static vector:** `template<class T, std::size_t Cap> struct SmallVec {
   std::array<T, Cap> buf; std::size_t n = 0; void push(T); ... };` — implement
   `push`, `size`, `operator[]`, range-for support.

6. **`std::array` vs `std::vector`:** ek fixed 8-slot cache — dono se implement,
   `-O2` pe insert/lookup time. Allocation count?

---

## Interview questions

1. `std::array` vs C array — 5 differences? Overhead?
2. `std::array` decay hota hai? `.data()` ka role?
3. `std::array` pass by value — kya hota hai? Better options?
4. `std::array` vs `std::vector` — decision?
5. `std::array<int, 5> a;` (local, no braces) — elements ki value?
6. `constexpr std::array` ka faayda?

---

## Next
→ [`09-std-span.md`](09-std-span.md)
