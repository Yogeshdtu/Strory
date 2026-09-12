# 03 — Comparison operators

## Prerequisites
`01-arithmetic-operators.md`, folder 03 file 06 (floating point), file 13 (conversions)

## Yeh topic abhi kyun
Conditions (folder 06) aur loops (folder 07) comparisons pe chalte hain. Aur
comparison mein 3 classic traps hain jo silent bugs banate hain.

---

## Six operators

```cpp
a == b      // barabar hai?
a != b      // barabar nahi hai?
a <  b      // chhota hai?
a >  b      // bada hai?
a <= b      // chhota ya barabar?
a >= b      // bada ya barabar?
```

Sab **`bool`** return karte hain — `true` (1) ya `false` (0).

```cpp
std::cout << (5 > 3);                            // 1
std::cout << std::boolalpha << (5 > 3);          // true
```

⚠️ `cout` ke saath **brackets zaroori hain** (folder 04 file 01 se yaad hai?):
```cpp
std::cout << 5 > 3;         // ❌ compile error (precedence)
std::cout << (5 > 3);       // ✅
```

---

## ⚠️ TRAP 1: `=` vs `==`

```cpp
int x = 5;

if (x = 10) { }     // ⚠️ ASSIGNMENT! x ab 10 hai, aur condition TRUE hai
if (x == 10) { }    // ✅ COMPARISON
```

**Yeh compile ho jaata hai.** Sirf warning milti hai:
```bash
g++ -Wall file.cpp
# warning: suggest parentheses around assignment used as truth value
```

### Bachne ke tareeke
```cpp
// 1. Warnings ON (sabse important)
g++ -Wall -Wextra -Werror

// 2. `bool` ke liye seedha likho
if (flag) { }              // ✅ `if (flag == true)` se behtar

// 3. Yoda condition (purana trick, ab zaroori nahi)
if (10 == x) { }           // agar `=` likh do to compile error
```

---

## ⚠️ TRAP 2: Signed/unsigned comparison

```cpp
int a = -1;
unsigned int b = 1;

if (a < b) std::cout << "a chhota\n";
else       std::cout << "a bada?!\n";       // ⚠️ YEH chalega
```

**Kyun?** Signed ko unsigned mein convert kiya jaata hai:
```
   a = -1  ->  unsigned mein  ->  4294967295
   4294967295 < 1 ?  ->  NAHI
```

### Aur bhi khatarnaak: loop
```cpp
std::vector<int> v;
for (int i = 0; i < v.size(); ++i) { }         // ⚠️ warning: sign compare
                                                //    v.size() unsigned hai
```

### Fixes
```cpp
for (std::size_t i = 0; i < v.size(); ++i) { }        // ✅ same type
for (const auto& x : v) { }                            // ✅ best
for (auto i = 0; i < std::ssize(v); ++i) { }           // ✅ C++20 signed size

// C++20: safe comparison functions
#include <utility>
if (std::cmp_less(a, b)) { }                           // ✅ mathematically correct
if (std::cmp_greater(a, b)) { }
if (std::cmp_equal(a, b)) { }
```

**`std::cmp_less` aur family (C++20) sahi answer dete hain** — chahe types alag hon.

```cpp
int a = -1;
unsigned b = 1;
a < b;                       // false  ⚠️ galat
std::cmp_less(a, b);         // true   ✅ sahi
```

### Warnings ON rakho
```bash
g++ -Wall -Wextra -Wsign-compare -Wsign-conversion
```

---

## ⚠️ TRAP 3: Floating point equality

```cpp
double a = 0.1 + 0.2;
if (a == 0.3) { }           // ⚠️ FALSE!
```

Folder 03 file 06 se yaad hai? `0.1` binary mein exact nahi banta.

### Sahi tareeka
```cpp
#include <cmath>
#include <algorithm>

bool nearlyEqual(double a, double b,
                 double relEps = 1e-9, double absEps = 1e-12) {
    const double diff = std::fabs(a - b);
    if (diff <= absEps) return true;                        // dono ~0
    return diff <= relEps * std::max(std::fabs(a), std::fabs(b));
}

if (nearlyEqual(0.1 + 0.2, 0.3)) { }        // ✅ true
```

### NaN ki ajeeb property
```cpp
double nan = std::numeric_limits<double>::quiet_NaN();

nan == nan;      // false! 😱
nan != nan;      // true
nan <  1.0;      // false
nan >  1.0;      // false
nan <= 1.0;      // false

std::isnan(nan); // ✅ true -- yahi use karo
```

**NaN kisi ke barabar nahi — apne barabar bhi nahi.**

