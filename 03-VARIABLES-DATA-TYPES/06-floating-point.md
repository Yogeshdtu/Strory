# 06 — Floating point (`float`, `double`)

## Prerequisites
`05-int-deep-dive.md`, folder 01 lesson 10 (binary)

## Yeh topic abhi kyun
Decimal numbers store karne hain. Lekin floating point **sabse zyada surprise dene wala**
type hai — `0.1 + 0.2 != 0.3` hota hai, aur `==` se compare karna galat hai.

Aur finance mein yeh **paisa doobane wali** cheez hai. Isliye dhyaan se.

---

## Basic

```cpp
float  a = 3.14f;      // 4 bytes, ~7 decimal digits precision
double b = 3.14159;    // 8 bytes, ~15-16 digits  ⭐ DEFAULT
long double c = 3.14L; // 8/12/16 bytes (platform pe depend)
```

| Type | Size | Precision | Range (approx) |
|---|---|---|---|
| `float` | 4 bytes | ~7 digits | ±3.4 × 10³⁸ |
| `double` | 8 bytes | ~15-16 digits | ±1.8 × 10³⁰⁸ |
| `long double` | 8-16 bytes | platform-dependent | bahut bada |

**Default kya use karein? `double`.** `float` sirf tab jab memory ya SIMD width matter kare.

---

## Literals

```cpp
3.14        // double (DEFAULT)
3.14f       // float (f suffix)
3.14F       // float
3.14L       // long double
1e6         // 1000000.0 (double)
1.5e-3      // 0.0015
1'000.5     // digit separator (C++14)
```

**⚠️ Common galti:**
```cpp
float x = 3.14;     // 3.14 double hai, phir float mein convert -- precision loss
float y = 3.14f;    // ✅ seedha float
```

---

## Andar kya hota hai: IEEE-754

Floating point **scientific notation** ki tarah store hota hai, par binary mein:

```
   value = sign × mantissa × 2^exponent
```

### `double` ka layout (64 bits)

```
   +---+-----------+----------------------------------------------------+
   | S |  EXPONENT |                    MANTISSA                        |
   +---+-----------+----------------------------------------------------+
     1      11                            52                    = 64 bits
```

- **Sign (1 bit):** 0 = positive, 1 = negative
- **Exponent (11 bits):** kitna shift karna hai (bias 1023 ke saath)
- **Mantissa (52 bits):** actual digits

### `float` ka layout (32 bits)
```
   +---+----------+-----------------------+
   | S | EXPONENT |       MANTISSA        |
   +---+----------+-----------------------+
     1      8               23              = 32 bits
```

---

## 🔴 THE BIG PROBLEM: sab decimals binary mein exactly nahi bante

Decimal mein `1/3` = `0.3333...` — kabhi exact nahi.

Binary mein **`0.1` kabhi exact nahi banta.**

```
   0.1 in binary = 0.0001100110011001100110011... (infinite repeating)
```

Aur aapke paas sirf 52 bits hain. To woh **kaat diya jaata hai**.

```cpp
#include <iostream>
#include <iomanip>

int main() {
    std::cout << std::setprecision(20);
    std::cout << 0.1 << "\n";
    std::cout << 0.2 << "\n";
    std::cout << 0.1 + 0.2 << "\n";
    std::cout << 0.3 << "\n";
}
```

**Output:**
```
0.10000000000000000555
0.20000000000000001110
0.30000000000000004441      <- 0.1 + 0.2
0.29999999999999998890      <- 0.3
```

**`0.1 + 0.2 != 0.3`** ✅ Yeh bug nahi hai. Yeh **binary floating point ka nature** hai.
Har language mein hota hai — Python, Java, JavaScript, sab.

---

## ⚠️ Isliye `==` se float compare mat karo

```cpp
double a = 0.1 + 0.2;
if (a == 0.3) {                    // ❌ FALSE hoga!
    std::cout << "barabar\n";
}
```

### Sahi tareeka: epsilon comparison

```cpp
#include <cmath>
#include <limits>

bool nearlyEqual(double a, double b, double epsilon = 1e-9) {
    return std::fabs(a - b) < epsilon;
}

if (nearlyEqual(0.1 + 0.2, 0.3)) {
    std::cout << "kaafi paas hain\n";     // ✅ yeh chalega
}
```

### Better: relative comparison (bade numbers ke liye)

```cpp
bool nearlyEqual(double a, double b,
                 double relEps = 1e-9, double absEps = 1e-12) {
    double diff = std::fabs(a - b);
    if (diff <= absEps) return true;                    // dono ~0 hain
    return diff <= relEps * std::max(std::fabs(a), std::fabs(b));
}
```

**Kyun relative?** Kyunki `1000000.0` aur `1000000.0001` ka absolute fark `0.0001` hai,
jo epsilon `1e-9` se bada hai — par relative fark bahut chhota hai.

---

## Precision loss — accumulation

