# 05 — `int` deep dive

## Prerequisites
`02-what-is-a-variable.md`, folder 01 lesson 10 (binary, two's complement)

## Yeh topic abhi kyun
`int` sabse zyada use hone wala type hai. Aur uske baare mein 5 cheezein aisi hain
jo 90% programmers ko nahi pata — aur wahi HFT interviews mein poocha jaata hai:

1. `int` ki size kya hai? (jawab "4" nahi hai)
2. Uski exact range kya hai?
3. Overflow pe kya hota hai?
4. `signed` aur `unsigned` mix karne pe kya hota hai?
5. Integer promotion kya hai?

---

## `int` kya hai?

**`int` = integer = poora number (decimal nahi).**

```cpp
int age = 20;
int temperature = -5;
int zero = 0;
```

Decimal nahi rakh sakta:
```cpp
int x = 3.7;      // x mein 3 aayega -- .7 CUT ho jaayega (truncate, round nahi!)
```

---

## Size — "4 bytes" ka jhoot

Aksar log kehte hain "`int` 4 bytes ka hota hai." Yeh **practically sach** hai, par
**standard mein guarantee nahi** hai.

### Standard kya kehta hai

C++ standard sirf yeh guarantee karta hai:

| Type | Minimum bits | Minimum range |
|---|---|---|
| `char` | 8 | −127 to 127 |
| `short` | 16 | −32,767 to 32,767 |
| `int` | 16 | −32,767 to 32,767 |
| `long` | 32 | −2,147,483,647 to 2,147,483,647 |
| `long long` | 64 | ±9.2 × 10^18 |

Aur yeh relationship:
```
sizeof(char) == 1  <=  sizeof(short)  <=  sizeof(int)  <=  sizeof(long)  <=  sizeof(long long)
```

### Practically (2026 mein)

| Platform | `int` | `long` | `long long` | pointer |
|---|---|---|---|---|
| Linux x86-64 | 4 | **8** | 8 | 8 |
| Windows x86-64 | 4 | **4** | 8 | 8 |
| macOS x86-64/ARM | 4 | **8** | 8 | 8 |
| 32-bit systems | 4 | 4 | 8 | 4 |
| Embedded (AVR) | **2** | 4 | 8 | 2 |

**Dekha `long` ka fark?** Linux pe 8, Windows pe 4. Yeh real portability bug ka source hai.

> **HFT relevance:** Isliye HFT codebases mein **kabhi** plain `int`/`long` use nahi
> hota wire formats ya struct layouts mein. Hamesha `int32_t`, `int64_t`, `uint64_t`
> use hota hai — jinki size **guaranteed** hai. File 09 mein detail.

### Khud check karo
```cpp
#include <iostream>
int main() {
    std::cout << "char:      " << sizeof(char)      << "\n";
    std::cout << "short:     " << sizeof(short)     << "\n";
    std::cout << "int:       " << sizeof(int)       << "\n";
    std::cout << "long:      " << sizeof(long)      << "\n";
    std::cout << "long long: " << sizeof(long long) << "\n";
    std::cout << "pointer:   " << sizeof(void*)     << "\n";
}
```

---

## Range — signed vs unsigned

### Signed `int` (default)

32 bits, jisme 1 bit sign ke liye (two's complement):

```
   Range: -2,147,483,648  se  +2,147,483,647
        = -2^31           se   2^31 - 1
```

**~2.1 arab.** Yeh number yaad rakho.

### Unsigned `int`

Saare 32 bits value ke liye:

```
   Range: 0  se  4,294,967,295
        = 0  se  2^32 - 1
```

**~4.2 arab.**

### Saare integer types ki ranges

| Type | Bytes | Signed range | Unsigned range |
|---|---|---|---|
| `char` | 1 | −128 .. 127 | 0 .. 255 |
| `short` | 2 | −32,768 .. 32,767 | 0 .. 65,535 |
| `int` | 4 | −2.1×10⁹ .. 2.1×10⁹ | 0 .. 4.29×10⁹ |
| `long long` | 8 | −9.2×10¹⁸ .. 9.2×10¹⁸ | 0 .. 1.8×10¹⁹ |

### Range pata karne ka sahi tareeka

```cpp
#include <limits>
#include <iostream>

int main() {
    std::cout << "int min: " << std::numeric_limits<int>::min() << "\n";
    std::cout << "int max: " << std::numeric_limits<int>::max() << "\n";
    std::cout << "unsigned max: " << std::numeric_limits<unsigned>::max() << "\n";
}
```

`<climits>` ke `INT_MAX`/`INT_MIN` bhi hain, par `<limits>` C++ way hai (templates
ke saath kaam karta hai).

---

## ⚠️ OVERFLOW — sabse important section

Kya ho jab aap max se aage badho?

### Unsigned overflow: DEFINED (wrap around)

```cpp
unsigned int x = 4294967295;    // max
x = x + 1;                       // 0 ban jaata hai
```

Yeh **well-defined** hai. Standard kehta hai: unsigned arithmetic **modulo 2^N** hoti hai.

```
   4294967295 + 1  =  4294967296
   4294967296 mod 2^32  =  0
```

Yeh predictable hai. Hash functions, checksums, circular buffers — sab isi pe chalte hain.

### Signed overflow: **UNDEFINED BEHAVIOUR** 😱

```cpp
int x = 2147483647;     // INT_MAX
x = x + 1;              // ⚠️ UNDEFINED BEHAVIOUR
```

**Kya hoga?** Standard kehta hai: **kuch bhi ho sakta hai.**
- Shayad `-2147483648` mile (wrap around)
- Shayad program crash ho
- Shayad compiler poora code hi hata de
- Shayad kuch aur ho

### UB kitna khatarnaak hai — real demo

```cpp
#include <iostream>

bool alwaysTrue(int x) {
    return x + 1 > x;      // math mein hamesha true, na?
}

int main() {
    std::cout << alwaysTrue(5) << "\n";           // 1
    std::cout << alwaysTrue(2147483647) << "\n";  // ???
}
```

`-O0` pe: `0` mil sakta hai (wrap around hua).
`-O2` pe: `1` milega — **compiler ne poora check hi hata diya!**

**Kyun?** Compiler sochta hai: "signed overflow UB hai, matlab programmer ne guarantee
di hai ki overflow nahi hoga. To `x + 1 > x` hamesha true hai. To check hata do."

Yeh **legal optimization** hai. Aur yeh aapka code todh sakta hai.

```bash
g++ -O0 ub.cpp -o ub0 && ./ub0
g++ -O2 ub.cpp -o ub2 && ./ub2
# Alag output mil sakta hai!
```

### Overflow pakadne ke tareeke

**1. UBSan (Undefined Behaviour Sanitizer)**
```bash
g++ -std=c++20 -fsanitize=undefined -g file.cpp -o file && ./file
```
```
runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'
```

**2. Compiler builtins (safe arithmetic)**
```cpp
int result;
if (__builtin_add_overflow(a, b, &result)) {
    // overflow hua!
} else {
    // result sahi hai
}
```

**3. Pehle check karo**
```cpp
if (a > 0 && b > std::numeric_limits<int>::max() - a) {
    // overflow hoga
}
```

**4. Bada type use karo**
```cpp
long long result = static_cast<long long>(a) * b;   // 64-bit mein
```

> **HFT relevance:** Order quantities, prices, position sizes — sab integers hain.
> Ek overflow se aap 2 arab shares ka order bhej sakte ho. Isliye HFT risk systems
> mein har arithmetic operation bounds-checked hoti hai, ya `int64_t` use hota hai
> jahan overflow practically impossible ho.

---

## ⚠️ Signed/Unsigned mixing — chhupa hua bug

```cpp
#include <iostream>
int main() {
    int a = -1;
    unsigned int b = 1;

    if (a < b) std::cout << "a chhota hai\n";
    else       std::cout << "a bada hai?!\n";
}
```

**Output:** `a bada hai?!` 😱

### Kyun?

Jab `signed` aur `unsigned` compare hote hain (same rank ke), **signed ko unsigned mein
convert kiya jaata hai** (usual arithmetic conversions).

```
   a = -1  ->  unsigned mein  ->  4294967295
   
   4294967295 < 1 ?  ->  NAHI
```

### Aur bhi khatarnaak: loop

```cpp
std::vector<int> v = {1, 2, 3};

for (int i = 0; i < v.size(); ++i) { }     // ⚠️ warning: sign compare
                                            //    v.size() unsigned hai!

for (int i = v.size() - 1; i >= 0; --i) { } // ⚠️⚠️ INFINITE LOOP agar v khali ho!
```

Agar `v` khali hai: `v.size()` = 0, aur `0 - 1` unsigned mein = **4294967295**.
`i` ko `int` mein assign kiya, to implementation-defined. Aur agar aap `size_t i`
likh do, to `i >= 0` **hamesha true** hai → infinite loop.

### Solutions

```cpp
// 1. Sahi type use karo
for (std::size_t i = 0; i < v.size(); ++i) { }

// 2. Range-based for (best)
for (const auto& item : v) { }

// 3. C++20 ssize
for (auto i = std::ssize(v) - 1; i >= 0; --i) { }   // signed size

// 4. Warnings ON
// g++ -Wall -Wextra -Wsign-compare -Wsign-conversion
```

**Rule:** Comparisons mein signed aur unsigned mix mat karo. Warnings ON rakho.

---

## Integer promotion — ek chhupa hua rule

Chhote types arithmetic mein **automatically `int` ban jaate hain**:

```cpp
char a = 100;
char b = 100;
auto c = a + b;         // c ka type CHAR nahi, INT hai!
std::cout << c;         // 200 (char mein 200 nahi aata!)
std::cout << sizeof(a + b);   // 4, not 1
```

```cpp
short s = 1;
std::cout << sizeof(s);       // 2
std::cout << sizeof(s + s);   // 4  <- int ban gaya!
std::cout << sizeof(+s);      // 4  <- unary + bhi promote karta hai
```

**Kyun?** CPU native word size (32-bit) pe hi efficiently kaam karti hai. Chhote types
pe arithmetic karne ke liye unhe pehle promote karna padta hai.

Yeh baat file 13 (conversions) mein aur detail mein.

---

## Integer division ka trap

```cpp
int a = 7;
int b = 2;
std::cout << a / b;       // 3, not 3.5!
```

**Do integers ka division hamesha integer deta hai.** Decimal part **cut** ho jaata hai
(zero ki taraf truncate).

```cpp
std::cout << 7 / 2;       // 3
std::cout << -7 / 2;      // -3  (zero ki taraf, -4 nahi)
std::cout << 7 % 2;       // 1   (remainder)
std::cout << -7 % 2;      // -1
```

### Decimal chahiye to?

```cpp
std::cout << 7.0 / 2;                          // 3.5 (ek double hai to double division)
std::cout << static_cast<double>(a) / b;       // 3.5
std::cout << (double)a / b;                    // 3.5 (C-style, avoid)
```

### Classic bug

```cpp
int total = 7;
int count = 2;
double average = total / count;      // ⚠️ 3.0, not 3.5!
//                ^^^^^^^^^^^^^^ integer division PEHLE hui, phir double mein convert

double average = static_cast<double>(total) / count;    // ✅ 3.5
```

### Division by zero

```cpp
int x = 5 / 0;      // ⚠️ UNDEFINED BEHAVIOUR (usually crash - SIGFPE)
```

Floating point mein alag hai:
```cpp
double y = 5.0 / 0.0;    // inf (defined! IEEE-754)
```

---

## `int` ke variants

```cpp
int              a;      // signed int
signed int       b;      // same as int
unsigned int     c;      // 0 se upar
short            d;      // short int
short int        e;      // same
unsigned short   f;
long             g;      // long int
long long        h;      // long long int
unsigned long long i;
```

### Literals ke suffixes

```cpp
42          // int
42u         // unsigned int
42L         // long
42UL        // unsigned long
42LL        // long long
42ULL       // unsigned long long

// Bases
42          // decimal
0x2A        // hex (0x prefix)
052         // octal (0 prefix -- ⚠️ dhyaan do!)
0b101010    // binary (C++14)

// Digit separators (C++14) - readability ke liye
1'000'000   // ek million, padhne mein aasan
0xFF'FF'FF  // hex bhi
```

**⚠️ Octal trap:**
```cpp
int a = 010;      // yeh 10 NAHI hai! Yeh octal hai = 8
int b = 08;       // ❌ error: 8 octal digit nahi hai
```
Leading zero se bacho jab tak jaan-boojh kar octal na chahiye.

---

## Hands-on

`examples/03_integer_overflow.cpp` chalao. Woh dikhata hai:
- Unsigned wrap around (defined)
- Signed overflow ka UB
- Signed/unsigned comparison bug
- Integer division traps

```bash
cd examples
g++ -std=c++20 -Wall -Wextra 03_integer_overflow.cpp -o ovf && ./ovf

# Ab UBSan ke saath
g++ -std=c++20 -fsanitize=undefined -g 03_integer_overflow.cpp -o ovf_san && ./ovf_san
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`int` hamesha 4 bytes" | Standard guarantee nahi karta. `int32_t` use karo jab pakka chahiye |
| "`long` 4 bytes hai" | Linux/Mac pe 8, Windows pe 4 |
| "Overflow pe error aata hai" | Signed = UB (silent!), unsigned = wrap around |
| "Signed overflow wrap around karta hai" | ❌ UB hai. Compiler kuch bhi kar sakta hai |
| "`7/2 = 3.5`" | Integer division = 3 |
| "`-7/2 = -4`" | −3 (zero ki taraf truncate) |
| "`0 - 1` unsigned mein −1" | Nahi, `4294967295` |

---

## Exercises

1. Apne system pe saare integer types ki sizes print karo.

2. Predict karo:
   ```cpp
   std::cout << 7 / 2 << " ";
   std::cout << 7 % 2 << " ";
   std::cout << -7 / 2 << " ";
   std::cout << -7 % 2 << " ";
   std::cout << 7.0 / 2 << "\n";
   ```
   <details><summary>Answer</summary>`3 1 -3 -1 3.5`</details>

3. Yeh bug fix karo:
   ```cpp
   int correct = 45;
   int total = 60;
   double percentage = correct / total * 100;
   std::cout << percentage;      // 0 aata hai!
   ```
   <details><summary>Answer</summary>
   `correct / total` = `45/60` = **0** (integer division!). Phir `0 * 100 = 0`.

   Fix: `double percentage = static_cast<double>(correct) / total * 100;` → 75
   </details>

4. Unsigned wrap around test:
   ```cpp
   unsigned int x = 0;
   std::cout << x - 1 << "\n";
   ```
   Kya aaya? Kyun?
   <details><summary>Answer</summary>
   `4294967295`. `0 - 1` unsigned mein modulo 2^32 hota hai = 2^32 − 1.
   </details>

5. Signed overflow ko UBSan se pakdo:
   ```cpp
   #include <iostream>
   #include <limits>
   int main() {
       int x = std::numeric_limits<int>::max();
       std::cout << x + 1 << "\n";
   }
   ```
   ```bash
   g++ -std=c++20 -fsanitize=undefined -g f.cpp -o f && ./f
   ```

6. `alwaysTrue` wala experiment `-O0` aur `-O2` pe chalao. Output alag aaya?

7. Infinite loop banao aur samjho:
   ```cpp
   std::vector<int> v;    // KHALI
   for (std::size_t i = v.size() - 1; i >= 0; --i) {
       std::cout << "loop\n";
   }
   ```
   ⚠️ Yeh infinite chalega. `Ctrl+C` se rokna. Kyun hua?
   <details><summary>Answer</summary>
   `v.size()` = 0 (unsigned). `0 - 1` = 4294967295 (wrap around).
   Aur `size_t i >= 0` **hamesha true** hai — unsigned kabhi negative nahi ho sakta.
   Infinite loop.

   Fix: range-based for, ya `if (!v.empty())` check, ya signed index.
   </details>

8. `1'000'000` aur `0b1010` literals try karo. Chale?

---

## Interview questions

1. `int` ki size kya hai? (trap question — sahi jawab kya hai?)
2. Signed aur unsigned overflow mein kya fark hai?
3. `-1 < 1u` ka result kya hai aur kyun?
4. Integer promotion kya hai?
5. `int a = 7, b = 2; double c = a / b;` — `c` kya hoga?
6. Signed overflow UB kyun hai, defined kyun nahi?
   <details><summary>Answer</summary>
   Historical: alag CPUs mein alag representations thi (two's complement, one's
   complement, sign-magnitude). Standard ne UB chhod diya taaki har platform pe
   efficient code bane.

   Modern: UB compiler ko powerful optimizations karne deta hai (loop bounds analysis,
   strength reduction). C++20 se signed integers **two's complement** guarantee hain,
   par overflow **ab bhi UB** hai.
   </details>

---

## Next
→ [`06-floating-point.md`](06-floating-point.md)