⚠️ **Yeh sorting todh deta hai:**
```cpp
std::vector<double> v = {3.0, NAN, 1.0};
std::sort(v.begin(), v.end());      // ⚠️ UB! strict weak ordering violate
```

---

## Chaining ka trap

```cpp
int a = 5, b = 3, c = 1;

if (a > b > c) { }          // ⚠️ Math mein "5 > 3 > 1" par C++ mein NAHI
```

**Kya hota hai:**
```
   (a > b) > c
   (5 > 3) > 1
   true    > 1
   1       > 1
   false
```

**Fix:**
```cpp
if (a > b && b > c) { }     // ✅
```

Python mein chaining kaam karti hai, C++ mein nahi. Yeh Python se aane walon ka
classic bug hai.

---

## Pointers ki comparison

```cpp
int arr[5];
int* p1 = &arr[0];
int* p2 = &arr[3];

p1 == p2;       // ✅ same object point karte hain?
p1 != p2;       // ✅
p1 < p2;        // ✅ SAME ARRAY mein defined hai
p2 - p1;        // ✅ 3 (ptrdiff_t)

// ⚠️ Alag objects ke pointers compare karna
int x, y;
&x < &y;        // ⚠️ UNSPECIFIED (== aur != theek hain)
```

### String comparison ka classic bug
```cpp
const char* a = "hello";
const char* b = "hello";

if (a == b) { }              // ⚠️ POINTERS compare ho rahe hain, content nahi!
                             //    (kabhi kabhi true hoga -- compiler literals merge
                             //     kar deta hai -- par yeh guarantee NAHI hai)

if (std::strcmp(a, b) == 0) { }        // ✅ content comparison (C-strings)

std::string s1 = "hello", s2 = "hello";
if (s1 == s2) { }                       // ✅ std::string mein == content compare karta hai
```

**`std::string` use karo** — usme `==` sahi kaam karta hai.

---

## Three-way comparison `<=>` (C++20)

**Spaceship operator** — ek hi operator se saare comparisons.

```cpp
#include <compare>

int a = 5, b = 3;
auto result = a <=> b;

if (result < 0)       std::cout << "a < b\n";
else if (result > 0)  std::cout << "a > b\n";
else                  std::cout << "a == b\n";
```

### Asli faayda: classes mein

```cpp
struct Point {
    int x, y;

    // Ek line mein SAARE 6 comparison operators mil gaye!
    auto operator<=>(const Point&) const = default;
    bool operator==(const Point&) const = default;
};

Point p1{1, 2}, p2{1, 3};
p1 < p2;      // ✅ works
p1 == p2;     // ✅ works
p1 >= p2;     // ✅ works
```

**Pehle** aapko 6 operators alag-alag likhne padte the. Ab ek line.

### Comparison categories
```cpp
std::strong_ordering     // total order, equal values substitutable (int)
std::weak_ordering       // total order, equal par distinguishable (case-insensitive string)
std::partial_ordering    // kuch values comparable nahi (float -- NaN ki wajah se)
```

`double` ke liye `<=>` `partial_ordering` deta hai — kyunki NaN kisi se comparable
nahi hai.

Detail folder 22 mein.

---

## Hands-on

