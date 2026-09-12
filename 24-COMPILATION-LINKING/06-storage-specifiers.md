# 06 — Storage: `static`, `extern`, `inline` variables, `thread_local`

## Prerequisites
- `05-linkage.md`
- [`examples/09_linkage_storage.cpp`](examples/09_linkage_storage.cpp)

## Yeh topic abhi kyun
Har variable ke do orthogonal properties hain: **linkage** (kaun dekh sakta —
file 05) aur **storage duration** (kab banta, kab marta). `static`, `thread_local`,
`extern`, `inline` (variables) — yeh keywords dono ko touch karte hain, aur inka
matlab **context** pe depend karta hai. Isse clear karna zaroori hai varna
static-init-order bugs, thread_local surprises, aur `inline` variable ki galatfehmi.

---

## Storage duration — chaar

| Duration | Kab banta / marta | Kaun |
|---|---|---|
| **Automatic** | scope enter / exit | local variables (default) |
| **Static** | program start (ya first-use) / program end | namespace-scope vars, `static` locals, class `static` members |
| **Thread** | thread start (ya first-use) / thread end | `thread_local` vars |
| **Dynamic** | `new` / `delete` (aap decide) | heap objects |

---

## `static` — context-dependent

### 1. Namespace scope → **internal linkage** (storage: static duration anyway)
```cpp
static int g_cache[256];        // file 05: TU-private. Storage: static duration
```

### 2. Function scope → **static storage duration**, first-use init
```cpp
int next_id() {
    static int counter = 0;     // ek hi `counter`, saare calls ke beech
    return ++counter;           // pehli call pe init; thread-safe init guard
}
```
- Pehli baar function chalne pe initialize (lazy). Uske baad wahi object.
- **C++11+: thread-safe initialization** — do threads ek saath pehli call karein to
  ek init karta, doosra wait. Compiler ek atomic guard variable daalta hai
  (`__cxa_guard_acquire`).
- Destruction: program exit pe (reverse order of construction), via `atexit`.

### 3. Class scope → class-wide member (ek instance, sab objects ke beech)
```cpp
struct Counter {
    static int total;           // DECLARATION (no storage yet)
    static constexpr int kMax = 1000;   // in-class init OK for constexpr/const-integral
};
int Counter::total = 0;         // DEFINITION — exactly one .cpp (pre-C++17)
// C++17: `inline static int total = 0;` in-class -> no .cpp definition needed
```

---

## `thread_local`

```cpp
thread_local int t_errno = 0;           // har thread ka apna
thread_local std::vector<char> t_scratch;   // per-thread scratch buffer

void worker() {
    ++t_scratch.size();     // sirf IS thread ka t_scratch
}
```

- Har thread apna instance paata hai. Thread start pe construct (ya first use),
  thread exit pe destruct.
- `errno` bilkul yahi hai. Per-thread caches, RNG state, scratch buffers, "current
  transaction" pointers — classic uses.
- **Cost:** access ek TLS lookup hai — platform pe `%fs`/`%gs`-relative load, ya
  ek `__tls_get_addr` call (dynamic TLS models). Local variable jitna free nahi.
  Hot inner loops mein `thread_local` ko ek local mein cache karo.
- `thread_local` + `static` (namespace scope) → `static thread_local` = internal
  linkage + per-thread. Function-local `thread_local` bhi allowed (implicitly
  static-ish).

---

## `extern` — declaration of "defined elsewhere"

```cpp
// config.hpp
extern int g_config_version;       // DECLARATION — no storage

// config.cxx
int g_config_version = 1;          // DEFINITION — exactly one
```

- Pre-C++17 way for a header-declared shared global.
- `extern` on a `const` → opt into external linkage (file 05).
- `extern template` — different feature (folder 21): "is instantiation ko yahan mat
  banao, kahin aur banega".

---

## `inline` variables (C++17) — the modern shared global

```cpp
// metrics.hpp
inline int g_orders_sent = 0;               // ONE shared variable, defined in the header
inline constexpr double kTickValue = 0.05;  // constexpr is implicitly inline anyway

struct Registry {
    inline static Registry* instance = nullptr;   // in-class, no .cpp definition
};
```

- **Problem it solves:** pre-C++17, a header-defined non-const variable → ODR
  violation (every TU defines it). You needed `extern` in the header + one
  definition in a `.cpp`. Annoying boilerplate, easy to forget.
- `inline` variable → "multiple identical definitions across TUs are fine, linker
  merges to one" (same vague/COMDAT linkage as `inline` functions).
