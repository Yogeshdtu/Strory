# 10 — Aapke pehle errors (aur unhe padhna)

## Prerequisites
Is folder ke lessons 01–09

## Yeh topic abhi kyun
Aap agle 2 saal mein **hazaaron** compiler errors dekhoge. Agar aap unse darte rahoge,
progress slow rahegi. Agar aap unhe **padhna** seekh gaye, aap 5x tez seekhoge.

Yeh file aapko jaan-boojh kar errors banane aur unhe decode karne sikhati hai.

---

## Error message ka anatomy

```
main.cpp:5:18: error: 'cout' was not declared in this scope; did you mean 'std::cout'?
    5 |     cout << "Hello";
      |     ^~~~
      |     std::cout
```

| Hissa | Matlab |
|---|---|
| `main.cpp` | file ka naam |
| `:5` | line number |
| `:18` | column number |
| `error:` | severity (error / warning / note) |
| `'cout' was not declared...` | actual problem |
| `did you mean 'std::cout'?` | compiler ka suggestion (aksar sahi hota hai!) |
| `5 \| cout << "Hello";` | actual source line |
| `^~~~` | exactly kahan point kar raha hai |

---

## 🥇 GOLDEN RULE

> **SABSE PEHLA error theek karo. Phir dobara compile karo.**

Ek galti se 50 errors aa sakte hain. Pehla theek karne pe baaki 49 gayab ho jaate hain.

**Neeche wale errors ko shuru mein ignore karo.** Woh aksar pehle error ka side-effect
hote hain.

---

## Error vs Warning vs Note

| Type | Matlab | Kya karo |
|---|---|---|
| **error** | Compile nahi hoga | Theek karna hi padega |
| **warning** | Compile ho jayega, par shayad bug hai | **Hamesha theek karo** |
| **note** | Extra information | Padho, context milega |

**Warnings ko errors jaisa treat karo.** HFT codebases mein `-Werror` hota hai —
koi bhi warning = build fail. Aadat abhi daalo.

---

# ERROR CATALOG — ek-ek karke banao aur samjho

Har error ke liye: **file banao, compile karo, error dekho, phir fix karo.**

---

## ERROR 1: Missing semicolon

```cpp
#include <iostream>
int main() {
    std::cout << "Hello"
    return 0;
}
```

**Error:**
```
error: expected ';' before 'return'
```

**Stage:** Parser (2)

**🔑 Sabse important baat:** Error **line 4** pe dikha raha hai, galti **line 3** pe hai.

> **Rule: Missing `;` ka error hamesha AGLI line pe dikhta hai. Jab "expected ;"
> dikhe, upar wali line dekho.**

---

## ERROR 2: `std::` bhool gaye

```cpp
#include <iostream>
int main() {
    cout << "Hello\n";
    return 0;
}
```

**Error:**
```
error: 'cout' was not declared in this scope; did you mean 'std::cout'?
```

**Stage:** Semantic analysis (2)

**Fix:** `std::cout` likho.

---

## ERROR 3: Header missing

```cpp
int main() {
    std::cout << "Hello\n";
    return 0;
}
```

**Error:**
```
error: 'cout' is not a member of 'std'
note: 'std::cout' is defined in header '<iostream>'; did you forget to '#include <iostream>'?
```

Compiler literally bata raha hai kaunsa header chahiye. Modern compilers bahut helpful hain.

---

## ERROR 4: Typo in header name

```cpp
#include <iostrem>
int main() { return 0; }
```

**Error:**
```
fatal error: iostrem: No such file or directory
compilation terminated.
```

**Stage:** Preprocessor (1)

**`fatal error`** ka matlab: compiler ne turant haar maan li, aage padha hi nahi.

---

## ERROR 5: Undefined reference (LINKER)

```cpp
#include <iostream>
int mystery();               // declare kiya
int main() {
    std::cout << mystery();  // use kiya
    return 0;
}
// definition kahin nahi hai!
```

**Error:**
```
/usr/bin/ld: /tmp/ccXXXX.o: in function `main':
undefined reference to `mystery()'
collect2: error: ld returned 1 exit status
```

**Stage:** Linker (4)

**Kaise pehchano ki linker error hai:**
- `ld` ya `collect2` naam aata hai
- "undefined reference" likha hota hai
- Line number nahi hota (ya function ka naam hota hai)

**Proof karo ki compiler khush tha:**
```bash
g++ -c file.cpp -o file.o     # ✅ yeh SUCCEED karega
g++ file.o -o file            # ❌ yeh FAIL karega
```

**Common causes:**
1. Function declare kiya, define nahi kiya
2. `.cpp` file compile command mein include nahi ki
3. Library link nahi ki (`-lm`, `-lpthread`)
4. Typo — declaration aur definition ke signatures match nahi karte

