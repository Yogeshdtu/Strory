# 02 — Preprocessor deep

## Prerequisites
- `01-translation-units.md`
- [`examples/08_preprocessor_demo.cpp`](examples/08_preprocessor_demo.cpp)

## Yeh topic abhi kyun
Preprocessor compile se **pehle** chalta hai aur sirf **text** pe kaam karta hai —
usko C++ ke types, scopes, ya expressions ka kuch nahi pata. Yeh power bhi hai aur
sabse ganande bugs ka source bhi. Har C++ engineer ko `#define` traps, conditional
compilation, predefined macros, aur "macro ki jagah kya use karo" pata hona chahiye.

---

## Directives — ek list

| Directive | Kaam |
|---|---|
| `#include` | file ka content yahan paste (file 01, 03) |
| `#define` / `#undef` | macro banao / hatao |
| `#if` / `#elif` / `#else` / `#endif` | conditional compilation (constant expr) |
| `#ifdef X` / `#ifndef X` | `#if defined(X)` / `#if !defined(X)` ka shortcut |
| `#error "msg"` / `#warning "msg"` | compile roko / warn (`#warning` C++23 mein standard) |
| `#pragma` | compiler-specific hint (`once`, `pack`, diagnostics) |
| `#line` | `__LINE__`/`__FILE__` override (generated code) |
| `_Pragma("...")` | `#pragma` ka operator form — macro ke andar use ho sakta |

---

## Object-like vs function-like macros

```cpp
#define TICK_SIZE 5                        // object-like: har "TICK_SIZE" -> "5"
#define SQ(x) ((x) * (x))                  // function-like: SQ(a+1) -> ((a+1) * (a+1))
```

**Blind text substitution.** `SQ` ko `x` ka type nahi pata, `x` evaluate nahi
karta — bas token stream mein paste karta hai.

### Trap 1 — missing parentheses
```cpp
#define SQ_BAD(x) x * x
SQ_BAD(1 + 2)        // -> 1 + 2 * 1 + 2  ->  1 + 2 + 2  =  5     (chahiye 9)
10 / SQ_BAD(2)       // -> 10 / 2 * 2     ->  (10/2)*2   =  10    (chahiye 2.5)
```
Fix: **har parameter aur poora body `()` mein**: `#define SQ(x) ((x) * (x))`.

### Trap 2 — argument ka double evaluation
```cpp
#define MAX(a, b) ((a) < (b) ? (b) : (a))
int m = MAX(i++, j++);      // i++ ya j++ me se ek DO baar evaluate -> extra ++
MAX(expensive(), other())   // expensive() do baar call ho sakta
```
Fix: `constexpr` function — `template <class T> constexpr T maxv(T a, T b) { return
a < b ? b : a; }`. Ek baar eval, type-safe, debuggable.

---

## `#` (stringize) aur `##` (token paste)

```cpp
#define STR(x)  #x               // x ko "x" string literal banao
#define XSTR(x) STR(x)           // pehle x expand, PHIR stringize
#define CAT(a, b) a##b           // do tokens jodo -> ek token

STR(hello)        // "hello"
STR(TICK_SIZE)    // "TICK_SIZE"     (no expansion — # apne argument ko expand nahi karta)
XSTR(TICK_SIZE)   // "5"             (extra layer se pehle expand hota hai)
CAT(order_, id)   // order_id        (ek identifier)
```

Common use: `WHERE()` = `__FILE__ ":" XSTR(__LINE__)` → `"foo.cpp:42"` (compile-time
string concat — adjacent string literals join ho jaate hain).

---

## Variadic macros aur `__VA_OPT__` (C++20)

```cpp
#define LOGF(fmt, ...) std::printf("[core] " fmt "\n", __VA_ARGS__)
LOGF("x=%d", 5);          // ok
LOGF("no args");           // ⚠️ std::printf("[core] no args\n", )  <- trailing comma!
```

