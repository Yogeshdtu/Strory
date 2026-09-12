# 05 — Linkage: internal / external / module, `static`, anonymous namespaces

## Prerequisites
- `04-odr-deep.md`
- [`examples/09_linkage_storage.cpp`](examples/09_linkage_storage.cpp)

## Yeh topic abhi kyun
"Linkage" = ek naam ki **reach**: kya doosri translation unit (ya module) is naam
ko dekh/use kar sakti hai? Yeh decide karta hai ki kaunse symbols `.o` mein export
hote hain, kaunse hidden rehte, aur ODR (file 04) kaise apply hota. `static` aur
anonymous namespace se aap deliberately TU-private banate ho — jo API hygiene aur
optimization dono ke liye achha hai.

---

## Teen linkages

| Linkage | Matlab | Kaun |
|---|---|---|
| **No linkage** | naam sirf apne scope mein | local variables, function parameters, local classes |
| **Internal** | naam sirf **is TU** mein | `static` at namespace scope, anonymous-namespace members, `const`/`constexpr` namespace-scope variables (default), unnamed types |
| **External** | naam **poore program** mein (doosri TUs se reachable) | non-`static` functions, non-`static` non-`const` namespace-scope variables, `inline` variables, class member functions, `extern` |
| **Module linkage** (C++20) | naam apne **module** mein reachable, module ke bahar nahi | non-`export`ed namespace-scope names in a module |

---

## Internal linkage — TU-private

### `static` at namespace scope (purana)
```cpp
// foo.cxx
static int cache_[256];              // sirf foo.cxx dekh sakti
static void warm() { ... }           // sirf foo.cxx
```
(Yahan `static` ka matlab "internal linkage" — function-local `static` aur class
`static` se bilkul alag meaning. C ka legacy.)

### Anonymous namespace (modern)
```cpp
// foo.cxx
namespace {
    int cache_[256];                 // internal linkage
    void warm() { ... }
    struct Helper { ... };           // ye type bhi TU-local
}
```
- Compiler ek unique hidden name generate karta hai per TU.
- **`static` se better** kyunki types ko bhi TU-local bana sakta (`static struct`
  aisa nahi hota), aur templates ke saath consistent.
- ⚠️ **Header mein anonymous namespace mat daalo** — har TU jo include kare use
  apna distinct copy milega (code bloat + ODR traps in inline contexts — file 04).

### `const` / `constexpr` namespace-scope variables
```cpp
// header ya .cxx
const int kMaxRetries = 3;           // INTERNAL linkage by default (C++ quirk)
constexpr double kPi = 3.14159;      // internal (constexpr implies const)
extern const int kShared = 7;        // `extern const` -> EXTERNAL (opt in)
```
Isliye `const int kX = 5;` ek header mein daalna safe hai (har TU apni copy, no
ODR clash) — jabki `int gX = 5;` nahi (external → multiple definition).

---

## External linkage — cross-TU

```cpp
// mathx.cxx
int add(int a, int b) { return a + b; }        // external — mathx.o exports `add`
int g_call_count = 0;                           // external — exports `g_call_count`
```
```cpp
// main.cxx
int add(int, int);                              // declaration — link se resolve
extern int g_call_count;                        // declaration — same variable
```

`nm mathx.o` → `T add`, `D g_call_count`. `nm main.o` → `U add`,
`U g_call_count`. Linker jodta hai.

---

## `extern` — "definition kahin aur hai"

```cpp
extern int g_config_version;         // DECLARATION only — koi storage yahan nahi
int g_config_version = 1;            // DEFINITION — exactly one .cpp mein
```

- Header mein `extern int g_x;` (declaration), ek `.cpp` mein `int g_x = ...;`
  (definition). Yeh **pre-C++17** way tha shared globals ke liye.
- C++17 se: `inline int g_x = 0;` header mein — no separate `.cpp` def needed
  (file 06).

`extern "C"` — alag cheez (name mangling, file 07), linkage se confuse mat karo.

---

## Module linkage (C++20)

```cpp
// geometry.ixx
export module geometry;

export double distance(Point, Point);   // external-ish — importers dekh sakte
double dot(Point, Point);               // MODULE linkage — module ke andar reachable,
                                        //   `import geometry;` karne waalon ko NAHI
```

- `export` kiya → consumers dekh sakte.
- Non-`export` namespace-scope name → **module linkage**: module ki doosri files
  dekh sakti, bahar nahi. (Folder 22 file 10 mein demo.)
