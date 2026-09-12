# 01 — Translation units aur compilation model

## Prerequisites
- `02-CPP-FIRST-STEPS` file 01–02 (compile karna, `g++` basics)
- `08-FUNCTIONS` (declaration vs definition)
- [`examples/01_multi_file_project/`](examples/01_multi_file_project/)

## Yeh topic abhi kyun
Folder 01/02 mein aapne ek `.cpp` → ek `exe` dekha, aur "compile" ko ek black box
maana. Ab jab projects 2+ files ke hote hain, aapko poora model chahiye:
**source → preprocessing → translation unit → object file → linking → executable**.
Bina iske, "undefined reference" errors, ODR violations, aur build systems sab
jaadu lagenge.

---

## Ek picture

```
foo.cpp ──►[ preprocess ]──► foo.ii ──►[ compile ]──► foo.s ──►[ assemble ]──► foo.o ─┐
bar.cpp ──►[ preprocess ]──► bar.ii ──►[ compile ]──► bar.s ──►[ assemble ]──► bar.o ─┼─►[ link ]──► a.out
                                                                     libc/libstdc++ ─┘
```

- **Preprocess** — `#include` ko file ke content se replace, macros expand,
  `#if`/`#ifdef` resolve, comments hataao. Output: ek bada text blob.
- **Compile** — us blob (ab **translation unit**) ko parse, semantic-check,
  optimize, aur **assembly** emit karo.
- **Assemble** — assembly ko **machine code** + metadata = **object file** (`.o`).
- **Link** — saari `.o` + libraries ko jodo, symbols resolve karo, ek
  **executable** (ya library) banao.

`g++ foo.cpp -o foo` yeh saare steps ek command mein chalata hai. Bade projects
inhe alag karte hain (`-c` = "sirf `.o` banao, link mat karo").

---

## Translation unit (TU) — theek definition

**TU = ek source file + uske saare `#include` ki hui files + preprocessing ke
baad ka result**, minus jo `#if` ne hataa diya.

- Ek `.cpp` = ek TU (usually). `foo.cpp` jo `<vector>`, `<string>`, aur
  `"mathx.hpp"` include karta hai → un sab ka content `foo.cpp` mein "paste" hoke
  ek TU banta hai — **hazaron lines**, aksar.
- Compiler **ek waqt mein ek TU** dekhta hai. `foo.cpp` compile karte waqt use
  `bar.cpp` ke baare mein **kuch nahi pata** — sirf woh declarations jo dono ne
  ek common header se paayi.
- Har TU → ek `.o` file.

```bash
g++ -std=c++20 -c mathx.cxx -o mathx.o     # TU 1 -> object 1
g++ -std=c++20 -c stats.cxx -o stats.o     # TU 2 -> object 2
g++ -std=c++20 -c main.cxx  -o main.o      # TU 3 -> object 3
g++ mathx.o stats.o main.o -o app          # link
```

---

## Declaration vs definition — TU model mein kyun matter karta hai

| | Declaration | Definition |
|---|---|---|
| Kya kehta hai | "yeh cheez exist karti hai, signature yeh hai" | "yeh rahi poori cheez (body / storage)" |
| Example | `int add(int, int);` | `int add(int a, int b) { return a+b; }` |
| Kitni baar | har TU mein jitni baar chaho | **poore program mein ek** (non-inline) — ODR (file 04) |
| Kahan | header (sab TUs ko chahiye) | ek `.cpp` |

- `main.cxx` ko `add` **call** karne ke liye sirf uska **declaration** chahiye
  (header se). Compiler `main.o` mein ek "yahaan `add` ka call hai, address baad
  mein bharna" wala **undefined reference** chhod deta hai.
- **Linker** `add` ki **definition** `mathx.o` mein dhoondta hai aur woh
  reference resolve karta hai.
- Definition kisi bhi `.o` mein na mile → `undefined reference to 'add'` (file 10).
- Definition **do** `.o` mein mile → `multiple definition of 'add'` (file 04).

---

## Header mein kya jaata hai / nahi jaata