C++20 `__VA_OPT__(x)` — `x` sirf tab include karo jab variadic args non-empty hon:

```cpp
#define LOGF(fmt, ...) std::printf("[core] " fmt "\n" __VA_OPT__(,) __VA_ARGS__)
LOGF("no args");           // ok: printf("[core] no args\n")
LOGF("x=%d", 5);           // ok: printf("[core] x=%d\n", 5)
```

(Pre-C++20 GNU hack tha `, ##__VA_ARGS__` — ab `__VA_OPT__` standard.)

---

## X-macros — ek list, kai jagah

Enum aur uske string names ko **sync mein** rakhne ka classic trick:

```cpp
#define ORDER_TYPES(X)  X(Market) X(Limit) X(Stop) X(StopLimit)

enum class OrderType {
#define X(name) name,
    ORDER_TYPES(X)
#undef X
};

const char* name_of(OrderType t) {
    switch (t) {
#define X(name) case OrderType::name: return #name;
        ORDER_TYPES(X)
#undef X
    }
    return "?";
}
```

Ek jagah list badlo → enum aur names dono update. (Modern alternative:
`magic_enum` / reflection (C++26) — par X-macros zero-dependency hai.)

---

## Conditional compilation

```cpp
#if defined(__linux__)
    // linux-only code
#elif defined(_WIN32)
    // windows-only
#endif

#if __cplusplus >= 202002L
    // C++20+ path
#endif

#ifdef NDEBUG
    // release-only
#endif

#if defined(__has_include)
#  if __has_include(<expected>)
#    define HAS_EXPECTED 1
#  endif
#endif
```

- `#if` ka expression **integer constant expression** (macros expand ke baad).
  `defined(X)`, `__has_include(...)`, arithmetic, comparisons. **Koi `sizeof`,
  koi types, koi function calls.**
- Undefined identifier `#if` mein → `0` (isliye `#if FOO` jab `FOO` undefined ho,
  bas false).

---

## Predefined macros (jaanne layak)

| Macro | Kya |
|---|---|
| `__FILE__` / `__LINE__` | current file/line (logging, asserts) |
| `__func__` | current function name (technically ek `const char[]`, macro nahi) |
| `__DATE__` / `__TIME__` | build ki date/time |
| `__cplusplus` | `202002L` (C++20), `201703L` (C++17), ... |
| `__STDC_HOSTED__` | 1 = hosted (OS hai), 0 = freestanding |
| `__GNUC__`, `__GNUC_MINOR__`, `__GNUC_PATCHLEVEL__` | GCC version |
| `__clang__`, `_MSC_VER` | compiler detection |
| `_WIN32`, `__linux__`, `__APPLE__` | OS |
| `__x86_64__`, `__aarch64__` | architecture |
| `NDEBUG` | *aap* / build system define karta — `assert` ko disable |
| `__OPTIMIZE__` | GCC/Clang: `-O1`+ pe define |
| `__SANITIZE_ADDRESS__` | ASan build |

`__cpp_lib_*` / `__cpp_*` feature-test macros (`<version>`) — library/language
feature detection ka portable tarika (folder 22).

---

## `#pragma`

```cpp
#pragma once                          // include guard (file 03)

#pragma GCC diagnostic push           // warning state save
#pragma GCC diagnostic ignored "-Wunused-variable"
    int scratch;
#pragma GCC diagnostic pop            // restore

#pragma pack(push, 1)                 // struct padding hatao (wire formats — CAREFUL)
struct WireHeader { std::uint8_t type; std::uint32_t len; };
#pragma pack(pop)

_Pragma("GCC diagnostic ignored \"-Wshadow\"")   // operator form — macro-friendly
```

`#pragma` compiler-specific — unknown pragma silently ignore hota (ya
`-Wunknown-pragmas`).

---

## Macro ki jagah kya (modern C++)

