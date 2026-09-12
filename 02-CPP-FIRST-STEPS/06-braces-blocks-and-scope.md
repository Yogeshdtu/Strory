# 06 — Braces, blocks, aur scope

## Prerequisites
`05-statements-and-semicolons.md`, folder 01 lesson 11 (memory basics)

## Yeh topic abhi kyun
`{ }` sirf "code ko group karna" nahi hai. Woh **scope** banate hain — aur scope hi
decide karta hai ki variable kab banta hai, kab marta hai, aur kahan use ho sakta hai.

Yeh concept aage **RAII (folder 17)** ka poora foundation hai, jo C++ ka sabse
important idiom hai. Isliye abhi se saaf rakhna.

---

## Block kya hai?

**Block = `{ }` ke beech ka code.**

```cpp
{
    statement1;
    statement2;
}
```

Ek block **ek statement ki tarah treat** hota hai. Isliye:

```cpp
if (x > 5)
    std::cout << "ek statement\n";     // ek statement, braces optional

if (x > 5) {
    std::cout << "kai\n";
    std::cout << "statements\n";        // block ke andar
}
```

---

## Scope kya hai?

**Scope = woh ilaaka jahan ek naam visible/valid hai.**

Simple rule: **variable apne block ke andar zinda hai, bahar nahi.**

```cpp
int main() {
    int a = 1;              // a yahan se zinda

    {
        int b = 2;          // b yahan se zinda
        std::cout << a;     // ✅ a dikh raha hai (bahar ka block)
        std::cout << b;     // ✅ b dikh raha hai
    }                       // <- b yahan MAR gaya

    std::cout << a;         // ✅ a abhi zinda hai
    std::cout << b;         // ❌ ERROR: 'b' was not declared in this scope
}                           // <- a yahan mara
```

---

## Scope ka mental model

```
   int main() {                     +--------------------------+
       int a = 1;                   |  MAIN KA SCOPE           |
                                    |                          |
       {                            |  +--------------------+  |
           int b = 2;               |  | ANDAR KA SCOPE     |  |
           // a aur b dono dikhte   |  | b sirf yahan       |  |
       }                            |  +--------------------+  |
                                    |                          |
       // sirf a dikhta hai         |  a poore main mein       |
   }                                +--------------------------+
```

**Rule:** Andar wala bahar dekh sakta hai. Bahar wala andar nahi dekh sakta.

---

## Lifetime — yeh sabse important hai

Scope aur **lifetime** saath chalte hain (local variables ke liye).

```cpp
{
    int x = 5;      // <- x BANTA hai (constructor chalta hai)
    // ...
}                   // <- x MARTA hai (destructor chalta hai)
```

Yeh **automatic** hai. Aapko kuch nahi karna. Compiler khud karta hai.

Yeh dekho:

```cpp
#include <iostream>

struct Tracer {
    const char* name;
    Tracer(const char* n) : name(n) { std::cout << "  [+] " << name << " BANA\n"; }
    ~Tracer()                        { std::cout << "  [-] " << name << " MARA\n"; }
};

int main() {
    std::cout << "main shuru\n";
    Tracer outer("OUTER");

    {
        std::cout << "block mein ghuse\n";
        Tracer inner("INNER");
        std::cout << "block se nikal rahe hain\n";
    }   // <- INNER yahan marta hai

    std::cout << "main khatam ho raha hai\n";
    return 0;
}   // <- OUTER yahan marta hai
```

**Output:**
```
main shuru
  [+] OUTER BANA
block mein ghuse
  [+] INNER BANA
block se nikal rahe hain
  [-] INNER MARA          <- block khatam hote hi
main khatam ho raha hai
  [-] OUTER MARA          <- main khatam hote hi
```

> **🔑 Yeh RAII ka foundation hai.**
> Agar `Tracer` ke destructor mein hum file close karte, memory free karte, ya lock
> release karte — to woh **automatically** sahi waqt pe ho jaata. Chahe function
> normally khatam ho, chahe `return` early ho, chahe exception aaye.
> Yeh C++ ki sabse badi taakat hai. Folder 17 mein poora.

