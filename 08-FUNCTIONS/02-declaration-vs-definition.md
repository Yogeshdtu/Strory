# 02 — Declaration vs definition, header/source split

## Prerequisites
- [`01-what-is-a-function.md`](01-what-is-a-function.md)
- `02-CPP-FIRST-STEPS/03-include-and-headers.md`
- `02-CPP-FIRST-STEPS/09-compilation-pipeline-revisited.md`
- `03-VARIABLES-DATA-TYPES/03-declaration-definition-initialization.md`

## Yeh topic abhi kyun
`add(2, 3)` likhne se pehle compiler ko pata hona chahiye ki `add` **exist karta
hai** aur uski **shakl kya hai** (return type, parameters). Yeh **declaration**
hai. Actual code **definition** hai. Yeh do alag cheezein hain — aur inhe alag
files (header/source) mein rakhna real C++ projects ka basic structure hai.

Yeh linker errors (`undefined reference`), ODR, aur build systems (folder 24) ka
foundation hai.

---

## Declaration — "aisa function hai"

Sirf signature + `;`. Koi body nahi.

```cpp
int add(int a, int b);              // declaration (prototype)
double average(const std::vector<int>& v);
void log(std::string_view msg);
```

Compiler ise dekh ke: "theek, `add` do `int` leta hai, `int` deta hai — call
type-check kar sakta hoon." Body kahan hai, ye linker ka kaam.

Parameter naam **optional** hain declaration mein (documentation ke liye rakhna
achha):

```cpp
int add(int, int);                  // valid
int add(int a, int b);              // better -- naam batate hain kya kya
```

## Definition — "aur yeh raha uska code"

```cpp
int add(int a, int b) {             // definition -- body ke saath
    return a + b;
}
```

Definition **bhi ek declaration hai** (usme signature to hai). Isli ye:

```cpp
int add(int a, int b) { return a + b; }   // definition + declaration dono
add(2, 3);                                 // ✅ pehle define ho chuka
```

---

## Declare-before-use

Compiler file **upar se neeche** padhta hai. Call se **pehle** declaration
(ya definition) chahiye:

```cpp
int main() {
    std::cout << twice(5);          // ❌ ERROR -- 'twice' abhi tak nahi dekha
}
int twice(int x) { return x * 2; }
```

Do fix:

```cpp
// Fix 1: definition ko upar le jao
int twice(int x) { return x * 2; }
int main() { std::cout << twice(5); }

// Fix 2: declaration upar, definition kahin bhi (bada codebase style)
int twice(int x);                    // declaration
int main() { std::cout << twice(5); }
int twice(int x) { return x * 2; }   // definition -- main ke baad bhi chalega
```

**Fix 2 zaroori hota hai** jab do functions ek doosre ko call karein
(mutual recursion — folder 09), ya definition alag file mein ho.

---

## ODR — One Definition Rule

| Cheez | Rule |
|---|---|
| **Declaration** | jitni baar chahe (har TU mein, har file mein) — bilkul theek |
| **Definition** | poore program mein **exactly ek** (non-`inline` functions ke liye) |

```cpp
int add(int, int);          // 100 baar declare karo -- OK
int add(int, int);          // OK

int add(int a, int b) { return a + b; }   // ek baar
int add(int a, int b) { return a - b; }   // ❌ ODR violation -> linker error
```

`inline` functions aur templates ODR ke exceptions hain (lesson 10) — isi liye
woh header mein reh sakte hain.

---

## Header / source split — real project structure

```
math.hpp    <- DECLARATIONS (aur inline/template definitions)
math.cpp    <- DEFINITIONS
main.cpp    <- #include "math.hpp", functions use karo
```

### `math.hpp`
```cpp
#pragma once                        // ya include guard -- ek baar include ho

int add(int a, int b);
double average(const std::vector<int>& v);
```

### `math.cpp`
```cpp
#include "math.hpp"                 // apna header khud bhi include karo (consistency check)

int add(int a, int b) { return a + b; }

double average(const std::vector<int>& v) {
    if (v.empty()) return 0.0;
    long long s = 0;
    for (int x : v) s += x;
    return static_cast<double>(s) / static_cast<double>(v.size());
}
```

### `main.cpp`
```cpp
#include "math.hpp"
#include <iostream>
int main() { std::cout << add(2, 3) << "\n"; }
```

### Build
```bash
g++ -std=c++20 -c math.cpp -o math.o      # math.cpp -> math.o
g++ -std=c++20 -c main.cpp -o main.o      # main.cpp -> main.o (add() declaration se satisfied)
g++ math.o main.o -o app                   # LINKER add() ki definition math.o mein dhoondhta hai
```

### Kyun alag

