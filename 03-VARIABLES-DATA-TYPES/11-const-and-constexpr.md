# 11 — `const` aur `constexpr`

## Prerequisites
`10-initialization-forms.md`

## Yeh topic abhi kyun
`const` C++ ka sabse under-used feature hai — aur sabse zyada bugs bachane wala.
`constexpr` compile-time computation deta hai, jo **zero-cost abstraction** ka core hai
aur HFT mein bahut use hota hai.

---

## `const` — "yeh badlega nahi"

```cpp
const int MAX_ORDERS = 1000;
MAX_ORDERS = 2000;              // ❌ COMPILE ERROR
```

`const` ek **promise** hai compiler se: "main isko kabhi nahi badlunga."

**`const` variable ko initialize karna ZAROORI hai:**
```cpp
const int x;            // ❌ error: uninitialized const
const int y = 5;        // ✅
const int z{5};         // ✅
```

---

## `const` kyun use karein?

### 1. Bugs pakadta hai
```cpp
const double PI = 3.14159;
PI = 3.0;                       // ❌ compiler turant rok dega
```

### 2. Intent batata hai
```cpp
void processOrder(const Order& order);      // "main order badlunga nahi"
void modifyOrder(Order& order);             // "main order badal sakta hoon"
```

Function ka signature dekh ke hi pata chal jaata hai.

### 3. Optimization enable karta hai
Compiler jab jaanta hai ki value nahi badlegi, woh use register mein rakh sakta hai,
ya inline kar sakta hai.

### 4. Magic numbers hataata hai
```cpp
// ❌ magic numbers
if (orderCount > 1000) { }
buffer[1000];

// ✅ named constants
const int MAX_ORDERS = 1000;
if (orderCount > MAX_ORDERS) { }
buffer[MAX_ORDERS];
```

---

## `const` pointers — 3 variations (interview favourite)

Yeh confusing hai. Trick: **`const` uske left wali cheez pe apply hota hai.
Agar left mein kuch nahi hai, to right wali pe.**

```cpp
int value = 10;
int other = 20;

// 1. POINTER TO CONST -- data const hai, pointer nahi
const int* p1 = &value;
// int const* p1 = &value;     // same cheez
*p1 = 20;        // ❌ data nahi badal sakte
p1 = &other;     // ✅ pointer badal sakte hain

// 2. CONST POINTER -- pointer const hai, data nahi
int* const p2 = &value;
*p2 = 20;        // ✅ data badal sakte hain
p2 = &other;     // ❌ pointer nahi badal sakte

// 3. CONST POINTER TO CONST -- dono const
const int* const p3 = &value;
*p3 = 20;        // ❌
p3 = &other;     // ❌
```

### Padhne ka trick: **right to left padho**

```
   const int* p         ->  p is a pointer to an int which is const
   int* const p         ->  p is a const pointer to an int
   const int* const p   ->  p is a const pointer to a const int
```

**Ya "spiral rule"** — `*` se shuru karo, phir bahar ki taraf jao.

Detail folder 12 mein.

---

## `const` member functions (preview)

```cpp
class Order {
    int price_;
public:
    int getPrice() const { return price_; }     // ✅ object nahi badlega
    void setPrice(int p) { price_ = p; }        // object badal sakta hai
};
```

`const` member function `const` object pe bhi call ho sakta hai:
```cpp
const Order o;
o.getPrice();       // ✅
o.setPrice(100);    // ❌ error
```

**Rule:** Jo function object nahi badalta, use `const` banao. Detail folder 15 mein.

---

## `constexpr` — compile time pe compute karo

```cpp
constexpr int MAX = 100;                 // compile time constant
constexpr int DOUBLE_MAX = MAX * 2;      // compile time pe calculate hua
```

**Fark kya hai `const` se?**

| | `const` | `constexpr` |
|---|---|---|
| Matlab | "runtime pe badlega nahi" | "**compile time** pe pata hai" |
| Value kab pata chalti hai | runtime pe bhi ho sakti hai | **compile time pe** |
| Array size ke liye use ho sakta? | sirf agar compile-time constant ho | ✅ hamesha |
| Template argument ban sakta? | sirf agar compile-time constant ho | ✅ hamesha |

