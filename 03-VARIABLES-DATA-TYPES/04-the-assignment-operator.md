# 04 — `=` assignment operator

## Prerequisites
`03-declaration-definition-initialization.md`

## Yeh topic abhi kyun
`=` ko log "barabar" padhte hain — aur wahi sabse badi confusion hai. C++ mein `=`
ka matlab **"barabar hai"** nahi, **"daalo"** hai.

Aur `=` vs `==` ki galti beginners ka #1 bug hai.

---

## `=` ka matlab: "assign karo"

```cpp
int x = 5;
```

Isko padho: **"x mein 5 daalo"** ya **"x ko 5 assign karo"**.

**Mat padho:** "x barabar 5" — yeh math ki soch hai, aur woh yahan galat hai.

---

## Math vs Programming

| Math mein | C++ mein |
|---|---|
| `x = 5` ek **statement of fact** hai | `x = 5;` ek **command** hai |
| `x = x + 1` **impossible** hai | `x = x + 1;` bilkul normal hai |
| `=` symmetric hai (`x = y` ⟺ `y = x`) | `=` **directional** hai (right → left) |
| Order matter nahi karta | Order sab kuch hai |

---

## Assignment kaise chalti hai

```cpp
x = expression;
```

**Hamesha yeh order:**
1. **Right side** evaluate karo → ek value milti hai
2. Us value ko **left side** ke box mein daalo

```cpp
int x = 10;
x = x + 5;
```

Trace:
```
   1. Right side: x + 5  ->  10 + 5  ->  15
   2. Left side mein daalo: x = 15
```

---

## `=` ka result bhi ek value hai

Yeh surprise hai: `=` ek **operator** hai, aur har operator ki ek value hoti hai.

```cpp
int a, b, c;
a = b = c = 5;      // chalta hai!
```

**Kaise?** `=` **right-to-left associative** hai:
```
   a = (b = (c = 5))
   
   c = 5  ->  result: 5
   b = 5  ->  result: 5
   a = 5
```

Sab 5 ho gaye.

```cpp
int x;
std::cout << (x = 10);      // 10 print hoga
```

**Note:** Yeh legal hai, par confusing hai. Real code mein use mat karo.

---

## ⚠️ `=` vs `==` — THE classic bug

```cpp
=       assignment    "daalo"
==      comparison    "kya barabar hai?"
```

### Ka bug

```cpp
int x = 5;

if (x = 10) {              // ⚠️ BUG! assignment, comparison nahi
    std::cout << "Yeh HAMESHA chalega\n";
}
std::cout << x;            // 10 -- x badal gaya!
```

**Kya hua:**
1. `x = 10` — assignment hui, `x` ab 10 hai
2. `=` ka result `10` hai
3. `if (10)` — non-zero = `true`
4. **Block hamesha chalega**, aur `x` corrupt ho gaya

### Sahi code
```cpp
if (x == 10) {             // ✅ comparison
    std::cout << "x is 10\n";
}
```

### Compiler madad karta hai
```bash
g++ -Wall -Wextra file.cpp -o file
```
```
warning: suggest parentheses around assignment used as truth value [-Wparentheses]
```

### Bug se bachne ke 3 tareeke

**1. Warnings ON rakho** (`-Wall`) — sabse important

**2. Yoda conditions** (purana trick)
```cpp
if (10 == x) { }      // agar galti se = likha, to compile error aayega
                      // kyunki `10 = x` invalid hai
```
Yeh kaam karta hai, par padhne mein ajeeb lagta hai. Modern compilers ke warnings
ke saath yeh zaroori nahi.

**3. Dhyaan se padho** — `if` ke andar `=` dikhe to ruko aur socho

---

## Compound assignment operators

Shortcuts jo bahut use hote hain:

```cpp
x += 5;      // x = x + 5
x -= 3;      // x = x - 3
x *= 2;      // x = x * 2
x /= 4;      // x = x / 4
x %= 3;      // x = x % 3

// Bitwise versions (folder 05 mein)
x &= mask;   // x = x & mask
x |= flag;   // x = x | flag
x ^= key;    // x = x ^ key
x <<= 2;     // x = x << 2
x >>= 1;     // x = x >> 1
```

### Yeh sirf shortcut nahi hai

```cpp
someVeryLongName[computeIndex()] += 5;

// vs

someVeryLongName[computeIndex()] = someVeryLongName[computeIndex()] + 5;
//                                                  ^^^^^^^^^^^^^^
//                                            computeIndex() DO BAAR chalega!
```