| Header mein ✅ | Header mein ❌ (→ `.cpp` mein) |
|---|---|
| Function **declarations** | Non-inline function **definitions** (bodies) |
| `class` / `struct` definitions | Namespace-scope variable **definitions** (`int g_x;`) |
| `template` definitions (poori) | |
| `constexpr` / `consteval` function bodies | |
| `inline` function / `inline` variable definitions | |
| `enum` definitions, type aliases | |
| `extern` variable **declarations** (`extern int g_x;`) | |

Non-inline function ya variable ki **definition header mein daali** → jo bhi 2+
TUs woh header include karein, unme 2 definitions → **ODR violation** → link error
(`examples/02_odr_violation/`).

---

## Separate compilation — kyun

```
100-file project. util.cpp badla.
  Full rebuild:        100 TUs recompile = minutes
  Separate compilation: 1 TU recompile + 1 link = seconds
```

- Har `.o` alag banti hai; **sirf badli hui files** rebuild.
- Parallel: `make -j8` 8 TUs ek saath compile.
- Build systems (Make file 11, CMake file 12) yeh dependency-tracking automate
  karte hain.

Trade-off: cross-TU optimization (inlining across files) default nahi hota —
compiler `mathx.o` ki body `main.o` compile karte waqt nahi dekhta. Iske liye
**LTO** (file 15) chahiye, jo "sab kuch ek saath dekho" ka fayda link time pe
wapas laata hai.

---

## `#include` — sirf text substitution

```cpp
// mathx.hpp
int gcd(int, int);
```
```cpp
// main.cxx
#include "mathx.hpp"        // <-- yeh line preprocessing ke baad literally
                            //     "int gcd(int, int);" ban jaati hai
int main() { return gcd(48, 18); }
```

`g++ -E main.cxx` chalakar dekho — poora expanded TU (hazaron lines agar `<iostream>`
include kiya). `#include` koi "import" nahi — bas copy-paste. Isliye include guards
chahiye (file 03), aur isliye bade headers build slow karte hain (file 13).
**Modules** (folder 22) ise fix karte hain.

---

## `<...>` vs `"..."` include

```cpp
#include <vector>          // system/library headers — compiler ke include paths
#include "mathx.hpp"       // project headers — pehle current file ki directory,
                           // phir -I paths, phir system paths
```

`-I<dir>` se extra search directories add hoti hain (`examples/05_makefile_project`
mein `-Iinclude`).

---

## > **HFT relevance**
> - **Build hygiene = iteration speed.** Ek HFT engineer din mein 100+ baar
>   compile karta hai. Headers ko patla rakhna (forward declarations, `pImpl`,
>   heavy stuff `.cpp` mein) minutes bachta hai — file 13.
> - **TU boundaries = optimization boundaries** (bina LTO). Hot code jo cross-file
>   inline hona chahiye woh ya to header mein `inline`/`constexpr`/template ho, ya
>   poora binary `-flto` se bane (file 15). HFT builds aksar LTO + single "unity"
>   TUs use karte hain taaki compiler poore hot path ko ek saath dekhe.
> - **`static_assert` in headers** — layout/size/ABI invariants har TU mein
>   enforce (folder 23 file 12), taaki ek TU ka `-DFEATURE` doosre se silently
>   drift na kare (ODR — file 04).

---

## Hands-on

```bash
cd 24-COMPILATION-LINKING/examples/01_multi_file_project && ./build.sh
```

Phir:
- `g++ -std=c++20 -E main.cxx | wc -l` — expanded TU kitni lines? (`<cstdio>`
  include karne se dekho farq).
- `g++ -std=c++20 -S mathx.cxx -o -` — sirf assembly, no linking.
- `g++ -c` se 3 alag `.o` banao; `nm main.o` mein `U` (undefined) symbols dekho,
  `nm mathx.o` mein `T` (defined).
- Link line se `mathx.o` hatao → `undefined reference to mathx::gcd`.

---

## ⚠️ Traps

### Trap 1 — non-inline function/variable ki definition header mein
```cpp
// util.hpp
int counter = 0;                 // ⚠️ har TU jo include kare -> ek definition -> ODR
inline int counter = 0;          // ✅ (C++17 inline variable) ya `extern` + one .cpp def
```

### Trap 2 — `#include` ko `import` samajhna
`#include` = text paste. Order matters, macros leak in, guards chahiye, slow.
Modules (folder 22) alag.

### Trap 3 — "ek file badla, poora rebuild"
Build system dependency tracking (`-MMD`, Make/CMake) set karo — file 11.