- **Header-only libraries** ka big enabler — globals, singletons, config all in the
  `.hpp`.
- `constexpr` variables at namespace scope are **implicitly `inline`** since C++17.
- `static` data members declared `constexpr` are implicitly `inline` (no
  out-of-class definition needed).

---

## `constinit` (C++20) — "guaranteed compile-time init"

```cpp
constinit int g_mode = compute_mode();   // compile error if compute_mode() isn't constexpr
```

- Asserts the variable has **constant initialization** — no runtime init, no
  init-order-fiasco, no thread-safe-init guard for it.
- **Not `const`** — the value can change later. It's about *when it's
  initialized*, not mutability.
- Fixes the static-init-order fiasco for globals that *can* be const-initialized.
- Pairs with `thread_local`: `constinit thread_local T x = ...;` avoids the
  per-access "is it initialized yet?" guard.

---

## Static initialization order fiasco

```cpp
// a.cxx
Config g_config = load_config();       // needs g_logger ready

// b.cxx
Logger g_logger;                        // ⚠️ init order across TUs is UNSPECIFIED
```

- Within one TU: top-to-bottom.
- **Across TUs: unspecified.** `g_config` might init before `g_logger` → using an
  uninitialized `g_logger` → UB.
- Fixes:
  1. **Construct-on-first-use** — `Logger& logger() { static Logger l; return l; }`
     (static local → initialized on first call, order guaranteed by use).
  2. **`constinit`** — if the global can be const-initialized, no ordering issue.
  3. Avoid non-trivial globals; pass dependencies explicitly.

Destruction order fiasco is the mirror image (globals destroyed in reverse of
construction, across TUs unspecified) — the first-use-static trick helps here too
(and `[[clang::no_destroy]]` / never-destroy singletons in some codebases).

---

## Andar kya hota hai

- **Const-initialized** static/thread vars → live in `.data` (or `.rodata` if
  `const`) with the value baked in; **zero-initialized** → `.bss` (no file bytes).
- **Dynamically-initialized** globals → compiler emits an init function per TU,
  registered so it runs before `main` (`.init_array` / `__attribute__((constructor))`).
  Order within TU = source order; across TUs = link order = unspecified.
- **`static` local** → a hidden guard byte + the storage. First call:
  `__cxa_guard_acquire` (atomic), run the constructor, `__cxa_guard_release`,
  register the destructor with `__cxa_atexit`. Subsequent calls: one cheap
  guard-byte check (often a single load + predicted branch).
- **`thread_local`** → TLS block per thread; access via architecture TLS ABI
  (`%fs:offset` on x86-64 Linux for the "initial-exec" model, or `__tls_get_addr`
  for "general-dynamic" in `.so`s). `constinit thread_local` skips the per-access
  init check.

---

## > **HFT relevance**
> - **`thread_local` for per-thread scratch / RNG / arena pointers** — but **cache
>   the address in a local** inside hot loops (`auto& s = t_scratch;`); repeated
>   `thread_local` access in a tight loop is a measurable TLS-lookup tax.
> - **`constinit` on config/const globals** — kills the init-order fiasco and the
>   per-access guard; deterministic startup.
> - **`inline constexpr` for all compile-time constants** in headers (tick sizes,
>   capacities, table data) — one copy, in `.rodata`, zero init cost, no ODR risk.
> - **Avoid non-trivial global constructors** — they run before `main`, in
>   unspecified order, and can pull surprising work (allocation, syscalls) into
>   process startup. Construct-on-first-use or explicit init in `main`.
> - **Static-local singletons in the hot path**: the guard check is cheap but
>   non-zero; for the very hottest accessors, prefer a `constinit` pointer set
>   once during init, or pass the dependency in.

---

## Hands-on

```bash
./build.ps1 24-COMPILATION-LINKING/examples/09_linkage_storage.cpp   # -pthread
```

Shows: `static` local (guard), `thread_local` across 2 threads (separate counts),
`inline` variable, `extern` decl+def, `constinit`. Then:
- `g++ -O2 -S` the example and find the `__cxa_guard_acquire` around `next_seq`'s
  `static int seq`.
- Add `constinit` to a global initialized by a non-`constexpr` function → compile
  error (proves the guarantee).

---

## ⚠️ Traps

### Trap 1 — `static` local with a side-effecting initializer, thread race (pre-C++11)
Not an issue in C++11+ (thread-safe init guaranteed), but the guard has a small
cost — don't put a `static` local in the hottest inner loop if a `constinit`
pointer would do.

