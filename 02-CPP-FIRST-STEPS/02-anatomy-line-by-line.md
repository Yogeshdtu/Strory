# 02 — Hello World ka har ek hissa

## Prerequisites
`01-hello-world.md` — program chal chuka ho.

## Yeh topic abhi kyun
**Yeh is folder ki sabse important file hai.**

Har beginner ye 6 lines likh leta hai. Bahut kam log bata paate hain ki `std::` ka
matlab kya hai, `<<` actually kya cheez hai, ya `int main()` ka `int` kyun hai.

Aaj hum har ek token kholenge.

---

## Poora program

```cpp
#include <iostream>

int main() {
    std::cout << "Hello World\n";
    return 0;
}
```

Isme **kul 13 alag concepts** hain. Ek-ek karke.

---

# TOKEN 1: `#`

```cpp
#include <iostream>
^
```

`#` batata hai: **"yeh line PREPROCESSOR ke liye hai, compiler ke liye nahi."**

Yaad karo folder 01 lesson 08 — preprocessor pehla stage hai, compiler se pehle chalta hai.

`#` se shuru hone wali lines ko **preprocessor directives** kehte hain:

| Directive | Kaam |
|---|---|
| `#include` | file ka content yahan paste karo |
| `#define` | text replacement |
| `#ifdef` / `#ifndef` / `#endif` | conditional compilation |
| `#pragma` | compiler-specific instruction |

**Important:** In lines ke aakhir mein `;` **nahi** lagta. Kyunki yeh C++ statements
hain hi nahi — yeh preprocessor commands hain, ek alag mini-language.

```cpp
#include <iostream>;    // ❌ GALAT - semicolon nahi chahiye
#include <iostream>     // ✅ SAHI
```

---

# TOKEN 2: `include`

```cpp
#include <iostream>
 ^^^^^^^
```

`include` = **"is file ka poora content, yahan, is jagah, paste kar do."**

Literally paste. Copy-paste. Preprocessor ek text editor ki tarah kaam karta hai.

```
   BEFORE preprocessing:              AFTER preprocessing:
   
   #include <iostream>       ---->    [30,000 lines of iostream code]
   int main() { ... }                 int main() { ... }
```

**Verify karo:**
```bash
g++ -E hello.cpp -o hello.i
wc -l hello.i          # ~30,000+ lines!
```

---

# TOKEN 3: `<iostream>`

```cpp
#include <iostream>
        ^^^^^^^^^^^
```

Yeh file ka naam hai jo include karni hai.

### `iostream` ka matlab
**i**nput **o**utput **stream** — input/output ke liye tools.

Isme yeh cheezein declare hoti hain:
- `std::cout` — console output
- `std::cin` — console input
- `std::cerr` — error output
- `std::endl` — newline + flush

### `< >` vs `" "` — bahut important fark

```cpp
#include <iostream>      // ANGLE BRACKETS: system/standard headers
#include "myfile.h"      // QUOTES: aapki apni files
```

| | `< >` | `" "` |
|---|---|---|
| Kahan dhoondhta hai | system include paths mein | pehle current folder, phir system paths |
| Kiske liye | standard library, external libraries | aapke project ki files |

### Extension kyun nahi hai?

`<iostream>` — koi `.h` nahi. Kyun?

- Purana C++ (1990s): `<iostream.h>` — **ab yeh mat use karna, deprecated hai**
- Modern C++: `<iostream>` — no extension

Standard headers ka extension nahi hota taaki:
1. Woh aapki files se clash na karein
2. Implementation ko azadi mile (kuch compilers mein yeh file exist hi nahi karti,
   built-in hoti hai)

**Note:** C library headers C++ mein `c` prefix ke saath aate hain:
`<cstdio>` (not `<stdio.h>`), `<cmath>`, `<cstring>`, `<cstdint>`.

---

# TOKEN 4: `int` (return type)

```cpp
int main() {
^^^
```

`int` = **integer** = poora number (decimal nahi).

