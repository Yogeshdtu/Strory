# 14 — `sizeof` aur `<limits>`

## Prerequisites
`05-int-deep-dive.md`, `09-fixed-width-types.md`

## Yeh topic abhi kyun
Aapko baar baar puchna padega: "yeh type kitni jagah leta hai? uski max value kya hai?"
Yeh file un dono ke tools deti hai — aur unke gotchas.

---

## `sizeof`

**`sizeof` batata hai ki koi type ya object kitne BYTES leta hai.**

```cpp
sizeof(int)         // 4
sizeof(double)      // 8
sizeof(char)        // 1  (hamesha, by definition)

int x = 5;
sizeof(x)           // 4
sizeof x            // 4  (variables ke saath brackets optional)
```

### Return type: `std::size_t`

```cpp
std::size_t s = sizeof(int);
std::cout << sizeof(int) << "\n";      // theek hai

// ⚠️ Signed/unsigned comparison ka risk
if (sizeof(int) > someSignedValue) { }     // warning
```

---

## 🔑 `sizeof` ek COMPILE-TIME operator hai

Yeh runtime pe kuch nahi karta. Compiler ise ek constant se replace kar deta hai.

```cpp
constexpr std::size_t s = sizeof(int);     // ✅ constexpr context mein chalta hai
int arr[sizeof(int)];                       // ✅ array size ban sakta hai
static_assert(sizeof(int) == 4);            // ✅ compile time check
```

### Iska ek surprising nateeja

```cpp
int i = 5;
sizeof(i++);                   // ⚠️ i++ CHALTA HI NAHI!
std::cout << i;                // 5, not 6
```

`sizeof` apne operand ko **evaluate nahi karta** — sirf uska type dekhta hai.

```cpp
sizeof(someExpensiveFunction());     // function call nahi hoga
```

**Exception:** VLA (C mein), par C++ mein VLA hai hi nahi.

---

## ⚠️ `sizeof` ke gotchas

### Gotcha 1: Array vs Pointer

```cpp
int arr[10];
sizeof(arr);                   // 40 (10 * 4)  ✅

void func(int arr[]) {
    sizeof(arr);               // 8! ⚠️ POINTER ka size hai, array ka nahi
}
```

**Kyun?** Function parameter mein array **decay** hokar pointer ban jaata hai.
(Folder 09 mein detail.)

**Solutions:**
```cpp
// 1. Size alag se pass karo
void func(int* arr, std::size_t n);

// 2. Reference to array (size compile time pe pata hona chahiye)
template <std::size_t N>
void func(int (&arr)[N]) {
    sizeof(arr);               // ✅ N * 4
}

// 3. std::array (best)
void func(const std::array<int, 10>& arr);

// 4. std::span (C++20, best for generic)
void func(std::span<int> arr) {
    arr.size();                // ✅
}
```

### Gotcha 2: `sizeof` string literals

```cpp
sizeof("Hello");               // 6  (5 chars + '\0')
strlen("Hello");               // 5  (null terminator nahi ginta)

const char* p = "Hello";
sizeof(p);                     // 8  (pointer ka size!)
```

### Gotcha 3: Struct padding

```cpp
struct A {
    char  c;      // 1 byte
    int   i;      // 4 bytes
};
sizeof(A);        // 8, not 5!  (padding ki wajah se)
```

**Kyun?** Alignment. `int` ko 4-byte boundary pe hona chahiye.

```
   Memory layout:
   +---+---+---+---+---+---+---+---+
   | c | ? | ? | ? |     i         |
   +---+---+---+---+---+---+---+---+
     0   1   2   3   4   5   6   7
         ^^^^^^^^^
         3 bytes PADDING
```

**Member order matter karta hai:**
```cpp
struct Bad  { char c; int i; char d; };     // 12 bytes
struct Good { int i; char c; char d; };     // 8 bytes
```

Folder 11 mein padding poora padhenge. **Yeh HFT ke liye critical hai** — struct size
directly cache usage affect karti hai.

### Gotcha 4: Empty class

```cpp
struct Empty { };
sizeof(Empty);                 // 1, not 0!
```

**Kyun?** Har object ka **unique address** hona chahiye. Agar size 0 hoti, do objects
ka address same ho jaata.

```cpp
Empty a, b;
&a != &b;                      // yeh true hona chahiye
```

### Gotcha 5: `sizeof(void)`

```cpp
sizeof(void);                  // ❌ error (incomplete type)
sizeof(void*);                 // ✅ 8
```

---

## `<limits>` — type ki limits

