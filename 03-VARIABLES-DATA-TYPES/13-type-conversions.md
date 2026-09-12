# 13 — Type conversions

## Prerequisites
`05-int-deep-dive.md`, `06-floating-point.md`, `10-initialization-forms.md`

## Yeh topic abhi kyun
C++ **chupke se** types convert kar deta hai. Yeh convenient hai, par bugs ka sabse
bada source bhi hai — silent data loss, sign flips, precision loss.

Yeh file aapko bataayegi kab conversion hoti hai, kya galat ho sakta hai, aur kaise
control karein.

---

## Do type ki conversions

| Type | Kaun karta hai | Example |
|---|---|---|
| **Implicit** | compiler, apne aap | `double d = 5;` |
| **Explicit** | aap, cast se | `int i = static_cast<int>(3.7);` |

---

## Implicit conversions — kab hoti hain?

### 1. Assignment mein
```cpp
double d = 5;             // int -> double
int i = 3.7;              // double -> int (3, .7 GAYA)
```

### 2. Function arguments mein
```cpp
void func(double x);
func(5);                  // int -> double
```

### 3. Return mein
```cpp
double getValue() {
    return 5;             // int -> double
}
```

### 4. Mixed arithmetic mein
```cpp
int i = 5;
double d = 2.5;
auto result = i + d;      // i -> double, result double hai (7.5)
```

---

## Usual arithmetic conversions (rules)

Jab do alag types ke saath arithmetic hoti hai, compiler ek "common type" chunta hai:

```
   1. Agar koi long double hai      -> dono long double
   2. Warna agar koi double hai      -> dono double
   3. Warna agar koi float hai       -> dono float
   4. Warna INTEGER PROMOTION karo (chhote types -> int)
   5. Phir:
      a) Agar dono same signedness   -> bade rank wale mein convert
      b) Agar unsigned ka rank >= signed ka rank -> SIGNED ko UNSIGNED mein  ⚠️
      c) Agar signed type unsigned ki poori range hold kar sakta -> unsigned ko signed mein
      d) Warna -> dono ko signed ke unsigned version mein  ⚠️
```

**Step 5b aur 5d hi bugs ka source hain.**

---

## Integer promotion

Chhote types arithmetic mein **automatically `int` ban jaate hain**:

```cpp
char a = 100, b = 100;
auto c = a + b;                   // c ka type INT hai, char nahi
std::cout << c;                   // 200 (char mein 200 nahi aata!)
std::cout << sizeof(a + b);       // 4

short s = 1;
std::cout << sizeof(s);           // 2
std::cout << sizeof(s + s);       // 4  <- promote ho gaya
std::cout << sizeof(+s);          // 4  <- unary + bhi promote karta hai
```

**Kyun?** CPU 32-bit words pe efficiently kaam karti hai.

**Kya promote hota hai:** `bool`, `char`, `signed char`, `unsigned char`, `short`,
`unsigned short`, `char8_t`, `char16_t`, `wchar_t`, unscoped `enum`

---

## ⚠️ Dangerous conversions

### 1. Floating point → Integer: TRUNCATION

```cpp
int a = 3.99;         // 3   (round nahi, TRUNCATE -- zero ki taraf)
int b = -3.99;        // -3  (zero ki taraf, -4 nahi)
int c = 3.5;          // 3
```

**Round karne ke liye:**
```cpp
#include <cmath>
int a = static_cast<int>(std::round(3.5));      // 4
int b = static_cast<int>(std::floor(3.9));      // 3
int c = static_cast<int>(std::ceil(3.1));       // 4
int d = static_cast<int>(std::lround(3.5));     // 4 (long return karta hai)
```

⚠️ **Agar value `int` ki range se bahar hai to UB:**
```cpp
double huge = 1e20;
int x = static_cast<int>(huge);      // ⚠️ UNDEFINED BEHAVIOUR
```

### 2. Bade → Chhote integer: WRAP AROUND

```cpp
int big = 300;
char small = big;             // 44  (300 mod 256)
std::cout << static_cast<int>(small);
```

```cpp
std::int64_t huge = 5000000000;
std::int32_t small = huge;    // ⚠️ implementation-defined (aksar wrap)
```

### 3. Signed ↔ Unsigned: SIGN FLIP

```cpp
int negative = -1;
unsigned int u = negative;    // 4294967295  😱

unsigned int big = 4000000000;
int i = big;                  // ⚠️ implementation-defined (aksar -294967296)
```

### 4. Signed/Unsigned comparison

```cpp
int a = -1;
unsigned int b = 1;
if (a < b) { }                // FALSE! (a unsigned mein convert hoke 4294967295)
```

