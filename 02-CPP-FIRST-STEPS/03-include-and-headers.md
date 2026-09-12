# 03 — `#include` aur headers

## Prerequisites
`02-anatomy-line-by-line.md`

## Yeh topic abhi kyun
`#include` har C++ file ki pehli line hoti hai. Agar aapko yeh clear nahi hai, to
aage har file mein confusion rahegi — "kaunsa header include karun?", "yeh error kyun
aa raha hai?"

---

## `#include` ka ek-line matlab

> **"Is file ka poora text, yahan, is exact jagah pe, paste kar do."**

Bas. Yeh literally copy-paste hai. Koi jaadu nahi.

---

## Isko sach mein dekho

Do files banao:

**`greeting.h`**
```cpp
// yeh sirf ek line hai
int lucky = 7;
```

**`test.cpp`**
```cpp
#include "greeting.h"

int main() {
    return lucky;
}
```

Ab preprocess karo:
```bash
g++ -E test.cpp
```

Output:
```cpp
# 1 "test.cpp"
# 1 "greeting.h" 1
int lucky = 7;              <- greeting.h ka content, paste ho gaya!
# 2 "test.cpp" 2

int main() {
    return lucky;
}
```

**Dekha?** `#include "greeting.h"` line **gayab** ho gayi, aur uski jagah file ka
content aa gaya. Yehi hota hai. Har baar.

---

## `< >` vs `" "` — poora fark

```cpp
#include <iostream>       // ANGLE BRACKETS
#include "myheader.h"     // QUOTES
```

| | `<file>` | `"file"` |
|---|---|---|
| Search order | sirf system include paths | **pehle current directory**, phir system paths |
| Kiske liye | standard library, installed libraries | aapke project ki files |
| Convention | `<vector>`, `<string>`, `<boost/asio.hpp>` | `"order_book.h"`, `"utils/parser.h"` |

**Practical rule:**
- Aapki apni file → `" "`
- Standard library ya installed library → `< >`

Technically `" "` se system headers bhi mil jaate hain, par convention follow karo.

### System paths dekho
```bash
g++ -E -v -x c++ /dev/null 2>&1 | sed -n '/search starts here/,/End of search/p'
```

Aapko kuch aisa dikhega:
```
/usr/include/c++/13
/usr/include/x86_64-linux-gnu/c++/13
/usr/include
```

### Apna path add karna
```bash
g++ -I./include main.cpp -o main      # ./include ko search list mein daalo
```

---

## Common standard headers

Yeh table bookmark kar lo:

| Header | Kya milta hai | Folder |
|---|---|---|
| `<iostream>` | `cout`, `cin`, `cerr` | 04 |
| `<string>` | `std::string` | 10 |
| `<vector>` | `std::vector` | 19 |
| `<array>` | `std::array` | 09 |
| `<map>`, `<unordered_map>` | maps | 19 |
| `<algorithm>` | `sort`, `find`, `count`... | 19 |
| `<memory>` | smart pointers | 17 |
| `<cmath>` | `sqrt`, `pow`, `abs` | 19 |
| `<cstdint>` | `int32_t`, `uint64_t` | 03 |
| `<climits>` | `INT_MAX`, `INT_MIN` | 03 |
| `<limits>` | `std::numeric_limits` | 03 |
| `<chrono>` | time measurement | 35 |
| `<thread>` | threads | 26 |
| `<atomic>` | atomics | 27 |
| `<bitset>` | binary display | 03 |
| `<iomanip>` | output formatting | 04 |
| `<fstream>` | file I/O | 04 |
| `<sstream>` | string streams | 04 |

**Note:** Kabhi kabhi ek header include karne se doosra bhi mil jaata hai (transitive
include). **Us pe kabhi bharosa mat karo.** Jo use karo, woh explicitly include karo.
Warna doosre compiler pe code toot jayega.

---

## C headers in C++

C se aaye headers ke do version hote hain:

```cpp
#include <stdio.h>       // purana C style
#include <cstdio>        // ✅ C++ style — YEH USE KARO
```

| C header | C++ version |
|---|---|
| `stdio.h` | `<cstdio>` |
| `stdlib.h` | `<cstdlib>` |
| `string.h` | `<cstring>` |
| `math.h` | `<cmath>` |
| `stdint.h` | `<cstdint>` |
| `time.h` | `<ctime>` |

**Fark:** `<cxxx>` version sab kuch `std::` namespace mein daalta hai:
```cpp
#include <cmath>
double x = std::sqrt(16.0);     // std:: ke saath
```

---

## Header file kyun banate hain? (motivation)

Abhi aapki sab code ek file mein hai. Jab project bada hoga, aap ise todoge:

```
   order_book.h      <- DECLARATIONS (kya exist karta hai)
   order_book.cpp    <- DEFINITIONS  (woh kya karta hai)
   strategy.cpp      <- order_book.h include karti hai
   main.cpp          <- order_book.h include karti hai
```

**Header = ek contract.** "Yeh functions/classes exist karte hain, aise use karo."

```cpp
// ============ order_book.h ============
// Yeh sirf BATATA hai ki kya hai. Kaise karta hai, woh nahi batata.

void addOrder(int price, int qty);      // declaration
int getBestBid();                       // declaration
```

```cpp
// ============ order_book.cpp ============
// Yeh ACTUALLY karta hai.

#include "order_book.h"

void addOrder(int price, int qty) {
    // asli logic yahan
}

int getBestBid() {
    // asli logic yahan
    return 0;
}
```

```cpp
// ============ main.cpp ============
#include "order_book.h"     // sirf contract chahiye, implementation nahi

int main() {
    addOrder(100, 50);      // kaam karta hai!
    return 0;
}
```

Compile:
```bash
g++ -std=c++20 main.cpp order_book.cpp -o program
```