| | Header (`.hpp`) | Source (`.cpp`) |
|---|---|---|
| Kya | declarations, inline/`constexpr` fns, templates | definitions |
| `#include` hota hai | haan, kai jagah | nahi (khud compile hota hai) |
| Badla to | **saare** includers recompile | sirf woh ek `.cpp` recompile |
| Purpose | "interface" — kya use kar sakte ho | "implementation" — kaise kaam karta hai |

Ek 500-file project mein ek `.cpp` badalne pe sirf woh recompile — headers change
mahaenge hote hain.

---

## `undefined reference` — sabse common linker error

```cpp
int compute(int x);                 // declaration hai
int main() { return compute(5); }   // call karta hai
// ...compute ki DEFINITION kahin nahi...
```

```
/usr/bin/ld: main.o: undefined reference to `compute(int)'
collect2: error: ld returned 1 exit status
```

- Compiler **khush** tha (declaration mil gayi)
- **Linker** fail — definition kisi bhi `.o` / library mein nahi mili

Wajah: definition likhna bhool gaye · `.cpp` ko build mein add karna bhool gaye ·
signature mismatch (declaration `int compute(int)`, definition `int compute(long)`
— alag function!) · library link karna bhool gaye (`-lm`, `-lpthread`).

(Folder 02 file 10 mein compile stages, folder 24 mein linking poora.)

---

## Hands-on

Teen file banao (`math.hpp`, `math.cpp`, `main.cpp`) upar wale code se. Separately
compile karke link karo. Phir:
1. `math.cpp` ko build se hataao → `undefined reference` dekho
2. `math.hpp` mein `add` ka declaration `int add(int)` kar do (1 param) → mismatch error

`examples/01_first_functions.cpp` mein `isPrime` — declaration upar, definition
`main()` ke baad. Woh pattern dekho.

---

## ⚠️ Traps

### Trap 1 — declaration aur definition ka signature mismatch
```cpp
// header
int scale(int value, int factor);
// source
int scale(int value, long factor) { ... }   // ⚠️ alag function! header wala undefined
```

### Trap 2 — header mein non-inline function ki definition
```cpp
// util.hpp
int helper() { return 42; }         // ⚠️ 2 .cpp ne include kiya -> 2 definitions -> ODR/linker error
```
Fix: `inline int helper() { return 42; }` ya definition `.cpp` mein.

### Trap 3 — include guard bhoolna
```cpp
// bina #pragma once -- do baar include -> "redefinition" errors
```

### Trap 4 — `.cpp` ko `#include` karna
```cpp
#include "math.cpp"                  // ⚠️ galat -- definitions duplicate ho jaayengi
#include "math.hpp"                  // ✅
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Declaration aur definition same" | Declaration = shakl; definition = code + shakl |
| "Function call se pehle definition chahiye" | Declaration kaafi (definition kahin bhi / doosri file) |
| "`undefined reference` = compile error" | **Linker** error — definition nahi mili |
| "Sab kuch ek file mein theek hai" | Chhote programs haan; bade projects header/source split |
| "Header mein function define kar do" | Sirf `inline` / `constexpr` / template; warna ODR violation |

---

## Exercises

1. **3-file project:** `geometry.hpp` (`double areaCircle(double r);`,
   `double areaRect(double w, double h);`), `geometry.cpp` (definitions),
   `main.cpp` (use). Separately compile + link.

2. **Break the link:** exercise 1 mein `geometry.cpp` ko link se hataao.
   Exact error message likho. Ab wapas add karo.

3. **Signature mismatch:** header mein `double areaRect(double, double);`,
   source mein `double areaRect(int, int) { ... }`. Compile hua? Link hua?

4. **Header pollution:** `util.hpp` mein `int magic() { return 42; }` (bina
   `inline`). 2 `.cpp` files se include karo, dono ko link karo. Error? Ab
   `inline` lagao.

5. **Forward declaration:** 2 functions `isEven`/`isOdd` jo ek doosre ko call
   karte hain (`isEven(n)` → `n == 0 ? true : isOdd(n-1)`). Bina forward
   declaration ke compile hoga? Fix.

6. **Declare-before-use:** `examples/01_first_functions.cpp` mein `isPrime` ki
   declaration hata do (definition `main()` ke baad hai). Error kya?

---

## Interview questions

1. Declaration aur definition mein fark? Definition bhi declaration hai?
2. ODR kya hai? Declaration aur definition pe alag kaise lagta hai?
3. `undefined reference` error kis stage ka? 3 common wajah?
4. Header aur source file mein kya-kya jaata hai? Alag kyun?
5. Header mein normal function define karne pe kya hota hai? Fix?
6. Forward declaration kab zaroori hoti hai?

---

## Next
→ [`03-parameters-and-arguments.md`](03-parameters-and-arguments.md)
