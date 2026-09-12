# 11 — `constexpr` functions

## Prerequisites
- [`10-inline-functions.md`](10-inline-functions.md)
- `03-VARIABLES-DATA-TYPES/11-const-and-constexpr.md` (`const` vs `constexpr`)
- `05-OPERATORS/06-bit-tricks.md` mein `constexpr` bit functions dekhe the

## Yeh topic abhi kyun
`constexpr` function woh hai jo **compile-time pe chal sakta hai** — result binary
mein pehle se baked hota hai, runtime pe kuch compute nahi hota. Lookup tables,
config validation, math constants, bit masks — sab compile-time pe. Zero runtime
cost wali abstraction. HFT aur embedded mein bahut use hota hai.

---

## Basic

```cpp
constexpr int square(int x) {
    return x * x;
}

constexpr int nine = square(3);        // COMPILE-TIME -- `nine` binary mein 9 hai
int r = square(runtimeValue);          // RUNTIME -- normal function ki tarah chalta hai
static_assert(square(4) == 16);        // compile-time check
```

`constexpr` function **dono** kaam kar sakti hai:
- **Constant context** (`constexpr` variable, `static_assert`, array size,
  template arg, `if constexpr`) → **compile-time** evaluate
- **Normal context** (runtime argument) → **runtime** evaluate, jaise koi bhi function

---

## `const` vs `constexpr` vs `consteval`

```cpp
const int a = getValue();          // runtime OK; value badal nahi sakti (read-only)
constexpr int b = 5 * 5;           // MUST be compile-time computable; implies const
consteval int c() { return 42; }   // C++20 -- MUST be compile-time; runtime call = ERROR
```

| | Compile-time? | Runtime bhi? |
|---|---|---|
| `const` variable | ho sakta | ho sakta |
| `constexpr` variable | **haan, mandatory** | — |
| `constexpr` function | **agar args constant** | haan, agar args runtime |
| `consteval` function | **hamesha** | ❌ never |
| `constinit` (C++20) | init compile-time | variable runtime-mutable |

---

## Kya `constexpr` function mein allowed hai

C++20 ke baad **lgbhg sab kuch** (rules bahut loose ho gaye):

```cpp
constexpr int factorial(int n) {
    int result = 1;
    for (int i = 2; i <= n; ++i)     // ✅ loops OK
        result *= i;
    return result;                    // ✅ local vars, mutation OK
}

constexpr int fib(int n) {            // ✅ recursion OK
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}

constexpr auto makeTable() {          // ✅ C++20: std::array, even std::vector (C++20, transient)
    std::array<int, 10> t{};
    for (int i = 0; i < 10; ++i) t[i] = i * i;
    return t;
}
```

### Kya NAHI allowed (compile-time evaluation ke liye)
- `new`/`delete` jo compile-time se leak ho (C++20 mein transient allocation OK
  jab tak result mein na aaye)
- `reinterpret_cast`
- Non-`constexpr` function call (jab compile-time chal raha ho)
- `static` local variables (mutable)
- Undefined behaviour (compile-time UB = **compile error**, jo actually achha hai —
  overflow yahan pakda jaata hai)
- I/O, syscalls, `std::cout`, random

Agar `constexpr` function inme se koi cheez use kare, woh **sirf runtime** pe
chalegi (ya `constexpr` context mein error degi).

---

## `if constexpr` — compile-time branch

```cpp
template <typename T>
auto process(T value) {
    if constexpr (std::is_integral_v<T>) {
        return value * 2;              // sirf yeh branch COMPILE hoti hai (integral T ke liye)
    } else {
        return value + value;          // yeh branch integral T ke liye code hi nahi banti
    }
}
```

Normal `if` mein dono branches compile hoti hain (chahe kbhi na chalein).
`if constexpr` mein **jhoothi branch discard** ho jaati hai — templates ke liye
critical (folder 21).

---

## Practical — compile-time lookup table

```cpp
#include <array>

constexpr std::array<std::uint32_t, 256> makeCrcTable() {
    std::array<std::uint32_t, 256> table{};
    for (std::uint32_t i = 0; i < 256; ++i) {
        std::uint32_t c = i;
        for (int k = 0; k < 8; ++k)
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        table[i] = c;
    }
    return table;
}

constexpr auto kCrcTable = makeCrcTable();   // poora table BINARY mein, runtime pe 0 compute

std::uint32_t crc32(std::span<const std::byte> data) {
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::byte b : data)
        crc = kCrcTable[(crc ^ std::to_integer<std::uint8_t>(b)) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}
```

Table generation loop **compile time** pe chala. Binary mein sirf 1 KB ka ready
table. Runtime pe koi table-build cost nahi.

---

## Andar kya hota hai

- **Constant context**: compiler function ko ek internal interpreter se chalata
  hai (constant evaluation). Result ek literal ban jaata hai — binary mein baked.
- **Runtime context**: bilkul normal function — `call` (ya inline), stack frame,
  wahi machine code.
- **Compile-time UB detection**: `constexpr` evaluation mein signed overflow,
  OOB access, uninitialized read → **compile error** (runtime UB ban ke chhupta
  nahi). Yeh ek bonus safety hai.

`constexpr` functions **implicitly `inline`** hain (ODR-wise) — header mein reh
sakti hain.

