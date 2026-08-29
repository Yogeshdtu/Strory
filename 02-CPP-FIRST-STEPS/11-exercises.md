# 11 — Phase 1 Revision + Exercises

## Prerequisites
Is folder ke saare lessons (01–10)

## Yeh file kyun
Ab aapke paas asli C++ knowledge hai. Check karo ki woh pakki hai — kyunki agla folder
(variables) isi pe khada hoga.

---

## PART A — Concept check

Bina dekhe, likhkar jawab do.

### Hello World anatomy
1. `#` kya batata hai? Uske baad `;` kyun nahi lagta?
2. `#include` exactly kya karta hai? (ek line mein)
3. `<iostream>` aur `"myfile.h"` mein kya fark hai?
4. `int main()` mein `int` kyun hai?
5. `std` kya hai? `::` kya karta hai?
6. `cout` function hai ya object? Explain.
7. `<<` actually kya hai? Chaining kaise kaam karti hai?
8. `"Hello"` ka type kya hai? Uska `sizeof` kya hai aur kyun?
9. `\n` kitne characters hai?
10. `return 0;` ka `0` kahan jaata hai?

### Headers
11. Include guard kya hai? `#pragma once` se kya fark?
12. Header file mein `int x = 5;` likhne se kya problem hoti hai?
13. ODR kya hai?
14. Header mein kya rakh sakte ho, kya nahi?

### main()
15. `void main()` kyun galat hai?
16. `main()` mein `return` optional kyun hai, baaki functions mein nahi?
17. `std::exit()` aur `return` mein kya fark hai?
18. `main()` se pehle koi code chal sakta hai?

### Statements & scope
19. `;` ka exact kaam kya hai?
20. Class/struct ke baad `;` lagta hai, function ke baad nahi. Kyun?
21. `if (x > 5);` mein kya problem hai?
22. Scope aur lifetime mein kya fark hai?
23. Destruction kis order mein hoti hai? Kyun?
24. Shadowing kya hai?

### Strings
25. `\n` aur `std::endl` mein kya fark hai? Kaunsa use karein aur kyun?
26. `'A'` aur `"A"` mein kya fark hai?
27. Raw string literal kya hai? Kab use karte hain?
28. String literal ko modify karne se kya hota hai?

### Compilation
29. Compilation ke 4 stages naam se batao.
30. Name mangling kya hai aur kyun zaroori hai?
31. `undefined reference` kis stage ka error hai?
32. `nm` output mein `U` ka kya matlab hai?
33. Static vs dynamic linking — 3 trade-offs.
34. `std::cout << "x"` aakhir mein kaunsa syscall banta hai?

---

## PART B — Output prediction

**Pehle guess karo, phir chalao.** Guess likhkar rakho.

### B1
```cpp
#include <iostream>
int main() {
    std::cout << "A" << "B" << "C";
    std::cout << "\n";
    std::cout << 1 << 2 << 3 << "\n";
    std::cout << 1 + 2 + 3 << "\n";
}
```
<details><summary>Answer</summary>

```
ABC
123
6
```
Teesri line mein `+` pehle chala (higher precedence than `<<`), phir result print hua.
</details>

### B2
```cpp
#include <iostream>
int main() {
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
    std::cout << x << "\n";
}
```
<details><summary>Answer</summary>`12321`</details>

### B3
```cpp
#include <iostream>
struct T {
    const char* n;
    T(const char* s) : n(s) { std::cout << "+" << n; }
    ~T()                    { std::cout << "-" << n; }
};
T g("G");
int main() {
    std::cout << "[";
    T a("A");
    { T b("B"); }
    T c("C");
    std::cout << "]";
}
```
<details><summary>Answer</summary>

```
+G[+A+B-B+C]-C-A-G
```
- `G` main se pehle bana
- `[` print
- `A` bana
- `B` bana, block khatam hote hi mara
- `C` bana
- `]` print
- main khatam: `C` phir `A` (ulta order)
- `G` sabse aakhir mein
</details>

### B4
```cpp
#include <iostream>
int main() {
    std::cout << sizeof("")      << " ";
    std::cout << sizeof("A")     << " ";
    std::cout << sizeof("a\n")   << " ";
    std::cout << sizeof("\\")    << " ";
    std::cout << sizeof('A')     << "\n";
}
```
<details><summary>Answer</summary>`1 2 3 2 1`

Har string literal mein `\0` extra hota hai. `\n` aur `\\` ek-ek character hain.
`'A'` ek `char` hai — 1 byte.
</details>

