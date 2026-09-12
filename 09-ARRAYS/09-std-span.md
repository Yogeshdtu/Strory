# 09 — `std::span<T>` (C++20)

## Prerequisites
- [`05-array-decay.md`](05-array-decay.md), [`07-arrays-as-parameters.md`](07-arrays-as-parameters.md), [`08-std-array.md`](08-std-array.md)
- `08-FUNCTIONS/06-scope-and-lifetime.md` (dangling)

## Yeh topic abhi kyun
`std::span` decay problem ka **saaf hal** hai: ek `{ T* ptr; size_t len; }` view
jo kisi bhi contiguous storage (C array, `std::array`, `std::vector`, part of one)
pe kaam karta hai — zero copy, zero overhead, `.size()` hamesha sahi. Yeh modern
"array parameter" hai.

---

## Kya hai

```cpp
#include <span>

std::span<int>       s1;    // mutable view of ints
std::span<const int> s2;    // read-only view

// span = { T* data; size_t size; }  -- 16 bytes (dynamic extent)
```

Non-owning: span **data ko own nahi karta** — bas "dekhta" hai. Copy karna sasta
(pointer + int), destroy karne pe underlying data ko kuch nahi hota.

---

## Banana

```cpp
int cArr[5] = {1, 2, 3, 4, 5};
std::array<int, 4> stdArr = {10, 20, 30, 40};
std::vector<int> vec = {100, 200, 300};

std::span<int> a = cArr;                 // from C array (size deduced)
std::span<int> b = stdArr;               // from std::array
std::span<int> c = vec;                  // from vector
std::span<int> d(vec.data(), 2);         // ptr + count -- first 2
std::span<int> e(vec.begin(), vec.end());// iterator pair
std::span<const int> f = cArr;           // read-only
```

---

## Function parameter — the point

```cpp
long long sum(std::span<const int> data) {
    long long s = 0;
    for (int x : data) s += x;
    return s;
}

sum(cArr);              // ✅
sum(stdArr);            // ✅
sum(vec);               // ✅
sum({vec.data(), 3});   // ✅ sub-range
```

**Ek function, sab contiguous containers.** No decay, no template, no `(ptr, len)`
pair.

```cpp
void doubleAll(std::span<int> data) { for (int& x : data) x *= 2; }   // mutating view
```

---

## API

```cpp
std::span<const int> s = big;

s.size()          s.size_bytes()      s.empty()
s[i]              s.front()  s.back()
s.data()          s.begin()  s.end()

s.first(n)        // pehle n
s.last(n)         // aakhri n
s.subspan(off)    s.subspan(off, count)   // slice -- ye bhi VIEWS hain (no copy)

// range-for, ranges algorithms sab kaam karte hain
```

```cpp
// as_bytes -- raw byte view (parsers ke liye)
std::span<const std::byte> raw = std::as_bytes(s);
```

---

## Static vs dynamic extent

```cpp
std::span<int>      dyn = vec;         // size runtime mein (16 bytes: ptr + len)
std::span<int, 4>   fix(stdArr);       // size TYPE mein baked (8 bytes: just ptr)
```

`std::span<T, N>` — compile-time size, ek pointer jitna. Mismatched size → compile
error. Use jab size genuinely fixed ho.

---

## 🔑 DANGLING — span data ko own nahi karta

```cpp
std::span<const int> bad;
{
    std::vector<int> tmp = {1, 2, 3};
    bad = tmp;                          // span points into tmp
}                                       // tmp destroyed
for (int x : bad) { }                   // ⚠️ UB -- bad dangling

// Aur:
std::span<int> f() {
    int local[3] = {1, 2, 3};
    return local;                       // ⚠️ returns view into dead local
}

// Reallocation:
std::vector<int> v = {1, 2, 3};
std::span<int> s = v;
v.push_back(4);                          // ⚠️ reallocation -> s dangling
```

**Rule: span apne source se ZYADA zinda nahi rehna chahiye.** Function
*parameters* mein yeh trivially safe (caller ka data call ke doraan zinda). Span
ko **store karna** (member, container) risky — tab ownership soch lo.

---

## `std::span` vs alternatives

