# 07 — Arrays as function parameters

## Prerequisites
- [`05-array-decay.md`](05-array-decay.md)
- `08-FUNCTIONS/03-parameters-and-arguments.md`

## Yeh topic abhi kyun
File 05 mein dekha ki array function ko pass karte hi pointer ban jaata hai
(decay) — size lost. Yeh lesson **practical decision** deta hai: "mujhe ek
function ko array dena hai — kaunsa parameter type?"

---

## Options — cheat sheet

| Parameter type | Kaam karta hai | Size milta hai | Kab |
|---|---|---|---|
| `int* arr, std::size_t n` | any contiguous | ✅ (aap pass karo) | C API boundary |
| `std::span<int>` (C++20) | C array, `std::array`, `std::vector` | ✅ auto | **modern default** |
| `std::span<const int>` | same, read-only | ✅ auto | read-only default |
| `const int (&arr)[N]` (template) | compile-time-sized C array only | ✅ auto (`N`) | fixed-size, no `std::array` |
| `const std::array<int, N>&` | that exact `std::array` only | ✅ (in type) | when caller uses `std::array` |
| `int arr[]` / `int arr[10]` | **= `int*`** — legacy | ❌ | avoid (misleading) |

**Default: `std::span<const T>` for read, `std::span<T>` for modify.**

---

## 1. `std::span` — the modern answer

```cpp
#include <span>

long long sum(std::span<const int> data) {
    long long s = 0;
    for (int x : data) s += x;
    return s;
}

int cArr[] = {1, 2, 3};
std::array<int, 4> stdArr = {10, 20, 30, 40};
std::vector<int> vec = {100, 200, 300};

sum(cArr);      // works
sum(stdArr);    // works
sum(vec);       // works
```

- 16 bytes (`ptr` + `len`), passed in registers — zero overhead
- `.size()`, `.subspan()`, `.first()`, `.last()`, range-for, `[]`, `.at()` (C++26)
- `std::span<T>` → mutable view; `std::span<const T>` → read-only
- ⚠️ Non-owning — caller ka storage function call ke doraan zinda rahe (usually
  trivially true for a parameter)

## 2. Pointer + size (C style)

```cpp
long long sum(const int* arr, std::size_t n) {
    long long s = 0;
    for (std::size_t i = 0; i < n; ++i) s += arr[i];
    return s;
}
sum(data, std::size(data));
```

Fine for C interop. Risk: caller `n` galat pass kar sakta hai (span mein yeh
automatic hai).

## 3. Reference-to-array (template)

```cpp
template <std::size_t N>
long long sum(const int (&arr)[N]) {          // N deduced -- no decay
    long long s = 0;
    for (int x : arr) s += x;
    return s;
}
sum(data);   // no size needed
```

- No decay → `N` known, range-for works, `sizeof` correct
- ⚠️ Only compile-time-sized C arrays. `std::array`, `std::vector`, decayed
  pointer → won't match. Har `N` ke liye alag instantiation (code bloat if many
  sizes).

## 4. Modifying — `std::span<T>` (non-const)

```cpp
void doubleAll(std::span<int> data) {
    for (int& x : data) x *= 2;
}
doubleAll(vec);
```

---

## Output arrays — fill a caller's buffer

```cpp
// Caller ka buffer + capacity; kitne likhe woh return
std::size_t formatHex(std::uint32_t value, std::span<char> out) {
    // out.size() se bounds check kar sakte ho
    ...
    return written;
}

char buf[16];
std::size_t n = formatHex(0xDEADBEEF, buf);
```

`std::span<char>` = buffer + capacity ek unit mein → overrun check trivial.

---

## Multi-dimensional (file 06)

```cpp
void f(int (*m)[COLS], std::size_t rows);      // COLS compile-time
void f(std::span<int> flat, std::size_t rows, std::size_t cols);   // flat + dims
void f(std::mdspan<int, std::dextents<std::size_t, 2>> m);          // C++23 -- cleanest
```

---

## Andar kya hota hai

