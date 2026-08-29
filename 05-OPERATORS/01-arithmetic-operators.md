# 01 — Arithmetic operators

## Prerequisites
Folder 03 (types, `int` deep dive, floating point)

## Yeh topic abhi kyun
Aapne variables bana liye, I/O aa gaya. Ab un values pe **kaam** karenge.

Arithmetic simple lagti hai — par usme 4 traps hain jo production bugs banate hain.

---

## Paanch basic operators

```cpp
int a = 10, b = 3;

a + b       // 13   addition
a - b       // 7    subtraction
a * b       // 30   multiplication
a / b       // 3    division    ⚠️ integer division!
a % b       // 1    modulo (remainder)
```

Aur unary:
```cpp
+a          // 10   unary plus (integer promotion karta hai)
-a          // -10  unary minus (negation)
```

---

## ⚠️ TRAP 1: Integer division

```cpp
std::cout << 7 / 2;         // 3, not 3.5!
```

**Do integers ka division hamesha integer deta hai.** Decimal part **truncate**
ho jaata hai — round nahi hota, **zero ki taraf** kat jaata hai.

```cpp
 7 / 2      //  3   (3.5 -> 3)
 9 / 2      //  4   (4.5 -> 4)
-7 / 2      // -3   (-3.5 -> -3, NOT -4)
 1 / 2      //  0
```

### Decimal chahiye to?

```cpp
int a = 7, b = 2;

a / b                            // 3     ❌
7.0 / 2                          // 3.5   ✅ (ek double hai to double division)
static_cast<double>(a) / b       // 3.5   ✅
(double)a / b                    // 3.5   ⚠️ C-style cast, avoid
a / static_cast<double>(b)       // 3.5   ✅
static_cast<double>(a / b)       // 3.0   ❌ BAHUT LATE! division pehle ho gaya
```

**⚠️ Last wala classic bug hai** — cast division ke **baad** lagaya, isliye kaam nahi kiya.

### THE CLASSIC BUG

```cpp
int correct = 45;
int total = 60;
double percentage = correct / total * 100;      // ⚠️ 0!
```

**Kya hua:**
```
   correct / total  ->  45 / 60  ->  0    (integer division!)
   0 * 100          ->  0
   0 ko double mein ->  0.0
```

**Fix:**
```cpp
double percentage = static_cast<double>(correct) / total * 100;    // ✅ 75
// ya
double percentage = correct * 100.0 / total;                       // ✅ 75
```

---

## ⚠️ TRAP 2: Modulo aur negative numbers

```cpp
 7 %  3      //  1
-7 %  3      // -1   ⚠️ (Python mein 2 aata hai!)
 7 % -3      //  1
-7 % -3      // -1
```

**Rule (C++11 se guaranteed):**
```
   (a / b) * b + (a % b) == a
```

Aur `%` ka sign **dividend** (left operand) ka hota hai.

### Yeh kab bug banta hai?

```cpp
// Circular buffer index -- ⚠️ BUG agar index negative ho
int index = (current - 1) % size;      // agar current = 0, size = 10
                                       // -> -1 % 10 = -1  ❌ out of bounds!

// ✅ Fix
int index = ((current - 1) % size + size) % size;      // -> 9

// ✅ Ya positive rakhо
int index = (current + size - 1) % size;               // -> 9
```

Yeh ring buffers mein bahut common bug hai (folder 28/36 mein relevant).

### `%` sirf integers ke liye

```cpp
7.5 % 2;                  // ❌ COMPILE ERROR
std::fmod(7.5, 2.0);      // ✅ 1.5  (<cmath>)
std::remainder(7.5, 2.0); // ✅ -0.5 (IEEE remainder, alag semantics)
```

---

## ⚠️ TRAP 3: Division by zero

```cpp
int a = 5 / 0;            // ⚠️ UNDEFINED BEHAVIOUR -- usually crash (SIGFPE)
int b = 5 % 0;            // ⚠️ UB bhi

double c = 5.0 / 0.0;     // ✅ inf     (IEEE-754 mein DEFINED)
double d = -5.0 / 0.0;    // ✅ -inf
double e = 0.0 / 0.0;     // ✅ NaN
```

**Integer division by zero = UB (crash).**
**Float division by zero = defined (inf/NaN).**

Yeh fark yaad rakho.

```cpp
// ✅ Hamesha check karo
if (divisor != 0) {
    result = dividend / divisor;
} else {
    // handle error
}
```

---

## ⚠️ TRAP 4: Overflow

Folder 03 file 05 se recap:

```cpp
int a = 2000000000;
int b = 2000000000;
int c = a + b;            // ⚠️ SIGNED OVERFLOW = UNDEFINED BEHAVIOUR

unsigned int x = 4294967295u;
unsigned int y = x + 1;   // ✅ 0 (wrap around -- DEFINED)
```

### Safe arithmetic

```cpp
// Tareeka 1: bada type
std::int64_t safe = static_cast<std::int64_t>(a) + b;

// Tareeka 2: compiler builtins
int result;
if (__builtin_add_overflow(a, b, &result)) {
    // overflow hua
}
// Bhi hain: __builtin_sub_overflow, __builtin_mul_overflow

// Tareeka 3: pehle check karo
if (a > 0 && b > std::numeric_limits<int>::max() - a) {
    // overflow hoga
}
```

> **HFT relevance:** Order quantities, position sizes, notional values — sab
> arithmetic hain. Ek overflow se aap 2 arab shares ka order bhej sakte ho.
> Isliye risk systems mein har calculation bounds-checked hoti hai, ya `int64_t`
> use hota hai jahan overflow practically impossible ho.

---

## Mixed-type arithmetic

Jab do alag types ke saath arithmetic hoti hai, **usual arithmetic conversions**
lagti hain (folder 03 file 13):