```cpp
#include <limits>

std::numeric_limits<int>::min()          // -2147483648
std::numeric_limits<int>::max()          // 2147483647
std::numeric_limits<unsigned>::max()     // 4294967295

std::numeric_limits<double>::min()       // ⚠️ 2.22507e-308 -- SABSE CHHOTA POSITIVE!
std::numeric_limits<double>::lowest()    // ✅ -1.79769e+308 -- SABSE CHHOTA (C++11)
std::numeric_limits<double>::max()       // 1.79769e+308

std::numeric_limits<double>::epsilon()   // 2.22045e-16 (smallest 1.0 se fark)
std::numeric_limits<double>::infinity()
std::numeric_limits<double>::quiet_NaN()

std::numeric_limits<int>::is_signed      // true
std::numeric_limits<char>::is_signed     // platform-dependent!
std::numeric_limits<int>::digits         // 31 (binary digits, sign chhod ke)
```

### ⚠️ `min()` vs `lowest()` — classic trap

```cpp
std::numeric_limits<int>::min()          // -2147483648  ✅ sabse chhota
std::numeric_limits<double>::min()       // 2.22e-308    ⚠️ sabse chhota POSITIVE!
std::numeric_limits<double>::lowest()    // -1.79e+308   ✅ sabse chhota
```

**Integers ke liye:** `min()` = sabse chhota
**Floating point ke liye:** `min()` = sabse chhota **positive normalized**

Agar aapko "sabse chhota possible value" chahiye, **hamesha `lowest()` use karo** —
woh dono ke liye sahi hai.

```cpp
// ❌ Bug
double minVal = std::numeric_limits<double>::min();    // 2.22e-308!
for (double x : values) if (x > minVal) minVal = x;    // galat starting point

// ✅ Sahi
double minVal = std::numeric_limits<double>::lowest();
```

---

## `<climits>` aur `<cfloat>` (C style)

```cpp
#include <climits>
CHAR_BIT      // 8 (ek byte mein kitne bits)
INT_MIN, INT_MAX
UINT_MAX
LONG_MIN, LONG_MAX
LLONG_MIN, LLONG_MAX

#include <cfloat>
DBL_MAX, DBL_MIN, DBL_EPSILON
FLT_MAX, FLT_MIN
```

**`<limits>` behtar hai** kyunki:
- Templates mein kaam karta hai
- `constexpr` hai
- Type-safe hai

```cpp
// Template code mein
template <typename T>
T getMax() {
    return std::numeric_limits<T>::max();     // ✅ har type ke liye
    // INT_MAX;                                // ❌ sirf int ke liye
}
```

---

## `alignof` aur `alignas`

```cpp
alignof(int)          // 4  (int ko 4-byte boundary pe hona chahiye)
alignof(double)       // 8
alignof(char)         // 1

struct alignas(64) CacheLineAligned {          // 64-byte boundary pe
    int data[16];
};
alignof(CacheLineAligned);                      // 64
sizeof(CacheLineAligned);                       // 64
```

> **HFT relevance:** Cache line alignment se **false sharing** rokte hain.
> Agar do threads alag variables likhte hain jo same cache line mein hain, to
> har write doosre ka cache invalidate karti hai — 10-100x slowdown.
>
> ```cpp
> struct alignas(64) ThreadCounter {
>     std::atomic<std::uint64_t> count;
>     // 56 bytes padding automatically
> };
> ```
>
> Folder 28 aur 32 mein poora.

### C++17 ka helper
```cpp
#include <new>
std::hardware_destructive_interference_size    // usually 64 -- cache line size
std::hardware_constructive_interference_size
```

---

## `offsetof` — member ka offset

```cpp
#include <cstddef>

struct Message {
    std::uint8_t  type;
    std::uint64_t timestamp;
    std::int32_t  price;
};

offsetof(Message, type);         // 0
offsetof(Message, timestamp);    // 8  (padding ki wajah se, 1 nahi!)
offsetof(Message, price);        // 16
```

Yeh binary protocol verification ke liye bahut useful hai:

```cpp
static_assert(offsetof(Message, timestamp) == 8, "Layout badal gaya!");
```

**Note:** `offsetof` technically sirf standard-layout types pe defined hai.

---

## Portable code likhne ke rules