---

## ERROR 6: Multiple definition (ODR violation)

**`shared.h`:**
```cpp
#pragma once
int counter = 0;        // ⚠️ DEFINITION header mein!
```

**`a.cpp`:**
```cpp
#include "shared.h"
void funcA() { counter++; }
```

**`main.cpp`:**
```cpp
#include "shared.h"
int main() { return counter; }
```

```bash
g++ a.cpp main.cpp -o prog
```

**Error:**
```
/usr/bin/ld: multiple definition of `counter'
```

**Kyun?** `#pragma once` sirf **ek file ke andar** double-include rokta hai. Par yahan
do alag `.cpp` files hain — dono mein `counter` ki definition ban gayi.

**Fixes:**
```cpp
// Option 1: extern (declaration header mein, definition ek .cpp mein)
// shared.h:
extern int counter;
// shared.cpp:
int counter = 0;

// Option 2: inline variable (C++17)
// shared.h:
inline int counter = 0;

// Option 3: constexpr (implicitly inline)
// shared.h:
constexpr int MAX = 100;
```

---

## ERROR 7: Type mismatch

```cpp
#include <iostream>
int main() {
    int x = "hello";
    return 0;
}
```

**Error:**
```
error: invalid conversion from 'const char*' to 'int' [-fpermissive]
```

Compiler bata raha hai: aap `const char*` (string literal) ko `int` mein daal rahe ho.
Woh possible nahi hai.

---

## ERROR 8: Wrong `main` signature

```cpp
#include <iostream>
void main() {
    std::cout << "Hi\n";
}
```

**Error:**
```
error: '::main' must return 'int'
```

---

## ERROR 9: Missing closing brace

```cpp
#include <iostream>
int main() {
    std::cout << "Hi\n";
    return 0;
// } missing!
```

**Error:**
```
error: expected '}' at end of input
```

Yeh error confusing ho sakta hai kyunki woh **file ke end** pe dikhta hai, galti kahin
beech mein ho sakti hai.

**Fix karne ka tareeka:** Achhi indentation rakho. VS Code mein `}` pe click karo —
matching brace highlight ho jayega. Ya `Ctrl+Shift+\` se matching brace pe jump karo.

---

## ERROR 10: Nested comment

```cpp
/* bahar
   /* andar */
   yeh code ban gaya
*/
int main() { return 0; }
```

**Error:**
```
error: expected unqualified-id before '/' token
```

Pehla `*/` comment band kar deta hai. Baaki text code ban jaata hai.

---

## WARNING 1: Unused variable

```cpp
int main() {
    int unused = 5;
    return 0;
}
```

```bash
g++ -Wall -Wextra file.cpp -o file
```

**Warning:**
```
warning: unused variable 'unused' [-Wunused-variable]
```

Yeh aksar batata hai ki aap kuch bhool gaye ho.

**Agar jaan-boojh kar unused hai:**
```cpp
[[maybe_unused]] int debugValue = 5;      // C++17
```

---

## WARNING 2: Extra semicolon (silent bug!)

```cpp
#include <iostream>
int main() {
    int x = 3;
    if (x > 5);
        std::cout << "bada hai\n";
    return 0;
}
```

**Warning:**
```
warning: suggest braces around empty body in an 'if' statement [-Wempty-body]
```

**Yeh compile ho jayega aur galat output dega!** Warning ke bina aap ise kabhi nahi
pakad paate.

**Yehi wajah hai ki `-Wall -Wextra` hamesha ON.**

---

## WARNING 3: Signed/unsigned comparison

```cpp
#include <iostream>
int main() {
    int a = -1;
    unsigned int b = 1;
    if (a < b) std::cout << "a chhota hai\n";
    else       std::cout << "a bada hai?!\n";
    return 0;
}
```

**Warning:**
```
warning: comparison of integer expressions of different signedness [-Wsign-compare]
```

**Output:** `a bada hai?!` 😱

**Kyun?** `-1` ko `unsigned` mein convert kiya jaata hai → `4294967295` → jo `1` se
bada hai.

Yeh classic bug hai. Folder 03 mein detail mein padhenge.

---

## WARNING 4: Uninitialized variable

```cpp
#include <iostream>
int main() {
    int x;                    // initialize nahi kiya
    std::cout << x << "\n";   // ⚠️ UB - garbage value
    return 0;
}
```

```bash
g++ -Wall -Wextra -O2 file.cpp -o file
```

**Warning:**
```
warning: 'x' is used uninitialized [-Wuninitialized]
```

**Yeh Undefined Behaviour hai.** Value kuch bhi ho sakti hai. Har run pe alag.

---

## Debugging strategy — jab error samajh na aaye

### Step 1: Sabse pehla error padho
Baaki ignore karo.

### Step 2: Line number pe jao
Aur **uski upar wali line bhi dekho.**

### Step 3: Compiler ka suggestion padho
`did you mean...?` aksar sahi hota hai.

### Step 4: Chhota karo (minimal reproduction)
Code ko kaat-kaat ke chhota karo jab tak error bacha rahe. Aksar isi process mein
galti mil jaati hai.

### Step 5: Google karo
Error message ko copy karo, apne variable names hata do, Google karo.
Stack Overflow pe 99% chance kisi ne yeh pehle poocha hai.

### Step 6: `-fsyntax-only` use karo (fast check)
```bash
g++ -fsyntax-only -std=c++20 file.cpp     # sirf syntax check, tez hai
```

---

## Compiler ko helpful banao

```bash
# Basic (hamesha)
g++ -std=c++20 -Wall -Wextra -g file.cpp -o file

