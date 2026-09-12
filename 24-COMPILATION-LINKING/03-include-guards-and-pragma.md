# 03 — Include guards, `#pragma once`, include hygiene

## Prerequisites
- `01-translation-units.md`, `02-preprocessor-deep.md`

## Yeh topic abhi kyun
`#include` = text paste (file 01). Agar ek header do raaston se ek hi TU mein aa
gaya (`a.hpp` includes `common.hpp`; `b.hpp` bhi includes `common.hpp`; TU dono
include karta) → `common.hpp` ka content **do baar** → "redefinition" errors.
Include guards yeh rokte hain. Aur "include hygiene" — sirf woh include karo jo
use karte ho — build speed aur maintainability ka bada lever hai.

---

## Problem: double inclusion

```cpp
// point.hpp   (NO GUARD)
struct Point { double x, y; };
```
```cpp
// shape.hpp
#include "point.hpp"
struct Circle { Point c; double r; };
```
```cpp
// main.cxx
#include "point.hpp"      // Point defined
#include "shape.hpp"      // -> #include "point.hpp" -> Point defined AGAIN
                          //    error: redefinition of 'struct Point'
```

Ek TU mein ek `class`/`struct`/`inline function` ki **do definitions** = error
(ODR — file 04). Fix: header ko "sirf ek baar expand ho" banao.

---

## Include guard (classic, portable)

```cpp
// point.hpp
#ifndef PROJECT_POINT_HPP        // pehli baar: undefined -> aage badho
#define PROJECT_POINT_HPP        // ab defined

struct Point { double x, y; };

#endif  // PROJECT_POINT_HPP
```

- Pehli inclusion: `PROJECT_POINT_HPP` undefined → `#define` + body.
- Doosri inclusion (usi TU mein): `PROJECT_POINT_HPP` defined → `#ifndef` false →
  poora body skip.
- **Har header ka macro unique hona chahiye.** Convention: `PROJECT_PATH_FILE_HPP`
  (`MYLIB_NET_SOCKET_HPP`). `_POINT_H_` jaisa generic → clash risk.

⚠️ `__` se shuru ya `_` + uppercase — **reserved** (implementation ke liye). Guard
macro `_FOO_H` / `__FOO_H` mat likho; `FOO_H` ya prefix wala.

---

## `#pragma once`

```cpp
// point.hpp
#pragma once

struct Point { double x, y; };
```

- Compiler file ko (canonical path se) ek baar hi process karta hai.
- **Fayda:** ek line, koi macro-name clash, koi typo risk, thoda faster (compiler
  ko file dobara open/scan karne ki zaroorat nahi).
- **Technically non-standard** — par GCC, Clang, MSVC, ICC sab support karte.
- **Edge case:** agar ek hi file do alag paths se dikhti hai (symlinks, network
  mounts, weird build setups) → compiler use "do alag files" samajh sakta →
  double inclusion. Modern compilers inode/content se detect karte, mostly OK.

### Kya use karein?

| Situation | Choice |
|---|---|
| Naya project, single toolchain family | **`#pragma once`** — simplest, safest in practice |
| Max portability / ancient compilers / paranoid | **Include guard** (unique macro) |
| Best of both | Dono: `#pragma once` + guard (kuch codebases karte hain) |

Zyada tar modern C++ codebases: `#pragma once`.

**Modules** (folder 22) yeh poora problem khatam karte — `import` idempotent hai by
design, koi guard chahiye hi nahi.

---

## Include hygiene

### Rule 1 — "include what you use" (IWYU)

Har file **directly** include kare jo woh use karti hai — transitive includes pe
mat depend karo.

```cpp
// bad.hpp
#include <vector>            // ye <string> bhi pull karta (implementation detail)

std::string name();          // ⚠️ <string> ko directly include nahi kiya
std::vector<int> nums();
```

Kal `<vector>` ne `<string>` include karna band kiya → aapki file toot gaya, bina
aapne kuch badle. Fix: `#include <string>` bhi likho.

Tool: **`include-what-you-use`** (Clang-based) — batata hai kaunsa include missing/
extra hai.

### Rule 2 — headers self-contained hon

Har header apne aap compile hona chahiye:

```cpp
// widget.hpp — self-contained?
struct Widget { std::string name; };   // ⚠️ <string> include nahi -> akele compile FAIL
```