Yahan `int` batata hai: **"yeh function ek integer return karega."**

### Function ka basic shape
```
   return_type  function_name  ( parameters )  { body }
        ^             ^              ^             ^
       int          main           (khali)      { ... }
```

`main` ko `int` return karna **standard ke hisaab se zaroori hai**. Kyun? Kyunki OS
ko ek number chahiye jo bataye ki program successfully khatam hua ya nahi.

```cpp
void main() { }     // ❌ GALAT (kuch compilers allow karte hain, par standard nahi)
int main() { }      // ✅ SAHI
```

---

# TOKEN 5: `main`

```cpp
int main() {
    ^^^^
```

`main` ek **special naam** hai. Yeh aapke program ka **entry point** hai — yahan se
aapka code shuru hota hai.

### Rules
1. Har C++ program mein **exactly ek** `main` hona chahiye
2. Naam bilkul `main` hona chahiye — `Main`, `MAIN`, `mian` kaam nahi karenge
3. Aap ise apne code se call **nahi** kar sakte (technically UB hai)
4. Iska return type `int` hi hona chahiye

### Do valid forms
```cpp
int main() { ... }                            // simple
int main(int argc, char* argv[]) { ... }      // command-line arguments ke saath
```

Doosra form folder 08 mein aayega.

### 🔑 Yaad hai? `main()` pehla function nahi hai!

Folder 01 lesson 08 se:
```
_start  ->  __libc_start_main  ->  global constructors  ->  main()  ->  exit()
```

`main()` se pehle bahut kuch hota hai. Yeh baat folder 25 mein important banegi.

---

# TOKEN 6: `()` — parentheses

```cpp
int main() {
        ^^
```

`()` do kaam karta hai C++ mein:

### Kaam 1: Function ke parameters ki list
```cpp
int main()                 // khali = koi parameter nahi
int add(int a, int b)      // do parameters
```

Yahan `()` khali hai, matlab `main` koi argument nahi leta.

### Kaam 2: Function ko call karna
```cpp
someFunction();     // yahan () ka matlab "chalao"
```

**Yeh dono alag cheezein hain.** Declaration mein `()` = "parameters yahan honge",
call mein `()` = "abhi chalao".

Aur `()` ka teesra kaam bhi hai — grouping:
```cpp
int x = (2 + 3) * 4;    // pehle jodo, phir guna karo
```

Teenon use folder 08 mein detail mein.

---

# TOKEN 7: `{ }` — curly braces

```cpp
int main() {
            ^
    ...
}
^
```

`{ }` ek **block** banate hain — statements ka group.

```
   {  <- yahan block shuru
      statement 1;
      statement 2;
      statement 3;
   }  <- yahan block khatam
```

### Blocks ki 3 ahem baatein

**1. Function ka body ek block hota hai**
```cpp
int main() {
    // yeh main ka body hai
}
```

**2. Blocks nest ho sakte hain**
```cpp
int main() {
    {
        // andar wala block
    }
}
```

**3. Block SCOPE banata hai** — yeh sabse important hai
```cpp
int main() {
    int x = 5;        // x yahan zinda hai
    {
        int y = 10;   // y sirf is block mein zinda hai
    }
    // y yahan MAR CHUKA hai - use nahi kar sakte
}
```

Scope ki poori kahani lesson 06 mein.

### Indentation — style ki baat
```cpp
int main() {
    std::cout << "Hi\n";     // 4 spaces andar
}
```

Compiler ko indentation se **koi farak nahi padta**. Yeh sirf insaanon ke liye hai.
Lekin **hamesha proper indent karo** — bina indentation ka code padha nahi ja sakta.

---

# TOKEN 8: `std`

```cpp
std::cout << "Hello World\n";
^^^
```

`std` ek **namespace** ka naam hai. `std` = "standard".

### Namespace kya hai?

Namespace ek **surname** jaisa hai. Naam clash rokne ke liye.

Socho ek school mein do "Rahul" hain. Confusion. Solution: surname lagao —
"Rahul Sharma" aur "Rahul Verma". Ab clear hai.

