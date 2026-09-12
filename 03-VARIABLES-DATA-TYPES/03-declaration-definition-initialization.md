# 03 — Declaration vs Definition vs Initialization

## Prerequisites
`02-what-is-a-variable.md`

## Yeh topic abhi kyun
Yeh teen shabd log mila dete hain. Lekin C++ mein inka **bilkul alag** matlab hai —
aur yeh fark folder 08 (functions), 24 (linking), aur ODR mein critical banega.

Aur yeh interview question hai.

---

## Teenon ka ek-line matlab

| Shabd | Matlab | Memory allocate hoti hai? |
|---|---|---|
| **Declaration** | "Yeh naam exist karta hai, aur iska type X hai" | ❌ nahi (zaroori nahi) |
| **Definition** | "Yeh raha woh cheez, actually" | ✅ haan |
| **Initialization** | "Aur banate waqt uski value yeh hai" | (definition ka hissa) |

---

## Analogy: naya employee

- **Declaration:** HR ne bola "hamare paas ek 'Rahul' naam ka engineer hoga."
  (Abhi tak Rahul aaya nahi. Bas plan hai.)
- **Definition:** Rahul actually join kar gaya. Uska desk, laptop, ID card — sab mil gaya.
- **Initialization:** Join karte hi usko onboarding aur pehla project mil gaya.

---

## Variables ke liye

```cpp
int x;              // DECLARATION + DEFINITION (par NO initialization) ⚠️
int y = 5;          // DECLARATION + DEFINITION + INITIALIZATION ✅
extern int z;       // SIRF DECLARATION (memory kahin aur hai)
```

### `int x;`
- Compiler ko pata chal gaya ki `x` naam ka `int` hai → **declaration**
- 4 bytes reserve ho gaye → **definition**
- Koi value nahi di → **no initialization** → **garbage** ⚠️

### `int y = 5;`
- Teenon ek saath

### `extern int z;`
- Sirf declaration. "`z` naam ka `int` kahin aur define hua hai, linker dhoondh lega."
- Yahan koi memory allocate **nahi** hui

---

## `extern` ka example (multi-file)

**`globals.cpp`**
```cpp
int sharedCounter = 0;      // DEFINITION (memory yahan banti hai)
```

**`globals.h`**
```cpp
#pragma once
extern int sharedCounter;   // DECLARATION (sirf batata hai ki exist karta hai)
```

**`main.cpp`**
```cpp
#include <iostream>
#include "globals.h"        // declaration mil gaya

int main() {
    sharedCounter = 5;      // kaam karta hai! definition globals.cpp mein hai
    std::cout << sharedCounter << "\n";
}
```

```bash
g++ -std=c++20 main.cpp globals.cpp -o prog && ./prog
```

**Agar header mein `extern` hata do** (`int sharedCounter = 0;` likh do)?
→ Har `.cpp` mein definition ban jaayegi → **linker error: multiple definition**
(ODR violation — folder 24).

---

## Functions ke liye (preview)

```cpp
// DECLARATION (prototype) - "yeh function exist karta hai"
int add(int a, int b);

// DEFINITION - "aur yeh karta hai"
int add(int a, int b) {
    return a + b;
}
```

Yahi wajah hai ki header files mein declarations hoti hain aur `.cpp` mein definitions.

**Agar sirf declaration ho aur definition kahin na ho?**
→ Compile ho jaayega, **linker fail** karega: `undefined reference`.

(Yaad hai folder 02 lesson 10 ka ERROR 5? Yahi tha.)

---

## The rule

> **Ek cheez ki KAI declarations ho sakti hain,
> par sirf EK definition. (One Definition Rule — ODR)**

```cpp
int add(int, int);      // declaration
int add(int, int);      // ✅ dobara declaration - koi problem nahi
int add(int, int);      // ✅ teesri baar bhi theek hai

int add(int a, int b) { return a + b; }    // definition
int add(int a, int b) { return a - b; }    // ❌ ERROR: redefinition
```

---

## Initialization ke tareeke (quick preview)

```cpp
int a = 5;          // copy initialization (C se aaya)
int b(5);           // direct initialization
int c{5};           // brace / list initialization (C++11) ⭐ RECOMMENDED
int d = {5};        // copy-list initialization
int e{};            // value initialization -> 0
int f = 5, g = 10;  // ek line mein do
```

Poora detail file 10 mein. Abhi bas jaan lo ki `{}` best hai.

### `int e{};` kya karta hai?

```cpp
int a;       // ⚠️ garbage (local variable)
int b{};     // ✅ 0
int c = 0;   // ✅ 0
```