Test: `echo '#include "widget.hpp"' > t.cpp && g++ -c t.cpp`. Har header ke liye
pass hona chahiye (CI mein automate karo).

### Rule 3 — forward declare jab possible ho

```cpp
// bad: header mein poora include
#include "order.hpp"                 // heavy
class Book { std::vector<Order> levels_; };

// good: sirf declaration chahiye (pointer/reference)
class Order;                          // forward declaration
class Book { std::vector<Order*> levels_; void add(const Order&); };
// order.hpp sirf book.cpp mein include
```

Forward-decl kaafi hai jab aap type ka sirf **pointer / reference** use karte ho,
ya function signature mein by-value pass/return (definition `.cpp` mein). **Nahi**
kaafi jab: member by value, inheritance, `sizeof`, member access, template arg
(usually).

### Rule 4 — heavy stuff `.cpp` mein / pImpl

```cpp
// gateway.hpp — chhota, koi heavy include nahi
class Gateway {
public:
    Gateway(); ~Gateway();
    void send(const Order&);
private:
    struct Impl;
    std::unique_ptr<Impl> p_;        // pImpl — <asio>, <ssl> sab gateway.cpp mein
};
```

Ek widely-included header ko patla rakhna = **poore project ka build time** kam.

---

## Andar kya hota hai

- Include guard: preprocessor har inclusion pe `#ifndef` check karta hai — file
  phir bhi **open + scan** hoti hai (bas body skip). "Include guard optimization"
  — GCC/Clang detect kar lete ki poora file ek guard mein hai aur agli baar file
  hi skip kar dete.
- `#pragma once`: compiler processed-files ka set (path/inode) rakhta hai; dobara
  aaya → skip, **file open bhi nahi**. Isliye thoda faster on huge include graphs.
- Bina guard, ek project ka include graph exponential blow-up kar sakta hai (N
  headers, har ek M ko include kare → ek TU mein ek header 2^k baar). Guards ise
  linear rakhte hain.

---

## > **HFT relevance**
> - **Build time = dev iteration speed.** Ek HFT codebase mein core headers (types,
>   book, config) sab jagah include hote hain. Unhe patla rakhna (forward decls,
>   pImpl for anything touching `<asio>`/`<boost>`/`<ssl>`, no heavy STL in
>   widely-used headers) full-rebuild ko minutes se seconds pe la sakta hai —
>   file 13.
> - **`#pragma once` everywhere** — teams ek toolchain family pe hoti hain,
>   simplicity jeetti.
> - **Self-contained headers + CI check** — "compile every header alone" test se
>   "kisi ne transitive include pe depend kiya" wale bugs pehle pakde jaate.
> - **Modules** (folder 22) jaha available hon — parse-once, no macro leakage,
>   deterministic. HFT shops slowly adopt kar rahe hain.

---

## Hands-on

```bash
# double-inclusion demo:
mkdir -p /tmp/inc && cd /tmp/inc
printf 'struct P { int x; };\n' > p.hpp                    # NO guard
printf '#include "p.hpp"\nstruct C { P p; };\n' > c.hpp
printf '#include "p.hpp"\n#include "c.hpp"\nint main(){}\n' > m.cpp
g++ -std=c++20 m.cpp -o m         # error: redefinition of 'struct P'
printf '#pragma once\nstruct P { int x; };\n' > p.hpp       # add guard
g++ -std=c++20 m.cpp -o m && echo OK
```

`examples/01_multi_file_project` ke headers pe `#pragma once` hai — hata ke `main`
compile karo.

---

## ⚠️ Traps

### Trap 1 — guard macro duplicate
```cpp
// a.hpp aur b.hpp dono:
#ifndef UTIL_H
#define UTIL_H
...
```
Ek include hone ke baad doosra poori tarah skip → missing declarations. Unique
macros (`PROJECT_A_HPP`, `PROJECT_B_HPP`).

### Trap 2 — reserved guard name
`#ifndef __POINT_H__` — `__`-prefixed identifiers reserved. `PROJECT_POINT_HPP`.

### Trap 3 — guard ke bahar code
```cpp
#pragma once
#include <vector>
#ifndef X_HPP
#define X_HPP
struct X {};
#endif
using XVec = std::vector<X>;    // ⚠️ guard ke bahar -> multiple inclusion pe redefine
```
Sab kuch guard ke **andar**.

