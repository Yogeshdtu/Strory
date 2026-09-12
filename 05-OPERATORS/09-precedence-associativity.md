# 09 — Precedence aur associativity

## Prerequisites
Is folder ke lessons 01–08

## Yeh topic abhi kyun
`a + b * c` mein pehle kya chalega? `a << b < c` kaise parse hoga?

Precedence bugs **silent** hote hain — code compile ho jaata hai aur galat answer
deta hai. Ek table yaad karne se bach jaate hain.

---

## Do concepts

| | Matlab |
|---|---|
| **Precedence** | Kaunsa operator **pehle** chalega |
| **Associativity** | Same precedence wale operators kis **direction** mein group honge |

```cpp
2 + 3 * 4        // precedence: * pehle -> 2 + 12 = 14
10 - 5 - 2       // associativity: left-to-right -> (10-5)-2 = 3
a = b = c        // associativity: right-to-left -> a = (b = c)
```

---

## Precedence table (practical version)

Upar se neeche — **highest to lowest**:

| # | Operators | Associativity | Note |
|---|---|---|---|
| 1 | `::` | left | scope resolution |
| 2 | `a++` `a--` `f()` `a[]` `.` `->` | left | postfix |
| 3 | `++a` `--a` `!` `~` `+a` `-a` `*p` `&a` `sizeof` casts `new` `delete` | **right** | unary |
| 4 | `.*` `->*` | left | member pointer |
| 5 | `*` `/` `%` | left | multiplicative |
| 6 | `+` `-` | left | additive |
| 7 | `<<` `>>` | left | **shift** ⚠️ |
| 8 | `<=>` | left | three-way (C++20) |
| 9 | `<` `<=` `>` `>=` | left | relational |
| 10 | `==` `!=` | left | equality |
| 11 | `&` | left | **bitwise AND** ⚠️ |
| 12 | `^` | left | **bitwise XOR** ⚠️ |
| 13 | `\|` | left | **bitwise OR** ⚠️ |
| 14 | `&&` | left | logical AND |
| 15 | `\|\|` | left | logical OR |
| 16 | `?:` | **right** | ternary |
| 17 | `=` `+=` `-=` `*=` `/=` `%=` `&=` `\|=` `^=` `<<=` `>>=` | **right** | assignment |
| 18 | `,` | left | comma |

**Yeh poora yaad karne ki zarurat nahi.** 5 traps yaad rakho (neeche).

---

## ⚠️ TRAP 1: Bitwise operators ki precedence BAHUT KAM hai

**Yeh sabse common precedence bug hai.**

```cpp
if (flags & MASK == 0) { }       // ⚠️ GALAT!
```

**Kya hua:**
```
   == ki precedence & se ZYADA hai
   -> flags & (MASK == 0)
   -> flags & (false)
   -> flags & 0
   -> 0  (hamesha false!)
```

**Fix:**
```cpp
if ((flags & MASK) == 0) { }     // ✅ brackets zaroori
```

### Yeh historical mistake hai

C ke designer Dennis Ritchie ne khud maana ki `&` aur `|` ki precedence galat rakhi
gayi thi — par tab tak bahut code likha ja chuka tha, isliye badla nahi ja saka.

**Rule: `&`, `|`, `^` use karo to HAMESHA brackets lagao.**

```cpp
if ((a & b) == c) { }            // ✅
if ((a | b) != 0) { }            // ✅
if ((x ^ y) < z) { }             // ✅
```

`-Wparentheses` isko warn karta hai.

---

## ⚠️ TRAP 2: `<<` aur comparison

```cpp
std::cout << a < b;              // ⚠️ GALAT
```

**Kya hua:**
```
   << ki precedence < se ZYADA hai
   -> (std::cout << a) < b
   -> ostream < int
   -> COMPILE ERROR
```

**Fix:**
```cpp
std::cout << (a < b);            // ✅
```

Aur agar `<<` shift ke roop mein use ho raha ho:
```cpp
int x = 1 << 2 + 3;              // ⚠️ 1 << 5 = 32, not 4 + 3 = 7
                                 //    (+ ki precedence << se zyada)
int y = (1 << 2) + 3;            // ✅ 7
```

---

## ⚠️ TRAP 3: Unary operators right-associative hain

```cpp
*p++            // ≡ *(p++)      -- p badha, purani address deref
(*p)++          // -- p ki VALUE badhi

!a == b         // ≡ (!a) == b
!(a == b)       // -- shayad yeh chahte the

-a * b          // ≡ (-a) * b    -- theek hai
~a & b          // ≡ (~a) & b    -- theek hai
```

### `*p++` breakdown
```cpp
int arr[] = {10, 20, 30};
int* p = arr;

int v = *p++;
// Steps:
//   1. p++ -> purani p value return, p aage badha
//   2. * us purani address pe -> 10
// v = 10, p ab arr[1] pe hai
```

---

## ⚠️ TRAP 4: `&&` `||` mix karna

