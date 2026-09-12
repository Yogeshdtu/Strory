# 04 — Arrays aur loops

## Prerequisites
- [`03-accessing-elements.md`](03-accessing-elements.md)
- `07-LOOPS/` (poora — range-for, `for`, off-by-one)

## Yeh topic abhi kyun
Array + loop = programming ki sabse common jodi. "Har element pe kuch karo."
Sahi traversal pattern se off-by-one bugs khatam, aur cache-friendly access
(file 10) free milta hai.

---

## 3 traversal patterns

```cpp
int a[5] = {10, 20, 30, 40, 50};

// 1. INDEX-based -- jab index chahiye (do arrays parallel, position matter kare)
for (std::size_t i = 0; i < std::size(a); ++i)
    std::cout << i << ": " << a[i] << "\n";

// 2. RANGE-based -- jab sirf values chahiye (BEST default)
for (int x : a)              std::cout << x << " ";      // copy (int -> free)
for (int& x : a)             x *= 2;                     // modify
for (const auto& x : a)      total += x;                 // read (bade elements ke liye)

// 3. ITERATOR / pointer -- STL style
for (auto it = std::begin(a); it != std::end(a); ++it)
    std::cout << *it << " ";
```

**Default: range-for.** Index tabhi jab genuinely `i` chahiye.

---

## Size — `std::size` (C++17)

```cpp
#include <iterator>   // ya <array>, <vector> -- std::size sab jagah

std::size(a)          // 5   -- C array pe elements, container pe .size()
std::ssize(a)         // 5   -- signed (C++20), sign-compare warnings se bachta hai
std::begin(a), std::end(a)   // iterators/pointers
```

⚠️ `std::size(a)` sirf tab jab `a` **array ho** (decay ke baad pointer → compile
error, jo actually achha hai — bug pakda gaya).

Purana idiom: `sizeof(a) / sizeof(a[0])` — chalta hai par `std::size` saaf.

---

## Common loop patterns (folder 07 file 08 se, arrays pe)

```cpp
// SUM
long long sum = 0;
for (int x : a) sum += x;

// FIND (+ index)
int found = -1;
for (std::size_t i = 0; i < std::size(a); ++i)
    if (a[i] == target) { found = static_cast<int>(i); break; }

// MAX
int mx = a[0];                        // ⚠️ a khali na ho
for (int x : a) mx = std::max(mx, x);

// COUNT (branchless)
int cnt = 0;
for (int x : a) cnt += (x > threshold);

// PARALLEL arrays (index zaroori)
for (std::size_t i = 0; i < n; ++i)
    result[i] = xs[i] + ys[i];
```

STL algorithms (folder 19) inme se zyada ka ready version dete hain:
`std::accumulate`, `std::find`, `std::max_element`, `std::count_if`,
`std::transform`.

---

## Reverse traversal — unsigned trap

```cpp
// ❌ INFINITE / OOB
for (std::size_t i = std::size(a) - 1; i >= 0; --i) { }   // i >= 0 hamesha true; --i wrap

// ✅
for (std::size_t i = std::size(a); i-- > 0; ) { }         // "i-- > 0" idiom
for (auto it = std::rbegin(a); it != std::rend(a); ++it) { }
```

(Folder 07 file 07.)

---

## 2D arrays — nested loop, order matters (file 06, 10)

```cpp
int m[ROWS][COLS];

// ✅ Row-major -- inner loop = last index -> sequential memory -> cache-friendly
for (std::size_t i = 0; i < ROWS; ++i)
    for (std::size_t j = 0; j < COLS; ++j)
        m[i][j] = ...;

// ❌ Column-major traversal -- stride jump har step -> ~8x slower (folder 07 file 09)
for (std::size_t j = 0; j < COLS; ++j)
    for (std::size_t i = 0; i < ROWS; ++i)
        m[i][j] = ...;
```

---

## Andar kya hota hai

- C array pe range-for andar se ek index loop hi hai (`begin` = `arr`, `end` =
  `arr + N`). `-O2` pe iska machine code haath se likhe index loop jaisa hi banta hai —
  isliye range-for "slow" nahi hai.
