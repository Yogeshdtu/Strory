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
| Memory layout | 20 bytes ek saath | **bilkul wahi** (koi extra kharcha nahi) |
| `.size()` | ❌ (decay se pehle `std::size`) | ✅ hamesha |
| Pointer mein decay | ✅ (size kho jaati hai) | ❌ (`.data()` se khud maango) |
| Copy (`b = a`) | ❌ | ✅ (element-by-element) |
| Compare (`a == b`) | ❌ (addresses compare hote hain) | ✅ (element-by-element) |
| Function se return | ❌ (array return type allowed hi nahi) | ✅ (RVO, free) |
| `.at()` bounds-check | ❌ | ✅ (exception phenkta hai) |
| STL algorithms | `std::begin/end` ke zariye | `.begin()/.end()` |
| Function ko dena | decay hota hai | by value **copy** hota hai (`const&` / `span` lo) |

**`sizeof(std::array<int,5>) == sizeof(int[5]) == 20`.** Koi chhupa hua kharcha nahi.

Analogy: C array ek khula dibba hai jiske upar size likha nahi — kisi ko doge to woh sirf
dibbe ka pata (pointer) le jaata hai. `std::array` wahi dibba hai, par ek wrapper ke andar
jispe size chhapa hai, aur jise poora ka poora copy, compare, return kar sakte ho.

---

## API

```cpp
std::array<int, 5> a = {10, 20, 30, 40, 50};

a.size()            // 5
a.empty()           // false
a[2]                // 30  (check nahi hota)
a.at(2)             // 30  (check hota hai -- galat index pe std::out_of_range)
a.front()           // 10
a.back()            // 50
a.data()            // int*  (khud maanga hua decay -- C APIs ke liye)
a.fill(7)           // sab 7
a.begin(), a.end()  // iterators
a.rbegin(), a.rend()// reverse

std::array<int, 5> b = a;              // copy
b == a                                 // element-by-element compare
std::swap(a, b);                       // O(N) swap
auto [p, q, r, s, t] = a;              // structured binding
```

---

## Decay nahi hota — par by-value COPY hota hai

```cpp
void f(std::array<int, 5> a);          // ⚠️ har call pe 20 bytes copy
void f(const std::array<int, 5>& a);   // ✅ copy nahi, par sirf size 5 ke liye
void f(std::span<const int> a);        // ✅ copy nahi, koi bhi size (file 09)
```

Rule: read-only parameter → `std::span<const T>` (flexible), ya `const std::array<T, N>&`
(jab size function ke contract ka hissa ho).

---

## CTAD (C++17)

```cpp
std::array a = {1, 2, 3};              // -> std::array<int, 3>
std::array b = {1.0, 2.0};            // -> std::array<double, 2>
std::to_array({1, 2, 3});             // C++20 -- braced list ya C array se
```

---

## Kahan use hota hai

```cpp
// Fixed lookup table (constexpr -- folder 08 lesson 11)
constexpr std::array<int, 7> daysInMonth = {31, 28, 31, 30, 31, 30, 31};

// Kai jude hue values ek saath return karna
std::array<double, 3> rgbToHsv(double r, double g, double b);

// Fixed-size buffer -- heap nahi
std::array<char, 64> lineBuffer;

// Pata ho ki gehrai kitni max hogi -- chhota stack
std::array<Node*, 32> dfsStack;
std::size_t top = 0;
```

---

## `std::array` vs `std::vector`

| | `std::array<T, N>` | `std::vector<T>` |
|---|---|---|
| Size | compile-time, fixed | runtime, badh sakta hai |
| Storage | inline (jahan object hai — aksar stack) | heap |
| Allocation | **koi nahi** | construct / badhne pe `new` |
| `push_back` | ❌ | ✅ |
| Copy ki cost | fixed `memcpy` | heap allocation + copy |

**Max size compile time pe pata hai → `std::array`.** Warna `std::vector` (aur agar upper
limit pata ho to `reserve()`).

---

## Andar kya hota hai

- `std::array` = `struct { T _elems[N]; }` — sach mein ek struct ke andar C array. Aggregate
  type hai, aur trivial `T` ke liye trivially copyable.