> **HFT relevance:** `constexpr` = zero-cost abstraction ka core. Message field
> offsets, protocol constants, bit masks, CRC/checksum tables, price-tick
> lookup, `enum`-to-string maps — sab compile-time pe generate karke binary mein
> bake karte hain. Runtime pe: sirf ek array index. Config validation
> (`static_assert(kBufferSize % 64 == 0)`) compile pe fail hoti hai, production
> mein nahi. Aur compile-time UB detection ek free correctness net hai. Folders
> 21 (templates), 36 (low-latency).

---

## Hands-on

```cpp
#include <array>
#include <iostream>

constexpr int factorial(int n) {
    int r = 1;
    for (int i = 2; i <= n; ++i) r *= i;
    return r;
}

constexpr auto squares = [] {
    std::array<int, 10> a{};
    for (int i = 0; i < 10; ++i) a[i] = i * i;
    return a;
}();

int main() {
    static_assert(factorial(5) == 120);          // compile-time
    constexpr int f10 = factorial(10);           // compile-time -> 3628800 baked

    int n; std::cin >> n;
    std::cout << factorial(n) << "\n";           // RUNTIME -- same function

    for (int s : squares) std::cout << s << " "; // table baked in binary
}
```

```bash
g++ -std=c++20 -O2 -S ce.cpp -o - | c++filt | grep -A3 'f10\|squares'
# f10 aur squares ka data literal/rodata mein -- koi compute code nahi
```

---

## ⚠️ Traps

### Trap 1 — `constexpr` var ko runtime value se init
```cpp
constexpr int x = getRuntimeValue();   // ❌ ERROR -- constexpr var must be compile-time
const int x = getRuntimeValue();       // ✅ (const, runtime)
```

### Trap 2 — `constexpr` function mein non-constexpr call
```cpp
constexpr int f() { return std::rand(); }   // ⚠️ compile-time context mein ERROR; runtime pe theek
```

### Trap 3 — expect karna ki `constexpr` function hamesha compile-time chalti hai
```cpp
constexpr int sq(int x) { return x * x; }
int y = sq(userInput);                  // ⚠️ RUNTIME -- constexpr guarantee nahi deta compile-time
constexpr int z = sq(5);                // ✅ compile-time (constant context)
// consteval chahiye "hamesha compile-time" ke liye
```

### Trap 4 — bada compile-time computation
```cpp
constexpr auto huge = buildMillionEntryTable();   // ⚠️ compile time + memory bahut badh sakta hai
```

### Trap 5 — `if` vs `if constexpr` templates mein
```cpp
template <class T> void f(T v) {
    if (std::is_pointer_v<T>) { use(*v); }        // ⚠️ non-pointer T ke liye bhi COMPILE hoti hai -> error
    if constexpr (std::is_pointer_v<T>) { use(*v); }  // ✅ discard for non-pointer T
}
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`constexpr` function hamesha compile-time chalti hai" | Sirf constant context mein; warna runtime |
| "`const` aur `constexpr` same" | `constexpr` = compile-time computable (implies const); `const` = read-only |
| "`constexpr` function mein loop nahi" | C++14+ mein loops, mutation, recursion — sab OK |
| "`if constexpr` aur `if` same" | `if constexpr` jhoothi branch discard karta hai (templates) |
| "`consteval` aur `constexpr` same" | `consteval` = **hamesha** compile-time; runtime call = error |

---

## Exercises

1. **Dual use:** `constexpr int cube(int x)` — `static_assert(cube(3) == 27)` aur
   `std::cin >> n; std::cout << cube(n);` dono. Compile + run.

2. **Compile-time table:** `constexpr std::array<int, 20> powersOf2()` — `2^0` se
   `2^19`. `constexpr auto t = powersOf2();`. `-S` se dekho — data rodata mein?

3. **`static_assert` validation:** `constexpr bool isPow2(std::size_t n)` likho,
   phir `static_assert(isPow2(kRingSize), "ring size power of 2 hona chahiye");`
   — `kRingSize` ko 1000 karke compile karo. Error?

4. **UB catch:** `constexpr int f(int x) { return x + 1; }` —
   `constexpr int bad = f(INT_MAX);` compile karo. Kya hua? (Runtime UB yahan
   compile error banta hai.)

5. **`consteval`:** `consteval int forceCT(int x) { return x * x; }` —
   `forceCT(5)` OK? `int n = 3; forceCT(n)` — error?

6. **`if constexpr`:** ek template `describe<T>()` jo `T` integral pe `"int-like"`,
   floating pe `"float-like"`, warna `"other"` return kare — `if constexpr` se.

7. **CRC table:** upar wala `makeCrcTable()` use karke `crc32` likho, kisi
   string ka CRC compute karo, online CRC-32 se verify.

---

## Interview questions

1. `constexpr` function kya hai? Compile-time aur runtime dono kaam kaise?
2. `const`, `constexpr`, `consteval`, `constinit` — fark?
3. `constexpr` function compile-time chalne ki guarantee deta hai? `consteval`?
4. `constexpr` evaluation mein UB (overflow) ka kya hota hai?
5. `if` vs `if constexpr` — templates mein kyun matter karta hai?
6. Compile-time lookup table ka faayda? Ek use case.
7. `constexpr` function implicitly `inline` hai — kyun matter karta hai?

---

## Next
→ [`12-function-attributes.md`](12-function-attributes.md)