---

## Destruction order: **ulta**

```cpp
{
    Tracer a("A");
    Tracer b("B");
    Tracer c("C");
}
```

**Output:**
```
[+] A BANA
[+] B BANA
[+] C BANA
[-] C MARA      <- ULTA order!
[-] B MARA
[-] A MARA
```

**Kyun ulta?** Kyunki `c` shayad `b` pe depend karta ho, aur `b` shayad `a` pe.
Isliye pehle jo bana, woh **aakhir** mein marta hai. Stack ki tarah — LIFO
(Last In, First Out).

Yeh guarantee hai, koi random behaviour nahi.

---

## Shadowing — ek naam, do variables

```cpp
int x = 10;         // bahar wala x

{
    int x = 20;     // andar wala x — bahar wale ko "SHADOW" kar diya
    std::cout << x; // 20 (andar wala)
}

std::cout << x;     // 10 (bahar wala, wapas dikhne laga)
```

**Yeh legal hai** par confusing hai. `-Wshadow` flag se warning milti hai:

```bash
g++ -Wshadow file.cpp -o file
```

**Salah:** Shadowing se bacho. Alag naam do. HFT codebases mein `-Wshadow -Werror`
common hai.

---

## Scope ke levels

```cpp
#include <iostream>

int globalVar = 1;                  // 1. GLOBAL scope (poore program mein)

namespace MyNS {
    int nsVar = 2;                  // 2. NAMESPACE scope
}

class MyClass {
    int memberVar = 3;              // 3. CLASS scope
};

void func(int paramVar) {           // 4. FUNCTION PARAMETER scope
    int localVar = 5;               // 5. FUNCTION/BLOCK scope
    {
        int blockVar = 6;           // 6. NESTED BLOCK scope
    }
    for (int i = 0; i < 3; i++) {   // 7. LOOP scope (i sirf loop mein)
        // i yahan dikhta hai
    }
    // i yahan NAHI dikhta
}
```

---

## Braces optional kab hain?

```cpp
// Ek statement ke liye braces optional hain
if (x > 5)
    std::cout << "bada\n";

// Yeh bilkul same hai:
if (x > 5) {
    std::cout << "bada\n";
}
```

### ⚠️ Lekin HAMESHA braces lagao. Kyun?

**Reason 1: Goto Fail bug (Apple, 2014 — asli disaster)**

```cpp
if (condition)
    goto fail;
    goto fail;          // <-- yeh HAMESHA chalega!
```

Apple ke SSL code mein exactly yeh bug tha. Result: **SSL verification bypass** —
crores users ka data risk mein aa gaya. Ek missing brace se.

**Reason 2: Baad mein line add karna**
```cpp
if (x > 5)
    doSomething();

// 6 mahine baad koi aur developer:
if (x > 5)
    doSomething();
    doSomethingElse();      // <-- yeh HAMESHA chalega, if ke bahar hai!
```

**Reason 3: Dangling else**
```cpp
if (a)
    if (b)
        std::cout << "1";
else                        // <-- yeh kis if ka else hai?
    std::cout << "2";
```
Indentation se lagta hai pehle `if` ka, par actually **doosre** `if` ka hai.
Braces se yeh confusion khatam ho jaati hai.

**Rule: Hamesha braces lagao. Har baar. Bina exception ke.**

---

## Scope aur memory (stack frame)

Yaad hai folder 01 lesson 11 se? Local variables **stack** pe hote hain.

```cpp
void func() {
    int a = 1;          // stack pe
    int b = 2;          // stack pe
    {
        int c = 3;      // stack pe
    }                   // c ki jagah "reuse" ho sakti hai
}                       // poora stack frame gaya
```

Stack se memory lena bahut tez hai — bas stack pointer move karna hai.
Isliye local variables heap se **bahut** tez hote hain.