```cpp
int runtimeValue = getUserInput();

const int a = runtimeValue;          // ✅ OK -- const runtime value ho sakti hai
constexpr int b = runtimeValue;      // ❌ ERROR -- compile time pe pata nahi

int arr1[a];                         // ❌ error (VLA C++ mein nahi hai)
int arr2[b];                         // ✅ (agar b constexpr hota)
```

---

## `constexpr` functions

```cpp
constexpr int square(int x) {
    return x * x;
}

constexpr int a = square(5);      // ✅ COMPILE TIME pe calculate hua -> 25
int n = 5;
int b = square(n);                // ✅ RUNTIME pe calculate hua
```

**`constexpr` function dono jagah kaam karta hai** — agar arguments compile time pe
pata hon to compile time pe, warna runtime pe.

### Verify karo ki compile time pe hua

```cpp
constexpr int factorial(int n) {
    return (n <= 1) ? 1 : n * factorial(n - 1);
}

int main() {
    constexpr int f10 = factorial(10);      // compile time pe
    static_assert(f10 == 3628800);          // compile time pe check
    return f10;
}
```

```bash
g++ -O2 -S file.cpp -o - | grep -A5 main
```
Aapko `3628800` **literally** assembly mein dikhega. Koi calculation runtime pe nahi hui.

---

## `consteval` (C++20) — "compile time pe HI"

```cpp
consteval int mustBeCompileTime(int x) {
    return x * 2;
}

constexpr int a = mustBeCompileTime(5);      // ✅
int n = 5;
int b = mustBeCompileTime(n);                // ❌ ERROR -- runtime pe call nahi kar sakte
```

`constexpr` = "compile time pe **ho sakta hai**"
`consteval` = "compile time pe **hona hi chahiye**"

---

## `constinit` (C++20) — static initialization order fiasco ka fix

```cpp
constinit int globalCounter = 42;      // static initialization guaranteed
```

Yeh guarantee karta hai ki variable **static initialization** ke time initialize hoga
(compile time pe), runtime dynamic initialization se nahi.

Yeh "static initialization order fiasco" se bachata hai (folder 25 mein).

**Note:** `constinit` variable ko `const` nahi banata — sirf initialization ke baare
mein hai.

---

## 🔴 HFT relevance

### 1. Compile-time lookup tables

```cpp
// Compile time pe table banao -- runtime pe zero cost
constexpr std::array<int, 256> makeLookupTable() {
    std::array<int, 256> table{};
    for (int i = 0; i < 256; ++i) {
        table[i] = i * i;      // ya koi bhi expensive calculation
    }
    return table;
}

constexpr auto SQUARES = makeLookupTable();

// Runtime pe: bas ek array lookup. Koi calculation nahi.
int result = SQUARES[value];
```

**Yeh binary mein already-computed data ke roop mein jaata hai.** Runtime cost: ek
memory read (aur woh bhi cache mein garam rahega).

### 2. Compile-time validation

```cpp
struct MarketDataMessage {
    std::uint8_t  type;
    std::uint64_t timestamp;
    std::int32_t  price;
};

static_assert(sizeof(MarketDataMessage) == 16, "Layout badal gaya!");
static_assert(alignof(MarketDataMessage) == 8);
static_assert(std::is_trivially_copyable_v<MarketDataMessage>);
```

Agar koi developer struct badle, **build fail hoga** — production mein bug nahi jaayega.

### 3. `const` correctness se bugs bachte hain

```cpp
// Hot path mein order book ko modify nahi karna
void strategy(const OrderBook& book) {
    // book.addOrder(...);      // ❌ compile error -- accidentally modify nahi kar sakte
    auto bid = book.getBestBid();     // ✅ const method
}
```

### 4. Zero-overhead constants

```cpp
constexpr std::size_t MAX_ORDERS = 1'000'000;
constexpr std::size_t CACHE_LINE = 64;

alignas(CACHE_LINE) std::array<Order, MAX_ORDERS> orderPool;
```

Sab compile time pe — koi runtime allocation nahi, koi runtime computation nahi.

---

## `#define` vs `const`/`constexpr`

```cpp
#define MAX 100                 // ❌ purana C style
const int MAX = 100;            // ✅ better
constexpr int MAX = 100;        // ✅ best
```