Yaad hai file 05 se? Yeh sabse common bug hai.

```cpp
std::vector<int> v;
for (int i = 0; i < v.size(); ++i) { }    // ⚠️ warning: sign compare
```

### 5. Precision loss: `int` → `float`

```cpp
int big = 16777217;           // 2^24 + 1
float f = big;                // 16777216  -- 1 kho gaya!
std::cout << (f == big);      // 0 (false!)
```

`float` mein sirf 24 bits mantissa hai — 2^24 se bade integers exactly represent nahi
ho sakte.

`double` mein 53 bits hain, to 2^53 tak safe hai:
```cpp
std::int64_t huge = 9007199254740993LL;   // 2^53 + 1
double d = huge;                           // precision loss!
```

> **HFT relevance:** Order IDs aur timestamps aksar `uint64_t` hote hain. Agar unhe
> `double` mein daal do (e.g. JSON serialization ke liye), to 2^53 se bade IDs corrupt
> ho jaayenge. JavaScript mein yeh classic problem hai (wahan sab numbers `double` hain).

---

## Explicit casts — chaar tarah ke

### 1. `static_cast` — normal conversions ⭐

```cpp
double d = 3.7;
int i = static_cast<int>(d);                    // 3

int a = 7, b = 2;
double avg = static_cast<double>(a) / b;        // 3.5

// Class hierarchy mein up/down cast (safe direction)
Base* b = static_cast<Base*>(derivedPtr);       // upcast (hamesha safe)
Derived* d = static_cast<Derived*>(basePtr);    // downcast (aap guarantee dete ho)

// void* se wapas
void* vp = &someInt;
int* ip = static_cast<int*>(vp);
```

**Yeh 95% cases mein use hota hai.** Compile time pe check hota hai — related types
ke beech hi kaam karta hai.

### 2. `const_cast` — `const` hataane ke liye

```cpp
const int x = 5;
int* p = const_cast<int*>(&x);
*p = 10;                        // ⚠️ UNDEFINED BEHAVIOUR agar x actually const hai
```

**Use case:** Purani C APIs jo `const` nahi leti par actually modify nahi karti.

```cpp
void legacyFunc(char* str);          // const nahi leta, par modify nahi karta
const char* myStr = "hello";
legacyFunc(const_cast<char*>(myStr));    // ⚠️ risky, par kabhi kabhi zaroori
```

**Rule:** Agar aap `const_cast` likh rahe ho, aksar design mein problem hai.

### 3. `reinterpret_cast` — bits ko dobara interpret karo ⚠️

```cpp
int i = 65;
char* c = reinterpret_cast<char*>(&i);      // same bits, alag type

std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(ptr);   // pointer -> integer
```

**Sabse khatarnaak cast.** Yeh compiler ko bolta hai "bas bits ko aise dekh lo."

⚠️ **Strict aliasing violate kar sakta hai** (folder 25 mein detail):
```cpp
float f = 1.0f;
int i = *reinterpret_cast<int*>(&f);        // ⚠️ UB! strict aliasing violation

// ✅ Sahi tareeka
std::int32_t i2;
std::memcpy(&i2, &f, sizeof(f));            // ✅ legal

// ✅ Aur bhi sahi (C++20)
auto i3 = std::bit_cast<std::int32_t>(f);   // ✅ legal, aur constexpr!
```

### 4. `dynamic_cast` — polymorphic downcast (runtime check)

```cpp
Base* b = getPtr();
Derived* d = dynamic_cast<Derived*>(b);
if (d) {
    // conversion successful
} else {
    // b actually Derived nahi tha
}
```

**Runtime pe check karta hai** — RTTI use karta hai, isliye **slow** hai (~50-100 ns).
Folder 16 aur 25 mein detail.

> **HFT relevance:** `dynamic_cast` hot path mein **kabhi nahi**. Woh RTTI lookup
> karta hai — mehnga aur unpredictable. Alternatives: `std::variant`, virtual functions,
> ya CRTP.

---

## ❌ C-style cast — mat use karo

```cpp
int i = (int)3.7;                 // ❌ C-style
int j = int(3.7);                 // ❌ functional style (same cheez)
int k = static_cast<int>(3.7);    // ✅ C++ style
```

**C-style cast kya karta hai?** Woh yeh sab try karta hai, is order mein:
1. `const_cast`
2. `static_cast`
3. `static_cast` + `const_cast`
4. `reinterpret_cast`
5. `reinterpret_cast` + `const_cast`