- Yeh anonymous-namespace/`static` ka cleaner successor hai for "shared within my
  component, hidden outside".

---

## Linkage aur ODR (file 04 se connect)

- **External** non-inline symbol ki definition **exactly ek** (poore program).
  Do → link error.
- **Internal** symbol — har TU ki apni copy, **koi clash nahi**. Isliye
  `static`/anon-namespace helpers headers-adjacent code mein safe (par header
  khud mein anon-ns → bloat).
- **`inline`** (external but vague linkage) — multiple *identical* defs OK, merged.

---

## Andar kya hota hai

- `nm` symbol classes: `T`/`t` = text (uppercase external, lowercase local), `D`/`d`
  = data, `B`/`b` = bss, `R`/`r` = rodata, `U` = undefined, `W`/`w` = weak,
  `C` = common.
- Internal-linkage symbol → object file mein **local** (`t`/`d`), often no entry in
  the dynamic symbol table, compiler ise freely rename/inline/delete kar sakta.
- **Optimization win:** internal-linkage function jo sirf ek jagah call hota → GCC
  usse inline karke original ko **delete** kar deta (`-Wunused` bhi warn karta agar
  unused). External function ko compiler delete nahi kar sakta (koi doosri TU use
  kar sakti hai) — jab tak LTO na ho.
- `-fvisibility=hidden` — external-linkage symbols ko bhi **dynamic** symbol table
  se hide karta (shared libraries ke liye — export sirf woh jo `__attribute__((
  visibility("default")))` / `__declspec(dllexport)`). Faster loads, smaller
  export tables, better inlining across the `.so` boundary.

---

## > **HFT relevance**
> - **Anonymous namespace har `.cpp` mein** jo TU-local hai (helpers, lookup
>   tables, constants) — compiler ko inline/DCE/constant-fold karne ki poori
>   azadi, aur symbol table clean. "Public surface" chhota = ABI simpler.
> - **`-fvisibility=hidden` + explicit exports** shared libraries pe — load time
>   kam, aur compiler `.so` ke andar ke calls ko devirtualize/inline kar pata hai
>   (external+visible symbols ko interpose kiya ja sakta hai, isliye compiler
>   conservative hota).
> - **`inline` variables / `constexpr` constants** headers mein for shared config
>   — no `extern`+`.cpp` boilerplate, no ODR risk (file 06).
> - **Modules ke module-linkage** se component ki internals genuinely hidden —
>   `static` ki tarah bloat ke bina.
> - **`static`/anon-ns lookup tables** — ek `.cpp` mein `namespace { constexpr
>   std::array<...> kTable = {...}; }` → `.rodata` mein, zero init cost, sirf us
>   TU ko visible.

---

## Hands-on

```bash
./build.ps1 24-COMPILATION-LINKING/examples/09_linkage_storage.cpp   # -pthread
```

Example: anonymous namespace, file-`static`, `const` (internal), `extern const`
(external), `inline` var, `extern` decl+def. Phir:
- `g++ -c -std=c++20 09_linkage_storage.cpp -o ls.o && nm -C ls.o | sort` — dekho
  kaunse symbols `t`/`d` (local) vs `T`/`D` (external).
- Ek anonymous-namespace function ko normal (external) bana ke dekho `nm` mein
  `t` → `T`.

---

## ⚠️ Traps

### Trap 1 — `static` ke teen alag meanings
```cpp
static int x;                 // namespace scope: INTERNAL LINKAGE
void f() { static int y; }    // function scope: STATIC STORAGE (one instance, lifetime)
struct S { static int z; };   // class scope: CLASS-WIDE member (needs a definition)
```
Context = meaning. File 06 storage ke liye.

### Trap 2 — anonymous namespace / `static` in a header
```cpp
// util.hpp
namespace { int counter; }    // ⚠️ har TU ko apna `counter` -> bloat + confusion
```
Header mein: `inline int counter = 0;` (shared) ya `extern` + one def.

### Trap 3 — `const` global "external" maan lena
```cpp
// a.cxx:  const int kSize = 64;
// b.cxx:  extern const int kSize;   // ⚠️ undefined reference! a.cxx ka kSize INTERNAL hai
```
Shared const ke liye: `extern const int kSize = 64;` (definition, external) + `extern
const int kSize;` (declaration). Ya `inline constexpr int kSize = 64;` header mein.