```cpp
// ❌ Assume mat karo
int x;                                   // size guaranteed nahi
if (sizeof(int) == 4) { }                // yeh check hi galat approach hai

// ✅ Explicit raho
std::int32_t x;                          // hamesha 4 bytes
static_assert(sizeof(std::int32_t) == 4);

// ✅ sizeof use karo, hardcode mat karo
std::memcpy(dest, src, sizeof(MyStruct));       // ✅
std::memcpy(dest, src, 16);                     // ❌ struct badla to bug

// ✅ Limits use karo, hardcode mat karo
if (value > std::numeric_limits<int>::max()) { }    // ✅
if (value > 2147483647) { }                          // ❌
```

---

## Hands-on

```bash
cd ~/cpp-practice
cat > sizes.cpp << 'END'
#include <iostream>
#include <limits>
#include <cstdint>
#include <cstddef>
#include <new>
#include <iomanip>

struct Padded    { char c; int i; char d; };
struct Optimized { int i; char c; char d; };
struct Empty     { };
struct alignas(64) CacheAligned { int data[4]; };

int main() {
    std::cout << "===== FUNDAMENTAL TYPE SIZES =====\n";
    std::cout << "bool        " << sizeof(bool)        << "\n";
    std::cout << "char        " << sizeof(char)        << "\n";
    std::cout << "short       " << sizeof(short)       << "\n";
    std::cout << "int         " << sizeof(int)         << "\n";
    std::cout << "long        " << sizeof(long)        << "  <- Windows pe 4!\n";
    std::cout << "long long   " << sizeof(long long)   << "\n";
    std::cout << "float       " << sizeof(float)       << "\n";
    std::cout << "double      " << sizeof(double)      << "\n";
    std::cout << "long double " << sizeof(long double) << "\n";
    std::cout << "void*       " << sizeof(void*)       << "\n";

    std::cout << "\n===== FIXED WIDTH =====\n";
    std::cout << "int8_t   " << sizeof(std::int8_t)   << "\n";
    std::cout << "int16_t  " << sizeof(std::int16_t)  << "\n";
    std::cout << "int32_t  " << sizeof(std::int32_t)  << "\n";
    std::cout << "int64_t  " << sizeof(std::int64_t)  << "\n";
    std::cout << "size_t   " << sizeof(std::size_t)   << "\n";

    std::cout << "\n===== LIMITS =====\n";
    std::cout << "int      min " << std::numeric_limits<int>::min() 
              << "  max " << std::numeric_limits<int>::max() << "\n";
    std::cout << "int64_t  min " << std::numeric_limits<std::int64_t>::min()
              << "  max " << std::numeric_limits<std::int64_t>::max() << "\n";
    std::cout << "uint32_t max " << std::numeric_limits<std::uint32_t>::max() << "\n";

    std::cout << "\n===== min() vs lowest() TRAP =====\n";
    std::cout << std::scientific;
    std::cout << "double min():    " << std::numeric_limits<double>::min()
              << "  <- sabse chhota POSITIVE!\n";
    std::cout << "double lowest(): " << std::numeric_limits<double>::lowest()
              << "  <- sabse chhota\n";
    std::cout << "double max():    " << std::numeric_limits<double>::max() << "\n";
    std::cout << "double epsilon():" << std::numeric_limits<double>::epsilon() << "\n";
    std::cout << std::defaultfloat;

    std::cout << "\n===== char SIGNEDNESS =====\n";
    std::cout << "char is signed? " 
              << (std::numeric_limits<char>::is_signed ? "YES" : "NO") << "\n";
    std::cout << "CHAR_BIT (bits per byte): " 
              << std::numeric_limits<unsigned char>::digits << "\n";

    std::cout << "\n===== STRUCT PADDING =====\n";
    std::cout << "struct Padded    { char; int; char; }  = " 
              << sizeof(Padded) << " bytes\n";
    std::cout << "struct Optimized { int; char; char; }  = " 
              << sizeof(Optimized) << " bytes\n";
    std::cout << "  ^ same members, alag order -> alag size!\n";
    std::cout << "  offsetof(Padded, i)    = " << offsetof(Padded, i) << "\n";
    std::cout << "  offsetof(Optimized, c) = " << offsetof(Optimized, c) << "\n";

    std::cout << "\n===== EMPTY STRUCT =====\n";
    std::cout << "sizeof(Empty) = " << sizeof(Empty) 
              << "  <- 0 nahi! (unique address chahiye)\n";

    std::cout << "\n===== ALIGNMENT =====\n";
    std::cout << "alignof(char)   " << alignof(char)   << "\n";
    std::cout << "alignof(int)    " << alignof(int)    << "\n";
    std::cout << "alignof(double) " << alignof(double) << "\n";
    std::cout << "alignof(CacheAligned) " << alignof(CacheAligned) << "\n";
    std::cout << "sizeof(CacheAligned)  " << sizeof(CacheAligned) 
              << "  <- padded to alignment\n";
    std::cout << "hardware_destructive_interference_size = "
              << std::hardware_destructive_interference_size << "\n";

    std::cout << "\n===== sizeof DOES NOT EVALUATE =====\n";
    int i = 5;
    std::size_t s = sizeof(i++);
    std::cout << "sizeof(i++) = " << s << ", i abhi bhi = " << i 
              << "  <- i++ CHALA HI NAHI!\n";

    std::cout << "\n===== ARRAY vs POINTER =====\n";
    int arr[10];
    int* ptr = arr;
    std::cout << "sizeof(arr) = " << sizeof(arr) << "  (10 ints)\n";
    std::cout << "sizeof(ptr) = " << sizeof(ptr) << "  (sirf pointer)\n";

    std::cout << "\n===== STRING LITERAL =====\n";
    std::cout << "sizeof(\"Hello\") = " << sizeof("Hello") 
              << "  (5 chars + null terminator)\n";
    const char* p = "Hello";
    std::cout << "sizeof(p)       = " << sizeof(p) << "  (pointer)\n";

    return 0;
}
END
g++ -std=c++20 -Wall -Wextra sizes.cpp -o sizes && ./sizes
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`sizeof` runtime pe chalta hai" | Compile-time operator hai |
| "`sizeof(i++)` `i` badhata hai" | Operand evaluate hi nahi hota |
| "`sizeof(arr)` function mein array size deta hai" | Pointer size deta hai (decay) |
| "`sizeof(struct)` = members ka sum" | Padding add hoti hai |
| "`sizeof(Empty)` = 0" | 1 hota hai |
| "`numeric_limits<double>::min()` sabse chhota hai" | ❌ Sabse chhota **positive**. `lowest()` use karo |

---

## Exercises

1. `sizes.cpp` chalao. Sab output samjho.

2. Struct padding optimize karo:
   ```cpp
   struct Order {
       char   side;          // 1
       double price;         // 8
       char   type;          // 1
       int    quantity;      // 4
       char   status;        // 1
   };
   ```
   Current size? Members reorder karke minimum size kya ho sakti hai?
   <details><summary>Answer</summary>
   Current: 32 bytes (bahut padding).

   Optimized (bade se chhote order mein):
   ```cpp
   struct Order {
       double price;         // 8
       int    quantity;      // 4
       char   side;          // 1
       char   type;          // 1
       char   status;        // 1
       // 1 byte trailing padding
   };                        // 16 bytes
   ```
   **Half size!** Yeh cache mein 2x zyada orders fit karega.
   </details>

3. `min()` vs `lowest()` bug reproduce karo:
   ```cpp
   double values[] = {-100.0, -50.0, -200.0};
   double minVal = std::numeric_limits<double>::min();
   for (double v : values) if (v < minVal) minVal = v;
   std::cout << minVal;      // sahi answer -200 hona chahiye
   ```

4. Array decay demonstrate karo:
   ```cpp
   void printSize(int arr[]) { std::cout << sizeof(arr) << "\n"; }
   int main() {
       int a[100];
       std::cout << sizeof(a) << "\n";
       printSize(a);
   }
   ```

5. `static_assert` se ek protocol struct verify karo:
   ```cpp
   #pragma pack(push, 1)
   struct Msg { std::uint8_t t; std::uint64_t ts; std::int32_t p; };
   #pragma pack(pop)
   static_assert(sizeof(Msg) == 13);
   static_assert(offsetof(Msg, ts) == 1);
   ```

6. Cache-line alignment test:
   ```cpp
   struct Normal { std::uint64_t counter; };
   struct alignas(64) Aligned { std::uint64_t counter; };
   std::cout << sizeof(Normal) << " " << sizeof(Aligned) << "\n";
   ```

---

## Interview questions

1. `sizeof` compile time pe hota hai ya runtime pe?
2. `sizeof(i++)` ke baad `i` badalta hai?
3. Function parameter mein `sizeof(arr)` kya deta hai aur kyun?
4. `sizeof(EmptyStruct)` kya hai aur kyun?
5. `numeric_limits<double>::min()` aur `lowest()` mein fark?
6. Struct padding kya hai? Use kaise minimize karein?
7. `alignas(64)` kyun use karte hain?

---

## Next
→ [`15-naming-and-style.md`](15-naming-and-style.md)