| Macro | Behtar |
|---|---|
| `#define PI 3.14159` | `constexpr double kPi = 3.14159;` (typed, scoped, debuggable) |
| `#define SQ(x) ((x)*(x))` | `constexpr auto sq(auto x) { return x*x; }` |
| `#define MAX(a,b) ...` | `std::max(a, b)` |
| `#define DEBUG_LOG(...) ` | `if constexpr (kDebug) log(...)` / a real logger |
| `#define BEGIN_NS namespace x {` | just write `namespace x {` |
| header guards | `#pragma once` (file 03) — ya modules |

**Macros abhi bhi kahan zaroori:** conditional compilation (`#if`), `__FILE__`/
`__LINE__` capture, `#include`, `static_assert` messages banana, X-macros,
platform shims. Baaki sab jagah `constexpr` / `template` / `inline` jeette hain
(type-safe, scoped, tooling-friendly, koi double-eval).

---

## > **HFT relevance**
> - **`#pragma pack` / manual layout** wire structs ke liye common — par har field
>   pe `static_assert(offsetof(...) == N)` + `static_assert(sizeof(T) == N)`
>   (folder 23 file 12), warna ek `#ifdef` ODR drift (file 04).
> - **Build-flavor macros** (`#ifdef ENGINE_FAST_PATH`, `#ifdef PROD`) se hot
>   path pe branches compile-out — par yeh macro har us type ke layout/behaviour
>   ko change kar sakta jo woh define dekhta hai → poore project mein consistent
>   flags (file 04).
> - **Macros != free.** `-Wpedantic` clean, `constexpr`/`if constexpr` prefer —
>   debugger inhe dekh sakta, macro ko nahi. HFT debugging (folder 45) mein yeh
>   matter karta hai.
> - **`#if __has_include` / feature-test macros** — teams jo bleeding-edge
>   toolchains use karti hain (`std::expected`, `<simd>`) inse graceful fallback.

---

## Hands-on

```bash
./build.ps1 24-COMPILATION-LINKING/examples/08_preprocessor_demo.cpp
g++ -std=c++20 -E 24-COMPILATION-LINKING/examples/08_preprocessor_demo.cpp | tail -60
g++ -std=c++20 -dM -E - < /dev/null | sort | less    # saare predefined macros
```

Example: macros, `#`/`##`, `__VA_OPT__`, X-macro, `__has_include`, `_Pragma`, aur
teen traps (measured output). `-E` se dekho preprocessor ne kya kiya.

---

## ⚠️ Traps

### Trap 1 — missing parens
`#define SQ(x) x*x` → precedence bugs. `((x)*(x))`.

### Trap 2 — double evaluation
`#define MAX(a,b) ((a)<(b)?(b):(a))` + `MAX(f(), g())` → ek do baar. `constexpr` fn.

### Trap 3 — macro semicolon / dangling else
```cpp
#define LOG(x) if (verbose) print(x)
if (cond) LOG(y); else other();     // ⚠️ `else` galat `if` se bind
#define LOG(x) do { if (verbose) print(x); } while (0)   // ✅
```

### Trap 4 — `#define min`/`max` (Windows.h) STL ko todta hai
```cpp
#include <windows.h>      // defines min/max macros
std::min(a, b);            // ⚠️ ((a)<(b)?(a):(b)) ban jaata -> STL min ke saath clash
// fix: #define NOMINMAX  (windows.h se pehle)
```

### Trap 5 — macro name lowercase / common word
`#define index 0`, `#define ready true` — har `index`/`ready` replace, doosre
headers mein bhi. Macros ALL_CAPS + prefix (`MYLIB_`).

