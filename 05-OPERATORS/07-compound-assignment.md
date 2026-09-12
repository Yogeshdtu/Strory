# 07 — Compound assignment

## Prerequisites
`01-arithmetic-operators.md`, `05-bitwise-operators.md`

## Yeh topic abhi kyun
`x += 5` sirf `x = x + 5` ka shortcut nahi hai. Uske do real faayde hain jo
aage classes (folder 15) mein aur bhi important ho jaayenge.

---

## Poori list

```cpp
// Arithmetic
x += 5;       // x = x + 5
x -= 3;       // x = x - 3
x *= 2;       // x = x * 2
x /= 4;       // x = x / 4
x %= 3;       // x = x % 3

// Bitwise
x &= mask;    // x = x & mask
x |= flag;    // x = x | flag
x ^= key;     // x = x ^ key
x <<= 2;      // x = x << 2
x >>= 1;      // x = x >> 1
```

---

## 🔑 Faayda 1: Left side EK BAAR evaluate hota hai

**Yeh sirf typing shortcut nahi hai.**

```cpp
arr[computeIndex()] += 5;

// vs

arr[computeIndex()] = arr[computeIndex()] + 5;
//   ^^^^^^^^^^^^^^        ^^^^^^^^^^^^^^
//   computeIndex() DO BAAR chalega!
```

**Compound assignment mein left operand ka evaluation exactly ek baar hota hai.**
Yeh standard ki guarantee hai.

### Yeh kab matter karta hai?

**1. Mehnge expressions**
```cpp
getLargeVector()[expensiveIndex()] += 1;      // ✅ ek baar
```

**2. Side effects wale expressions**
```cpp
int i = 0;
arr[i++] += 5;                    // ✅ i sirf ek baar badha
arr[i++] = arr[i++] + 5;          // ⚠️ i do baar -- UB/confusing
```

**3. Iterators aur function calls**
```cpp
*it++ += 5;                       // ✅
map[getKey()] += 1;               // ✅ getKey() ek baar, lookup ek baar
map[getKey()] = map[getKey()] + 1;// ❌ getKey() do baar, lookup DO baar
```

---

## 🔑 Faayda 2: Classes mein zyada efficient

Built-in types ke liye compiler dono ko same code mein badal deta hai.
**Par classes ke liye bilkul alag functions call hote hain:**

```cpp
class BigObject {
public:
    BigObject& operator+=(const BigObject& other) {
        // ✅ IN-PLACE modify -- koi temporary nahi
        for (std::size_t i = 0; i < data_.size(); ++i) {
            data_[i] += other.data_[i];
        }
        return *this;
    }

    friend BigObject operator+(BigObject a, const BigObject& b) {
        // ⚠️ `a` ek COPY hai (by value liya), phir += kiya, phir return
        a += b;
        return a;
    }
private:
    std::vector<int> data_;
};
```

```cpp
BigObject a, b;
a += b;              // ✅ in-place, koi copy nahi
a = a + b;           // ⚠️ copy banti hai, phir move/copy assign
```

**Convention (folder 15 mein detail):**
- Pehle `operator+=` likho (in-place)
- Phir `operator+` ko usse implement karo

### `std::string` ka example

```cpp
std::string s = "Hello";

s += " World";                    // ✅ in-place append, shayad koi allocation nahi
s = s + " World";                 // ⚠️ naya temporary string banta hai, phir assign

// Loop mein yeh disaster hai
std::string result;
for (int i = 0; i < 1000; ++i) {
    result += "x";                // ✅ amortized O(1)
    // result = result + "x";     // ❌ O(n²) -- har baar poori string copy
}
```

**`+=` version 100-1000x tez ho sakta hai** bade strings ke liye.

---

## Type conversion ka trap

```cpp
int x = 10;
x += 3.7;             // ✅ compile hota hai
std::cout << x;       // 13  -- .7 gaya
```

**Kya hua?**
```
   x += 3.7
   ≡ x = static_cast<int>(x + 3.7)        <- implicit cast BACK to int
   ≡ x = static_cast<int>(13.7)
   ≡ x = 13
```

Compound assignment mein **implicit conversion back to left type** hoti hai.

