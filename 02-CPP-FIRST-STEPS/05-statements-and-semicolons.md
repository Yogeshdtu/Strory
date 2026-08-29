# 05 — Statements aur semicolons

## Prerequisites
`02-anatomy-line-by-line.md`

## Yeh topic abhi kyun
Har C++ line mein `;` hota hai. Beginner iska matlab samjhe bina bas laga deta hai —
aur phir galat jagah laga deta hai. Yeh 10 minute ki file aapke bahut se future bugs
bacha degi.

---

## Statement kya hai?

**Statement = ek complete instruction.**

Hindi mein ek vaakya jaisa. "Main ghar ja raha hoon." — poora vaakya, poorna viram
(`.`) ke saath.

C++ mein:
```cpp
std::cout << "Hi";      // ek statement, ; ke saath
```

---

## `;` ka asli kaam

`;` compiler ko batata hai: **"yeh statement yahan khatam."**

C++ mein **newline ka koi matlab nahi hai**. Compiler ke liye yeh sab bilkul same hai:

```cpp
int x = 5;
```
```cpp
int
x
=
5
;
```
```cpp
int x = 5; int y = 10; int z = 15;
```

Sirf `;` batata hai statement kahan khatam hui.

**Yeh Python se bilkul ulta hai** — wahan newline hi statement khatam karta hai.

---

## Statement ke types

### 1. Expression statement
Ek expression + `;`
```cpp
x = 5;
std::cout << "hi";
someFunction();
x++;
```

### 2. Declaration statement
Naya naam banana
```cpp
int x;
int y = 10;
std::string name = "Rahul";
```

### 3. Compound statement (block)
`{ }` mein statements ka group. **Iske baad `;` nahi lagta.**
```cpp
{
    int a = 1;
    int b = 2;
}
```

### 4. Selection statement
```cpp
if (x > 5) { }
switch (x) { }
```

### 5. Iteration statement
```cpp
while (x < 10) { }
for (int i = 0; i < 10; i++) { }
```

### 6. Jump statement
```cpp
return 0;
break;
continue;
goto label;      // exist karta hai, par mat use karo
```

### 7. Null statement (khali statement)
```cpp
;               // kuch nahi karta, par legal hai
```
Yeh chhota sa cheez bade bugs ka source hai — neeche dekho.

---

## `;` kahan lagta hai — poori list

### ✅ LAGTA HAI

```cpp
int x = 5;                          // declaration
x = 10;                             // assignment
std::cout << "hi";                  // expression
return 0;                           // return
someFunction();                     // function call
x++;                                // increment

// 🔑 SURPRISE: class/struct definition ke baad LAGTA hai
class MyClass {
    int x;
};                                  // <-- yeh ; ZAROORI hai

struct Point {
    int x, y;
};                                  // <-- yeh bhi

enum Color { RED, GREEN };          // <-- yeh bhi

// do-while ke baad bhi
do {
    x++;
} while (x < 10);                   // <-- yeh bhi
```

### ❌ NAHI LAGTA

```cpp
#include <iostream>                 // preprocessor directive
#define MAX 100                     // preprocessor directive

int main() {                        // function definition
    // ...
}                                   // <-- yahan ; nahi

if (x > 5) {
    // ...
}                                   // <-- yahan ; nahi

for (int i = 0; i < 10; i++) {
    // ...
}                                   // <-- yahan ; nahi

while (x < 10) {
    // ...
}                                   // <-- yahan ; nahi

namespace MyNS {
    // ...
}                                   // <-- yahan ; nahi
```

---

## 🔑 Class ke baad `;` kyun lagta hai?

Yeh C se aayi legacy hai. C mein aap yeh kar sakte the:

```cpp
struct Point { int x, y; } p1, p2;      // struct define + variables declare, ek saath!
                                        //                          ^^^^^^^^ ^
```

Matlab `struct Point { ... }` ek **type specifier** hai, aur uske baad variable names
aa sakte hain. Isliye `;` chahiye — declaration khatam karne ke liye.

Function definition ke saath aisa nahi ho sakta, isliye wahan `;` nahi.

**Yaad rakhne ka tareeka:**
- Kuch **define** kar rahe ho jo ek *type* hai (`class`, `struct`, `enum`, `union`) → `;` lagao
- Kuch **define** kar rahe ho jo *code* hai (function, if, loop) → `;` mat lagao

---

## ⚠️ Bug #1: Extra semicolon after `if`

```cpp
if (x > 5);                       // <-- yeh ; ne sab barbaad kar diya
    std::cout << "bada hai\n";    // yeh HAMESHA chalega
```

**Compiler ne isko aise padha:**
```cpp
if (x > 5)
    ;                             // null statement - if ka body yahi hai!

std::cout << "bada hai\n";        // yeh if ke bahar hai
```

**Yeh compile ho jaata hai. Koi error nahi.** Bas galat kaam karta hai.

`-Wall` isko pakad leta hai:
```
warning: suggest braces around empty body in an 'if' statement
```

**Isliye warnings hamesha ON.**

---

## ⚠️ Bug #2: Extra semicolon after loop

```cpp
for (int i = 0; i < 10; i++);      // <-- yeh ;
    std::cout << i << "\n";        // yeh loop ke BAHAR hai (aur compile bhi nahi hoga,
                                   // kyunki i scope mein nahi hai)
```

```cpp
int sum = 0;
for (int i = 1; i <= 100; i++);    // <-- loop 100 baar KHALI chala
    sum += i;                      // yeh sirf EK baar chala, aur i undefined hai
```

---

## ⚠️ Bug #3: Missing semicolon — confusing error

```cpp
int x = 5
int y = 10;
```

