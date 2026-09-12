# 07 — Name mangling, `extern "C"`, ABI

## Prerequisites
- `05-linkage.md`, `06-storage-specifiers.md`
- [`examples/07_binary_inspection.sh`](examples/07_binary_inspection.sh)

## Yeh topic abhi kyun
Linker sirf **naam** (strings) se symbols match karta hai. Par C++ mein `add(int,
int)` aur `add(double, double)` alag functions hain — same name. Compiler inhe alag
**mangled** names deta hai jisme signature encode hota. Yeh samajhna zaroori hai:
`extern "C"` kyun chahiye (FFI, plugin APIs), `undefined reference to
'foo(int)'` kyun aata hai jab library `foo(long)` export karti, aur "ABI break"
ka matlab kya.

---

## Overloading → mangling ki zaroorat

```cpp
int  add(int, int);          // linker naam?  _Z3addii
double add(double, double);  // linker naam?  _Z3adddd
namespace mathx { long add(long, long); }   //  _ZN5mathx3addEll
```

C ke paas overloading nahi → C mein `add` ka symbol bas `add` (ya `_add`). C++
mein compiler function ka **naam + namespace + parameter types** (aur bahut kuch)
ek deterministic string mein encode karta — **name mangling**. Isse:

- Overloads distinct symbols bante.
- Namespaces / classes symbol mein reflect hote.
- Linker type-mismatched calls ko "undefined reference" se pakadta hai (agar
  signature badla to mangled naam badla).

---

## Itanium C++ ABI mangling (GCC / Clang)

```
_Z <name-encoding> <param-types>

_Z3addii              ::add(int, int)
_Z3adddd              ::add(double, double)
_ZN5mathx3addEll      mathx::add(long, long)          (N...E = nested name)
_ZNK4Book9best_bidEv  Book::best_bid() const          (K = const method, v = void params)
_ZN4Book4pushERK5Order  Book::push(Order const&)      (RK = ref-to-const)
_Z3maxIiET_S0_S0_     int max<int>(int, int)          (I...E = template args)
```

Rough decoder:
- `_Z` — "this is a mangled C++ name".
- `3add` — length-prefixed identifier ("add", 3 chars).
- `N ... E` — nested (namespace/class qualified).
- `K` — `const` (member function, or `KRi` = `const int&`).
- `R` — lvalue ref, `O` — rvalue ref, `P` — pointer.
- builtins: `i`=int, `l`=long, `x`=long long, `d`=double, `f`=float, `c`=char,
  `b`=bool, `v`=void, `s`=short, `j`=unsigned int, `m`=unsigned long.
- `I ... E` — template arguments.
- Substitution codes (`S_`, `S0_`, ...) — repeated types abbreviated.

**MSVC** ka mangling bilkul alag (`?add@@YAHHH@Z`) — isliye GCC-compiled aur
MSVC-compiled C++ **link nahi hote** (alag ABI).

### `c++filt` — demangle

```bash
$ echo _ZN5mathx3addEll | c++filt
mathx::add(long, long)

$ nm -C libfoo.a        # -C = demangle inline
$ objdump -d -C prog
```

`nm` bina `-C` → raw mangled. `-C` → readable.

---

## `extern "C"` — mangling band karo

```cpp
extern "C" int c_api_add(int a, int b) { return a + b; }   // symbol: c_api_add  (no _Z...)

extern "C" {
    void plugin_init();
    int  plugin_process(const char* msg, int len);
}
```

- `extern "C"` → **C linkage**: naam mangle nahi hota, symbol literally
  `c_api_add`.
- **Zaroori jab:**
  - C code se C++ function call karna (ya ulta).
  - `dlopen`/`GetProcAddress` se symbol name se function dhoondna (plugin APIs).
  - Stable ABI chahiye across compilers / language boundaries.
- **Cost / limits:** `extern "C"` function **overload nahi ho sakta** (C ke paas
  overloading nahi — ek naam, ek symbol). Namespaces symbol mein reflect nahi
  hote. Par **body** C++ hi hai (templates, RAII, exceptions — sab chalta,
  bas naam C-style).
- **Common pattern:** C++ implementation, `extern "C"` shim layer:
  ```cpp
  // impl (C++)
  Result process_impl(std::string_view);
  // C API
  extern "C" int process(const char* p, int n) {
      auto r = process_impl({p, static_cast<size_t>(n)});
      return r ? 0 : -1;
  }
  ```

⚠️ `extern "C"` function se exception **throw karke C code ke through jaana = UB**.
Boundary pe `try/catch` + error code (folder 23 file 09).

---

## ABI — Application Binary Interface

Mangling ABI ka ek hissa hai. Poora ABI = **binary-level contract** jo do
separately-compiled pieces ko interoperate karne deta hai:

| ABI element | Kya |
|---|---|
| Name mangling | symbol naming (upar) |
| Calling convention | args register/stack mein kaise, return kaise, kaun registers save karta |
| Struct/class layout | member offsets, padding, alignment, vtable layout, base class order |
| `sizeof` / `alignof` of builtin types | `long` = 4 (Windows) vs 8 (Linux); `long double` |
| Exception handling / RTTI format | `.eh_frame`, `type_info` layout |
| `std::string` / `std::list` etc. layout | libstdc++ vs libc++ — different! |

**ABI break** = ek change jisse purane compiled code naye ke saath link/run karne
pe todta hai. Examples:
- `struct` mein field add/reorder (offsets shift).
- Virtual function add/reorder (vtable shift).
- `inline` function ki body badalna jabki purane callers ne purani inline ki hui hai.
- Default argument change (caller-side baked).
- `enum` ka underlying type change.
- Compiler/stdlib ka major version (libstdc++ ne `std::string` COW → SSO switch
  kiya C++11 mein — the famous `_GLIBCXX_USE_CXX11_ABI` dual-ABI saga).

**ABI stability** = library ka naya version drop-in replace ho sakta bina callers
recompile kiye. C libraries yeh achha karti hain; C++ libraries mushkil (templates,
inline, layout) — isliye stable C++ library APIs aksar `extern "C"` + opaque
pointers (pImpl) use karti hain.

---

## Andar kya hota hai

- Compiler har external function/variable ke liye ek mangled symbol string emit
  karta hai object file ki symbol table mein. Call site pe ek relocation entry
  jo us string ko name-karta hai.
- Linker string-match karta hai — `U _Z3addi` (undefined, "add(int)") ko `T
  _Z3addi` (defined) se. `_Z3addl` ("add(long)") **match nahi karega** →
  `undefined reference to 'add(int)'` even though a same-named function exists.
- `extern "C"` → symbol string = bare name, no `_Z`. Ek TU mein do `extern "C"`
  functions same naam se → `multiple definition` (no signature to disambiguate).
- Demanglers (`c++filt`, `abi::__cxa_demangle`, `nm -C`) mangled grammar ko parse
  karke human form dete.

---

## > **HFT relevance**
> - **Plugin / strategy APIs** — `dlopen`-loaded strategy `.so`s ke entry points
>   `extern "C"` hote hain (`strategy_create`, `strategy_on_tick`,
>   `strategy_destroy`), taaki host `dlsym` se stable naam se resolve kare aur
>   compiler-version drift matter na kare. Andar poora C++ (templates, `constexpr`,
>   the works).
> - **Exchange / vendor C libraries** — feed handler, FIX engine SDKs aksar
>   `extern "C"` headers deti hain. Aap unpe C++ wrapper (RAII, `std::span`) chadhate
>   ho.
> - **ABI discipline** — ek hi toolchain + stdlib version poore build + vendored
>   deps ke liye (file 04 se connect). `long` size, `_GLIBCXX_USE_CXX11_ABI`,
>   `-D_GLIBCXX_DEBUG` — inme mismatch = silent corruption.
> - **`nm -C` / `c++filt`** — production binary debugging (folder 45): crash mein
>   `_ZN4Book4pushERK5Order` dikha → `Book::push(Order const&)` — turant pata.
> - **`extern "C"` + opaque pointer** for anything you ship to another team as a
>   binary — insulates them from your layout/inline changes (ABI stability).

---

## Hands-on

```bash
bash 24-COMPILATION-LINKING/examples/07_binary_inspection.sh    # nm / c++filt section

echo '_ZN5mathx3addEll' | c++filt
echo '_ZNK4Book9best_bidEv' | c++filt

cat > /tmp/m.cpp <<'EOF'
int add(int,int){return 0;}
double add(double,double){return 0;}
namespace n { long add(long,long){return 0;} }
extern "C" int c_add(int,int){return 0;}
EOF
g++ -c /tmp/m.cpp -o /tmp/m.o && nm /tmp/m.o        # raw
g++ -c /tmp/m.cpp -o /tmp/m.o && nm -C /tmp/m.o     # demangled
```

Dekho `add(int,int)` → `_Z3addii`, `n::add(long,long)` → `_ZN1n3addEll`,
`c_add` → `c_add` (no mangling).

---

## ⚠️ Traps

### Trap 1 — C header ko C++ se include bina `extern "C"`
```cpp
// legacy.h (C header, no guard for C++)
int legacy_init(void);
```
C++ se include → `legacy_init` **mangled** expected (`_Z11legacy_initv`), par C
library `legacy_init` export karti → `undefined reference`. Fix: `extern "C" {
#include "legacy.h" }` ya header mein `#ifdef __cplusplus extern "C" { #endif`.

### Trap 2 — `extern "C"` function overload
```cpp
extern "C" int f(int);
extern "C" int f(double);     // ⚠️ error: conflicting C linkage / redefinition
```