- `[]` → C array jaisa hi ek scaled load. `.at()` → upar se ek `cmp`/`jae` (bounds check) judta hai.
- `.size()` → compile-time `N` lautata hai — constant ban ke aksar gayab ho jaata hai.
- By value dena → `N*sizeof(T)` bytes ka `memcpy`. `-O2` chhote `N` pe ise hata bhi sakta hai.
- `constexpr std::array` → data seedha `.rodata` mein baith jaata hai (folder 08 lesson 11).

> **HFT relevance:** HFT mein `std::array` har jagah hai: fixed message buffers, price-level
> arrays (`std::array<Level, kDepth>`), chhote object pools, ring-buffer ki storage, `constexpr`
> protocol tables. **Heap zero, cache mein rehta hai, size-safe.** By-value copy se bachne ke liye
> `std::span` se pass karo. Jab size sach mein runtime ho par ek limit ke andar, to `std::array` +
> ek `size_` counter (a "static vector" — `boost::static_vector`, ya C++26 ka `std::inplace_vector`,
> jo GCC 16.2 pe chal gaya — folder 22 file 15 ki C++26 probe) latency ke liye `std::vector` se
> behtar hai (folder 36).

---

## Hands-on

`examples/04_std_array.cpp` — API, `.at()` ka exception, value semantics, decay nahi hota,
STL algorithms, zero overhead:

```bash
./build.ps1 09-ARRAYS/examples/04_std_array.cpp
```

---

## ⚠️ Traps

### Trap 1 — by value pass karna
```cpp
void process(std::array<Big, 100> a);   // ⚠️ bahut badi copy. const& ya span
```

### Trap 2 — `.data()` array se zyada jee gaya
```cpp
int* p;
{ std::array<int, 3> a = {1,2,3}; p = a.data(); }   // ⚠️ dangling
```

### Trap 3 — `std::array<int>` (N bhool gaye)
```cpp
std::array<int> a;      // ❌ N zaroori hai
std::array<int, 5> a;   // ✅
```

### Trap 4 — uninitialized `std::array` (trivial T ke liye C array jaisa)
```cpp
std::array<int, 5> a;   // ⚠️ local hai to elements mein garbage
std::array<int, 5> a{}; // ✅ sab zero
```

### Trap 5 — alag `N` wale arrays pe `a == b`
```cpp
std::array<int, 3> a;  std::array<int, 4> b;
a == b;                 // ❌ compile error (types alag hain) -- "false" nahi
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`std::array` C array se slow / bhaari hai" | Layout aur speed bilkul same |
| "`std::array` decay nahi hota, to by value dena theek hai" | Copy hota hai — `const&` / `span` lo |
| "`std::array<int, 5> a;` elements ko zero karta hai" | Garbage (local, trivial T) — `a{}` likho |
| "`std::array` badh sakta hai" | Fixed hai — badhne wala chahiye to `std::vector` |
| "`std::array` heap pe rehta hai" | Jahan object hai wahin (aksar stack) |
| "C array function se return hoke decay ho jaata hai" | Return type array ho hi nahi sakta — compile error |

---

## Exercises

1. **API tour:** `std::array<int, 6> a = {5,2,8,1,9,3}` — `size`, `front`, `back`,
   `at(2)`, `at(10)` (catch karo), `fill(0)`, sort, `max_element`.

2. **Value semantics:** `auto b = a; b[0] = 99;` — `a[0]` badla? `a == b`?

3. **Copy cost:** `void f(std::array<int, 1000> a)` vs `const&` — 100000 calls,
   `-O2`, time lo.

4. **`constexpr` table:** `constexpr std::array<int, 13> fib()` — fib(0..12).
   `static_assert(fib()[10] == 55);` `-S` se dekho ki data `.rodata` mein hai.

5. **Static vector:** `template<class T, std::size_t Cap> struct SmallVec {
   std::array<T, Cap> buf; std::size_t n = 0; void push(T); ... };` — `push`, `size`,
   `operator[]`, aur range-for support implement karo.

6. **`std::array` vs `std::vector`:** ek fixed 8-slot cache — dono se banao, `-O2` pe
   insert/lookup time lo. Allocations kitne hue?

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