### Trap 4 — transitive include pe depend
`<vector>` se `<string>` "mil gaya" → apne code mein `<string>` mat use karo bina
include kiye. Library change tumhe todega.

### Trap 5 — header not self-contained
`widget.hpp` `std::string` use karta par `<string>` include nahi — kisi file mein
kaam karta (jisme pehle `<string>` aaya), doosri mein fail.

### Trap 6 — `#pragma once` + copied file
Ek header ki do physical copies (build systems, vendored deps) → `#pragma once`
dono ko alag samajhta → double definition. Ek canonical copy rakho.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "include guard file ko dobara open hone se rokta" | Guard: open + scan, body skip. `#pragma once`: file hi skip |
| "`#pragma once` standard nahi, use mat karo" | Har major compiler supports; practically safe aur simpler |
| "transitive includes pe depend karna fine" | IWYU — jo use karo woh directly include |
| "har header apne aap compile ho jaayega" | Nahi jab tak self-contained na ho — CI mein test |
| "forward declaration se kaam nahi chalta" | Pointer/ref/signature ke liye kaafi; heavy include tab hatata |
| "guards linker ke liye hain" | Preprocessor/compiler ke liye — ek TU ke andar double-def rokte |

---

## Exercises

1. **Why the error:** `a.hpp` defines `struct S {};` (no guard). `main.cpp`
   `#include "a.hpp"` twice. Error at which stage, exact-ish message?

   <details><summary>Answer</summary>

   **Compile** (not link): `error: redefinition of 'struct S'` — the second paste
   in the same TU. Add `#pragma once` or an include guard.
   </details>

2. **Guard collision:** `net.hpp` and `math.hpp` both start `#ifndef HEADER_H /
   #define HEADER_H`. A TU includes `net.hpp` then `math.hpp`. What breaks?

   <details><summary>Answer</summary>

   After `net.hpp` defines `HEADER_H`, `math.hpp`'s `#ifndef HEADER_H` is false →
   `math.hpp`'s entire body is skipped → every symbol declared in `math.hpp` is
   "undeclared" at use sites. Unique guard macros per file.
   </details>

3. **Forward declare or not:** for each, can `class Order;` (fwd decl) replace
   `#include "order.hpp"`? (a) `Order* p;` member, (b) `Order o;` member,
   (c) `void f(const Order&);`, (d) `class Book : Order`, (e) `std::vector<Order>`
   member.

   <details><summary>Answer</summary>

   (a) yes (pointer). (b) no (needs size/layout). (c) yes (reference in
   signature; definition needed only where `f` is defined/called with a complete
   `Order`). (d) no (base class needs full definition). (e) no in general
   (`std::vector<Order>` as a *member* needs `Order` complete at least by the
   point of use; practically include it).
   </details>

4. **Self-contained test:** how do you check, in CI, that every `.hpp` compiles
   on its own?

   <details><summary>Answer</summary>

   For each header `h`, generate a tiny `.cpp` that just does `#include "h"` and
   compile it with `-c` (and the project's warning flags). Fail the build if any
   don't compile. (Or use `include-what-you-use` / a CMake helper that adds a
   per-header compile test.)
   </details>

5. **Build-time lever:** `types.hpp` is included by 400 of 500 TUs and pulls in
   `<regex>` (heavy). It only needs `std::string` and a forward-declared
   `class Parser`. What do you change and what's the payoff?

   <details><summary>Answer</summary>

   Remove `#include <regex>` and `#include "parser.hpp"` from `types.hpp`; replace
   with `#include <string>` and `class Parser;`. Move the `<regex>`/`parser.hpp`
   includes into the few `.cpp` that actually use them. Payoff: 400 TUs stop
   parsing `<regex>` (thousands of lines each) → large full-build speedup, and
   touching `parser.hpp` no longer triggers a near-full rebuild.
   </details>

---

## Interview questions

1. Double-inclusion problem — kyun error, kaunse stage pe?
2. Include guard vs `#pragma once` — mechanism aur trade-offs.
3. "Include what you use" — kyun, ek concrete breakage jo isse bachta hai.
4. Self-contained header — definition aur test.
5. Forward declaration kab kaafi hai, kab nahi?
6. pImpl — build time aur ABI ke liye kya deta hai?
7. Guard macro naming: kya avoid karo aur kyun (`__X`, `_X`)?

---

## Next
→ [`04-odr-deep.md`](04-odr-deep.md)
