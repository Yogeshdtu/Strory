# 10 — Har common linker error aur uska fix

## Prerequisites
- `07-name-mangling.md`, `08-object-files-elf.md`, `09-static-vs-dynamic-linking.md`

## Yeh topic abhi kyun
Linker errors beginners ko sabse zyada frustrate karte hain kyunki message cryptic
lagta hai (`_ZN...`) aur line number nahi milta. Par inke patterns limited hain.
Ek baar har pattern ka **shape + cause + fix** pata ho jaaye, to yeh 30-second
problems ban jaate. Yeh file woh catalog hai.

---

## Golden rule

**Linker sirf symbols (strings) match karta hai.** Har error in me se ek hai:
1. Ek symbol **chahiye** (`U`) par kisi ne **define** nahi kiya → *undefined reference*.
2. Ek symbol **do baar** define hua → *multiple definition*.
3. Symbol mila par **mangled naam match nahi kiya** (galat signature / linkage /
   `extern "C"`) → *undefined reference* (dhoka: "par function to hai!").
4. Library / object **link line pe nahi** ya **galat order** mein.

---

## 1. `undefined reference to 'foo()'`

```
main.o: in function `main':
main.cpp:(.text+0x1a): undefined reference to `foo()'
```

**Cause + fix:**

| Cause | Fix |
|---|---|
| `foo.cpp` (jisme `foo` ki body hai) link line pe nahi | `g++ main.o foo.o -o app` |
| Library `.a`/`.so` link line pe nahi | `-lfoo` add karo (+ `-L<dir>`) |
| Library **galat order** (GNU ld) | library ko users ke **baad**: `g++ main.o -lfoo` (not `-lfoo main.o`) |
| Sirf **declaration** likha, definition kabhi nahi | body likho, ya `= default`/`= delete` |
| Signature mismatch → alag mangled naam | header aur definition ka signature exactly match karo (`const`, `&`, param types) |
| `extern "C"` ek jagah, doosri nahi | dono jagah consistent |
| Template method use kiya par definition `.cpp` mein chhipa | template definitions header mein (ya explicit instantiation) |
| `static`/anonymous-namespace function ko doosri TU se call | usse external banao (ya header-inline) |
| `virtual` function declared par not defined | har non-pure virtual ki body do (even `~Base() = default;` in the `.cpp`) — "undefined reference to vtable for X" |
| `constexpr`/`static` data member odr-used, pre-C++17 no out-of-class def | `inline static` (C++17) ya out-of-class definition |

### "undefined reference to `vtable for X`"
`X` ki koi **non-inline non-pure virtual** function (aksar first-declared, ya
destructor) define nahi hui. Rule: har polymorphic class ki **ek** virtual (usually
`~X`) ki out-of-line definition ek `.cpp` mein ho ("key function").

### "undefined reference to `X::X()` / `X::~X()`"
Constructor/destructor declared but not defined (ya `= default` in header lekin
member type incomplete). Define karo / `= default` at a point where members are
complete.

### "undefined reference to `__imp_...`" (Windows)
DLL import symbol missing — import library (`-lfoo` where `libfoo.dll.a` exists)
link line pe nahi, ya `__declspec(dllimport)` mismatch.

---

## 2. `multiple definition of 'foo()'`

```
b.o:(.text+0x0): multiple definition of `foo()';
a.o:(.text+0x0): first defined here
```

**Cause + fix:**

| Cause | Fix |
|---|---|
| Non-inline function/variable ki **definition ek header mein** (2+ TUs include) | `inline` lagao, ya declaration header + body one `.cpp` (file 04) |
| Non-`const` global variable header mein defined | `inline` variable (C++17), ya `extern` decl + one def |
| Same `.cpp` galti se do baar link line pe | ek hi baar |
| Ek `.cpp` ka content do jagah (`#include "foo.cpp"` — kabhi mat karo) | include headers, not sources |
| ODR: `inline` function ki **alag body** do headers mein | ek canonical definition (file 04) |
| `-fcommon` (old GCC default) + tentative definitions (`int x;` in header in C) | `-fno-common` (GCC 10+ default), ya `extern` |

---

## 3. `undefined reference` **but the function clearly exists**

Yeh confusing wala — 99% cases **signature / linkage mismatch** → alag mangled
naam.