C++ mein waise hi:
```cpp
std::cout       // standard library ka cout
mylib::cout     // agar aapki library mein bhi cout hota
```

Poori C++ standard library `std` namespace ke andar hai. Isliye:
`std::cout`, `std::string`, `std::vector`, `std::sort` — sab.

**Kyun?** Taaki aapka `vector` (physics wala) standard `std::vector` se na takraye.

---

# TOKEN 9: `::` — scope resolution operator

```cpp
std::cout << "Hello World\n";
   ^^
```

`::` ka matlab: **"iske andar wala"** ya **"ismein se"**.

```cpp
std::cout       // "std ke andar ka cout"
```

Isko **scope resolution operator** kehte hain.

Yeh aur jagah bhi use hota hai:
```cpp
std::vector<int>          // namespace se
MyClass::staticMember     // class se
MyClass::MyClass()        // class ka constructor define karte waqt
::globalVariable          // global namespace se (kuch bhi pehle nahi = global)
```

---

## 🤔 Ek zaroori baat: `using namespace std;`

Aapne shayad yeh dekha hoga:

```cpp
#include <iostream>
using namespace std;      // <-- yeh

int main() {
    cout << "Hello\n";    // ab std:: nahi likhna padta
    return 0;
}
```

Yeh kaam karta hai, aur beginner tutorials mein bahut milta hai.

### Lekin isse bachna chahiye. Kyun?

**Wajah 1: Naam clash**
```cpp
using namespace std;
int count = 0;             // ⚠️ std::count bhi ek algorithm hai!
count++;                   // ambiguous ho sakta hai
```

**Wajah 2: Aapko pata nahi chalta ki cheez kahan se aa rahi hai**
```cpp
sort(v.begin(), v.end());     // yeh std::sort hai ya koi aur?
std::sort(v.begin(), v.end()); // ab clear hai
```

**Wajah 3: Header files mein yeh disaster hai**
Agar aap header mein `using namespace std;` likh do, to har file jo us header ko
include karti hai, us pe yeh thop diya jayega. Yeh professional codebases mein
**strictly forbidden** hai.

### Meri salah
**Is course mein hum hamesha `std::` likhenge.** Thoda zyada typing hai, par:
- Code clear rehta hai
- Professional codebases yahi karti hain
- Interview mein achha impression padta hai

Agar bahut hi lamba naam ho, to selective using kar sakte ho:
```cpp
using std::cout;      // sirf cout import karo, poora namespace nahi
```

---

# TOKEN 10: `cout`

```cpp
std::cout << "Hello World\n";
     ^^^^
```

`cout` = **"c**haracter **out**put" (ya "console output").

**Yeh function nahi hai. Yeh ek OBJECT hai.**

Yeh baat bahut beginners ko confuse karti hai. `cout` ek variable hai —
`std::ostream` class ka ek object, jo standard output (aapki screen) se juda hua hai.

```cpp
// cout roughly aisa declare hua hai (simplified):
namespace std {
    extern ostream cout;      // ek object, function nahi
}
```

### Teen standard streams

| Stream | Kya | Buffered? | Kahan jaata hai |
|---|---|---|---|
| `std::cout` | normal output | haan | stdout (screen) |
| `std::cerr` | errors | **nahi** (auto-flush) | stderr (screen) |
| `std::clog` | logging | haan | stderr (screen) |
| `std::cin` | input | haan | stdin (keyboard) |

`cerr` unbuffered kyun hai? Taaki agar program crash ho jaye, error message pehle hi
dikh jaye. Buffer mein atka na rahe.

Detail folder 04 mein.

---

# TOKEN 11: `<<` — stream insertion operator

```cpp
std::cout << "Hello World\n";
          ^^
```

Yeh **sabse confusing** token hai beginners ke liye. Dhyaan se.

### Yeh "arrow" nahi hai

`<<` ko log "arrow" samajh lete hain — "text ko cout ki taraf bhejo". Yeh
**intuition** theek hai, par technically yeh ek **operator** hai.