> **HFT relevance:** HFT hot path mein hum jitna ho sake **stack pe** kaam karte hain,
> heap pe nahi. Stack allocation ~1 cycle ka hai, heap allocation 100-10000 cycles ka.
> Aur stack cache mein garam rehta hai (hot). Folder 36.

---

## Hands-on

```bash
cd ~/cpp-practice
cat > scope.cpp << 'END'
#include <iostream>

struct Tracer {
    const char* name;
    Tracer(const char* n) : name(n) { std::cout << "  [+] " << name << "\n"; }
    ~Tracer()                        { std::cout << "  [-] " << name << "\n"; }
};

int globalVar = 100;

void demoFunction() {
    std::cout << "\n--- demoFunction() ---\n";
    Tracer t1("func-local");
    std::cout << "global dikhta hai: " << globalVar << "\n";
}

int main() {
    std::cout << "--- main shuru ---\n";
    Tracer outer("outer");
    int x = 10;

    {
        std::cout << "--- block 1 ---\n";
        Tracer inner1("inner-1");
        int y = 20;
        std::cout << "x = " << x << ", y = " << y << "\n";
        
        {
            std::cout << "--- block 2 (nested) ---\n";
            Tracer inner2("inner-2");
            int x = 999;                          // SHADOWING!
            std::cout << "shadowed x = " << x << "\n";
        }
        std::cout << "wapas block 1, x = " << x << "\n";
    }

    // std::cout << y;    // <- uncomment karo, error dekho

    demoFunction();

    // Destruction order dekho
    std::cout << "\n--- destruction order (ulta hoga) ---\n";
    Tracer a("A");
    Tracer b("B");
    Tracer c("C");

    std::cout << "--- main khatam ---\n";
    return 0;
}
END
g++ -std=c++20 -Wall -Wextra -Wshadow scope.cpp -o scope
./scope
```

Warnings dhyaan se padho — `-Wshadow` shadowing pakad lega.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`{}` sirf grouping ke liye hai" | Scope + lifetime bhi banate hain |
| "Variable poore function mein zinda hai" | Sirf uske block mein |
| "Destruction creation order mein hoti hai" | **Ulta** order mein hoti hai |
| "Braces optional hain to skip kar do" | Hamesha lagao — real bugs bache hain |
| "Shadowing error hai" | Legal hai, par confusing. `-Wshadow` se pakdo |

---

## Exercises

1. `scope.cpp` chalao. Destruction order note karo. Kya ulta tha?

2. `std::cout << y;` wali line uncomment karo. Kya error aaya?

3. Predict karo output:
   ```cpp
   int x = 1;
   {
       std::cout << x;
       int x = 2;
       std::cout << x;
       {
           int x = 3;
           std::cout << x;
       }
       std::cout << x;
   }
   std::cout << x;
   ```
   <details><summary>Answer</summary>
   `12321`
   </details>

4. Yeh code kyun galat hai? Fix karo:
   ```cpp
   int main() {
       for (int i = 0; i < 5; i++) {
           int sum = 0;
           sum += i;
       }
       std::cout << sum;    // ???
   }
   ```
   <details><summary>Answer</summary>
   Do problems:
   1. `sum` loop ke andar declare hua hai, isliye bahar dikhta nahi.
   2. `sum` har iteration mein naya banta hai aur 0 se shuru hota hai — accumulate
      nahi ho raha.
   
   Fix: `int sum = 0;` loop se **pehle** likho.
   </details>

5. `-Wshadow` ke bina aur saath compile karke fark dekho.

6. Ek `Tracer` banao jo file kholta hai constructor mein aur band karta hai destructor
   mein. Verify karo ki block khatam hote hi file band ho jaati hai. (Yeh RAII ka
   preview hai!)

---

## Interview questions

1. Scope aur lifetime mein kya fark hai?
2. Destruction kis order mein hoti hai aur kyun?
3. Variable shadowing kya hai?
4. RAII kya hai? (abhi preview level pe socho)
5. Ek block se return karne pe local objects ka kya hota hai?

---

## Next
→ [`07-comments.md`](07-comments.md)