**Problem:**
- Aapko nahi pata kaunsa hua
- Accidentally `reinterpret_cast` ho sakta hai (khatarnaak)
- Code mein search karna mushkil (`grep static_cast` easy hai, `grep "(int)"` nahi)

**`-Wold-style-cast`** flag se pakdo.

---

## Conversion pakadne ke tareeke

### 1. Warnings ON
```bash
g++ -Wall -Wextra -Wconversion -Wsign-conversion -Wold-style-cast file.cpp
```

`-Wconversion` har implicit narrowing pe warning deta hai. Bahut noisy ho sakta hai,
par bugs pakadta hai.

### 2. Brace initialization
```cpp
int a{3.99};       // ❌ compile error -- narrowing
```

### 3. `static_assert` aur type traits
```cpp
static_assert(sizeof(int) == 4);
static_assert(std::is_same_v<decltype(x), int>);
```

### 4. `explicit` constructors (folder 15)
```cpp
class Price {
public:
    explicit Price(int ticks);      // implicit conversion nahi hogi
};
Price p = 100;        // ❌ error
Price p{100};         // ✅
```

---

## Hands-on

`examples/06_conversions.cpp` chalao:

```bash
cd examples
g++ -std=c++20 -Wall -Wextra 06_conversions.cpp -o conv && ./conv

# Ab -Wconversion ke saath -- kitni warnings aayi?
g++ -std=c++20 -Wall -Wextra -Wconversion -Wsign-conversion 06_conversions.cpp -o conv2
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`int i = 3.7` mein 4 aayega" | 3 — truncate hota hai, round nahi |
| "Signed → unsigned safe hai" | ❌ Negative values wrap ho jaati hain |
| "C-style cast aur `static_cast` same hain" | C-style `reinterpret_cast` bhi kar sakta hai |
| "`reinterpret_cast` safe hai" | ❌ Strict aliasing tod sakta hai. `memcpy`/`bit_cast` use karo |
| "`int` `float` mein exactly fit hota hai" | 2^24 se bade integers nahi |
| "Compiler galat conversion nahi karega" | Karega — silently. Warnings ON rakho |

---

## Exercises

1. Predict karo:
   ```cpp
   std::cout << (int)3.99 << "\n";
   std::cout << (int)-3.99 << "\n";
   std::cout << (char)300 << "\n";
   std::cout << (unsigned)(-1) << "\n";
   std::cout << (int)(unsigned)(-1) << "\n";
   ```
   <details><summary>Answers</summary>
   `3`, `-3`, ASCII 44 = `,`, `4294967295`, `-1`
   </details>

2. Yeh bug fix karo:
   ```cpp
   int correct = 45, total = 60;
   double pct = correct / total * 100;      // 0 aata hai!
   ```

3. Signed/unsigned comparison bug:
   ```cpp
   std::vector<int> v = {1, 2, 3};
   for (int i = 0; i < v.size(); ++i) { }
   ```
   `-Wsign-compare` ke saath compile karo. Warning aayi? 3 alag tareekon se fix karo.
   <details><summary>Answers</summary>

   ```cpp
   for (std::size_t i = 0; i < v.size(); ++i) { }        // 1
   for (const auto& x : v) { }                            // 2 (best)
   for (auto i = 0; i < std::ssize(v); ++i) { }           // 3 (C++20)
   ```
   </details>

4. Float precision test:
   ```cpp
   int big = 16777217;      // 2^24 + 1
   float f = big;
   std::cout << big << " " << f << " " << (f == big) << "\n";
   ```

5. Safe bit reinterpretation:
   ```cpp
   float f = 1.0f;
   // Uske bits ko int ki tarah dekho -- 3 tareekon se:
   // (a) reinterpret_cast (⚠️ UB)
   // (b) memcpy (✅)
   // (c) std::bit_cast (✅ C++20)
   ```

6. `-Wconversion` ke saath apna koi purana program compile karo. Kitni warnings aayi?

7. `explicit` conversion se bacho:
   ```cpp
   void processOrder(std::int64_t priceInTicks);
   double price = 100.5;
   processOrder(price);           // ⚠️ silent truncation -- 100
   ```
   Ise safe banao.

---

## Interview questions

1. Implicit aur explicit conversion mein fark?
2. Integer promotion kya hai?
3. `static_cast` aur `reinterpret_cast` mein fark?
4. C-style cast kyun avoid karna chahiye?
5. `-1 < 1u` false kyun hai?
6. `int` → `float` mein data loss ho sakta hai?
7. `dynamic_cast` kab use karte hain? Uski cost kya hai?

---

## Next
→ [`14-sizeof-and-limits.md`](14-sizeof-and-limits.md)