### Trap 2 — static init order fiasco
```cpp
extern Logger g_log;          // b.cxx
Config g_cfg = read(g_log);   // a.cxx  — ⚠️ g_log may be uninitialized
```
Construct-on-first-use or `constinit`.

### Trap 3 — `inline` variable pre-C++17 assumption
```cpp
// header:
int g_count = 0;              // ⚠️ pre-C++17: ODR violation across TUs
inline int g_count = 0;       // ✅ C++17
```

### Trap 4 — `thread_local` cost in a loop
```cpp
for (...) t_buf.push_back(x);     // ⚠️ TLS lookup each iteration
auto& b = t_buf; for (...) b.push_back(x);   // ✅ cache the reference
```

### Trap 5 — `constinit` ≠ `const`
```cpp
constinit int x = 5;
x = 10;                        // ✅ legal — constinit is about init timing, not mutability
```

### Trap 6 — class `static` member: declaration vs definition (pre-C++17)
```cpp
struct S { static std::string name; };   // declaration
// forgot: std::string S::name = "x";     // -> undefined reference when odr-used
// C++17: `inline static std::string name = "x";` in-class
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`static` ek cheez karta hai" | Namespace: internal linkage. Function: one-instance + lifetime. Class: shared member |
| "`thread_local` local variable jitna fast" | TLS lookup — cache the address in hot loops |
| "`inline` variable = performance hint" | It's a linkage rule — one shared definition across TUs from a header |
| "`constinit` variable is const" | Not const — guarantees *compile-time initialization*, value still mutable |
| "global init order is source order" | Within a TU yes; across TUs unspecified (init-order fiasco) |
| "class `static` member auto-defined" | Pre-C++17 needs an out-of-class definition; C++17 `inline static` doesn't |

---

## Exercises

1. **Duration + linkage:** classify `static int a;` (file scope), `static int b;`
   (in a function), `inline int c = 0;` (file scope), `thread_local int d;`
   (file scope).

   <details><summary>Answer</summary>

   `a`: static duration, internal linkage. `b`: static duration, no linkage
   (local name). `c`: static duration, external linkage (inline var). `d`: thread
   duration, external linkage.
   </details>

2. **Fiasco fix:** `Registry g_registry;` in `reg.cxx` is used by a global
   constructor in `plugin.cxx`. Sometimes crashes at startup. Fix without changing
   `plugin.cxx`.

   <details><summary>Answer</summary>

   Replace the global with construct-on-first-use: `Registry& registry() { static
   Registry r; return r; }` and have `plugin.cxx` (and everyone) call
   `registry()`. Now it's initialized on first use, before any user can touch it,
   regardless of TU init order. (Or `constinit` if `Registry` can be
   const-initialized.)
   </details>

3. **Guard cost:** where does `__cxa_guard_acquire` show up, and how do you avoid
   it for a hot-path accessor?

   <details><summary>Answer</summary>

   Around the first-time initialization of a function-local `static` with a
   non-constant initializer. Avoid it by: (a) making the static `constinit` /
   const-initializable, or (b) not using a function-local static at all — set a
   `constinit` pointer during explicit init and read that, or pass the dependency
   in.
   </details>

4. **`thread_local` semantics:** 3 threads each call `worker()` which does
   `thread_local int n = 0; n += 100;` in a loop of 10. Final `n` in each thread?
   In `main`'s thread if it never called `worker`?

   <details><summary>Answer</summary>

   Each worker thread ends with `n == 1000` (its own instance). `main`'s thread
   has its own `n` too; if it never ran the loop, `n == 0` there. They don't
   interfere.
   </details>

5. **`inline` var vs `extern`:** show the pre-C++17 and C++17 way to have a single
   shared `g_build_id` string declared in a header.

   <details><summary>Answer</summary>

   Pre-C++17: header `extern const std::string g_build_id;`, one `.cpp` `const
   std::string g_build_id = "...";`. C++17: header `inline const std::string
   g_build_id = "...";` — done, no `.cpp`.
   </details>

---

## Interview questions

1. Storage duration ke chaar types — ek-ek example.
2. `static` local: kab init, thread-safety, destruction — C++11 se kya badla?
3. `thread_local` ki access cost — hot loop mein kya karo?
4. `inline` variable (C++17) kis problem ko solve karta?
5. `constinit` vs `const` vs `constexpr` — teen alag cheezein.
6. Static initialization order fiasco — kya, do fixes.
7. Class `static` data member: pre-C++17 vs C++17 definition.
8. Dynamically-initialized global kab chalta (`.init_array`), order guarantee?

---

## Next
→ [`07-name-mangling.md`](07-name-mangling.md)