```cpp
if (a || b && c) { }             // ≡ a || (b && c)
```

`&&` ki precedence `||` se **zyada** hai (jaise `*` `+` se).

**Yeh technically sahi hai**, par padhne mein confusing. Brackets lagao:
```cpp
if (a || (b && c)) { }           // ✅ clear
```

`-Wparentheses` warn karta hai.

---

## ⚠️ TRAP 5: Ternary ki precedence bahut kam hai

```cpp
int x = a > b ? a : b;           // ✅ (a > b) ? a : b -- theek hai

int y = a + b ? c : d;           // ⚠️ (a + b) ? c : d
                                 //    shayad a + (b ? c : d) chahte the

std::cout << cond ? "y" : "n";   // ❌ (std::cout << cond) ? "y" : "n"
```

**Rule: `?:` ko hamesha brackets mein rakho** jab kisi bade expression mein ho.

---

## Associativity examples

### Left-to-right (zyada tar)
```cpp
10 - 5 - 2       // (10 - 5) - 2 = 3         ✅ jo aap expect karte ho
100 / 10 / 2     // (100 / 10) / 2 = 5
a << b << c      // (a << b) << c
cout << a << b   // (cout << a) << b         <- chaining isi se kaam karti hai
```

### Right-to-left (unary, ternary, assignment)
```cpp
a = b = c = 5           // a = (b = (c = 5))
++--x                   // ++(--x)
!~x                     // !(~x)
a ? b : c ? d : e       // a ? b : (c ? d : e)
```

### ⚠️ Exponentiation ka trap (C++ mein `**` nahi hai)
```cpp
// Python:  2 ** 3 ** 2  =  2 ** (3 ** 2)  =  512   (right-assoc)
// C++ mein `**` operator hai hi nahi
std::pow(2, std::pow(3, 2));      // 512
```

---

## 🔑 Practical rules (table yaad karne se better)

### Rule 1: Bitwise = brackets
```cpp
(a & b) == c
(a | b) != 0
(x ^ y) < z
```

### Rule 2: Comparison in `cout` = brackets
```cpp
std::cout << (a < b);
std::cout << (x == y);
std::cout << (cond ? "y" : "n");
```

### Rule 3: `&&`/`||` mix = brackets
```cpp
if (a || (b && c))
```

### Rule 4: Shift ke saath arithmetic = brackets
```cpp
(1 << n) + 1
1 << (n + 1)
```

### Rule 5: Confusion ho to brackets
**Brackets free hain.** Compiler unke liye extra code nahi banata. Agar aapko
2 second bhi sochna pada, brackets laga do.

```cpp
// Kisi ko yeh yaad nahi rahega
result = a + b * c - d / e % f;

// Yeh sabko samajh aayega
result = a + (b * c) - ((d / e) % f);
```

---

## Warnings ON rakho

```bash
g++ -Wall -Wextra -Wparentheses file.cpp
```

`-Wparentheses` yeh pakadta hai:
- `a || b && c` (suggest parentheses)
- `flags & MASK == 0`
- `if (x = 5)` (assignment in condition)

`-Wall` mein `-Wparentheses` already included hai.

---

## Hands-on