### Yeh actually kya hai?

`<<` original mein **left shift** operator hai (bits ko left shift karna):
```cpp
int x = 1 << 3;    // 1 ko 3 bits left shift = 8
```

Lekin C++ mein aap operators ko **overload** kar sakte ho — matlab alag types ke liye
alag behaviour de sakte ho. `iostream` ne `<<` ko `ostream` ke liye overload kiya hai:

```cpp
// simplified idea:
std::ostream& operator<<(std::ostream& os, const char* text);
```

Matlab `std::cout << "Hello"` actually yeh hai:
```cpp
operator<<(std::cout, "Hello");     // ek function call!
```

**Yeh operator overloading ka pehla example hai.** Poora folder 15 mein.

### Chaining kaise kaam karti hai?

```cpp
std::cout << "Naam: " << "Rahul" << " Umar: " << 25 << "\n";
```

Yeh kaam karta hai kyunki `<<` **`cout` ko wapas return karta hai**:

```
   std::cout << "Naam: "        -> returns std::cout
   (std::cout) << "Rahul"       -> returns std::cout
   (std::cout) << " Umar: "     -> returns std::cout
   (std::cout) << 25            -> returns std::cout
   (std::cout) << "\n"          -> returns std::cout
```

Left se right, ek chain ki tarah. Isliye ise "chaining" kehte hain.

### `<<` ka type kaise pata chalta hai?

```cpp
std::cout << 42;         // int version chalta hai
std::cout << 3.14;       // double version
std::cout << "text";     // const char* version
std::cout << 'A';        // char version
```

Compiler **overload resolution** karke sahi version chunta hai. Folder 08 mein detail.

---

# TOKEN 12: `"Hello World\n"` — string literal

```cpp
std::cout << "Hello World\n";
             ^^^^^^^^^^^^^^^
```

Do quotes ke beech ka text = **string literal**.

### Iska type kya hai? (surprise!)

```cpp
auto s = "Hello";       // s ka type: const char*
```

`"Hello"` ka type `const char[6]` hai — 6 kyun? Kyunki:
```
   'H' 'e' 'l' 'l' 'o' '\0'
    1   2   3   4   5   6
                        ^
                   null terminator!
```

C++ mein har string literal ke aakhir mein automatic ek **`\0`** (null character)
lagta hai. Yeh batata hai ki string yahan khatam hui.

Yeh baat folder 10 (strings) mein bahut important banegi.

### String literals `.rodata` mein rehti hain

```cpp
const char* s = "Hello";
s[0] = 'J';       // ❌ CRASH! read-only memory hai
```

String literals **read-only memory** mein hoti hain (`.rodata` section — yaad hai
folder 01 lesson 11 se?). Unhe modify karna **undefined behaviour** hai.

---

# TOKEN 13: `\n` — escape sequence

```cpp
std::cout << "Hello World\n";
                         ^^
```

`\n` **do characters nahi hai**. Yeh **ek** character hai — newline (ASCII 10).

`\` (backslash) ek **escape character** hai. Woh agle character ka matlab badal deta hai.

### Common escape sequences

| Escape | Naam | ASCII | Kya karta hai |
|---|---|---|---|
| `\n` | newline | 10 | agli line pe jao |
| `\t` | tab | 9 | tab space |
| `\\` | backslash | 92 | ek actual `\` |
| `\"` | double quote | 34 | ek actual `"` |
| `\'` | single quote | 39 | ek actual `'` |
| `\0` | null | 0 | string ka end |
| `\r` | carriage return | 13 | line ke start pe jao |
| `\a` | alert/bell | 7 | beep! |

### Escape kyun chahiye?

```cpp
std::cout << "Usne kaha "Hi"";      // ❌ compiler confuse ho gaya
std::cout << "Usne kaha \"Hi\"";    // ✅ ab clear hai
```

Detail lesson 08 mein.

---

# TOKEN 14: `;` — semicolon

```cpp
std::cout << "Hello World\n";
                            ^