```bash
cd ~/cpp-practice
cat > compare.cpp << 'END'
#include <iostream>
#include <cmath>
#include <limits>
#include <algorithm>
#include <utility>
#include <cstring>
#include <vector>

bool nearlyEqual(double a, double b, double relEps = 1e-9, double absEps = 1e-12) {
    const double diff = std::fabs(a - b);
    if (diff <= absEps) return true;
    return diff <= relEps * std::max(std::fabs(a), std::fabs(b));
}

int main() {
    std::cout << std::boolalpha;

    std::cout << "===== 1. SIGNED/UNSIGNED TRAP =====\n";
    int a = -1;
    unsigned int b = 1;
    std::cout << "int a = -1, unsigned b = 1\n";
    std::cout << "  a < b              = " << (a < static_cast<int>(b))
              << "   <- explicit cast se sahi\n";
    std::cout << "  (unsigned)a        = " << static_cast<unsigned>(a)
              << "  <- YEH problem hai\n";
    std::cout << "  std::cmp_less(a,b) = " << std::cmp_less(a, b)
              << "   <- ✅ C++20, mathematically correct\n";

    std::cout << "\n===== 2. FLOAT EQUALITY =====\n";
    const double x = 0.1 + 0.2;
    std::cout << "0.1 + 0.2 == 0.3            = " << (x == 0.3) << "   ❌\n";
    std::cout << "nearlyEqual(0.1+0.2, 0.3)   = " << nearlyEqual(x, 0.3) << "    ✅\n";

    std::cout << "\n===== 3. NaN =====\n";
    const double nan = std::numeric_limits<double>::quiet_NaN();
    std::cout << "nan == nan      = " << (nan == nan) << "   <- apne barabar bhi nahi!\n";
    std::cout << "nan != nan      = " << (nan != nan) << "\n";
    std::cout << "nan < 1.0       = " << (nan < 1.0)  << "\n";
    std::cout << "nan > 1.0       = " << (nan > 1.0)  << "   <- dono false\n";
    std::cout << "std::isnan(nan) = " << std::isnan(nan) << "   <- ✅ yahi use karo\n";

    std::cout << "\n===== 4. CHAINING TRAP =====\n";
    const int p = 5, q = 3, r = 1;
    std::cout << "p=5, q=3, r=1\n";
    std::cout << "  (p > q > r)      = " << (p > q > r)
              << "   ⚠️ (p>q)=true=1, phir 1>1 = false\n";
    std::cout << "  (p > q && q > r) = " << (p > q && q > r) << "    ✅ sahi tareeka\n";

    std::cout << "\n===== 5. STRING COMPARISON =====\n";
    const char* s1 = "hello";
    const char* s2 = "hello";
    std::cout << "const char* == const char*  -> POINTERS compare hote hain\n";
    std::cout << "  s1 == s2            = " << (s1 == s2)
              << "  <- literals merge ho gaye (guarantee NAHI hai)\n";
    std::cout << "  strcmp(s1,s2) == 0  = " << (std::strcmp(s1, s2) == 0)
              << "   <- ✅ content comparison\n";

    std::string t1 = "hello", t2 = "hello";
    std::cout << "  std::string ==      = " << (t1 == t2)
              << "   <- ✅ content compare karta hai\n";

    std::cout << "\n===== 6. = vs == =====\n";
    int flag = 0;
    std::cout << "flag = " << flag << "\n";
    if (flag = 5) {                       // ⚠️ jaan-boojh kar bug
        std::cout << "  if (flag = 5) chala -- HAMESHA chalega!\n";
    }
    std::cout << "  flag ab = " << flag << "  <- CORRUPT ho gaya\n";
    std::cout << "  (-Wall se warning milti hai)\n";

    return 0;
}
END
g++ -std=c++20 -Wall -Wextra compare.cpp -o compare && ./compare
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`a > b > c` kaam karta hai" | ❌ `(a>b) > c` ban jaata hai |
| "`-1 < 1u` true hai" | ❌ False — signed unsigned ban jaata hai |
| "`0.1+0.2 == 0.3`" | ❌ False. Epsilon comparison karo |
| "`NaN == NaN` true" | ❌ False. `std::isnan()` use karo |
| "`char* == char*` content compare karta hai" | ❌ Pointers compare karta hai |
| "`if (x = 5)` compile error" | ❌ Sirf warning. Silently galat chalega |

---

## Exercises

1. `compare.cpp` chalao. Har trap samjho.

2. Predict karo:
   ```cpp
   std::cout << (5 > 3 > 1) << " ";
   std::cout << (-1 < 1u) << " ";
   std::cout << (0.1 + 0.2 == 0.3) << " ";
   std::cout << (0.5 + 0.25 == 0.75) << "\n";
   ```
   <details><summary>Answer</summary>
   `0 0 0 1`

   Chautha `1` hai kyunki 0.5, 0.25, 0.75 exact powers of 2 hain — binary mein
   exactly represent ho jaate hain.
   </details>

3. `nearlyEqual` likho aur in cases pe test karo:
   - `0.1+0.2` vs `0.3`
   - `1e10` vs `1e10 + 1`
   - `0.0` vs `1e-15`
   - `NaN` vs `NaN`

4. `std::cmp_less` family use karo aur verify karo ki woh sahi answer deta hai.

5. `<=>` se ek `Point` struct banao jisme saare comparisons kaam karein:
   ```cpp
   struct Point {
       int x, y;
       auto operator<=>(const Point&) const = default;
       bool operator==(const Point&) const = default;
   };
   ```

6. NaN se sorting todo:
   ```cpp
   std::vector<double> v = {3.0, std::nan(""), 1.0, 2.0};
   std::sort(v.begin(), v.end());
   for (double d : v) std::cout << d << " ";
   ```
   Output kya aaya? Consistent hai?

7. `-Wsign-compare` ke saath apna koi purana loop compile karo. Warning aayi?

---

## Interview questions

1. `-1 < 1u` ka result aur kyun?
2. Floats ko compare kaise karte hain?
3. `NaN == NaN` kya deta hai?
4. `a > b > c` kya karta hai C++ mein?
5. `<=>` operator kya hai? Uska faayda?
6. `char*` comparison mein kya problem hai?

---

## Next
→ [`04-logical-operators.md`](04-logical-operators.md)