| | `std::span<T>` | `const std::vector<T>&` | `const T*, size_t` | `const T (&)[N]` |
|---|---|---|---|---|
| C array | ✅ | ❌ | ✅ | ✅ |
| `std::array` | ✅ | ❌ | via `.data()` | ❌ |
| `std::vector` | ✅ | ✅ | via `.data()` | ❌ |
| sub-range | ✅ | ❌ | ✅ (manual) | ❌ |
| size safe | ✅ | ✅ | ⚠️ manual | ✅ |
| owns data | ❌ | ✅ | ❌ | ❌ |

---

## Andar kya hota hai

- `std::span<T>` (dynamic) = 2 words: `T* _ptr; size_t _size;`. Passed in 2
  registers. Zero overhead vs raw pointer + separate int.
- `s[i]` → `_ptr[i]` — same scaled load as an array.
- `s.subspan(a, b)` → `{ _ptr + a, b }` — pure pointer arithmetic, no allocation.
- `std::span<T, N>` (static) = 1 word (`_ptr`), size is a compile-time constant.
- `-O2` inlines span methods → the span "disappears", leaving a plain pointer
  loop that vectorizes.

> **HFT relevance:** `std::span<const std::byte>` is the canonical wire-decoder
> parameter — buffer + length, every field access a `.size()` check away from
> safe. `std::span<Level>` over a `std::array` price-level pool. Sub-spanning a
> receive buffer to hand messages to handlers — zero copy. The dangling rule
> matters: a span stored past its buffer's life is a classic zero-copy parser
> bug (folder 10, 38). Debug builds: `_GLIBCXX_ASSERTIONS` bounds-checks
> `span::operator[]`.

---

## Hands-on

`examples/05_span_demo.cpp` — one function all containers, modify, subspan,
dangling:

```bash
./build.ps1 09-ARRAYS/examples/05_span_demo.cpp
```

---

## ⚠️ Traps

### Trap 1 — storing a span past its source
```cpp
struct Parser { std::span<const std::byte> buf; };
Parser p{ getTempBuffer() };   // ⚠️ temp gone, p.buf dangling
```

### Trap 2 — span + vector that reallocates
```cpp
std::span<int> s = v;  v.push_back(x);   // ⚠️ s may dangle
```

### Trap 3 — `std::span<int>` from a `const` container
```cpp
const std::vector<int> v = {1,2,3};
std::span<int> s = v;      // ❌ can't get mutable view. std::span<const int>
```

### Trap 4 — `subspan` out of range
```cpp
s.subspan(10);            // ⚠️ UB if 10 > s.size(). Check first
```

### Trap 5 — passing `std::span` by `const&` (pointless)
```cpp
void f(const std::span<int>& s);   // ⚠️ span is already cheap -- pass by value
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`std::span` owns / copies data" | Non-owning view — 16 bytes |
| "`std::span` is safe against dangling" | Only if source outlives it (params: yes; stored: careful) |
| "Pass `std::span` by `const&`" | By value — it's already tiny |
| "`std::span<int>` from a `const` vector" | `std::span<const int>` |
| "`subspan` copies the slice" | It's another view — pure pointer math |

---

## Exercises

1. **One function:** `long long product(std::span<const int>)` — C array,
   `std::array`, `std::vector`, and a `subspan` — all four.

2. **Modify:** `void clampSpan(std::span<int> s, int lo, int hi)` — clamp each
   element. Test on all three container types.

3. **Sub-views:** `std::vector<int> v(20)` filled `0..19`. Print `first(5)`,
   `last(5)`, `subspan(5, 10)`. Confirm no data copied (`.data()` pointers).

4. **Dangling:** write `std::span<int> makeSpan()` returning a view of a local
   array. `-Wall` warning? Run (UB).

5. **Byte view:** `std::span<const std::byte> bytes = std::as_bytes(std::span(v));`
   — print each byte hex. `bytes.size()` vs `v.size()`?

6. **Static extent:** `std::span<int, 4>` from a `std::array<int, 4>`. Try from a
   `std::array<int, 5>` — compile error? `sizeof` of `span<int,4>` vs `span<int>`?

---

## Interview questions

1. `std::span` kya hai (contents, size, ownership)?
2. `std::span` decay problem ko kaise solve karta hai?
3. Static vs dynamic extent — fark, size?
4. `std::span` kab dangle karta hai? Parameter vs stored?
5. `std::span<const int>` vs `const std::vector<int>&` parameter — kab kaunsa?
6. `std::span` ko `const&` se pass karna kyun pointless?

---

## Next
→ [`10-array-performance.md`](10-array-performance.md)