### Trap 4 — TU ke andar `bar.cpp` ke internals use karne ki koshish
`foo.cpp` sirf woh dekhta hai jo dono ne common header se paaya. Cross-file access
= shared declaration (header) + link.

### Trap 5 — header ko `.cpp` samajh ke compile line pe daalna
`g++ main.cpp mathx.hpp -o app` — GCC `mathx.hpp` ko alag precompile karne ki
koshish karega (`.gch`), ya warn karega. Headers include hote hain, compile nahi.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`g++ file.cpp` ek step hai" | 4 phases: preprocess → compile → assemble → link |
| "TU = ek `.cpp` file" | `.cpp` + saare includes, preprocessing ke baad (+ `#if` cuts) |
| "compiler poore project ko dekhta hai" | Ek TU at a time; cross-TU sirf link (ya LTO) |
| "`#include` header ko import karta hai" | Text copy-paste — guards + macro hygiene chahiye |
| "declaration aur definition same" | Decl = "exists"; def = body/storage; def exactly once (non-inline) |
| "sab kuch header mein daal do, convenient" | Non-inline defs → ODR; heavy headers → slow builds |

---

## Exercises

1. **Phases:** `g++ -std=c++20 a.cpp b.cpp -o prog` — internally kaunse steps
   kitni baar? (a.cpp aur b.cpp dono).

   <details><summary>Answer</summary>

   preprocess ×2, compile ×2, assemble ×2 (→ `a.o`, `b.o` in a temp dir), link ×1
   (a.o + b.o + libstdc++/libc → prog). `-c` deta to link skip, `.o` files rakh
   leta.
   </details>

2. **Decl/def:** `mathx.hpp` mein `int square(int x) { return x*x; }` (no `inline`).
   Do `.cxx` isse include karti hain. Compile OK? Link?

   <details><summary>Answer</summary>

   Compile: har TU OK (definition dikhti hai). Link: **`multiple definition of
   square(int)`** — do `.o` mein body. Fix: `inline` lagao, ya declaration header
   mein + body ek `.cxx` mein.
   </details>

3. **Undefined ref:** `main.cxx` `foo()` call karta hai, `foo` ka declaration
   header mein hai, definition `foo.cxx` mein. `g++ main.cxx -o app` (foo.cxx
   nahi) — kaunsa step fail, kya message?

   <details><summary>Answer</summary>

   Compile succeeds (`main.o` banti hai, `foo` ko `U`). **Link** fails:
   `undefined reference to 'foo()'`. Fix: `g++ main.cxx foo.cxx -o app` ya pehle
   `foo.o` bana ke link line pe do.
   </details>

4. **Expansion size:** `echo '#include <iostream>' > t.cpp; echo 'int main(){}' >> t.cpp`
   — `g++ -E t.cpp | wc -l` roughly kitna? Aur `<cstdio>` se?

   <details><summary>Answer</summary>

   `<iostream>` → typically **20,000–40,000+** lines (pulls locale, streams,
   etc.). `<cstdio>` → few hundred–~1000. Isiliye `<cstdio>`/`printf` chhote,
   fast-compiling examples ke liye handy, aur heavy headers build time khaate hain.
   </details>

5. **Separate compile win:** 50-file project, har TU ~1s compile, link ~2s. Ek
   `.cpp` badla. Full rebuild vs incremental time?

   <details><summary>Answer</summary>

   Full: 50×1 + 2 = 52s. Incremental (with dependency tracking): 1 (changed TU) +
   2 (link) = 3s. `-j8` full: ~50/8 + 2 ≈ 8s. Yehi build systems ka poora point.
   </details>

---

## Interview questions

1. Ek `g++ file.cpp` command internally kaunse phases chalata hai?
2. Translation unit ka theek definition — `#include` ka role.
3. Declaration vs definition — TU/linking model mein farq kyun matter karta?
4. `undefined reference` vs `multiple definition` — kaunsa phase, kaunsa fix?
5. Header mein kya rakh sakte ho, kya nahi (aur kyun)?
6. Separate compilation ka fayda aur ek cost (optimization boundary).
7. `#include` `import` se kaise alag hai?

---

## Next
→ [`02-preprocessor-deep.md`](02-preprocessor-deep.md)
