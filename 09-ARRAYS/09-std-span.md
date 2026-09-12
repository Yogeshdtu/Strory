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

std::span<int>       s1;    // ints ka view jisse badal bhi sakte ho
std::span<const int> s2;    // sirf padhne wala view

// span = { T* data; size_t size; }  -- 16 bytes (dynamic extent)
```

Non-owning: span **data ka maalik nahi** — bas use "dekhta" hai. Analogy: span ek **khidki**
hai jisse aap kisi aur ke ghar ka kamra dekh rahe ho. Khidki copy karna sasta hai (pointer +
ek number), aur khidki tod doge to kamre ko kuch nahi hota. Par ghar gir gaya, to khidki se
khali jagah dikhegi (dangling).

---

## Banana

```cpp
int cArr[5] = {1, 2, 3, 4, 5};
std::array<int, 4> stdArr = {10, 20, 30, 40};
std::vector<int> vec = {100, 200, 300};

std::span<int> a = cArr;                 // C array se (size apne aap)
std::span<int> b = stdArr;               // std::array se
std::span<int> c = vec;                  // vector se
std::span<int> d(vec.data(), 2);         // ptr + count -- pehle 2
std::span<int> e(vec.begin(), vec.end());// iterator pair
std::span<const int> f = cArr;           // sirf padhne ke liye
```

---

## Function parameter — asli kaam

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

**Ek function, saare contiguous containers.** Na decay, na template, na alag `(ptr, len)` jodi.

```cpp
void doubleAll(std::span<int> data) { for (int& x : data) x *= 2; }   // badalne wala view
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
s.subspan(off)    s.subspan(off, count)   // slice -- ye bhi VIEWS hain (copy nahi)

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
std::span<int, 4>   fix(stdArr);       // size TYPE mein baked (8 bytes: sirf ptr)
```

`std::span<T, N>` — size compile time pe fixed, sirf ek pointer jitna bada (GCC 16.2 pe check
kiya: `sizeof(std::span<int>)` = 16, `sizeof(std::span<int, 4>)` = 8). Size match na kare →
compile error. Tab use karo jab size sach mein fixed ho.

---

## 🔑 DANGLING — span data ka maalik nahi

```cpp
std::span<const int> bad;
{
    std::vector<int> tmp = {1, 2, 3};
    bad = tmp;                          // span tmp ke andar dekh raha hai
}                                       // tmp khatam
for (int x : bad) { }                   // ⚠️ UB -- bad dangling

// Aur:
std::span<int> f() {
    int local[3] = {1, 2, 3};
    return local;                       // ⚠️ mar chuke local ka view return
}