Compound assignment mein left side **ek hi baar** evaluate hota hai. Yeh:
- Tez hai
- Side-effects se bachata hai
- Padhne mein saaf hai

---

## Increment / Decrement

```cpp
x++;        // x = x + 1  (post-increment)
++x;        // x = x + 1  (pre-increment)
x--;        // x = x - 1
--x;        // x = x - 1
```

### Pre vs Post — fark kya hai?

Fark tab dikhta hai jab aap **result use karte ho**:

```cpp
int a = 5;
int b = a++;      // POST: pehle b ko a ki purani value do, phir a badhao
                  // b = 5, a = 6

int c = 5;
int d = ++c;      // PRE: pehle c badhao, phir d ko value do
                  // d = 6, c = 6
```

**Yaad rakhne ka tareeka:**
- `a++` — `a` **pehle** aata hai (purani value use hoti hai)
- `++a` — `++` **pehle** aata hai (increment pehle hoti hai)

### Kaunsa use karein?

```cpp
for (int i = 0; i < 10; i++)     // theek hai
for (int i = 0; i < 10; ++i)     // ⭐ better habit
```

`int` ke liye koi fark nahi (compiler optimize kar deta hai). Lekin **iterators aur
class objects** ke liye `++i` tez hota hai, kyunki `i++` ko purani value ki copy
banani padti hai.

**Aadat `++i` ki daalo.** Folder 19 (STL) mein yeh matter karega.

---

## ⚠️ Multiple modifications ka trap

```cpp
int i = 5;
i = i++ + ++i;      // ⚠️ UNDEFINED BEHAVIOUR (C++17 se pehle)
                    //    C++17 mein bhi confusing hai
```

**Aisa code kabhi mat likho.** Ek statement mein ek variable ko ek hi baar modify karo.

```cpp
int x = 5;
int y = x++ + x++;     // ⚠️ mat likho
```

C++17 ne evaluation order ke kuch rules fix kiye, par yeh ab bhi unreadable hai.
Folder 05 mein sequence points detail mein.

---

## `=` initialization vs assignment — ek zaroori fark

```cpp
int x = 5;      // INITIALIZATION -- object ban raha hai
x = 10;         // ASSIGNMENT -- existing object badal raha hai
```

**Built-in types ke liye fark nahi dikhta.** Lekin **classes ke liye yeh bilkul alag
functions call karta hai:**

```cpp
MyClass a = b;      // COPY CONSTRUCTOR chalega
MyClass c;
c = b;              // COPY ASSIGNMENT OPERATOR chalega
```

Yeh fark folder 18 (copy/move semantics) mein **bahut** important banega. Abhi bas
note kar lo.

---

## Hands-on

```bash
cd ~/cpp-practice
cat > assignment.cpp << 'END'
#include <iostream>

int main() {
    std::cout << "===== BASIC ASSIGNMENT =====\n";
    int x = 10;
    std::cout << "x = " << x << "\n";
    x = 20;
    std::cout << "x = " << x << "  (badal gaya)\n";

    std::cout << "\n===== SELF-REFERENCE =====\n";
    x = x + 5;
    std::cout << "x = x + 5  ->  " << x << "\n";
    x += 5;
    std::cout << "x += 5     ->  " << x << "\n";

    std::cout << "\n===== CHAINED ASSIGNMENT =====\n";
    int a, b, c;
    a = b = c = 7;
    std::cout << "a=" << a << " b=" << b << " c=" << c << "\n";

    std::cout << "\n===== PRE vs POST INCREMENT =====\n";
    int p = 5;
    int postResult = p++;
    std::cout << "int p = 5; int r = p++;  ->  r=" << postResult << " p=" << p << "\n";

    int q = 5;
    int preResult = ++q;
    std::cout << "int q = 5; int r = ++q;  ->  r=" << preResult << " q=" << q << "\n";

    std::cout << "\n===== THE = vs == BUG =====\n";
    int y = 5;
    std::cout << "y shuru mein: " << y << "\n";

    // Yeh JAAN-BOOJH KAR bug hai -- warning aayegi
    if (y = 10) {
        std::cout << "if (y = 10) chala -- yeh HAMESHA chalega!\n";
    }
    std::cout << "y ab: " << y << "  <- CORRUPT ho gaya!\n";

    // Sahi tareeka
    int z = 5;
    if (z == 10) {
        std::cout << "yeh nahi chalega\n";
    } else {
        std::cout << "if (z == 10) sahi se false hua, z = " << z << "\n";
    }

    std::cout << "\n===== COMPOUND OPERATORS =====\n";
    int n = 100;
    std::cout << "n = " << n << "\n";
    n += 50;  std::cout << "n += 50  ->  " << n << "\n";
    n -= 30;  std::cout << "n -= 30  ->  " << n << "\n";
    n *= 2;   std::cout << "n *= 2   ->  " << n << "\n";
    n /= 4;   std::cout << "n /= 4   ->  " << n << "\n";
    n %= 7;   std::cout << "n %= 7   ->  " << n << "\n";

    return 0;
}
END
g++ -std=c++20 -Wall -Wextra assignment.cpp -o assignment && ./assignment
```