### Trap 3 — exception through `extern "C"` boundary into C
```cpp
extern "C" void cb() { throw std::runtime_error("x"); }   // ⚠️ UB if C caller unwinds through
```
Catch at the boundary, return an error code.

### Trap 4 — mixing GCC and MSVC C++ objects
Different mangling + ABI → won't link, or links and crashes. Use `extern "C"` +
POD across that boundary, or a single compiler.

### Trap 5 — "same function name exists, why undefined reference?"
```
undefined reference to `parse(std::string_view)'
```
Library actually exports `parse(const std::string&)` or `parse(const char*)` —
different mangled name. Check the exact signature in the library's header /
`nm -C libfoo.so | grep parse`.

### Trap 6 — assuming `nm` output is readable
`nm libfoo.a` → `_ZN...` soup. Always `nm -C` (or pipe through `c++filt`).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "linker function ko signature se match karta" | Sirf mangled **string** se; signature *encoded into* the string |
| "`extern \"C\"` C++ features disable karta" | Sirf **naam** C-style; body poora C++ (RAII, templates, ...) |
| "`extern \"C\"` functions overload ho sakti" | Nahi — ek C name, ek symbol |
| "GCC aur MSVC objects link ho jaayenge" | Alag mangling + ABI — `extern \"C\"` + POD only across |
| "ABI = mangling" | Mangling ek part; + calling convention + layout + EH + stdlib types |
| "struct mein field add karna safe hai" | ABI break — offsets shift, callers must recompile |

---

## Exercises

1. **Demangle:** `_ZN6Engine7processERKN2md6PacketE` — what function?

   <details><summary>Answer</summary>

   `Engine::process(md::Packet const&)`. `N6Engine7process...E` = `Engine::process`,
   `RK` = `const&`, `N2md6PacketE` = `md::Packet`. (`echo ... | c++filt` to check.)
   </details>

2. **Why undefined reference:** header says `long hash(std::string_view);`, you
   call `hash("abc")`, link fails: `undefined reference to hash(char const*)`.
   What happened?

   <details><summary>Answer</summary>

   `"abc"` is `const char[4]` → decays to `const char*`; if there's an overload or
   the compiler picked a `hash(const char*)` (maybe from an older header), it
   mangles to `_Z4hashPKc`, but the library only defines
   `_Z4hashSt17basic_string_view...` (the `string_view` one). Signature mismatch →
   different symbol → undefined. Fix the call (`hash(std::string_view{"abc"})`) or
   the header.
   </details>

3. **`extern "C"` shim:** write a C API `int md_decode(const void* buf, int n,
   Quote* out)` that forwards to a C++ `std::expected<Quote, Err>
   decode(std::span<const std::byte>)`.

   <details><summary>Answer</summary>

   ```cpp
   extern "C" int md_decode(const void* buf, int n, Quote* out) {
       auto r = decode({static_cast<const std::byte*>(buf), static_cast<size_t>(n)});
       if (!r) return -static_cast<int>(r.error());
       *out = *r;
       return 0;
   }
   ```
   No exceptions escape; error is a return code; `Quote` must be a C-compatible
   POD.
   </details>

4. **ABI break or not:** (a) add a non-virtual method to a class, (b) add a data
   member, (c) add a virtual method, (d) change a function's return type from
   `int` to `long`, (e) reorder two `enum` values.

   <details><summary>Answer</summary>

   (a) No (methods aren't in the layout). (b) Yes — layout/`sizeof` change.
   (c) Yes — vtable layout change. (d) Yes — mangled name unchanged for the
   return type in Itanium (return type isn't mangled for free functions!) but the
   calling convention for the return value changes → callers break; also it's a
   source break. (e) Yes if the numeric values shift — any code baking the old
   value breaks.
   </details>

5. **Mixed toolchain:** you must call a function in a `.dll` built by MSVC from
   your MinGW-GCC program. What's the safe interface?

   <details><summary>Answer</summary>

   An `extern "C"` function with only C-compatible types (integers, pointers,
   POD structs with explicit layout) — no C++ classes, no `std::string`, no
   exceptions across the boundary, no throwing. Match the calling convention
   (`__stdcall`/`__cdecl`) the DLL expects. Effectively a C ABI.
   </details>

---

## Interview questions

1. Name mangling kyun zaroori (overloading, namespaces)?
2. `_Z3addii` ko decode karo. `c++filt` kya karta?
3. `extern "C"` — kya band karta, kya nahi, kab chahiye?
4. `extern "C"` function overload kyun nahi ho sakti?
5. GCC aur MSVC C++ objects link kyun nahi hote?
6. ABI kya-kya cover karta hai (mangling ke alawa)?
7. "ABI break" ke 3 examples.
8. Stable C++ library API kaise banaoge (extern "C" + pImpl kyun)?

---

## Next
→ [`08-object-files-elf.md`](08-object-files-elf.md)