- `sum += a[i]` jaise simple loops ko compiler `-O2`/`-O3` pe **auto-vectorize** kar
  deta hai (SIMD) — ek instruction mein 4 se 16 elements tak (folder 07 file 09).
- Jab loop ki limit `std::size(a)` compile time pe pata ho, to chhote `N` ke liye compiler
  loop ko khol ke (unroll) seedhi instructions bana sakta hai.
- Elements ek ke baad ek memory mein hain → CPU ka hardware prefetcher agla data pehle se
  cache mein le aata hai → memory ka intezaar chhup jaata hai.

> **HFT relevance:** Hot loops hamesha contiguous arrays pe chalte hain (`std::array`,
> `std::vector`, ya raw array) — compiler vectorize karta hai, prefetcher data pehle se
> laata hai, aur branches predictable rehti hain. `std::list` jaisa pointer se jura data
> matlab har element pe ek cache miss — isliye woh hot path se bahar rehta hai.
> Parallel arrays (SoA — file 10, folder 11) pe traversal SIMD ke liye sabse achha hai.
> Rule yaad rakho: hot loop → contiguous array → `-O2` ko vectorize karne do.

---

## Hands-on

```bash
./build.ps1 09-ARRAYS/examples/01_array_basics.cpp     # 3 traversal styles
```

---

## ⚠️ Traps

### Trap 1 — `i <= size`
```cpp
for (std::size_t i = 0; i <= std::size(a); ++i) a[i] = 0;   // ⚠️ last iter OOB
```

### Trap 2 — `int i` ko `size()` se compare karna
```cpp
for (int i = 0; i < std::size(a); ++i) { }   // ⚠️ -Wsign-compare. size_t i, ya std::ssize
```

### Trap 3 — range-for mein bade elements ki copy
```cpp
for (auto row : matrix) { }        // ⚠️ har row (poora array) copy. const auto&
```

### Trap 4 — loop chalte-chalte size badalna (vector)
```cpp
for (int x : v) if (cond(x)) v.push_back(...);   // ⚠️ reallocation -> UB (folder 07 file 07)
```

### Trap 5 — decay ho chuke array pe `std::size`
```cpp
void f(int a[]) { for (std::size_t i = 0; i < std::size(a); ++i) ... }   // ❌ compile error (a yahan pointer hai)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Index loop range-for se tez" | Same codegen `-O2` pe |
| "`std::size(a)` decayed array pe chalega" | Nahi — compile error (feature, not bug) |
| "Reverse loop `for (size_t i = n-1; i >= 0; --i)`" | Infinite — `i-- > 0` idiom |
| "2D traversal order se farq nahi" | Row vs column ~8x (cache) |
| "`for (auto x : matrix)` efficient hai" | Har row copy — `const auto&` |

---

## Exercises

1. **3 ways:** `int a[6] = {5,2,8,1,9,3}` ka sum — index loop, range-for,
   `std::accumulate`. Teenon same?

2. **Parallel arrays:** `int price[5]`, `int qty[5]` — `notional[i] = price[i] *
   qty[i]`. Range-for se ho sakta hai? (Nahi — index chahiye.)

3. **Find + index:** `firstIndexOf(const int* a, std::size_t n, int target)` →
   `std::optional<std::size_t>`.

4. **Reverse:** `int a[5]` ko ulta print — `i-- > 0` idiom, phir `std::rbegin`/
   `std::rend`.

5. **`std::size` fail:** ek function `void f(int a[])` jismein `std::size(a)`
   likho. Error kya? Fix (size param, `std::span`, reference-to-array, `std::array`).

6. **Vectorize check:** `for (i) c[i] = a[i] + b[i];` — `g++ -O3 -march=native
   -fopt-info-vec` se compile. Vectorized?

---

## Interview questions

1. Array traverse karne ke 3 tareeke — kab kaunsa?
2. `std::size` vs `sizeof(a)/sizeof(a[0])` — kya farq?
3. `std::size(a)` decayed pointer pe kyun nahi chalta — yeh feature kyun?
4. Reverse traversal ka unsigned trap aur fix?
5. Range-for over `int a[5]` andar se kya expand hota hai?
6. Simple array loop `-O2` pe kaise optimize hota hai (vectorize, unroll)?

---

## Next
→ [`05-array-decay.md`](05-array-decay.md)