```
undefined reference to `parse(std::string_view)'
```
...jabki library `parse` "export karti hai".

**Debug:**
```bash
nm -C libfoo.so | grep parse
#  T parse(std::basic_string_view<char, ...>)      <- library ka actual signature
#  T parse(char const*)                             <- ya yeh
```
Aapka call site alag overload resolve kar raha (`"abc"` → `const char*`), ya header
ka declaration library se drift kar gaya. **Fix:** call / declaration ko library ke
actual signature se match karo.

Aur bhi causes: `const` on a method (`foo() const` vs `foo()`), reference vs value
param, `extern "C"` on one side only, different namespace, 32-bit vs 64-bit object,
ABI flag mismatch (`_GLIBCXX_USE_CXX11_ABI=0` vs `1` → `std::string` ka mangled
naam alag!).

### The `_GLIBCXX_USE_CXX11_ABI` one (classic)
```
undefined reference to `foo(std::__cxx11::basic_string<...>)'
```
Ek object `-D_GLIBCXX_USE_CXX11_ABI=1` (default, `std::__cxx11::string`), doosra
`=0` (old COW `std::string`). Alag mangled names. **Fix:** poora build + deps ek
hi ABI setting se.

---

## 4. Library / order / path errors

| Message | Cause | Fix |
|---|---|---|
| `cannot find -lfoo` | library file na mili | `-L<dir>` do; file `libfoo.a`/`libfoo.so` naam mein ho |
| `undefined reference` sirf jab `-lfoo` line pe **hai** | library user `.o` ke **pehle** likhi | `g++ main.o -lfoo` (user pehle, lib baad) |
| Circular dep between two `.a` | `--start-group -la -lb --end-group`, ya repeat `-la -lb -la` |
| `relocation ... against ... can not be used when making a shared object; recompile with -fPIC` | `.o` non-PIC, `.so` mein daal rahe | source ko `-fPIC` se recompile |
| `error while loading shared libraries: libX.so: cannot open` (runtime) | RUNPATH/`LD_LIBRARY_PATH` mein nahi | `-Wl,-rpath,'$ORIGIN'`, install, ya `LD_LIBRARY_PATH` |
| `DSO missing from command line` | tum ek `.so` ke symbol use karte ho jo transitively aaya | usse **explicitly** `-l` karo |
| `version node not found for symbol foo@@VERS` | `.so` ka version aapke expect se purana | correct library version |

---

## 5. `main`-related

| Message | Cause | Fix |
|---|---|---|
| `undefined reference to 'main'` / `WinMain` | koi `int main()` nahi (ya galat signature, ya library-only build) | `int main()` do, ya `-shared`/`-c` intent tha |
| `multiple definition of 'main'` | do `.cpp` mein `main` | ek hi entry point |

---

## Systematic debug flow

1. **Undefined ya multiple?** Message padho.
2. **Undefined:**
   - `nm -C <the .o that has the U> | grep <symbol>` → confirm `U`.
   - `nm -C <every .o and lib>` / `nm -C -D libfoo.so | grep <symbol>` → kaun define
     karta hai? Koi nahi → definition missing / not on link line. Milta hai par
     **thoda alag** naam → signature/ABI mismatch.
   - Link line check: saari zaroori `.o` + `-l` present? Order sahi (users pehle)?
3. **Multiple:**
   - `nm -C *.o | grep ' T <symbol>'` → kaun-kaun define karta? 2+ → ek ko `inline`
     / move to `.cpp` / `static`.
4. **`c++filt`** har mangled naam pe — signature clearly dekho.
5. **`-Wl,--verbose`** / `-Wl,-t` — linker kaunsi files/libs actually use kar raha.

---

## > **HFT relevance**
> - **ABI-flag discipline** (file 04, 07) — `_GLIBCXX_USE_CXX11_ABI`,
>   `_GLIBCXX_DEBUG`, `-std`, `long` size — mismatch = `undefined reference to
>   foo(std::__cxx11::...)` at best, silent corruption at worst. One toolchain,
>   one flag set, rebuild vendored deps.
> - **Static-link + `--gc-sections`** (file 09, 08) — fewer link-line surprises, no
>   runtime `.so`-not-found, and dead-symbol errors surface at build not deploy.
> - **"undefined reference to vtable"** shows up when refactoring polymorphic
>   types — keep a key function (out-of-line `~Base`) defined in the `.cpp`.
> - **`nm -C` reflex** — every "but the function exists!" linker error is solved
>   by grepping the actual exported symbol and comparing signatures.

---

## Hands-on

```bash
cd 24-COMPILATION-LINKING/examples/01_multi_file_project
g++ -std=c++20 -c *.cxx
g++ stats.o main.o -o app          # drop mathx.o -> undefined reference to mathx::is_prime
g++ mathx.o stats.o main.o -o app  # fixed

cd ../02_odr_violation && ./build.sh   # DEMO 1: multiple definition of venue_name()
```