- `std::span` parameter → 2 register values (ptr, len). Function inside: pointer
  deref + bounds from len. `-O2` inlines → same codegen as raw pointer loop.
- Reference-to-array template → each `N` a separate function; `N` is a compile-
  time constant so loop can fully unroll.
- Pointer + size → identical to span minus the safety.

> **HFT relevance:** `std::span<const T>` / `std::span<T>` is the standard HFT
> array-parameter type — zero cost, size always correct, works with stack
> arrays, `std::array` pools, and `std::vector`. Wire decoders take
> `std::span<const std::byte>` (buffer + length) so an OOB read is a `.size()`
> check away, not a silent overrun. C-style `(ptr, len)` only at OS/library
> boundaries.

---

## Hands-on

`examples/02_array_decay.cpp` (size param + reference-to-array),
`examples/05_span_demo.cpp` (one function, all containers):

```bash
./build.ps1 09-ARRAYS/examples/02_array_decay.cpp
./build.ps1 09-ARRAYS/examples/05_span_demo.cpp
```

---

## ⚠️ Traps

### Trap 1 — `void f(int a[])` and expecting size
```cpp
void f(int a[]) { /* size? nahi hai */ }
```

### Trap 2 — `std::span` outliving its data
```cpp
std::span<int> s;
{ std::vector<int> v = {1,2,3}; s = v; }   // ⚠️ v gone -> s dangling
```

### Trap 3 — reference-to-array with `std::array`
```cpp
template <std::size_t N> void f(int (&a)[N]);
std::array<int, 5> a;  f(a);    // ❌ no match -- std::array is not int[N]. Use f(a.data() ...) or span
```

### Trap 4 — passing `std::vector` by value "to be safe"
```cpp
void f(std::vector<int> v);     // ⚠️ full copy. std::span<const int> or const&
```

### Trap 5 — output buffer without capacity
```cpp
void format(std::uint32_t v, char* out);   // ⚠️ overrun risk. std::span<char> out
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`int a[]` param array leta hai" | Pointer — size lost |
| "`std::span` copy hota hai (16 bytes)" | 2 registers — zero overhead |
| "Reference-to-array `std::array` pe chalega" | Nahi — only C arrays. `std::span` for both |
| "Output buffer ke saath capacity optional" | `std::span<char>` — overrun check |
| "`std::vector` by value safe hai" | Full copy — `const&` / `span` |

---

## Exercises

1. **4 sum signatures:** `sum(const int*, size_t)`, `sum(span<const int>)`,
   `template<size_t N> sum(const int(&)[N])`, `sum(const array<int,10>&)` — sab
   likho. Kaunsa `std::vector` pe chalega? Kaunsa C array pe? `std::array` pe?

2. **`std::span` modify:** `void addOne(std::span<int>)` — C array, `std::array`,
   `std::vector` teenon pe. Original badla?

3. **Output buffer:** `std::size_t toBinary(std::uint8_t v, std::span<char> out)`
   — `v` ka binary string `out` mein, bits count return. `out` chhota ho to?

4. **Dangling span:** ek function jo `std::span<int>` return kare local array ka.
   `-Wall` warning? Chalao (UB).

5. **2D param:** `int (*m)[4]` param wala `rowSum` likho, `int grid[3][4]` pe use.
   Phir flat `std::span<int>` + `cols` param wala.

6. **Bench:** `sum(span<const int>)` vs `sum(const int*, size_t)` — 10M ints,
   `-O2`. Farq? (Nahi — same codegen.)

---

## Interview questions

1. Function ko array pass karne ke saare tareeke — trade-offs?
2. `std::span` kya hai (size, cost)? C-style `(ptr, len)` se kya behtar?
3. Reference-to-array parameter — faayda aur limitation?
4. `std::span<T>` vs `std::span<const T>` — kab kaunsa?
5. Output buffer parameter — safe design?
6. `std::span` kis situation mein dangling ho sakta hai?

---

## Next
→ [`08-std-array.md`](08-std-array.md)