```cpp
#include <iostream>
#include <iomanip>

int main() {
    double sum = 0.0;
    for (int i = 0; i < 1000000; ++i) {
        sum += 0.1;
    }
    std::cout << std::setprecision(20) << sum << "\n";
    std::cout << "Expected: 100000\n";
}
```

**Output:**
```
100000.00000133288
Expected: 100000
```

Har addition mein thoda error, 10 lakh baar mein woh jud gaya.

### Kahani: Patriot missile (1991)

Gulf War mein Patriot missile system ne ek Scud missile miss kiya. **28 log mare.**

Wajah: system time ko 0.1 second increments mein count karta tha, aur 24-bit fixed
point mein `0.1` ko store karne mein chhota error tha. 100 ghante chalne ke baad woh
error 0.34 second ban gaya. Us waqt mein missile 600 meter chal chuka tha.

**Floating point errors real hote hain.**

---

## 💰 FINANCE: paise ke liye float MAT use karo

```cpp
// ❌ GALAT
double price = 100.10;
double qty = 3;
double total = price * qty;
std::cout << std::setprecision(20) << total;    // 300.29999999999995453

// ✅ SAHI: integer paise (ya ticks)
int64_t priceInPaise = 10010;      // Rs 100.10
int64_t qty = 3;
int64_t totalInPaise = priceInPaise * qty;    // 30030 = Rs 300.30 exactly
```

> **HFT relevance — YEH CRITICAL HAI:**
> HFT systems mein prices **kabhi** floating point mein nahi rakhi jaatin. Do reasons:
>
> **1. Correctness:** Exchange prices discrete ticks mein hote hain
> (e.g. NSE mein 0.05 ka multiple). Integer ticks se exact representation milti hai.
>
> **2. Speed:** Integer arithmetic floating point se tez hai, aur **deterministic** hai.
> Integer add = 1 cycle. Float divide = 15-40 cycles. Aur integer ops mein koi
> denormal/NaN slowdown nahi hota.
>
> Standard practice: price ko **ticks** ya **paise/cents** mein `int64_t` mein rakho.
> Display ke waqt hi decimal mein badlo.
>
> ```cpp
> struct Order {
>     uint64_t orderId;
>     int64_t  priceInTicks;    // ✅ NOT double
>     uint32_t quantity;
> };
> ```

---

## Special values

```cpp
#include <cmath>
#include <limits>

double inf    =  1.0 / 0.0;                                    // infinity
double ninf   = -1.0 / 0.0;                                    // -infinity
double nan    =  0.0 / 0.0;                                    // Not a Number

// Better way
double inf2 = std::numeric_limits<double>::infinity();
double nan2 = std::numeric_limits<double>::quiet_NaN();

// Check karo
std::isnan(nan);      // true
std::isinf(inf);      // true
std::isfinite(1.0);   // true
```

### 🔑 NaN ki ajeeb property

```cpp
double nan = std::numeric_limits<double>::quiet_NaN();

nan == nan;      // FALSE! 😱
nan != nan;      // TRUE
nan < 1.0;       // false
nan > 1.0;       // false
nan == 1.0;      // false
```

**NaN kisi ke barabar nahi hai — apne barabar bhi nahi.**

Yeh actually NaN detect karne ka trick hai:
```cpp
if (x != x) { /* x NaN hai */ }
```

**⚠️ Yeh sorting ko todh deta hai:**
```cpp
std::vector<double> v = {3.0, NAN, 1.0, 2.0};
std::sort(v.begin(), v.end());     // ⚠️ UB! std::sort ko strict weak ordering chahiye
                                    //    NaN woh violate karta hai
```

### Integer division vs float division

```cpp
int x = 5 / 0;          // ⚠️ UB, crash (SIGFPE)
double y = 5.0 / 0.0;   // ✅ inf (defined by IEEE-754)
```

---

## Performance notes

| Operation | Latency (cycles, typical) |
|---|---|
| Integer add | 1 |
| Integer multiply | 3 |
| Float add | 4 |
| Float multiply | 4 |
| Float divide | 15–40 |
| `sqrt` | 15–20 |
| Integer divide | 20–40 |

**Division sabse mehnga hai.** Isliye:

```cpp
// ❌ slow (loop mein division)
for (int i = 0; i < n; ++i) result[i] = arr[i] / 3.0;

// ✅ fast (ek baar reciprocal, phir multiply)
const double inv3 = 1.0 / 3.0;
for (int i = 0; i < n; ++i) result[i] = arr[i] * inv3;
```

### Denormals — ek chhupa hua latency killer

Bahut chhote numbers (`~1e-310`) "denormal" ya "subnormal" ban jaate hain. Kuch CPUs
pe **denormal operations 100x slow** hote hain.

```cpp
// HFT systems mein aksar denormals disable kar dete hain:
// FTZ (Flush To Zero) aur DAZ (Denormals Are Zero) flags set karke
#include <xmmintrin.h>
_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
```

