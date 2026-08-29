# 02 — Increment aur decrement

## Prerequisites
`01-arithmetic-operators.md`, folder 03 file 04 (assignment)

## Yeh topic abhi kyun
`++` aur `--` har loop mein dikhte hain. Aur `++i` vs `i++` ka fark interview mein
poocha jaata hai — aur STL iterators (folder 19) ke saath yeh **performance** ka
sawal ban jaata hai.

---

## Chaar forms

```cpp
int i = 5;

++i;        // PRE-increment:  pehle badhao, phir value do
i++;        // POST-increment: pehle value do, phir badhao
--i;        // PRE-decrement
i--;        // POST-decrement
```

---

## Fark tab dikhta hai jab result USE karo

```cpp
int a = 5;
int x = a++;          // POST: x ko a ki PURANI value mili
                      // x = 5, a = 6

int b = 5;
int y = ++b;          // PRE: pehle b badha, phir y ko value mili
                      // y = 6, b = 6
```

### Yaad rakhne ka tareeka

```
   a++     ->  `a` PEHLE aata hai  ->  purani value use hoti hai
   ++a     ->  `++` PEHLE aata hai ->  increment pehle hota hai
```

### Agar result use nahi karte?

```cpp
i++;        // koi fark nahi
++i;        // koi fark nahi
```

Standalone statement mein `int` ke liye **bilkul same** hai. Compiler dono ke liye
identical code banata hai.

---

## Andar kya hota hai

### Pre-increment (`++i`)
```cpp
// Roughly:
int& operator++() {
    value += 1;
    return *this;        // ✅ khud ko return karta hai -- koi copy nahi
}
```

### Post-increment (`i++`)
```cpp
// Roughly:
int operator++(int) {    // `int` parameter dummy hai, sirf overload distinguish karne ko
    int old = *this;     // ⚠️ COPY banani padti hai
    value += 1;
    return old;          // ⚠️ purani copy return
}
```

**Post-increment ko purani value ki copy banani padti hai.**

---

## 🔑 Isliye `++i` prefer karo

### `int` ke liye — koi fark nahi
```cpp
for (int i = 0; i < n; i++)      // compiler optimize kar deta hai
for (int i = 0; i < n; ++i)      // same machine code
```

Verify karo godbolt pe — `-O2` pe assembly identical hoti hai.

### Iterators aur objects ke liye — **fark padta hai**
```cpp
std::vector<std::string> v;

for (auto it = v.begin(); it != v.end(); it++) { }   // ⚠️ har baar iterator COPY
for (auto it = v.begin(); it != v.end(); ++it) { }   // ✅ koi copy nahi
```

Simple iterators ke liye compiler aksar copy elide kar deta hai. Par **complex
iterators** (nested containers, custom iterators, debug builds) mein cost real hoti hai.

### Aadat banao
> **Hamesha `++i` likho jab tak `i++` ki purani value chahiye na ho.**

Yeh muft ki aadat hai — kabhi nuksaan nahi karti, kabhi kabhi faayda karti hai.

---

## Loop patterns

```cpp
// Standard
for (int i = 0; i < n; ++i) { }

// Reverse
for (int i = n - 1; i >= 0; --i) { }

// ⚠️ Reverse with unsigned -- INFINITE LOOP
for (std::size_t i = n - 1; i >= 0; --i) { }    // ❌ size_t kabhi < 0 nahi hota
                                                 //    aur n=0 pe n-1 wrap ho jaata hai

// ✅ Reverse with unsigned -- sahi tareeke
for (std::size_t i = n; i-- > 0; ) { }          // "goes to" idiom
for (std::size_t i = n; i > 0; --i) {
    auto& item = arr[i - 1];                    // 1 se shift karo
}
```

### The "goes to" operator (joke, par useful idiom)
```cpp
for (int i = n; i --> 0; ) { }
//              ^^^^  yeh `-->` operator nahi hai!
//              Actually: i-- > 0     (post-decrement, phir compare)
```

---

## ⚠️ Multiple modifications = UB

```cpp
int i = 5;
i = i++ + ++i;              // ⚠️ UNDEFINED BEHAVIOUR (C++17 se pehle)
                            //    C++17 mein bhi confusing hai

int j = 5;
int k = j++ + j++;          // ⚠️ mat likhna
arr[i] = i++;               // ⚠️ mat likhna
f(i++, i++);                // ⚠️ argument order unspecified
```

**Rule: ek statement mein ek variable ko sirf ek baar modify karo.**

C++17 ne kuch evaluation order rules fix kiye, par yeh code ab bhi **unreadable**
hai. Poora detail file 10 mein.

---

## Pointers ke saath

```cpp
int arr[] = {10, 20, 30};
int* p = arr;

std::cout << *p++;      // 10 print, phir p aage badha
                        // (`++` ki precedence `*` se zyada hai, par POST hai)

std::cout << *++p;      // p aage badha, phir print

std::cout << (*p)++;    // *p ki value print, phir us VALUE ko badha
```

**Yeh confusing hai.** Brackets use karo:
```cpp
std::cout << *(p++);    // clear
std::cout << *(++p);    // clear
```

---

## Custom types ke saath (preview)

```cpp
class Counter {
    int value_ = 0;
public:
    // PRE-increment: ++c
    Counter& operator++() {
        ++value_;
        return *this;              // ✅ reference, koi copy nahi
    }

    // POST-increment: c++
    Counter operator++(int) {      // dummy `int` parameter
        Counter old = *this;       // ⚠️ COPY
        ++value_;
        return old;                // ⚠️ copy return
    }
};
```

**Convention:**
- Pre-increment → `T&` return karo
- Post-increment → `T` (copy) return karo, aur usse pre-increment se implement karo