**Warning dhyaan se padho** — compiler `if (y = 10)` pakad lega.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`=` ka matlab barabar hai" | `=` ka matlab "daalo" hai |
| "`x = y` aur `y = x` same hain" | Bilkul ulta result dete hain |
| "`x = x + 1` galat hai" | Bilkul valid — right pehle, phir left |
| "`i++` aur `++i` same hain" | Value use karne pe alag. Classes mein performance alag |
| "`if (x = 5)` compile error dega" | ❌ Nahi! Sirf warning. Silently galat chalega |

---

## Exercises

1. Predict karo:
   ```cpp
   int a = 5;
   int b = 10;
   a = b;
   b = 20;
   std::cout << a << " " << b;
   ```
   <details><summary>Answer</summary>`10 20` — `a = b` ne value copy ki, link nahi banaya</details>

2. Predict karo:
   ```cpp
   int x = 3;
   int y = x++;
   int z = ++x;
   std::cout << x << " " << y << " " << z;
   ```
   <details><summary>Answer</summary>
   `5 3 5`
   - `y = x++` → y=3, x=4
   - `z = ++x` → x=5, z=5
   </details>

3. Do variables swap karo bina teesra variable use kiye:
   ```cpp
   int a = 5, b = 10;
   // yahan code likho
   // a = 10, b = 5 hona chahiye
   ```
   <details><summary>Answers</summary>

   **Tareeka 1 (arithmetic):**
   ```cpp
   a = a + b;   // a = 15
   b = a - b;   // b = 15 - 10 = 5
   a = a - b;   // a = 15 - 5 = 10
   ```
   ⚠️ Overflow ka risk hai agar numbers bade hon.

   **Tareeka 2 (XOR):**
   ```cpp
   a = a ^ b;
   b = a ^ b;
   a = a ^ b;
   ```
   ⚠️ Agar `a` aur `b` same variable hon to 0 ban jaata hai.

   **Tareeka 3 (best):**
   ```cpp
   std::swap(a, b);      // <utility> — yahi use karo real code mein
   ```
   Interview mein tareeka 1/2 poochte hain, par production mein `std::swap` hi sahi hai.
   </details>

4. Yeh bug dhoondho:
   ```cpp
   int count = 0;
   if (count = 5) {
       std::cout << "count 5 hai\n";
   }
   ```
   Chalao aur dekho kya hota hai.

5. Compound assignment se yeh chhota karo:
   ```cpp
   total = total + price;
   total = total * quantity;
   index = index + 1;
   ```

6. Predict karo (mushkil):
   ```cpp
   int i = 0;
   int arr[5] = {10, 20, 30, 40, 50};
   std::cout << arr[i++] << " " << arr[i];
   ```
   <details><summary>Answer</summary>
   `10 20`. Pehle `arr[0]` = 10 print hua (post-increment ne purani value di),
   phir `i` 1 ho gaya, phir `arr[1]` = 20.

   ⚠️ Lekin yeh style **avoid karo** — evaluation order pe bharosa karna khatarnaak hai.
   </details>

---

## Interview questions

1. `=` aur `==` mein fark?
2. `i++` aur `++i` mein fark? Kaunsa tez hai aur kyun?
3. `a = b = c = 5;` kaise kaam karta hai?
4. `x += 5` aur `x = x + 5` mein koi fark hai?
5. `if (x = 5)` compile hoga? Kya karega?

---

## Next
→ [`05-int-deep-dive.md`](05-int-deep-dive.md)