### Trap 6 — `#if` mein undefined macro
```cpp
#if CONFIG_LEVEL > 2      // CONFIG_LEVEL undefined -> `0 > 2` -> false (no error!)
```
`#ifdef CONFIG_LEVEL` se pehle check karo, ya `-Wundef`.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "macro function jaisa behave karta" | Text paste — no eval, no types, no scope; double-eval + precedence bugs |
| "`#define PI 3.14` fine hai" | `constexpr double kPi` — typed, scoped, debugger dekh sakta |
| "`#if` mein `sizeof` use kar sakte" | Nahi — `#if` sirf integer constant expr (macros + `defined` + `__has_include`) |
| "unknown `#pragma` error deta" | Silently ignore (ya `-Wunknown-pragmas` warn) |
| "`__func__` ek macro hai" | Nahi — ek implicit `static const char[]` local |
| "variadic macro trailing comma theek" | `LOGF("x")` → `printf("x\n", )` — `__VA_OPT__(,)` chahiye |

---

## Exercises

1. **Expand it:** `#define ADD(a,b) a+b` — `ADD(1,2)*3` ka result?

   <details><summary>Answer</summary>

   `1+2*3` = `1 + 6` = **7** (chahiye tha `(1+2)*3 = 9`). `#define ADD(a,b)
   ((a)+(b))`.
   </details>

2. **Stringize:** `#define S(x) #x` and `#define XS(x) S(x)`. What are `S(__LINE__)`
   and `XS(__LINE__)` on line 42?

   <details><summary>Answer</summary>

   `S(__LINE__)` → `"__LINE__"` (# doesn't expand its arg). `XS(__LINE__)` →
   `S(42)` → `"42"` (the extra macro layer expands `__LINE__` first).
   </details>

3. **Double eval:** `#define SQUARE(x) ((x)*(x))`, `int i = 3; int r = SQUARE(i++);`
   — `r` and `i` after?

   <details><summary>Answer</summary>

   `((i++)*(i++))` — `i` incremented **twice**; the product is unspecified (order
   of the two `i++` isn't sequenced pre-C++17; even after, it's `3*4` or `4*3`
   depending — actually both read before either increment commits → likely `3*3`
   with `i` ending at 5, but it's a code smell / UB-adjacent). `r` = 9-ish, `i` =
   5. Use `constexpr` function.
   </details>

4. **Conditional:** write a block that uses `std::expected` if `<expected>` is
   available, else a typedef to a fallback `Expected`.

   <details><summary>Answer</summary>

   ```cpp
   #if defined(__has_include) && __has_include(<expected>) && defined(__cpp_lib_expected)
   #  include <expected>
      template <class T, class E> using Expected = std::expected<T, E>;
   #else
   #  include "fallback_expected.hpp"
      template <class T, class E> using Expected = fallback::Expected<T, E>;
   #endif
   ```
   (Same pattern as `23-ERROR-HANDLING/examples/05_expected.cpp`.)
   </details>

5. **X-macro:** ek `enum class Side { Buy, Sell }` + `to_string` + `from_string`
   ek X-macro se — sketch.

   <details><summary>Answer</summary>

   ```cpp
   #define SIDES(X) X(Buy) X(Sell)
   enum class Side {
   #define X(n) n,
       SIDES(X)
   #undef X
   };
   inline const char* to_string(Side s) { switch (s) {
   #define X(n) case Side::n: return #n;
       SIDES(X)
   #undef X
   } return "?"; }
   ```
   `from_string` similar: compare `s == #n` → `return Side::n`.
   </details>

---

## Interview questions

1. Preprocessor kab chalta hai, kis pe kaam karta (text vs AST)?
2. Function-like macro ke do classic bugs (parens, double-eval) + fix.
3. `#` vs `##` — kya karte, `#` apne argument ko expand kyun nahi karta?
4. `__VA_OPT__` kis problem ko solve karta?
5. `#if` ke expression mein kya allowed hai, kya nahi?
6. `#pragma once` vs include guards (file 03) — trade-off?
7. Aaj macro ki jagah kya use karo — 3 examples.
8. X-macro pattern kis liye — ek real use case.

---

## Next
→ [`03-include-guards-and-pragma.md`](03-include-guards-and-pragma.md)