// Reallocation:
std::vector<int> v = {1, 2, 3};
std::span<int> s = v;
v.push_back(4);                          // ⚠️ reallocation -> s dangling
```

**Rule: span apne source se ZYADA zinda nahi rehna chahiye.** Function *parameters* mein yeh
apne aap safe hai (caller ka data call ke dauraan zinda). Span ko **store karna** (member,
container mein) khatarnak hai — tab soch lo ki data ka maalik kaun hai aur kab tak.

---

## `std::span` vs doosre options

| | `std::span<T>` | `const std::vector<T>&` | `const T*, size_t` | `const T (&)[N]` |
|---|---|---|---|---|
| C array | ✅ | ❌ | ✅ | ✅ |
| `std::array` | ✅ | ❌ | `.data()` se | ❌ |
| `std::vector` | ✅ | ✅ | `.data()` se | ❌ |
| sub-range | ✅ | ❌ | ✅ (haath se) | ❌ |
| size safe | ✅ | ✅ | ⚠️ haath se | ✅ |
| data ka maalik | ❌ | ✅ | ❌ | ❌ |

---

## Andar kya hota hai

- `std::span<T>` (dynamic) = 2 words: `T* _ptr; size_t _size;`.
- **Function ko kaise pahunchta hai — ABI pe depend karta hai.** Linux (SysV ABI) pe 2
  registers mein. Windows x64 ABI pe (is course ki machine) 16-byte struct **pointer ke
  through** jaata hai — function shuru mein `mov rdx, [rcx+8]` aur `mov rax, [rcx]` se ptr/len
  load karta hai (file 07 mein GCC 16.2 assembly aur timing; 10M-element loop mein koi fark
  nahi naapa gaya).
- `s[i]` → `_ptr[i]` — array jaisa hi scaled load.
- `s.subspan(a, b)` → `{ _ptr + a, b }` — sirf pointer arithmetic, koi allocation nahi.
- `std::span<T, N>` (static) = 1 word (`_ptr`), size compile-time constant hai.
- `-O2` span ke methods inline kar deta hai → span "gayab" ho jaata hai, peeche sirf ek
  seedha pointer loop bachta hai jo vectorize ho sakta hai.

> **HFT relevance:** `std::span<const std::byte>` wire decoder ka standard parameter hai —
> buffer + length, aur har field padhne se pehle ek `.size()` check. `std::array` price-level
> pool pe `std::span<Level>`. Receive buffer ko `subspan` karke messages handlers ko dena — zero
> copy. Dangling wala rule yahan serious hai: buffer ki zindagi ke baad bhi rakha gaya span ek
> classic zero-copy parser bug hai (folder 10, 38). Debug builds mein `_GLIBCXX_ASSERTIONS`
> `span::operator[]` ka bounds check karta hai.

---

## Hands-on

`examples/05_span_demo.cpp` — ek function saare containers pe, badalna, subspan, dangling:

```bash
./build.ps1 09-ARRAYS/examples/05_span_demo.cpp
```

---

## ⚠️ Traps

### Trap 1 — span ko source se zyada der rakhna
```cpp
struct Parser { std::span<const std::byte> buf; };
Parser p{ getTempBuffer() };   // ⚠️ temp khatam, p.buf dangling
```

### Trap 2 — span + vector jo realloc kare
```cpp
std::span<int> s = v;  v.push_back(x);   // ⚠️ s dangle ho sakta hai
```

### Trap 3 — `const` container se `std::span<int>`
```cpp
const std::vector<int> v = {1,2,3};
std::span<int> s = v;      // ❌ badalne wala view nahi mil sakta. std::span<const int> lo
```

### Trap 4 — range ke bahar `subspan`
```cpp
s.subspan(10);            // ⚠️ agar 10 > s.size() to UB. Pehle check karo
```

### Trap 5 — `std::span` ko `const&` se pass karna
```cpp
void f(const std::span<int>& s);   // ⚠️ by value hi idiom hai
```
Span pehle se chhota hai. By value lene pe function ke paas apni copy hoti hai — compiler
jaanta hai ki koi aur use chupke se nahi badal raha (aliasing ki chinta kam). Linux pe by value
registers mein bhi aata hai. `const&` se kuch nahi bachta.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`std::span` data ka maalik hai / copy karta hai" | Non-owning view — 16 bytes |
| "`std::span` dangling se safe hai" | Sirf tab jab source zyada jeeye (parameters: haan; store kiya: dhyaan se) |
| "`std::span` ko `const&` se pass karo" | By value — woh pehle se chhota hai |
| "`const` vector se `std::span<int>` ban jaayega" | `std::span<const int>` lo |
| "`subspan` slice ki copy banata hai" | Woh ek aur view hai — sirf pointer math |
| "Span hamesha registers mein pass hota hai" | Linux (SysV) pe haan; Windows x64 pe pointer ke through (file 07) |

---

## Exercises

1. **Ek function:** `long long product(std::span<const int>)` — C array, `std::array`,
   `std::vector`, aur ek `subspan` — chaaron pe chalao.

2. **Badlo:** `void clampSpan(std::span<int> s, int lo, int hi)` — har element ko clamp karo.
   Teeno container types pe test karo.

3. **Sub-views:** `std::vector<int> v(20)` mein `0..19` bharo. `first(5)`, `last(5)`,
   `subspan(5, 10)` print karo. `.data()` pointers se confirm karo ki koi data copy nahi hua.

4. **Dangling:** `std::span<int> makeSpan()` likho jo local array ka view return kare.
   `-Wall` warning deta hai? Chalao (UB).

5. **Byte view:** `std::span<const std::byte> bytes = std::as_bytes(std::span(v));`
   — har byte hex mein print karo. `bytes.size()` vs `v.size()`?
   <details><summary>Answer</summary>

   20 `int` ke liye `bytes.size()` = 80, `v.size()` = 20 (GCC 16.2 pe chala ke). `as_bytes`
   element nahi, **bytes** ginta hai: `size() * sizeof(int)`.
   </details>

6. **Static extent:** `std::array<int, 4>` se `std::span<int, 4>` banao. Phir
   `std::array<int, 5>` se try karo — compile error? `span<int,4>` aur `span<int>` ka `sizeof`?
   <details><summary>Answer</summary>

   `array<int,5>` se `span<int,4>` → compile error (size type ka hissa hai). `sizeof`: 8 vs 16
   (GCC 16.2 pe chala ke) — static extent sirf pointer rakhta hai.
   </details>

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