```bash
cd ~/cpp-practice
cat > precedence.cpp << 'END'
#include <iostream>
#include <cstdint>

int main() {
    std::cout << std::boolalpha;

    std::cout << "===== 1. BITWISE PRECEDENCE TRAP =====\n";
    constexpr std::uint32_t FLAG = 0b0100;
    std::uint32_t flags = 0b0110;

    std::cout << "flags = 0b0110, FLAG = 0b0100\n";
    std::cout << "  flags & FLAG == 0    -> " << (flags & FLAG == 0)
              << "   ⚠️ GALAT (== pehle chala)\n";
    std::cout << "  (flags & FLAG) == 0  -> " << ((flags & FLAG) == 0)
              << "  ✅ SAHI\n";
    std::cout << "  (flags & FLAG) != 0  -> " << ((flags & FLAG) != 0)
              << "   ✅ flag set hai\n";

    std::cout << "\n===== 2. SHIFT PRECEDENCE =====\n";
    std::cout << "  1 << 2 + 3    -> " << (1 << 2 + 3)
              << "  ⚠️ (1 << 5), kyunki + pehle chala\n";
    std::cout << "  (1 << 2) + 3  -> " << ((1 << 2) + 3) << "   ✅\n";

    std::cout << "\n===== 3. ARITHMETIC PRECEDENCE =====\n";
    std::cout << "  2 + 3 * 4        -> " << (2 + 3 * 4) << "\n";
    std::cout << "  (2 + 3) * 4      -> " << ((2 + 3) * 4) << "\n";
    std::cout << "  10 - 5 - 2       -> " << (10 - 5 - 2) << "   (left-assoc)\n";
    std::cout << "  100 / 10 / 2     -> " << (100 / 10 / 2) << "    (left-assoc)\n";
    std::cout << "  2 + 10 % 3       -> " << (2 + 10 % 3) << "    (% pehle)\n";

    std::cout << "\n===== 4. LOGICAL MIX =====\n";
    const bool a = true, b = false, c = true;
    std::cout << "  a=true, b=false, c=true\n";
    std::cout << "  a || b && c      -> " << (a || b && c)
              << "   (&& pehle: a || (b&&c))\n";
    std::cout << "  (a || b) && c    -> " << ((a || b) && c) << "\n";
    std::cout << "  a || (b && c)    -> " << (a || (b && c)) << "   ✅ explicit\n";

    std::cout << "\n===== 5. UNARY (right-assoc) =====\n";
    int arr[] = {10, 20, 30};
    int* p = arr;
    std::cout << "  int arr[] = {10,20,30}; int* p = arr;\n";
    std::cout << "  *p++    -> " << *p++ << "   (value mili, phir p badha)\n";
    std::cout << "  *p ab   -> " << *p << "\n";
    p = arr;
    std::cout << "  *++p    -> " << *++p << "   (p badha, phir value)\n";

    std::cout << "\n===== 6. NEGATION =====\n";
    const int x = 5, y = 5;
    std::cout << "  x=5, y=5\n";
    std::cout << "  !x == y     -> " << (!x == y)
              << "  ⚠️ (!x)==y -> false==5 -> 0==5 -> false\n";
    std::cout << "  !(x == y)   -> " << !(x == y) << "  ✅ jo chahte the\n";

    std::cout << "\n===== 7. ASSIGNMENT (right-assoc) =====\n";
    int i, j, k;
    i = j = k = 7;
    std::cout << "  i = j = k = 7  ->  i=" << i << " j=" << j << " k=" << k << "\n";
    std::cout << "  (right-to-left: k=7, phir j=7, phir i=7)\n";

    std::cout << "\n===== 8. TERNARY =====\n";
    const bool cond = true;
    // std::cout << cond ? "yes" : "no";     // ❌ compile error
    std::cout << "  cout << cond ? \"y\" : \"n\";   -> COMPILE ERROR\n";
    std::cout << "  cout << (cond ? \"y\" : \"n\"); -> "
              << (cond ? "y" : "n") << "  ✅\n";

    return 0;
}
END
g++ -std=c++20 -Wall -Wextra precedence.cpp -o precedence && ./precedence
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`flags & MASK == 0` theek hai" | ❌ `==` pehle chalta hai. Brackets lagao |
| "`&` ki precedence `==` se zyada hai" | ❌ Ulta hai — historical mistake |
| "`1 << 2 + 3` = 7" | ❌ 32 — `+` pehle chalta hai |
| "`!x == y` = `!(x == y)`" | ❌ `(!x) == y` |
| "Brackets se code slow hota hai" | Zero cost — compiler ko farak nahi padta |

---

## Exercises

1. `precedence.cpp` chalao. Har trap samjho.

2. Bina chalaye predict karo:
   ```cpp
   std::cout << (2 + 3 * 4) << " ";
   std::cout << ((2 + 3) * 4) << " ";
   std::cout << (1 << 2 + 3) << " ";
   std::cout << ((1 << 2) + 3) << " ";
   std::cout << (10 - 5 - 2) << " ";
   std::cout << (2 + 10 % 3) << "\n";
   ```
   <details><summary>Answers</summary>`14 20 32 7 3 3`</details>

3. Yeh sab bugs fix karo:
   ```cpp
   if (flags & MASK == 0) { }
   if (a & b != c) { }
   std::cout << x < y;
   int v = 1 << n + 1;
   if (p || q && r) { }
   if (!a == b) { }
   ```
   <details><summary>Answers</summary>

   ```cpp
   if ((flags & MASK) == 0) { }
   if ((a & b) != c) { }
   std::cout << (x < y);
   int v = 1 << (n + 1);       // ya (1 << n) + 1 -- jo chahiye
   if (p || (q && r)) { }
   if (!(a == b)) { }
   ```
   </details>

4. `-Wparentheses` ke saath compile karo aur warnings dekho:
   ```cpp
   int flags = 6, MASK = 4;
   if (flags & MASK == 0) { }
   bool a = true, b = false, c = true;
   if (a || b && c) { }
   ```

5. `*p++`, `*++p`, `(*p)++` — teenon ka fark demonstrate karo.

6. Ek complex expression likho aur usko brackets se poori tarah explicit banao:
   ```cpp
   result = a + b * c - d / e % f << g & h;
   ```

---

## Interview questions

1. `flags & MASK == 0` mein kya problem hai?
2. Bitwise operators ki precedence kam kyun hai?
3. `*p++` aur `(*p)++` mein fark?
4. Kaunse operators right-associative hain?
5. `a || b && c` kaise parse hota hai?

---

## Next
→ [`10-evaluation-order.md`](10-evaluation-order.md)