### B5
```cpp
#include <iostream>
int main() {
    int x = 3;
    if (x > 5);
        std::cout << "bada\n";
    std::cout << "done\n";
}
```
<details><summary>Answer</summary>

```
bada
done
```
Extra `;` ne `if` ka body khali kar diya. `"bada"` wali line `if` ke bahar hai.
`-Wall` isko warning deta hai.
</details>

---

## PART C — Find the bug

Har snippet mein ek bug hai. Batao: **kya bug hai aur kis stage pe pakda jayega?**

### C1
```cpp
#include <iostream>;
int main() { return 0; }
```
<details><summary>Answer</summary>
`#include` ke baad extra `;`. **Stage 2 (parser)** — woh `;` ek stray statement ban jaata hai
global scope mein. GCC warning deta hai, kuch compilers error.
</details>

### C2
```cpp
#include <iostream>
struct Point {
    int x, y;
}
int main() { return 0; }
```
<details><summary>Answer</summary>
Struct ke baad `;` missing. **Stage 2 (parser)**. Error confusing hota hai —
compiler sochta hai aap `Point` type ka `main` naam ka variable bana rahe ho.
</details>

### C3
```cpp
#include <iostream>
int compute();
int main() { std::cout << compute(); }
```
<details><summary>Answer</summary>
`compute()` declare kiya, define nahi. **Stage 4 (linker)** —
`undefined reference to 'compute()'`.
Proof: `g++ -c` succeed karega, full link fail karega.
</details>

### C4
```cpp
// header.h
#pragma once
int globalCount = 0;
```
Do `.cpp` files isko include karti hain.
<details><summary>Answer</summary>
ODR violation. **Stage 4 (linker)** — `multiple definition of 'globalCount'`.
`#pragma once` sirf ek TU ke andar double-include rokta hai, alag TUs mein nahi.
Fix: `inline int globalCount = 0;` (C++17) ya `extern` + ek `.cpp` mein definition.
</details>

### C5
```cpp
#include <iostream>
int main() {
    const char* s = "Hello";
    s[0] = 'J';
    std::cout << s << "\n";
}
```
<details><summary>Answer</summary>
Do problems:
1. `s[0] = 'J'` compile hi nahi hoga kyunki `s` `const char*` hai.
2. Agar aap `const` hata do, to yeh **Stage 5 (runtime)** pe crash karega —
   string literals read-only memory (`.rodata`) mein hoti hain.

Fix: `char s[] = "Hello";` — yeh ek modifiable copy banata hai.
</details>

---

## PART D — Practical tasks

### D1. Poora program bina dekhe
Ek khali file mein, bina kuch dekhe, likho:
- Hello World program
- Compile command (saare flags ke saath)
- Run command

### D2. Pipeline explorer
Apne Hello World pe poori pipeline chalao:
```bash
g++ -E hello.cpp -o hello.i && wc -l hello.i
g++ -S hello.cpp -o hello.s && c++filt < hello.s | grep -A8 "^main:"
g++ -c hello.cpp -o hello.o && nm -C hello.o
g++ hello.o -o hello && ldd hello
strace -c ./hello 2>&1 | tail
```
Har output ko apne notes mein likho.

### D3. Error catalog banao
Lesson 10 ke saare 10 errors + 4 warnings **khud banao**. Ek table banao:

| # | Error message | Stage | Fix |
|---|---|---|---|
| 1 | | | |

### D4. `06_broken_on_purpose.cpp` fix karo
Time note karo. Kitne minute lage? Kitne compile cycles?

### D5. Escape sequence challenge
Ek program likho jo **exactly** yeh output de:
```
+----------------------+
| Naam    | "Rahul"    |
| Path    | C:\Users   |
| Tab		| Yahan      |
+----------------------+
```
<details><summary>Hint</summary>
`\"`, `\\`, `\t` chahiye. Ya raw strings use karo.
</details>

### D6. `endl` benchmark
```cpp
#include <iostream>
#include <chrono>
int main() {
    const int N = 100000;
    auto t1 = std::chrono::steady_clock::now();
    for (int i = 0; i < N; ++i) std::cout << i << "\n";
    auto t2 = std::chrono::steady_clock::now();
    for (int i = 0; i < N; ++i) std::cout << i << std::endl;
    auto t3 = std::chrono::steady_clock::now();
    std::cerr << "\\n:   " << std::chrono::duration<double,std::milli>(t2-t1).count() << " ms\n";
    std::cerr << "endl: " << std::chrono::duration<double,std::milli>(t3-t2).count() << " ms\n";
}
```
```bash
g++ -std=c++20 -O2 bench.cpp -o bench && ./bench > /dev/null
```
Kitna fark mila? Note karo.