Detail folder 15 (operator overloading) mein.

---

## `bool` ke saath

```cpp
bool flag = false;
flag++;          // ⚠️ C++17 se REMOVED (deprecated tha C++98 se)
++flag;          // ⚠️ C++17 se REMOVED

flag = true;     // ✅ yahi karo
flag = !flag;    // ✅ toggle
```

`bool` pe `++` ka koi sensible meaning nahi tha, isliye hata diya gaya.
(`--` to kabhi allowed hi nahi tha.)

---

## Hands-on

```bash
cd ~/cpp-practice
cat > incdec.cpp << 'END'
#include <iostream>
#include <vector>
#include <string>

int main() {
    std::cout << "===== PRE vs POST =====\n";
    int a = 5;
    int x = a++;
    std::cout << "int a=5; int x = a++;  ->  x=" << x << " a=" << a
              << "   (POST: purani value mili)\n";

    int b = 5;
    int y = ++b;
    std::cout << "int b=5; int y = ++b;  ->  y=" << y << " b=" << b
              << "   (PRE: nayi value mili)\n";

    std::cout << "\n===== STANDALONE -- KOI FARK NAHI =====\n";
    int c = 5, d = 5;
    c++;
    ++d;
    std::cout << "c++ ke baad: " << c << ",  ++d ke baad: " << d
              << "   (dono same)\n";

    std::cout << "\n===== ARRAY INDEXING =====\n";
    int arr[] = {10, 20, 30, 40, 50};
    int i = 0;
    std::cout << "arr[i++] = " << arr[i++] << "  (arr[0] mila, ab i=" << i << ")\n";
    std::cout << "arr[++i] = " << arr[++i] << "  (i pehle 2 hua, phir arr[2])\n";

    std::cout << "\n===== POINTERS =====\n";
    int* p = arr;
    std::cout << "*p       = " << *p << "\n";
    std::cout << "*(p++)   = " << *(p++) << "   (value mili, phir p badha)\n";
    std::cout << "*p ab    = " << *p << "\n";
    std::cout << "*(++p)   = " << *(++p) << "   (p pehle badha, phir value)\n";

    std::cout << "\n===== REVERSE LOOPS =====\n";
    const int n = 5;

    std::cout << "signed reverse:   ";
    for (int k = n - 1; k >= 0; --k) std::cout << k << " ";
    std::cout << "\n";

    std::cout << "unsigned 'goes to': ";
    for (std::size_t k = n; k-- > 0; ) std::cout << k << " ";
    std::cout << "\n";

    std::cout << "unsigned shifted:   ";
    for (std::size_t k = n; k > 0; --k) std::cout << (k - 1) << " ";
    std::cout << "\n";

    std::cout << "\n(⚠️ `for (size_t k = n-1; k >= 0; --k)` INFINITE hota --\n";
    std::cout << "    size_t kabhi negative nahi ho sakta)\n";

    std::cout << "\n===== ITERATORS =====\n";
    std::vector<std::string> v = {"alpha", "beta", "gamma"};
    std::cout << "++it (recommended): ";
    for (auto it = v.begin(); it != v.end(); ++it) std::cout << *it << " ";
    std::cout << "\n";
    std::cout << "(it++ bhi chalta, par iterator ki COPY banati hai)\n";

    return 0;
}
END
g++ -std=c++20 -Wall -Wextra incdec.cpp -o incdec && ./incdec
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`i++` aur `++i` hamesha same hain" | Value use karne pe alag |
| "`++i` `int` ke liye tez hai" | Compiler optimize kar deta hai — same code |
| "`i++` iterators ke liye bhi same hai" | Copy banata hai — `++it` prefer karo |
| "`i = i++ + ++i` clever hai" | ⚠️ UB / unreadable — kabhi mat likhna |
| "`for (size_t i = n-1; i >= 0; --i)` theek hai" | ❌ Infinite loop |

---

## Exercises

1. Predict karo:
   ```cpp
   int i = 3;
   std::cout << i++ << " " << i << " " << ++i << " " << i;
   ```
   <details><summary>Answer</summary>
   ⚠️ **Trick question.** Yeh technically ek hi statement hai jisme `i` kai baar
   modify ho raha hai — evaluation order unspecified hai (C++17 mein `<<` ka order
   left-to-right guaranteed hai, par phir bhi confusing).

   C++17 mein: `3 4 5 5`

   **Lekin aisa code kabhi mat likhna.** Alag statements mein todo.
   </details>

2. Predict karo (yeh safe hai):
   ```cpp
   int a = 3;
   int b = a++;
   int c = ++a;
   std::cout << a << " " << b << " " << c;
   ```
   <details><summary>Answer</summary>`5 3 5`</details>

3. Infinite loop bug reproduce karo:
   ```cpp
   std::size_t n = 5;
   for (std::size_t i = n - 1; i >= 0; --i) {
       std::cout << i << " ";
   }
   ```
   ⚠️ `Ctrl+C` se rokna. Phir 2 tareekon se fix karo.

4. Ek `Counter` class banao pre aur post increment ke saath. Trace karo ki
   post-increment mein copy banti hai (constructor mein print karo).

5. Godbolt pe verify karo ki `int` ke liye `++i` aur `i++` same assembly dete hain
   (`-O2` pe).

6. `bool` pe `++` try karo — kya error aaya?

---

## Interview questions

1. `++i` aur `i++` mein fark?
2. Kaunsa tez hai aur kyun?
3. `i = i++ + ++i` mein kya problem hai?
4. Post-increment ko kaise implement karte hain?
5. Reverse loop unsigned index ke saath kaise likhein?

---

## Next
→ [`03-comparison-operators.md`](03-comparison-operators.md)
