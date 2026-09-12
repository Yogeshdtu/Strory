# 08 — Ternary operator `?:`

## Prerequisites
`04-logical-operators.md`

## Yeh topic abhi kyun
`?:` ek compact conditional hai. Sahi use se code saaf hota hai, galat use se
unreadable.

Aur yeh **ek expression** hai (statement nahi) — jo `const` initialization mein
critical faayda deta hai.

---

## Syntax

```cpp
condition ? valueIfTrue : valueIfFalse
```

```cpp
int max = (a > b) ? a : b;

// Equivalent if-else
int max;
if (a > b) max = a;
else       max = b;
```

**Yeh C++ ka akela ternary (3-operand) operator hai.**

---

## 🔑 Yeh EXPRESSION hai, statement nahi

Yeh sabse important baat hai.

```cpp
// ✅ Expression -- value deta hai, kahin bhi use ho sakta hai
int x = cond ? 1 : 2;
func(cond ? a : b);
std::cout << (cond ? "yes" : "no");
return cond ? a : b;

// ❌ if-else statement hai -- value nahi deta
int y = if (cond) 1; else 2;      // COMPILE ERROR
```

### Faayda 1: `const` initialization

```cpp
// ✅ const bana sakte ho
const int value = (cond) ? computeA() : computeB();

// ❌ if-else ke saath const nahi ban sakta
const int value;                  // error: uninitialized const
if (cond) value = computeA();     // error: assignment to const
```

**Yeh bada faayda hai** — const-correctness maintain rehti hai.

Alternative (agar logic complex ho):
```cpp
const int value = [&] {
    if (cond1) return computeA();
    if (cond2) return computeB();
    return computeC();
}();                              // immediately-invoked lambda (folder 22)
```

### Faayda 2: Member initializer list
```cpp
class Order {
    int price_;
public:
    Order(bool isBuy, int bid, int ask)
        : price_(isBuy ? ask : bid)      // ✅ ternary hi kaam karega
    {}
};
```

Member initializer list mein `if` nahi likh sakte.

---

## Short-circuit hota hai

Sirf **ek branch** evaluate hota hai:

```cpp
int result = cond ? expensiveA() : expensiveB();
// Sirf ek function chalega
```

```cpp
// ✅ Safe -- ptr null hone pe deref nahi hoga
int value = (ptr != nullptr) ? ptr->data : 0;
```

---

## Return type ke rules

Dono branches ka type **compatible** hona chahiye. Compiler ek **common type**
dhoondhta hai:

```cpp
auto a = cond ? 1 : 2;              // int
auto b = cond ? 1 : 2.5;            // double! (int -> double promote)
auto c = cond ? 'x' : 'y';          // char
auto d = cond ? 1 : 'x';            // int (char promote)

// ❌ Incompatible types
auto e = cond ? 1 : "text";         // COMPILE ERROR
```

### ⚠️ Surprise conversion

```cpp
int i = 5;
double d = 3.14;
auto result = cond ? i : d;         // double! `i` promote ho gaya

std::cout << (cond ? i : d);        // hamesha double print hoga
```

Yeh silent bug ban sakta hai agar aap `int` expect kar rahe ho.

---

## Nesting — dhyaan se

```cpp
// ⚠️ Do levels tak theek hai
std::string grade = (score >= 90) ? "A"
                  : (score >= 80) ? "B"
                  : (score >= 70) ? "C"
                                  : "F";

// ❌ Isse zyada -- if-else use karo
```

**Right-associative hai:**
```cpp
a ? b : c ? d : e
≡ a ? b : (c ? d : e)
```

### Formatting matters
```cpp
// ❌ Unreadable
std::string s = a ? b : c ? d : e ? f : g;

// ✅ Readable
std::string s = a ? b
              : c ? d
              : e ? f
                  : g;
```

**Salah:** 2 levels se zyada nesting ho to `if-else` ya `switch` use karo.

---

## Precedence — bahut kam hai

`?:` ki precedence **assignment se sirf zyada** hai. Matlab **bahut kam**.

```cpp
int x = a > b ? a : b;            // ✅ (a > b) ? a : b
int y = a + b ? c : d;            // ⚠️ (a + b) ? c : d  -- shayad yeh nahi chahte the
```