```cpp
int x = 10;
x = x + 3.7;          // ⚠️ warning: conversion (with -Wconversion)
x += 3.7;             // ⚠️ SAME conversion, par kabhi kabhi warning nahi milti
```

`-Wconversion` use karo:
```bash
g++ -Wall -Wextra -Wconversion file.cpp
```

---

## Bitwise compound — flags ke liye

```cpp
#include <cstdint>

constexpr std::uint32_t FLAG_A = 1u << 0;
constexpr std::uint32_t FLAG_B = 1u << 1;
constexpr std::uint32_t FLAG_C = 1u << 2;

std::uint32_t flags = 0;

flags |= FLAG_A;                  // ✅ set
flags |= (FLAG_B | FLAG_C);       // ✅ multiple set

flags &= ~FLAG_B;                 // ✅ clear
flags ^= FLAG_C;                  // ✅ toggle

bool hasA = (flags & FLAG_A) != 0;   // test
```

**Yeh idiomatic hai.** Har systems codebase mein yeh pattern dikhega.

---

## Shift compound

```cpp
std::uint32_t x = 1;
x <<= 4;              // x = 16
x >>= 2;              // x = 4
```

⚠️ Wahi UB rules jo file 05 mein the:
```cpp
std::uint32_t x = 1;
x <<= 32;             // ⚠️ UB (shift >= bit width)
```

---

## Assignment ka result

`=` aur compound assignments ek **value** return karte hain:

```cpp
int a, b, c;
a = b = c = 5;        // ✅ right-to-left: c=5, phir b=5, phir a=5

int x = 10;
std::cout << (x += 5);      // 15 print, aur x = 15
```

**Note:** Yeh legal hai par real code mein confusing hai. Alag statements likho.

---

## Custom types ke liye (preview)

```cpp
class Money {
    std::int64_t paise_ = 0;
public:
    explicit Money(std::int64_t p) : paise_(p) {}

    // ✅ Compound assignment PEHLE likho
    Money& operator+=(const Money& other) {
        paise_ += other.paise_;
        return *this;                        // ✅ reference return -- chaining ke liye
    }

    Money& operator-=(const Money& other) {
        paise_ -= other.paise_;
        return *this;
    }

    // ✅ Phir binary operator ko usse implement karo
    friend Money operator+(Money a, const Money& b) {
        a += b;                              // pehla parameter by VALUE liya (copy)
        return a;
    }

    std::int64_t paise() const { return paise_; }
};
```

**Convention:**
| Operator | Return type | Kyun |
|---|---|---|
| `operator+=` | `T&` (reference) | in-place modify, chaining |
| `operator+` | `T` (value) | naya object banta hai |

Detail folder 15 mein.

---

## Performance summary

| Case | `x += y` | `x = x + y` |
|---|---|---|
| Built-in types (`int`, `double`) | same | same |
| Left side mehnga expression | **1 evaluation** | 2 evaluations ⚠️ |
| `std::string` append | **in-place** | temporary + copy ⚠️ |
| `std::vector` operations | in-place | copy ⚠️ |
| Custom class | `operator+=` | `operator+` + assignment ⚠️ |

**Default: `+=` use karo.** Kabhi nuksaan nahi, aksar faayda.

---

## Hands-on