Folder 31/36 mein detail.

---

## `-ffast-math` — powerful par khatarnaak

```bash
g++ -O2 -ffast-math file.cpp -o file
```

Yeh compiler ko batata hai: "float arithmetic ko real math ki tarah treat karo" —
matlab reordering, associativity assume karna, NaN/inf ignore karna.

**Faayda:** 2-5x speedup ho sakta hai (vectorization possible ho jaati hai).

**Nuksaan:**
- `x + 0.0 == x` assume karta hai (NaN ke liye galat)
- `(a + b) + c == a + (b + c)` assume karta hai (float mein galat!)
- `std::isnan()` toot sakta hai
- Results non-reproducible ho sakte hain

**HFT mein:** Aksar **nahi** use hota, kyunki reproducibility chahiye. Agar use bhi
karein to sirf specific functions pe, poore program pe nahi.

---

## Printing floats

```cpp
#include <iomanip>

double pi = 3.14159265358979;

std::cout << pi << "\n";                              // 3.14159 (default 6 digits)
std::cout << std::setprecision(10) << pi << "\n";     // 3.141592654
std::cout << std::fixed << std::setprecision(2) << pi << "\n";     // 3.14
std::cout << std::scientific << pi << "\n";           // 3.141593e+00
std::cout << std::defaultfloat;                       // reset

// C++20: std::format (behtar)
#include <format>
std::cout << std::format("{:.2f}\n", pi);             // 3.14
```

---

## Hands-on

`examples/04_float_traps.cpp` chalao:

```bash
cd examples
g++ -std=c++20 -Wall -Wextra 04_float_traps.cpp -o traps && ./traps
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`0.1 + 0.2 == 0.3`" | ❌ False. Binary mein 0.1 exact nahi banta |
| "Yeh C++ ka bug hai" | Har language mein hota hai — IEEE-754 ki nature hai |
| "`float` use karo, memory bachegi" | `double` default use karo. `float` sirf specific cases mein |
| "Paise ke liye `double` theek hai" | ❌ **Kabhi nahi.** Integer paise/ticks use karo |
| "`NaN == NaN` true hai" | ❌ False. NaN apne barabar bhi nahi |
| "`5.0/0.0` crash karega" | Nahi, `inf` deta hai. Integer division crash karti hai |
| "`setprecision` value badalta hai" | Sirf **display** badalta hai |

---

## Exercises

1. Yeh chalao aur samjho:
   ```cpp
   #include <iostream>
   #include <iomanip>
   int main() {
       std::cout << std::setprecision(20);
       std::cout << 0.1 << "\n" << 0.2 << "\n" << 0.1 + 0.2 << "\n" << 0.3 << "\n";
       std::cout << (0.1 + 0.2 == 0.3) << "\n";
   }
   ```

2. `nearlyEqual` function likho aur test karo.

3. Accumulation error test:
   ```cpp
   float sumF = 0.0f;
   double sumD = 0.0;
   for (int i = 0; i < 1000000; ++i) { sumF += 0.1f; sumD += 0.1; }
   std::cout << std::setprecision(20) << sumF << "\n" << sumD << "\n";
   ```
   `float` aur `double` mein kitna fark aaya?

4. NaN properties test karo:
   ```cpp
   double n = std::numeric_limits<double>::quiet_NaN();
   std::cout << (n == n) << " " << (n != n) << " " << (n < 1) << " " << (n > 1) << "\n";
   ```

5. **Financial bug fix karo:**
   ```cpp
   double price = 0.1;
   double total = 0;
   for (int i = 0; i < 10; ++i) total += price;
   if (total == 1.0) std::cout << "Exactly Rs 1\n";
   else std::cout << "Rs 1 nahi hai! total = " << std::setprecision(20) << total << "\n";
   ```
   Ise integer paise se rewrite karo.
   <details><summary>Answer</summary>

   ```cpp
   int64_t priceInPaise = 10;     // 0.10 rupaye
   int64_t total = 0;
   for (int i = 0; i < 10; ++i) total += priceInPaise;
   if (total == 100) std::cout << "Exactly Rs 1\n";     // ✅ chalega
   ```
   </details>

6. Division optimization test:
   ```cpp
   // Ek loop division ke saath, ek reciprocal multiply ke saath.
   // 10 million iterations. Time compare karo.
   ```

7. `sizeof` check: `float`, `double`, `long double` — aapke system pe kitne bytes?

---

## Interview questions

1. `0.1 + 0.2 == 0.3` kya deta hai aur kyun?
2. Floats ko compare kaise karte hain?
3. NaN kya hai? `NaN == NaN` kya deta hai?
4. Finance mein `double` kyun use nahi karte?
5. `float` aur `double` mein kab kaunsa use karein?
6. IEEE-754 mein `double` ka layout batao.

---

## Next
→ [`07-char-and-ascii.md`](07-char-and-ascii.md)