Deliberately cause each: remove an `.o`, misspell a declaration's parameter type,
put a non-inline function body in a header, swap library order.

---

## ⚠️ Traps

### Trap 1 — "function is right there, why undefined"
Signature/linkage/ABI mismatch → different mangled name. `nm -C` the library, match
exactly.

### Trap 2 — library before its users
`g++ -lfoo main.o` → GNU ld discards unused `libfoo` symbols before seeing
`main.o` needs them. `g++ main.o -lfoo`.

### Trap 3 — `#include "impl.cpp"`
Never include a source. Two TUs → multiple definition. Include headers.

### Trap 4 — forgetting the out-of-line virtual
`class Base { virtual void f(); virtual ~Base(); };` with no definitions →
`undefined reference to vtable for Base`. Define at least the key function.

### Trap 5 — `constexpr` static member odr-used (pre-C++17)
`struct S { static constexpr int N = 8; }; ... f(S::N);` where `f` takes `const
int&` → odr-use → needs `constexpr int S::N;` out of class (or `inline` in C++17).

### Trap 6 — mixing `-m32` and `-m64` objects
`i386 architecture of input file ... is incompatible`. Same `-m` everywhere.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "linker error = compile error" | Different phase — no line number; think symbols |
| "function exists → no undefined reference" | Must match the exact mangled name (signature + linkage + ABI) |
| "library order doesn't matter" | GNU ld: libraries after the objects that use them |
| "multiple definition = I linked a file twice" | Usually a non-inline definition in a header |
| "`_GLIBCXX_USE_CXX11_ABI` is obscure" | Mismatch is a very common `std::string`-signature undefined reference |
| "undefined reference to vtable is a weird bug" | Just define the class's key (out-of-line) virtual function |

---

## Exercises

1. **Diagnose:** `undefined reference to 'Logger::log(std::string const&)'` but
   your `logger.cpp` defines `Logger::log(std::string_view)`. What's wrong?

   <details><summary>Answer</summary>

   The header declares `log(const std::string&)` (or a caller passes something
   that picks that overload), but only `log(std::string_view)` is defined →
   different mangled names → undefined. Unify the signature: change the header/def
   to match, or add the missing overload.
   </details>

2. **Fix the link line:** `g++ -lcrypto main.o util.o -o app` fails with undefined
   references to `crypto_*`. `libcrypto` is definitely installed.

   <details><summary>Answer</summary>

   Order: `-lcrypto` is before the objects that use it, so GNU ld pulls nothing
   from it. `g++ main.o util.o -lcrypto -o app`.
   </details>

3. **Header bug:** three TUs include `util.hpp` which contains
   `std::string upper(std::string s) { ... }`. Link error?

   <details><summary>Answer</summary>

   `multiple definition of upper(std::string)` — non-inline function defined in a
   header. Fix: `inline std::string upper(...)`, or declaration in the header +
   body in `util.cpp`.
   </details>

4. **Vtable:** minimal repro of "undefined reference to vtable for Shape" and its
   fix.

   <details><summary>Answer</summary>

   ```cpp
   // shape.hpp
   struct Shape { virtual double area() const; virtual ~Shape(); };
   // no shape.cpp defining them  -> "undefined reference to vtable for Shape"
   ```
   Fix: define at least one non-inline virtual (the "key function"), e.g. in
   `shape.cpp`: `Shape::~Shape() = default;` and `double Shape::area() const {
   return 0; }` (or make the class abstract with `= 0`).
   </details>

5. **ABI mismatch:** you get `undefined reference to
   foo(std::__cxx11::basic_string<...>)`. What single build inconsistency causes
   this?

   <details><summary>Answer</summary>

   One translation unit / library was built with `-D_GLIBCXX_USE_CXX11_ABI=0` (old
   COW `std::string`) and the other with `=1` (default) → the `std::string`
   parameter mangles differently → the symbols don't match. Rebuild everything
   with the same ABI setting.
   </details>

---

## Interview questions

1. `undefined reference` vs `multiple definition` — kaunsa cause-family, kaunsa fix-family?
2. "Function clearly exists, still undefined reference" — top 3 causes.
3. GNU ld library order rule — kyun.
4. "undefined reference to vtable for X" — kya missing hai?
5. `_GLIBCXX_USE_CXX11_ABI` mismatch ka symptom.
6. Non-inline function ki definition header mein — kaunsa error, do fixes.
7. Systematic debug: undefined reference aaya, `nm` se kaise track karo?
8. `recompile with -fPIC` — kab, kyun.

---

## Next
→ [`11-make.md`](11-make.md)