| | `#define` | `constexpr` |
|---|---|---|
| Type safety | ❌ nahi (sirf text) | ✅ haan |
| Scope | ❌ file ke end tak, sab jagah | ✅ normal scoping |
| Debugger mein dikhta? | ❌ nahi | ✅ haan |
| Namespace mein daal sakte? | ❌ nahi | ✅ haan |

### `#define` ke classic bugs

```cpp
#define SQUARE(x) x * x
int a = SQUARE(2 + 3);          // 2 + 3 * 2 + 3 = 11, not 25! 😱

#define MAX(a, b) ((a) > (b) ? (a) : (b))
int i = 5;
int m = MAX(i++, 10);           // i do baar increment ho sakta hai!
```

**Modern C++ mein `#define` sirf yeh cheezon ke liye:**
- Include guards
- Conditional compilation (`#ifdef DEBUG`)
- Platform detection

---

## Where to put constants

```cpp
// header.h
#pragma once

constexpr int MAX_ORDERS = 1000;        // ✅ constexpr implicitly inline hai
                                        //    ODR violation nahi hogi

inline constexpr int MAX_2 = 1000;      // ✅ explicit (C++17), same cheez

const int MAX_3 = 1000;                 // ✅ const at namespace scope has
                                        //    internal linkage -- har TU mein
                                        //    apni copy (memory waste, par legal)

int MAX_4 = 1000;                       // ❌ ODR violation! multiple definition
```

---

## Hands-on

`examples/08_const_constexpr.cpp` chalao:

```bash
cd examples
g++ -std=c++20 -Wall -Wextra 08_const_constexpr.cpp -o cc && ./cc

# Verify karo ki constexpr compile time pe hua
g++ -std=c++20 -O2 -S 08_const_constexpr.cpp -o - | grep 3628800
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`const` aur `constexpr` same hain" | `constexpr` compile time pe pata hai, `const` sirf immutable |
| "`const int* p` mein pointer const hai" | Nahi, **data** const hai |
| "`constexpr` function hamesha compile time pe chalta hai" | Nahi — arguments pe depend karta hai. `consteval` mandatory hai |
| "`#define` aur `const` same hain" | `#define` text replacement hai, koi type safety nahi |
| "`const` performance kharab karta hai" | Ulta — optimization enable karta hai |

---

## Exercises

1. Yeh declarations padho aur batao kya const hai:
   ```cpp
   a) const int* p;
   b) int* const p;
   c) const int* const p;
   d) int const* p;
   e) const int& r;
   ```
   <details><summary>Answers</summary>
   a) data const (pointer badal sakta hai)
   b) pointer const (data badal sakta hai)
   c) dono const
   d) same as (a) — `const int` aur `int const` same hain
   e) reference to const int (reference khud hamesha const hoti hai)
   </details>

2. `constexpr` factorial likho aur `static_assert` se verify karo:
   ```cpp
   constexpr int factorial(int n) { /* ... */ }
   static_assert(factorial(5) == 120);
   ```

3. Assembly mein verify karo ki calculation compile time pe hui:
   ```bash
   g++ -O2 -S yourfile.cpp -o - | grep 120
   ```

4. Yeh code fix karo:
   ```cpp
   const int size;
   size = 10;
   int arr[size];
   ```
   <details><summary>Answer</summary>

   ```cpp
   constexpr int size = 10;
   int arr[size];      // ✅
   ```
   `const` uninitialized nahi ho sakta, aur array size ke liye compile-time constant chahiye.
   </details>

5. `#define SQUARE(x) x * x` ka bug reproduce karo, phir `constexpr` function se fix karo.

6. Compile-time lookup table banao:
   ```cpp
   constexpr std::array<int, 10> makeSquares() {
       std::array<int, 10> a{};
       for (int i = 0; i < 10; ++i) a[i] = i * i;
       return a;
   }
   constexpr auto SQUARES = makeSquares();
   static_assert(SQUARES[5] == 25);
   ```

7. `static_assert` se ek struct ki size verify karo. Phir struct mein ek member add
   karo — build fail hua?

---

## Interview questions

1. `const` aur `constexpr` mein kya fark hai?
2. `const int* p` aur `int* const p` mein fark?
3. `constexpr` function runtime pe chal sakta hai?
4. `consteval` kya hai?
5. `#define` ki jagah `constexpr` kyun better hai?
6. `const` member function kya hai?

---

## Next
→ [`12-auto-and-type-deduction.md`](12-auto-and-type-deduction.md)