`{}` khali chhodne se **value-initialization** hoti hai:
- Numbers → `0`
- Pointers → `nullptr`
- `bool` → `false`
- Classes → default constructor chalta hai

Yeh bahut kaam ki cheez hai — uninitialized variable ka risk khatam.

---

## Default initialization ke rules (important table)

| Kahan | Kya hota hai |
|---|---|
| **Local variable** (`int x;` function ke andar) | ⚠️ **garbage** (indeterminate) |
| **Global variable** (`int x;` file scope pe) | ✅ zero |
| **`static` variable** (local ya global) | ✅ zero |
| **`thread_local`** | ✅ zero |
| **Class member** (bina initializer ke) | ⚠️ depends — built-in types garbage |
| **`new int`** | ⚠️ garbage |
| **`new int()`** ya **`new int{}`** | ✅ zero |

```cpp
#include <iostream>

int globalX;                    // ✅ 0

int main() {
    int localX;                 // ⚠️ garbage
    static int staticX;         // ✅ 0
    int* p1 = new int;          // ⚠️ garbage
    int* p2 = new int{};        // ✅ 0

    std::cout << "global: " << globalX  << "\n";
    std::cout << "static: " << staticX  << "\n";
    std::cout << "local:  " << localX   << "  <- garbage!\n";
    std::cout << "new:    " << *p1      << "  <- garbage!\n";
    std::cout << "new{}:  " << *p2      << "\n";

    delete p1;
    delete p2;
}
```

> **HFT relevance:** Uninitialized memory reads ek classic production bug hain.
> Woh testing mein "kaam karte" dikhte hain (kyunki memory aksar 0 hoti hai), aur
> production mein random values dete hain. `-fsanitize=memory` (MSan) ya Valgrind
> se pakde jaate hain. Isliye rule: **hamesha initialize karo.**

---

## Declaration se pehle use — allowed nahi

```cpp
int main() {
    std::cout << x;      // ❌ ERROR: 'x' was not declared in this scope
    int x = 5;
}
```

C++ **top-to-bottom** padhta hai. Naam use karne se pehle declare hona chahiye.

**Exception: class members**
```cpp
class Foo {
    void method() { helper(); }     // ✅ chalta hai!
    void helper() { }               // baad mein declare hua
};
```
Class ke andar compiler poori class pehle padhta hai. Folder 15 mein.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Declaration aur definition same hain" | Definition memory allocate karti hai, declaration nahi |
| "`int x;` mein x = 0" | Local mein garbage, global mein 0 |
| "`extern` variable banata hai" | Nahi, sirf batata hai ki woh kahin aur hai |
| "Ek cheez ki ek hi declaration ho sakti hai" | Kai declarations OK, ek hi definition |
| "`new int` zero deta hai" | Nahi! `new int{}` deta hai |

---

## Exercises

1. In mein se kaunsa declaration hai, kaunsa definition, kaunsa dono?
   ```cpp
   a) int x;
   b) int y = 10;
   c) extern int z;
   d) int add(int, int);
   e) int add(int a, int b) { return a+b; }
   f) class Foo;
   g) class Foo { int x; };
   ```
   <details><summary>Answers</summary>
   a) dono (definition, no init)
   b) dono + initialization
   c) sirf declaration
   d) sirf declaration
   e) definition
   f) sirf declaration (forward declaration)
   g) definition
   </details>

2. Multi-file `extern` example khud banao aur chalao.

3. Ab header mein `extern` hata do aur dobara compile karo. Kya error aaya? Kis stage ka?

4. Default initialization test chalao (upar wala program). Har run mein `local` ki
   value note karo — badalti hai?

5. Yeh code mein bug hai. Batao kya:
   ```cpp
   #include <iostream>
   int main() {
       int sum;
       for (int i = 1; i <= 10; ++i) {
           sum += i;
       }
       std::cout << sum << "\n";
   }
   ```
   <details><summary>Answer</summary>
   `sum` initialize nahi hua! `sum += i` garbage value pe add kar raha hai.
   Result random hoga. Fix: `int sum = 0;` ya `int sum{};`

   `-Wall` isko `-Wuninitialized` se pakad leta hai (`-O1` ya upar ke saath).
   </details>

6. Ek function declare karo par define mat karo. Use call karo. Kya error aaya?

---

## Interview questions

1. Declaration aur definition mein fark?
2. ODR kya hai?
3. `extern` kya karta hai?
4. Local aur global uninitialized variables mein kya fark hai?
5. `new int` aur `new int{}` mein fark?

---

## Next
→ [`04-the-assignment-operator.md`](04-the-assignment-operator.md)