### Trap 4 — external helper jo har TU chhod raha
Ek `void debug_dump()` external jo sirf ek `.cpp` use karta → har build mein symbol,
inliner conservative. `static`/anon-ns karo → freely inlined/removed.

### Trap 5 — `-fvisibility=hidden` ke bina bloated `.so`
Har external symbol dynamic table mein → bada `.so`, slow load, interposition
inline rokta. Explicit-export model use karo.

### Trap 6 — module ki non-`export` cheez ko `import` karke use karne ki koshish
Module linkage — bahar reachable nahi. `export` add karo ya wrapper `export` function.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`static` ka ek matlab" | 3: internal linkage / static storage / class member — scope decides |
| "`const` global external hota" | Namespace-scope `const` = **internal** by default; `extern const` to opt in |
| "anonymous namespace = `static`" | Mostly, par anon-ns types ko bhi TU-local karta; header mein neither |
| "internal linkage se performance farq nahi" | Compiler internal symbols ko freely inline/DCE karta; external nahi (bina LTO) |
| "shared lib sab symbols export kare, theek hai" | `-fvisibility=hidden` + explicit exports — chhota, fast, better opt |
| "module ka har naam importers ko dikhta" | Sirf `export` kiye; baaki module linkage |

---

## Exercises

1. **Linkage of each:** `static void f();` at file scope; `int g;` at file scope;
   `const int h = 3;` at file scope; `inline int k = 0;` at file scope;
   `void m() { static int n; }`.

   <details><summary>Answer</summary>

   `f` — internal. `g` — external. `h` — internal (namespace-scope `const`).
   `k` — external (inline variable). `n` — *no* linkage (it's a local), but static
   storage duration (file 06).
   </details>

2. **Undefined reference:** `a.cxx` has `const int kCap = 128;`. `b.cxx` has
   `extern const int kCap; int buf[kCap];`. Link fails — why, two fixes.

   <details><summary>Answer</summary>

   `kCap` in `a.cxx` has internal linkage, so `b.cxx`'s `extern` declaration finds
   no external definition → `undefined reference to kCap`. Fixes: (1) in `a.cxx`
   write `extern const int kCap = 128;` (external definition). (2) Better: put
   `inline constexpr int kCap = 128;` in a shared header and include it in both.
   </details>

3. **Bloat check:** a header has `namespace { std::array<int,1024> kLut = {...}; }`.
   50 TUs include it. What's wrong, and the fix?

   <details><summary>Answer</summary>

   Each of the 50 TUs gets its own private 4 KB `kLut` (≈200 KB of duplicated
   `.rodata`, and 50 separate initializations if non-`constexpr`). Fix: `inline
   constexpr std::array<int,1024> kLut = {...};` (one shared copy, compile-time),
   or declare `extern const` + define once in a `.cpp`.
   </details>

4. **`nm` reading:** you run `nm -C mod.o` and see `t helper()`, `T api()`,
   `U printf`, `W std::vector<int>::~vector()`. Explain each letter.

   <details><summary>Answer</summary>

   `t` — `helper` is defined, **local** (internal linkage). `T` — `api` is
   defined, **external** (exported). `U` — `printf` is **undefined** (linker must
   supply it). `W` — the vector destructor instantiation is **weak/COMDAT**
   (deduplicated across TUs at link).
   </details>

5. **Visibility:** a `.so` exports 20,000 symbols; load is slow and the compiler
   won't inline internal calls. One flag + one attribute to fix.

   <details><summary>Answer</summary>

   Compile the library with `-fvisibility=hidden` (everything hidden by default),
   then mark the real API with `__attribute__((visibility("default")))` (or a
   `LIB_API` macro). Export table shrinks to the intended API; the loader binds
   fewer symbols; the compiler can inline/devirtualize the now-non-interposable
   internal calls.
   </details>

---

## Interview questions

1. Teen linkages — internal/external/module — ek-ek example.
2. `static` ke teen meanings (namespace / function / class scope).
3. Namespace-scope `const` ka default linkage — aur usse header mein daalna kyun safe.
4. Anonymous namespace vs `static` — kya extra deta?
5. Anonymous namespace header mein kyun bura?
6. Internal linkage optimization ke liye kya deta (inline/DCE)?
7. `-fvisibility=hidden` — kya, kyun (load time + inlining)?
8. C++20 module linkage — `static`/anon-ns se kaise behtar?

---

## Next
→ [`06-storage-specifiers.md`](06-storage-specifiers.md)