```bash
cd ~/cpp-practice
cat > compound.cpp << 'END'
#include <iostream>
#include <string>
#include <chrono>
#include <cstdint>

// Side effect track karne ke liye
int callCount = 0;
int getIndex() {
    ++callCount;
    return 0;
}

int main() {
    std::cout << "===== 1. SINGLE EVALUATION =====\n";
    int arr[5] = {10, 20, 30, 40, 50};

    callCount = 0;
    arr[getIndex()] += 5;
    std::cout << "arr[getIndex()] += 5;            -> getIndex() "
              << callCount << " baar chala  ✅\n";

    callCount = 0;
    arr[getIndex()] = arr[getIndex()] + 5;
    std::cout << "arr[getIndex()] = arr[...] + 5;  -> getIndex() "
              << callCount << " baar chala  ⚠️\n";

    std::cout << "\n===== 2. TYPE CONVERSION TRAP =====\n";
    int x = 10;
    x += 3.7;
    std::cout << "int x = 10; x += 3.7;  ->  x = " << x
              << "   (.7 chupchap gaya)\n";
    std::cout << "  Kyunki: x = static_cast<int>(x + 3.7)\n";
    std::cout << "  -Wconversion se warning milti hai\n";

    std::cout << "\n===== 3. BITWISE FLAGS =====\n";
    constexpr std::uint32_t FLAG_READ  = 1u << 0;
    constexpr std::uint32_t FLAG_WRITE = 1u << 1;
    constexpr std::uint32_t FLAG_EXEC  = 1u << 2;

    std::uint32_t flags = 0;
    std::cout << "  shuru:            " << flags << "\n";
    flags |= FLAG_READ;
    std::cout << "  |= READ           " << flags << "\n";
    flags |= (FLAG_WRITE | FLAG_EXEC);
    std::cout << "  |= WRITE|EXEC     " << flags << "\n";
    flags &= ~FLAG_WRITE;
    std::cout << "  &= ~WRITE         " << flags << "\n";
    flags ^= FLAG_EXEC;
    std::cout << "  ^= EXEC (toggle)  " << flags << "\n";
    std::cout << "  READ set hai? " << ((flags & FLAG_READ) != 0) << "\n";

    std::cout << "\n===== 4. STRING PERFORMANCE =====\n";
    const int N = 20000;

    auto t1 = std::chrono::steady_clock::now();
    {
        std::string s;
        for (int i = 0; i < N; ++i) s += "x";           // ✅ in-place
    }
    auto t2 = std::chrono::steady_clock::now();
    {
        std::string s;
        for (int i = 0; i < N; ++i) s = s + "x";        // ⚠️ temporary har baar
    }
    auto t3 = std::chrono::steady_clock::now();

    auto ms = [](auto a, auto b) {
        return std::chrono::duration<double, std::milli>(b - a).count();
    };

    std::cout << "  s += \"x\"      : " << ms(t1, t2) << " ms  ✅\n";
    std::cout << "  s = s + \"x\"   : " << ms(t2, t3) << " ms  ⚠️ "
              << (ms(t2,t3) / ms(t1,t2)) << "x SLOWER\n";
    std::cout << "  (s = s + \"x\" har baar POORI string copy karta hai -> O(n^2))\n";

    std::cout << "\n===== 5. CHAINED ASSIGNMENT =====\n";
    int a, b, c;
    a = b = c = 7;
    std::cout << "  a = b = c = 7;  ->  a=" << a << " b=" << b << " c=" << c << "\n";
    std::cout << "  (right-to-left chalta hai)\n";

    return 0;
}
END
g++ -std=c++20 -O2 -Wall -Wextra compound.cpp -o compound && ./compound
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`x += y` sirf shortcut hai" | Left side 1 baar evaluate hota hai |
| "Classes ke liye dono same" | ❌ Alag operators call hote hain |
| "`s = s + x` `s += x` jaisa hai" | ❌ Temporary banta hai — O(n²) loop mein |
| "`x += 3.7` warning dega" | Kabhi kabhi nahi — `-Wconversion` chahiye |

---

## Exercises

1. `compound.cpp` chalao. String benchmark mein kitna fark aaya?

2. Single-evaluation demonstrate karo:
   ```cpp
   int counter = 0;
   int getIdx() { ++counter; return 0; }
   // arr[getIdx()] += 1;  vs  arr[getIdx()] = arr[getIdx()] + 1;
   ```

3. Type conversion trap:
   ```cpp
   int x = 10;
   x += 3.7;
   std::cout << x;
   ```
   `-Wconversion` ke saath compile karo. Warning aayi?

4. Flags system banao:
   ```cpp
   // Set, clear, toggle, test -- sab compound assignment se
   ```

5. Ek `Money` class banao `operator+=` aur `operator+` ke saath. Verify karo ki
   `+=` copy nahi banata (constructor mein print karke).

6. `std::vector` ke saath test karo:
   ```cpp
   std::vector<int> v;
   // v.insert(v.end(), other.begin(), other.end());   vs concatenation
   ```

---

## Interview questions

1. `x += y` aur `x = x + y` mein kya fark hai?
2. Classes ke liye kaunsa prefer karein aur kyun?
3. `operator+=` kya return karna chahiye? Kyun?
4. `s = s + "x"` loop mein kyun slow hai?

---

## Next
→ [`08-ternary-operator.md`](08-ternary-operator.md)