# Strict (recommended)
g++ -std=c++20 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -g file.cpp -o file

# Paranoid (HFT codebases jaisa)
g++ -std=c++20 -Wall -Wextra -Wpedantic -Wshadow -Wconversion \
    -Wsign-conversion -Wcast-align -Wunused -Wold-style-cast \
    -Wnull-dereference -Wdouble-promotion -Wformat=2 -Werror \
    -g file.cpp -o file
```

### Sanitizers — runtime bugs pakadne ke liye

```bash
# Address Sanitizer - memory bugs (buffer overflow, use-after-free, leaks)
g++ -std=c++20 -fsanitize=address -g file.cpp -o file && ./file

# Undefined Behaviour Sanitizer
g++ -std=c++20 -fsanitize=undefined -g file.cpp -o file && ./file

# Dono
g++ -std=c++20 -fsanitize=address,undefined -g file.cpp -o file && ./file
```

**Sanitizers game-changer hain.** Woh runtime bugs pakad lete hain jo kabhi kabhi
pakde nahi jaate. Folder 45 mein poora.

**Try karo:**
```cpp
#include <iostream>
int main() {
    int arr[5] = {1,2,3,4,5};
    std::cout << arr[10] << "\n";     // out of bounds!
    return 0;
}
```
```bash
g++ -std=c++20 file.cpp -o file && ./file
# shayad chal jayega, garbage value degi -- ya crash

g++ -std=c++20 -fsanitize=address -g file.cpp -o file && ./file
# ASan turant pakad lega aur exact line batayega!
```

---

## Hands-on: `06_broken_on_purpose.cpp`

`examples/06_broken_on_purpose.cpp` file mein **8 galtiyan** hain. Aapka kaam:

1. Compile karo, pehla error padho
2. Us error ko fix karo
3. Dobara compile karo
4. Repeat, jab tak clean compile na ho

Har error ke liye note karo:
- Kya error tha?
- Kis stage ka tha? (preprocessor / parser / semantic / linker)
- Kaise fix kiya?

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "50 errors = 50 galtiyan" | Aksar 1 galti se 50 errors aate hain |
| "Warnings ignore kar sakte hain" | Warnings aksar asli bugs hote hain |
| "Error line pe hi galti hai" | Missing `;` ka error agli line pe dikhta hai |
| "Compile ho gaya = code sahi hai" | Bilkul nahi. Logic bugs compile ho jaate hain |
| "Errors mera fail hai" | Errors normal hain. Har programmer ko roz aate hain |

---

## Exercises

1. Upar wale saare 10 errors aur 4 warnings **khud banao**. Har ek ka message note karo.

2. `examples/06_broken_on_purpose.cpp` ko fix karo. Kitne minute lage?

3. Ek 50-error cascade banao:
   ```cpp
   #include <iostream>
   int main() {
       int x = 5              // ; missing
       std::cout << x;
       std::cout << x;
       // ... 20 aur lines
   }
   ```
   Kitne errors aaye? Ek `;` daalne pe kitne bache?

4. Sanitizer test:
   ```cpp
   int main() {
       int* p = new int(5);
       delete p;
       return *p;              // use after free!
   }
   ```
   Pehle normally compile karo, phir `-fsanitize=address` ke saath. Fark dekho.

5. `-Werror` ke saath compile karo ek warning wale code pe. Kya hua?

6. Ek "paranoid" build script banao apne liye:
   ```bash
   cat > build.sh << 'END'
   #!/bin/bash
   g++ -std=c++20 -Wall -Wextra -Wpedantic -Wshadow -Wconversion \
       -fsanitize=address,undefined -g "$1" -o "${1%.cpp}"
   END
   chmod +x build.sh
   ./build.sh myfile.cpp
   ```

---

## Next
→ [`11-exercises.md`](11-exercises.md)