```

`;` ka matlab: **"yeh statement yahan khatam."**

C++ mein compiler **newline ko ignore karta hai**. Uske liye yeh sab same hai:

```cpp
std::cout << "Hello\n";
```
```cpp
std::cout
<<
"Hello\n"
;
```
```cpp
std::cout << "Hello\n"; return 0;
```

Compiler `;` dekh kar hi jaanta hai ki statement khatam hui.

### Kahan `;` lagta hai
```cpp
int x = 5;                    // ✅ declaration
std::cout << "hi";            // ✅ expression statement
return 0;                     // ✅ return statement
class MyClass { };            // ✅ class definition (yeh surprise hai!)
```

### Kahan `;` NAHI lagta
```cpp
#include <iostream>           // ❌ preprocessor directive
int main() { }                // ❌ function definition ke baad
if (x > 5) { }                // ❌ if block ke baad
for (...) { }                 // ❌ loop block ke baad
```

### ⚠️ Classic bug: extra semicolon
```cpp
if (x > 5);                   // <- yeh ; galti hai!
    std::cout << "bada hai";  // yeh HAMESHA chalega
```
Yeh compile ho jayega (koi error nahi!) par galat kaam karega. `-Wall` isko pakad leta hai —
isliye warnings hamesha ON rakho.

---

# TOKEN 15: `return`

```cpp
return 0;
^^^^^^
```

`return` ka matlab: **"is function ko yahan khatam karo, aur yeh value wapas do."**

```cpp
int add(int a, int b) {
    return a + b;         // function khatam, value wapas
    std::cout << "hi";    // yeh KABHI nahi chalega (dead code)
}
```

---

# TOKEN 16: `0`

```cpp
return 0;
       ^
```

Yeh `main` ka return value hai, jo **OS ko** jaata hai.

### Convention
| Value | Matlab |
|---|---|
| `0` | Success — sab theek hua |
| Non-zero | Error — kuch galat hua |

**Yeh ulta lagta hai** (0 = "false" jaisa), par yeh Unix convention hai:
- Success ka sirf ek tareeka hota hai → `0`
- Fail hone ke kai tareeke hote hain → `1`, `2`, `3`... har ek alag error

Better readability ke liye:
```cpp
#include <cstdlib>
return EXIT_SUCCESS;    // 0
return EXIT_FAILURE;    // implementation-defined (usually 1)
```

### Check karo
```bash
./hello
echo $?        # 0 dikhega
```

### 🔑 `main` ka special rule
`main` **akela function** hai jisme `return` optional hai:
```cpp
int main() {
    std::cout << "hi\n";
    // koi return nahi -- compiler apne aap `return 0;` lagata hai
}
```

Baaki har non-void function mein `return` zaroori hai.

---

# TOKEN 17: `}` — closing brace

```cpp
}
^
```

Function ka body khatam. Aur iske baad **koi semicolon nahi**.

```cpp
int main() { }        // ✅ sahi
int main() { };       // technically legal (empty statement), par mat likho
class A { };          // ✅ class ke baad ; ZAROORI hai (fark note karo!)
```

---

## 🎯 Poora program, ab poori samajh ke saath

```cpp
#include <iostream>
// ^^  ^^^^^^^ ^^^^^^^^^^^
// |   |       +-- iostream naam ki standard header file
// |   +---------- "iska poora content yahan paste kar do"
// +-------------- "yeh preprocessor ke liye hai" (koi ; nahi)