### ⚠️ `cout` ke saath brackets zaroori
```cpp
std::cout << cond ? "yes" : "no";       // ❌ COMPILE ERROR ya galat
// Parse: (std::cout << cond) ? "yes" : "no"

std::cout << (cond ? "yes" : "no");     // ✅
```

**Rule: `?:` ke around hamesha brackets lagao** jab kisi bade expression mein ho.

---

## ⚠️ Assignment ke saath

```cpp
// ❌ C mein illegal, C++ mein legal par CONFUSING
(cond ? a : b) = 5;               // a ya b ko 5 set karo

// ✅ Clear
if (cond) a = 5;
else      b = 5;
```

C++ mein `?:` **lvalue** return kar sakta hai (agar dono operands lvalue hon aur
same type ke hon). Par yeh feature confusing hai — use mat karo.

---

## Common patterns

### Default value
```cpp
int timeout = (userTimeout > 0) ? userTimeout : DEFAULT_TIMEOUT;
```

### Clamping
```cpp
int clamped = (value < min) ? min : (value > max) ? max : value;
// ✅ Better: std::clamp(value, min, max);  (C++17)
```

### Plural
```cpp
std::cout << count << " item" << (count == 1 ? "" : "s") << "\n";
```

### Safe access
```cpp
int value = (index < size) ? arr[index] : 0;
```

### Sign
```cpp
int sign = (x > 0) ? 1 : (x < 0) ? -1 : 0;
```

---

## Performance

```cpp
int max = (a > b) ? a : b;
```

Compiler `-O2` pe aksar **`cmov`** (conditional move) instruction banata hai —
jo **branchless** hai:

```asm
cmp     edi, esi
cmovle  eax, esi        ; conditional move -- koi branch nahi
```

**Iska matlab:** ternary aksar `if-else` se tez hota hai — kyunki koi branch
misprediction nahi hoti.

⚠️ **Lekin:**
- Compiler `if-else` ko bhi `cmov` bana sakta hai
- `cmov` mein dono values compute hoti hain — agar ek side mehnga ho to bura hai
- Predictable branches mein `if` tez ho sakta hai

**Rule: measure karo, assume mat karo.** Folder 31/36 mein detail.

### Godbolt pe verify karo
```cpp
int f(int a, int b) { return a > b ? a : b; }
int g(int a, int b) { if (a > b) return a; return b; }
```
`-O2` pe dono ka assembly compare karo — aksar **identical** hota hai.

---

## `if constexpr` se fark (preview)

```cpp
// Runtime ternary
int x = cond ? a : b;                    // dono branches COMPILE hote hain

// Compile-time (C++17)
if constexpr (someConstexprCond) {
    // sirf yeh branch COMPILE hoti hai
} else {
    // yeh code exist hi nahi karta
}
```

`if constexpr` templates mein critical hai (folder 21).

---

## Hands-on