### D7. Sanitizer experiment
```cpp
int main() {
    int arr[5] = {1,2,3,4,5};
    return arr[10];
}
```
Normal compile karo, phir `-fsanitize=address` ke saath. Fark dekho.

### D8. Multi-file project
Teen files banao:

**`math_utils.h`**
```cpp
#pragma once
int square(int x);
int cube(int x);
```

**`math_utils.cpp`**
```cpp
#include "math_utils.h"
int square(int x) { return x * x; }
int cube(int x)   { return x * x * x; }
```

**`main.cpp`**
```cpp
#include <iostream>
#include "math_utils.h"
int main() {
    std::cout << square(5) << " " << cube(3) << "\n";
}
```

Compile:
```bash
g++ -std=c++20 -Wall main.cpp math_utils.cpp -o prog && ./prog
```

Ab try karo: `g++ main.cpp -o prog` (sirf main.cpp). Kya error aaya? Kis stage ka?
<details><summary>Answer</summary>
`undefined reference to 'square(int)'` — **linker error**. `math_utils.cpp` compile
hi nahi hui, isliye definition kahin nahi mili. Header sirf declaration deta hai.
</details>

---

## PART E — Self-assessment

```
[ ] Main bina dekhe Hello World likh sakta hoon
[ ] Main har token ka matlab bata sakta hoon (#, include, std, ::, cout, <<, ;, ...)
[ ] Mujhe #include ka exact kaam pata hai (text paste)
[ ] Mujhe < > vs " " ka fark pata hai
[ ] Mujhe include guards / #pragma once samajh aate hain
[ ] Mujhe pata hai main() special kyun hai
[ ] Main exit code ka matlab samajh sakta hoon
[ ] Mujhe pata hai ; kahan lagta hai aur kahan nahi
[ ] Mujhe scope aur lifetime ka fark pata hai
[ ] Mujhe pata hai destruction ulta order mein hoti hai
[ ] Mujhe \n vs endl ka fark pata hai (aur kyun)
[ ] Mujhe 'A' vs "A" ka fark pata hai
[ ] Main compilation ke 4 stages bata sakta hoon
[ ] Main error dekh kar bata sakta hoon woh kis stage ka hai
[ ] Maine sabhi examples chalaye hain
[ ] Maine 06_broken_on_purpose.cpp fix kar liya hai
[ ] Main -Wall -Wextra ke saath compile karta hoon (hamesha)
```

**Scoring:**
- **15-17** → Bahut badhiya. Folder 03 pe jao. 🎉
- **12-14** → Achha. Jo miss hua padho, phir aage.
- **8-11** → Lessons 02, 06, 09 dobara karo.
- **< 8** → Poora folder dobara. Examples zaroor chalao.

---

## PART F — Challenge

**Challenge: "Compiler Detective"**

Ek chhota program likho jisme **jaan-boojh kar** ek-ek karke yeh 5 alag stage ke errors
hon. Har version ko alag file mein save karo:

1. `err1_preprocessor.cpp` — preprocessor error
2. `err2_parser.cpp` — syntax error
3. `err3_semantic.cpp` — type/name error
4. `err4_linker.cpp` — linker error
5. `err5_runtime.cpp` — runtime crash

Har file ke top pe comment mein likho:
- Expected error message
- Expected stage
- Fix

Phir sab chalao aur verify karo ki aapka prediction sahi tha.

**Yeh challenge poore course mein sabse zyada kaam aayega.** Debugging speed hi aapko
ek achha engineer banati hai.

---

## Aapne Phase 1 complete kar liya! 🎉

Ab aap:
- C++ program likh sakte ho
- Compile aur run kar sakte ho
- Har token ka matlab jaante ho
- Errors ko padh aur classify kar sakte ho
- Compilation pipeline samajhte ho
- Scope aur lifetime ka concept jaante ho

**Agle folder mein hum data store karna seekhenge — variables.**

Aur ab aapko pata hoga ki `int age = 20;` mein exactly kya ho raha hai, memory mein
kya bana, aur woh kab marega.

---

## Next
→ [`../03-VARIABLES-DATA-TYPES/00-README.md`](../03-VARIABLES-DATA-TYPES/00-README.md)