int main() {
// ^^^ ^^^^ ^^ ^
// |   |    |  +-- block shuru: function ka body
// |   |    +----- parameters ki list (khali = koi parameter nahi)
// |   +---------- special naam: program ka entry point
// +-------------- return type: main hamesha int return karta hai

    std::cout << "Hello World\n";
//  ^^^ ^^ ^^^^ ^^ ^^^^^^^^^^^^^^^ ^
//  |   |  |    |  |               +-- statement khatam
//  |   |  |    |  +------------------ string literal (\n = ek newline character)
//  |   |  |    +--------------------- stream insertion operator (overloaded)
//  |   |  +-------------------------- output stream OBJECT (function nahi)
//  |   +----------------------------- scope resolution: "std ke andar ka"
//  +--------------------------------- namespace ka naam ("standard")

    return 0;
//  ^^^^^^ ^ ^
//  |      | +-- statement khatam
//  |      +---- exit code: 0 = success (OS ko jaata hai)
//  +----------- function se bahar niklo, yeh value do
}
// ^-- block khatam (koi ; nahi)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`cout` ek function hai" | Object hai — `std::ostream` type ka |
| "`<<` ek arrow hai" | Overloaded operator hai (originally left-shift) |
| "`\n` do characters hain" | Ek character hai (newline, ASCII 10) |
| "`std::` optional hai" | `using namespace std;` ke bina zaroori hai — aur usse bachna chahiye |
| "`return 0;` zaroori hai" | `main` mein optional hai, baaki functions mein zaroori |
| "`#include` ke baad `;` lagta hai" | Nahi lagta — woh C++ statement nahi hai |
| "`main` pehla function hai jo chalta hai" | `_start` pehle chalta hai |
| "Newline se statement khatam hoti hai" | `;` se hoti hai. Newline ignore hoti hai |

---

## Exercises

1. Bina dekhe, `hello.cpp` poora likho. Compile karo.

2. Har token ko apne shabdon mein explain karo (kisi dost ko, ya zor se khud ko):
   `#`, `include`, `<iostream>`, `int`, `main`, `()`, `{}`, `std`, `::`, `cout`,
   `<<`, `"..."`, `\n`, `;`, `return`, `0`

3. Yeh program ek line mein likho (sab kuch ek line pe). Kya chalta hai?
   <details><summary>Answer</summary>

   ```cpp
   #include <iostream>
   int main() { std::cout << "Hello World\n"; return 0; }
   ```
   Chalta hai! (`#include` alag line pe hona zaroori hai kyunki preprocessor
   line-based hai.) Compiler ko newlines se farak nahi padta — par **kabhi aisa
   mat likhna**, padhne mein bakwaas hai.
   </details>

4. Chaining test karo:
   ```cpp
   std::cout << "A" << "B" << "C" << "\n";
   ```
   Output kya aayega? Aur yeh kaam kyun karta hai?

5. `cerr` try karo:
   ```cpp
   std::cerr << "Yeh error hai\n";
   ```
   Compile karo aur chalao:
   ```bash
   ./prog > output.txt        # sirf cout file mein jayega
   cat output.txt
   ```
   `cerr` ka message file mein gaya ya screen pe?
   <details><summary>Answer</summary>
   Screen pe. `>` sirf stdout (cout) redirect karta hai. `cerr` stderr pe jaata hai.
   Isliye error messages log files mein aksar alag rakhe jaate hain.
   `2>` se stderr redirect hota hai: `./prog 2> errors.txt`
   </details>

6. Yeh compile karke dekho:
   ```cpp
   #include <iostream>
   int main() {
       std::cout << 1 << 2 << 3 << "\n";
       std::cout << 1 + 2 + 3 << "\n";
   }
   ```
   Do lines mein alag output kyun aaya?
   <details><summary>Answer</summary>
   Pehli: `123` — teen alag values print hui.
   Doosri: `6` — pehle `+` chala (higher precedence), phir result print hua.
   Operator precedence — folder 05 mein detail.
   </details>

---

## Interview questions

1. `std::` kya hai aur kyun likhte hain?
2. `using namespace std;` kyun avoid karna chahiye?
3. `cout` object hai ya function? Explain.
4. `<<` operator actually kya karta hai?
5. `main()` ka return value kahan jaata hai?
6. String literal ka type kya hota hai?
7. `\n` aur `std::endl` mein kya fark hai? (Answer folder 04 mein — abhi socho)

---

## Next
→ [`03-include-and-headers.md`](03-include-and-headers.md)