Poori kahani folder 08 aur 24 mein. Abhi bas idea le lo.

---

## ⚠️ Double inclusion problem

Yeh sabse common header bug hai.

```cpp
// ===== a.h =====
struct Point { int x, y; };

// ===== b.h =====
#include "a.h"

// ===== main.cpp =====
#include "a.h"      // Point define ho gaya
#include "b.h"      // b.h ne bhi a.h include kiya -> Point DOBARA define!
```

**Error:**
```
error: redefinition of 'struct Point'
```

### Solution 1: Include guards (classic, portable)

```cpp
// ===== a.h =====
#ifndef A_H          // "agar A_H define NAHI hai, to..."
#define A_H          // "...ab kar do"

struct Point { int x, y; };

#endif               // A_H
```

**Kaise kaam karta hai:**
```
Pehli baar:  A_H defined nahi -> andar ka code process hua -> A_H define ho gaya
Doosri baar: A_H already defined -> poora block SKIP ho gaya
```

### Solution 2: `#pragma once` (modern, aasan)

```cpp
// ===== a.h =====
#pragma once

struct Point { int x, y; };
```

Ek line. Bas.

| | Include guards | `#pragma once` |
|---|---|---|
| Standard mein hai? | haan | nahi (par sab compilers support karte hain) |
| Typing | 3 lines | 1 line |
| Naam clash ka risk | haan (agar do headers same macro use karein) | nahi |
| Speed | compiler ko file kholni padti hai | compiler skip kar sakta hai |

**Meri salah:** `#pragma once` use karo. GCC, Clang, MSVC — sab support karte hain.
Kuch bahut purane/exotic compilers mein nahi hota, par aapko wo kabhi nahi milenge.

---

## Header mein kya rakhein, kya nahi

### ✅ Header mein rakho
```cpp
#pragma once

// declarations
void doSomething(int x);
class MyClass { /* ... */ };
struct Point { int x, y; };

// constants
constexpr int MAX_ORDERS = 10000;

// inline functions
inline int square(int x) { return x * x; }

// templates (inhe header mein hi hona padta hai)
template <typename T>
T maximum(T a, T b) { return (a > b) ? a : b; }
```

### ❌ Header mein MAT rakho
```cpp
using namespace std;           // ❌ har include karne wali file pe thop dega
int globalCounter = 0;         // ❌ ODR violation - multiple definition error
void doWork() { /* ... */ }    // ❌ non-inline function definition
```

**ODR = One Definition Rule.** Poori duniya mein ek cheez ki sirf ek definition ho
sakti hai. Header agar 5 files mein include hui, to 5 definitions ban jayengi →
linker error. Folder 24 mein detail.

---

## `#include` ki cost (performance)

`#include <iostream>` **~30,000 lines** paste karta hai.

```bash
echo '#include <iostream>' > t.cpp
echo 'int main(){}' >> t.cpp
g++ -E t.cpp | wc -l
```

Yeh compile time pe asar daalta hai. Bade projects mein compile time ghanton mein
ja sakta hai.

**Solutions** (folder 24 mein detail):
- Sirf zaroori headers include karo
- Forward declarations use karo jahan possible ho
- Precompiled headers
- **C++20 Modules** — yeh `#include` ka replacement hai

```cpp
// C++20 modules (folder 22 mein)
import std;         // poora standard library, ek line, tez
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`#include` link karta hai" | Nahi, sirf text paste karta hai. Linking alag step hai |
| "`< >` aur `" "` same hain" | Search order alag hai |
| "Ek baar include kiya, sab jagah mil jayega" | Nahi, har `.cpp` file ko apna include chahiye |
| "Header compile hoti hai" | Nahi, header `.cpp` mein paste hokar `.cpp` compile hoti hai |
| "`#include` mein `;` lagta hai" | Nahi lagta |

---

## Exercises

1. `greeting.h` aur `test.cpp` wala experiment khud karo. `g++ -E` ka output dekho.

2. Ek header banao `mymath.h`:
   ```cpp
   #pragma once
   inline int square(int x) { return x * x; }
   inline int cube(int x)   { return x * x * x; }
   ```
   Use `main.cpp` mein use karo aur chalao.

3. **Double inclusion bug banao aur theek karo:**
   ```cpp
   // point.h (BINA guard ke)
   struct Point { int x, y; };
   
   // shape.h
   #include "point.h"
   
   // main.cpp
   #include "point.h"
   #include "shape.h"
   int main() { return 0; }
   ```
   Compile karo → error dekho → `#pragma once` daalo → dobara compile karo.

4. `#include <iostream>` ke baad kitni lines hoti hain? Aur `<vector>`? `<algorithm>`?
   ```bash
   for h in iostream vector algorithm string; do
     echo "#include <$h>" > t.cpp && echo "int main(){}" >> t.cpp
     echo "$h: $(g++ -E t.cpp | wc -l) lines"
   done
   ```

5. Yeh code compile kyun nahi hoga? Fix karo:
   ```cpp
   #include <iostream>
   int main() {
       std::vector<int> v;
       return 0;
   }
   ```
   <details><summary>Answer</summary>
   `<vector>` include nahi kiya. Kabhi kabhi yeh galti se chal jaata hai (agar
   `<iostream>` ne internally `<vector>` include kiya ho), par woh **luck** hai,
   guarantee nahi. Hamesha explicitly include karo.
   </details>

---

## Interview questions

1. `#include <x>` aur `#include "x"` mein fark?
2. Include guards kya hain? `#pragma once` se fark?
3. ODR kya hai?
4. Header file mein function definition daal sakte ho? Kab?
5. Forward declaration kya hai aur kab use karte hain?

---

## Next
→ [`04-main-function.md`](04-main-function.md)