```cpp
int i = 5;
double d = 2.0;
auto r1 = i + d;          // double (5.0 + 2.0 = 7.0)

char c = 100;
auto r2 = c + c;          // int! (integer promotion) -> 200

unsigned u = 1;
int neg = -1;
auto r3 = neg + u;        // unsigned! -> 0 (kyunki -1 + 1, par unsigned mein)
                          // ⚠️ dangerous
```

### Promotion table (simplified)
```
   1. Koi long double hai?  -> sab long double
   2. Koi double hai?       -> sab double
   3. Koi float hai?        -> sab float
   4. INTEGER PROMOTION (char/short -> int)
   5. Signed/unsigned rules -> ⚠️ signed aksar unsigned ban jaata hai
```

---

## Floating point arithmetic

```cpp
double a = 0.1, b = 0.2;
std::cout << (a + b == 0.3);      // 0 (false!) -- folder 03 file 06 se yaad hai?
```

**Aur:**
```cpp
// Associativity guaranteed NAHI hai floating point mein
(a + b) + c    !=    a + (b + c)      // ho sakta hai
```

Isliye compiler `-ffast-math` ke bina float operations reorder **nahi** kar sakta.

### `<cmath>` functions

```cpp
#include <cmath>

std::abs(-5);           // 5    (int)
std::fabs(-5.5);        // 5.5  (double)
std::pow(2, 10);        // 1024
std::sqrt(16.0);        // 4.0
std::cbrt(27.0);        // 3.0
std::floor(3.7);        // 3.0
std::ceil(3.2);         // 4.0
std::round(3.5);        // 4.0
std::trunc(3.7);        // 3.0
std::fmod(7.5, 2.0);    // 1.5
std::hypot(3.0, 4.0);   // 5.0  (overflow-safe sqrt(x²+y²))
```

### Performance costs (approximate cycles)

| Operation | Integer | Float |
|---|---|---|
| add / sub | 1 | 4 |
| multiply | 3 | 4 |
| **divide** | **20–40** | **15–40** |
| sqrt | — | 15–20 |
| pow | — | 50–100+ |

**Division sabse mehngi hai.** Isliye:

```cpp
// ❌ Loop mein division
for (int i = 0; i < n; ++i) result[i] = arr[i] / 3.0;

// ✅ Ek baar reciprocal, phir multiply
const double inv3 = 1.0 / 3.0;
for (int i = 0; i < n; ++i) result[i] = arr[i] * inv3;
```

```cpp
// ❌ Power-of-2 division
int half = x / 2;

// ✅ Compiler yeh khud kar deta hai, par samajhna zaroori hai
int half = x >> 1;        // sirf UNSIGNED ke liye safe!
                          // signed negative numbers mein alag behaviour
```

**Note:** Modern compilers `-O2` pe yeh optimizations khud karte hain. Par jab
divisor **runtime variable** ho, tab nahi kar sakte — tab aapko sochna padta hai.

---

## Hands-on

`examples/01_arithmetic.cpp` chalao.

```bash
cd examples
g++ -std=c++20 -Wall -Wextra 01_arithmetic.cpp -o arith && ./arith
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`7/2 = 3.5`" | Integer division = 3 |
| "`-7/2 = -4`" | −3 (zero ki taraf truncate) |
| "`-7 % 3 = 2`" | −1 (C++ mein; Python mein 2) |
| "`5/0` inf deta hai" | Integer mein **UB/crash**. Sirf float mein inf |
| "Overflow pe error aata hai" | Signed = UB (silent), unsigned = wrap |
| "`(double)(a/b)` kaam karega" | ❌ Division pehle ho chuka. Cast **pehle** lagao |

---

## Exercises

1. Predict karo:
   ```cpp
   std::cout << 7/2 << " " << 7%2 << " " << -7/2 << " " << -7%2 << " ";
   std::cout << 7.0/2 << " " << 7/2.0 << "\n";
   ```
   <details><summary>Answer</summary>`3 1 -3 -1 3.5 3.5`</details>

2. Yeh bug fix karo (3 alag tareekon se):
   ```cpp
   int passed = 45, total = 60;
   double pct = passed / total * 100;
   ```

3. Circular buffer index bug reproduce karo aur fix karo:
   ```cpp
   int size = 10;
   for (int current = 0; current < 3; ++current) {
       int prev = (current - 1) % size;
       std::cout << "current=" << current << " prev=" << prev << "\n";
   }
   ```
   <details><summary>Answer</summary>
   `current=0` pe `prev=-1` ❌ (out of bounds).
   Fix: `int prev = (current + size - 1) % size;` → 9
   </details>

4. Overflow safely handle karo:
   ```cpp
   int a = 2000000000, b = 2000000000;
   // Sum safely calculate karo -- 3 tareeke
   ```

5. Division optimization benchmark:
   ```cpp
   // 10 million elements
   // Version 1: arr[i] / 3.0
   // Version 2: arr[i] * (1.0/3.0)
   // Timing compare karo (-O2 ke saath!)
   ```

6. `%` `fmod` se kaise alag hai? Test karo:
   ```cpp
   std::cout << (-7 % 3) << " " << std::fmod(-7.0, 3.0) << "\n";
   ```

7. UBSan se signed overflow pakdo:
   ```bash
   g++ -std=c++20 -fsanitize=undefined -g overflow.cpp -o ovf && ./ovf
   ```

---

## Interview questions

1. `7 / 2` kya deta hai aur kyun?
2. `-7 % 3` kya deta hai? Python se fark kyun?
3. Integer aur float division by zero mein kya fark?
4. Signed overflow UB kyun hai?
5. Division optimization ke tareeke batao.

---

## Next
→ [`02-increment-decrement.md`](02-increment-decrement.md)