```bash
cd ~/cpp-practice
cat > ternary.cpp << 'END'
#include <iostream>
#include <string>
#include <algorithm>

int expensiveA() { std::cout << "[A chala] "; return 1; }
int expensiveB() { std::cout << "[B chala] "; return 2; }

int main() {
    std::cout << "===== 1. BASIC =====\n";
    const int a = 10, b = 20;
    const int max = (a > b) ? a : b;
    std::cout << "max(" << a << "," << b << ") = " << max << "\n";

    std::cout << "\n===== 2. CONST INITIALIZATION =====\n";
    const bool useFirst = true;
    const int value = useFirst ? 100 : 200;      // ✅ const ban gaya
    std::cout << "const int value = cond ? 100 : 200;  ->  " << value << "\n";
    std::cout << "(if-else ke saath const nahi ban sakta tha)\n";

    std::cout << "\n===== 3. SHORT-CIRCUIT =====\n";
    std::cout << "cond=true:  ";
    const int r1 = true ? expensiveA() : expensiveB();
    std::cout << "-> " << r1 << "  (sirf A chala)\n";

    std::cout << "cond=false: ";
    const int r2 = false ? expensiveA() : expensiveB();
    std::cout << "-> " << r2 << "  (sirf B chala)\n";

    std::cout << "\n===== 4. TYPE PROMOTION TRAP =====\n";
    const int i = 5;
    const double d = 3.14;
    std::cout << "cond ? int : double  ->  type: double\n";
    std::cout << "  true  ke saath: " << (true  ? i : d) << "   <- 5 nahi, 5.0\n";
    std::cout << "  false ke saath: " << (false ? i : d) << "\n";

    std::cout << "\n===== 5. NESTED (2 levels tak theek) =====\n";
    for (const int score : {95, 85, 75, 60}) {
        const std::string grade = (score >= 90) ? "A"
                                : (score >= 80) ? "B"
                                : (score >= 70) ? "C"
                                                : "F";
        std::cout << "  score " << score << " -> grade " << grade << "\n";
    }

    std::cout << "\n===== 6. COMMON PATTERNS =====\n";
    for (const int count : {0, 1, 5}) {
        std::cout << "  " << count << " item"
                  << (count == 1 ? "" : "s") << "\n";
    }

    const int val = 150;
    const int lo = 0, hi = 100;
    std::cout << "  clamp(150, 0, 100) manual: "
              << ((val < lo) ? lo : (val > hi) ? hi : val) << "\n";
    std::cout << "  std::clamp (C++17):        "
              << std::clamp(val, lo, hi) << "   <- ✅ behtar\n";

    const int x = -5;
    std::cout << "  sign(-5) = " << ((x > 0) ? 1 : (x < 0) ? -1 : 0) << "\n";

    std::cout << "\n===== 7. ⚠️ PRECEDENCE =====\n";
    const bool cond = true;
    // std::cout << cond ? "yes" : "no";      // ❌ compile error!
    std::cout << "  std::cout << cond ? \"yes\" : \"no\";   -> COMPILE ERROR\n";
    std::cout << "  std::cout << (cond ? \"yes\" : \"no\"); -> "
              << (cond ? "yes" : "no") << "  ✅\n";

    return 0;
}
END
g++ -std=c++20 -Wall -Wextra ternary.cpp -o ternary && ./ternary
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`?:` `if-else` jaisa hai" | Woh **expression** hai, statement nahi |
| "Dono branches evaluate hote hain" | ❌ Sirf ek |
| "Return type pehle branch ka hota hai" | Common type nikala jaata hai (promotion ho sakta hai) |
| "`cout << cond ? a : b` chalega" | ❌ Precedence — brackets chahiye |
| "`?:` `if` se tez hai" | Aksar same assembly. Measure karo |

---

## Exercises

1. `ternary.cpp` chalao. Type promotion trap samjho.

2. Precedence bug reproduce karo:
   ```cpp
   bool cond = true;
   std::cout << cond ? "yes" : "no";
   ```
   Error kya aaya? Fix karo.

3. `const` initialization ka faayda demonstrate karo:
   ```cpp
   // if-else se const banane ki koshish karo -- fail hoga
   // ternary se karo -- kaam karega
   ```

4. Type promotion test:
   ```cpp
   auto a = true ? 1 : 2.5;
   auto b = true ? 'x' : 65;
   std::cout << a << " " << b << "\n";
   std::cout << sizeof(a) << " " << sizeof(b) << "\n";
   ```
   <details><summary>Answer</summary>
   `a` = `1` (par type `double`, size 8)
   `b` = `120` (par type `int`, size 4 — `'x'` promote ho gaya)
   </details>

5. Godbolt pe verify karo ki `?:` aur `if-else` same assembly dete hain (`-O2`).

6. Nested ternary se grade calculator likho, phir usko `if-else` mein convert karo.
   Kaunsa zyada readable hai?

7. `std::clamp` (C++17) use karo manual ternary ki jagah.

---

## Interview questions

1. `?:` expression hai ya statement? Fark kya padta hai?
2. Dono branches evaluate hote hain?
3. Return type kaise decide hota hai?
4. `?:` ki precedence kya hai?
5. `const` initialization mein `?:` ka faayda?

---

## Next
→ [`09-precedence-associativity.md`](09-precedence-associativity.md)