**Error:**
```
error: expected ',' or ';' before 'int'
```

Note: error **line 2** pe dikha raha hai, jabki galti **line 1** pe hai.

**Rule:** Jab error samajh na aaye, **upar wali line dekho.** Missing `;` ka error
hamesha agli line pe dikhta hai.

---

## ⚠️ Bug #4: Missing semicolon after class

```cpp
class Order {
    int price;
}                                  // <-- ; nahi hai

int main() { return 0; }
```

**Error** (yeh bahut confusing hota hai):
```
error: expected ';' after class definition
```
Ya kabhi kabhi:
```
error: two or more data types in declaration of 'main'
```

Kyunki compiler ne socha aap `class Order { ... } main` likh rahe ho — matlab `main`
naam ka `Order` type ka variable!

---

## Whitespace ki kahani

Compiler ke liye yeh sab **bilkul same** hain:

```cpp
int x=5;
int x = 5;
int    x    =    5    ;
int
x
=
5
;
```

Toh phir spacing kyun karein? **Insaanon ke liye.**

### Achha style
```cpp
int main() {
    int price = 100;
    int qty = 50;
    int total = price * qty;
    
    if (total > 1000) {
        std::cout << "Bada order\n";
    }
    
    return 0;
}
```

### Bura style (legal, par mat likhna)
```cpp
int main(){int price=100;int qty=50;int total=price*qty;if(total>1000){std::cout<<"Bada order\n";}return 0;}
```

Dono compile honge. Ek ko 6 mahine baad aap padh paoge, doosre ko nahi.

### Indentation conventions

Popular styles:
```cpp
// Allman
int main()
{
    // ...
}

// K&R / 1TBS (yeh course use karta hai)
int main() {
    // ...
}
```

Kaunsa "sahi" hai? **Koi nahi.** Bas **consistent** raho. Team ke style ko follow karo.

**Tool:** `clang-format` yeh apne aap kar deta hai:
```bash
clang-format -i myfile.cpp        # file ko format kar do
```

---

## Ek statement, kai lines

Lambi statement ko todna bilkul theek hai:

```cpp
std::cout << "Naam: " << name
          << ", Umar: " << age
          << ", Sheher: " << city
          << "\n";
```

Ek hi statement hai (ek `;`), par 4 lines mein. Padhne mein aasan.

---

## Hands-on

```bash
cd ~/cpp-practice
cat > semicolons.cpp << 'END'
#include <iostream>

int main() {
    int x = 10;

    // ===== BUG 1: extra semicolon after if =====
    if (x > 5);
        std::cout << "Yeh HAMESHA print hoga (bug!)\n";

    // ===== Sahi version =====
    if (x > 100) {
        std::cout << "Yeh print NAHI hoga\n";
    }

    // ===== BUG 2: extra semicolon after for =====
    int count = 0;
    for (int i = 0; i < 5; i++);
        count++;
    std::cout << "count = " << count << " (5 hona chahiye tha, par 1 hai)\n";

    // ===== Sahi version =====
    int count2 = 0;
    for (int i = 0; i < 5; i++) {
        count2++;
    }
    std::cout << "count2 = " << count2 << " (sahi!)\n";

    return 0;
}
END
g++ -std=c++20 -Wall -Wextra semicolons.cpp -o semicolons
./semicolons
```

**Warnings dhyaan se padho.** Compiler aapko dono bugs bata raha hai.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Har line ke end mein `;`" | Har **statement** ke end mein. Line ≠ statement |
| "`}` ke baad hamesha `;`" | Class/struct/enum ke baad haan, function/if/loop ke baad nahi |
| "Extra `;` se error aata hai" | Aksar nahi! Silently galat behaviour deta hai |
| "Newline se statement khatam hoti hai" | Nahi, `;` se. Newline ignore hota hai |
| "Indentation compiler ke liye zaroori hai" | Nahi (Python ke ulta). Insaanon ke liye zaroori |

---

## Exercises

1. In mein se kaunse mein `;` chahiye? (kagaz pe likho, phir compile karke check karo)
   ```cpp
   a) #include <iostream>
   b) int x = 5
   c) class Foo { }
   d) int main() { }
   e) if (x > 0) { }
   f) return 0
   g) struct Point { int x; }
   h) while (true) { }
   i) do { } while (x)
   ```
   <details><summary>Answers</summary>
   a) ❌ nahi   b) ✅ haan   c) ✅ haan   d) ❌ nahi   e) ❌ nahi
   f) ✅ haan   g) ✅ haan   h) ❌ nahi   i) ✅ haan
   </details>

2. `semicolons.cpp` chalao. Warnings padho. Dono bugs theek karo.

3. Yeh program mein 4 semicolon-related galtiyan hain. Dhoondho:
   ```cpp
   #include <iostream>;
   
   struct Point {
       int x, y;
   }
   
   int main() {
       int a = 5
       if (a > 0);
           std::cout << "positive\n";
       return 0;
   }
   ```
   <details><summary>Answers</summary>
   1. Line 1: `#include <iostream>;` — extra `;`
   2. Line 5: `}` ke baad `;` missing (struct ke baad)
   3. Line 8: `int a = 5` — `;` missing
   4. Line 9: `if (a > 0);` — extra `;`
   </details>

4. Ek statement ko 5 lines mein todo (chaining se). Compile karke verify karo.

5. `clang-format` install karo aur ek gande code pe chalao:
   ```bash
   sudo apt install clang-format      # ya brew install clang-format
   clang-format -i myfile.cpp
   ```

---

## Next
→ [`06-braces-blocks-and-scope.md`](06-braces-blocks-and-scope.md)
