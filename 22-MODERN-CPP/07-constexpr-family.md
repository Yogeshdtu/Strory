# 07 — The `constexpr` family: `constexpr` / `consteval` / `constinit`

## Prerequisites
- [`06-lambdas-deep.md`](06-lambdas-deep.md), folder 08 (`constexpr` intro), folder 21 file 13 (TMP)
- Folder 14 (static initialization order fiasco)

## Yeh topic abhi kyun
Teen keywords, teen alag guarantees:
- **`constexpr`** — "**can** run at compile time" (also runs at runtime).
- **`consteval`** (C++20) — "**must** run at compile time".
- **`constinit`** (C++20) — "this static variable **is initialized** at compile
  time" (kills the init-order fiasco, removes the runtime init guard).

Inhe theek se samajhna = compile-time computation aur deterministic startup ka
foundation. HFT ke liye dono matter.

---

## `constexpr`

A `constexpr` function/variable **may** be evaluated at compile time; whether it
*is* depends on the context.

```cpp
constexpr int square(int x) { return x * x; }

constexpr int a = square(5);        // MUST be compile-time -> a is 25, baked in
int b = square(runtimeValue);       // runs at RUNTIME (square is also a normal function)
int arr[square(4)];                 // MUST be compile-time -> arr has 16 elements
static_assert(square(6) == 36);     // MUST be compile-time
```

**What's allowed in a `constexpr` function** has grown every standard:
- C++11: a single `return`.
- C++14: loops, local variables, mutation, `if`/`switch`, multiple statements.
- C++17: `constexpr` lambdas, `if constexpr`.
- C++20: `try`/`catch` (no throwing at compile time), `dynamic_cast`, `typeid`,
  `virtual` `constexpr`, **`std::vector`/`std::string`** (in a `constexpr`
  context, must not escape), `std::is_constant_evaluated()`, changing the active
  union member.
- C++23: more relaxations, `constexpr` `std::to_chars`, goto/labels.

```cpp
constexpr auto make_sieve() {
    std::array<bool, 100> s{}; s.fill(true); s[0] = s[1] = false;
    for (std::size_t i = 2; i * i < s.size(); ++i)
        if (s[i]) for (std::size_t j = i * i; j < s.size(); j += i) s[j] = false;
    return s;
}
constexpr auto primes = make_sieve();     // a 100-entry table built by the compiler, lands in .rodata
```

A `constexpr` **variable** is `const` and, if it has static storage duration, is
initialized at compile time (a `constexpr` global has no init-order problem).

---

## `consteval` (C++20) — immediate functions

`consteval` = "every call **must** be a constant expression". If it can't be
evaluated at compile time, it's a **hard error**, not a fallback to runtime.

```cpp
consteval int factorial(int n) {
    int r = 1; for (int i = 2; i <= n; ++i) r *= i; return r;
}

constexpr int f5 = factorial(5);    // OK -> 120
int x = 5;
int fx = factorial(x);              // ❌ ERROR: x isn't a constant expression
```

Use when a computation should **never** happen at runtime: building a lookup
table, a compile-time hash of a fixed string, validating a format string
(`std::format`'s internals are `consteval`), enforcing that a magic constant is
computed by the compiler.

`consteval` functions leave **no runtime code** (unless the address is taken).

---

## `constinit` (C++20) — guaranteed static initialization

`constinit` applies to a **variable with static or thread-local storage
duration**. It asserts the variable is **constant-initialized** — its initial
value is computed at compile time — but the variable is **not** `const` (you can
mutate it at runtime).

```cpp
constinit int g_counter = 0;                       // zero-init at compile time; mutable at runtime
constinit Config g_config{DEFAULT_WIDTH, true};    // only if Config's ctor is constexpr-evaluable
```

Why it matters:
- **No static-initialization-order fiasco** (folder 14) — there's no runtime
  initialization to be ordered against other TUs' globals.
- **No init guard** — a non-`constinit` function-local `static` needs a
  thread-safe "has this been initialized?" check on every access (a branch + an
  atomic load). `constinit` (at namespace scope) removes it.
- If the initializer *can't* be evaluated at compile time → **compile error**,
  so you find out immediately instead of getting silent dynamic init.

`constinit` doesn't imply `const`; `constexpr` (on a global) implies both
`const` **and** constant-init.

---

## The decision table

| You want | Use |
|---|---|
| A value/function usable at compile time **and** runtime | `constexpr` |
| A function that must **never** run at runtime | `consteval` |
| A `const` compile-time constant | `constexpr` (variable) |
| A **mutable** global that's initialized at compile time (no init-order fiasco, no guard) | `constinit` |
| A `const` global initialized at compile time | `constexpr` (it's both) |
| Branch on "am I in a constant evaluation right now?" | `if consteval` (C++23) / `std::is_constant_evaluated()` (C++20) |

---

## Andar kya hota hai

- A `constexpr` function is compiled to normal machine code **and** is available
  to the compiler's constant-expression interpreter. In a `constexpr`-required
  context the interpreter runs it and bakes in the result; elsewhere the emitted
  code runs.
- `consteval` functions are only ever run by the interpreter; the compiler emits
  no callable body (unless `&fn` is taken) — pure compile-time.
- A `constexpr` / `constinit` global's value is placed directly in `.data` /
  `.rodata` at link time — the program starts with it populated, **no static
  initializer runs** for it. A plain non-trivial global needs a `__static_
  initialization_and_destruction` function called at startup (and ordered
  non-deterministically across TUs).
- A function-local `static X x = f();` compiles to: `if (!guard.initialized) {
  lock; if (!guard.initialized) { x = f(); guard.initialized = true; } }` on
  every entry — a load + a predictable branch in the fast path, but still not
  free. `constinit` at namespace scope avoids the whole dance.
- `std::is_constant_evaluated()` lets one function have a compile-time-friendly
  path (no `reinterpret_cast`, no intrinsics) and a fast runtime path.

> **HFT relevance:** two payoffs. **Compile-time tables** — CRC/checksum tables,
> tick-size maps, FSM transition tables, a perfect hash of a fixed symbol set,
> built by a `constexpr`/`consteval` function into a `std::array` → `.rodata` at
> runtime, **zero initialization cost**, which matters for a deterministic,
> jitter-free warm-up. **`constinit` globals** — the config block, the logger,
> the arena — get compile-time init, so there's no static-init-order fiasco
> between components and no per-access init guard on the hot path. `consteval`
> for `static_assert`-style "this constant must be computed by the compiler"
> enforcement. `std::is_constant_evaluated()` to keep one function usable in both
> a `constexpr` table build and a runtime fast path.

---

## Hands-on

```bash
./build.ps1 21-TEMPLATES/examples/04_if_constexpr.cpp   # compile-time factorial
```

Write: a `consteval` CRC-8 table builder into a `std::array<uint8_t, 256>` +
`static_assert` a known entry; a `constinit` global counter; a function that uses
`std::is_constant_evaluated()` to pick a `bit_cast` path vs a byte-loop path.

---

## ⚠️ Traps

### Trap 1 — expecting `constexpr` to force compile-time
```cpp
constexpr int f(int);
int r = f(userInput);   // runs at RUNTIME. Assign to a `constexpr` var, or use `consteval`
```

### Trap 2 — `constinit` on a type without a constexpr constructor
```cpp
constinit std::map<int,int> g_map{{1,2}};   // ❌ std::map's ctor isn't constexpr-evaluable -> compile error
```

### Trap 3 — `consteval` function whose argument might be runtime
```cpp
consteval int h(int n);
int arr[]{ h(1), h(getN()) };   // ❌ h(getN()) -> getN() isn't constant. h must always get constants
```

### Trap 4 — `constexpr std::vector` escaping the evaluation
```cpp
constexpr std::vector<int> v{1,2,3};   // ❌ pre-C++23 -- the allocation can't persist. Use std::array
```

### Trap 5 — assuming `constexpr` global == `constinit`
```cpp
// constexpr global: const + compile-time-init. constinit: compile-time-init but MUTABLE. Pick per need.
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`constexpr` means it runs at compile time" | It *can*; force it with a `constexpr`-required context or `consteval` |
| "`consteval` and `constexpr` are the same" | `consteval` **must** be compile-time (error otherwise); `constexpr` may be either |
| "`constinit` makes the variable `const`" | No — it's mutable; it only guarantees compile-time *initialization* |
| "A function-local `static` init is free" | It has a thread-safe init guard checked on every access |
| "`constexpr` functions can't run at runtime" | They can — they're also normal functions |

---

## Exercises

1. **Which keyword:** (a) a `pi` constant, (b) a table that must be built by the
   compiler, (c) a mutable global you want compile-time-initialized, (d) a
   function usable in both `constexpr` and runtime contexts.

   <details><summary>Answer</summary>

   (a) `constexpr` (variable). (b) `consteval` function (→ `constexpr` array).
   (c) `constinit`. (d) `constexpr` function.
   </details>

2. **Init-order fiasco:** you have `A::instance` (a global) whose constructor
   reads `B::config` (another global in a different TU). How does `constinit`
   help, and what's the limitation?

   <details><summary>Answer</summary>

   Make `B::config` `constinit` (compile-time init) → `A::instance`'s ctor can
   safely read it regardless of TU order, because there's no runtime init to
   race. Limitation: `config`'s value must be a constant expression — if it needs
   runtime setup (a file read), `constinit` won't compile and you need the
   "construct on first use" idiom instead.
   </details>

3. **consteval enforcement:** write `consteval std::uint32_t fnv1a(const char*
   s)` and show that `fnv1a("SYMBOL")` is a compile-time constant but
   `fnv1a(userStr)` fails.

   <details><summary>Answer</summary>

   `consteval std::uint32_t fnv1a(const char* s){ std::uint32_t h = 2166136261u;
   while (*s) { h ^= (std::uint8_t)*s++; h *= 16777619u; } return h; }`.
   `constexpr auto k = fnv1a("SYMBOL");` compiles (baked in). `fnv1a(runtimePtr)`
   → error: not a constant expression.
   </details>

4. **is_constant_evaluated:** why can't you just use `if constexpr
   (std::is_constant_evaluated())`?

   <details><summary>Answer</summary>

   `std::is_constant_evaluated()` is a runtime-value-ish function that returns
   `true` only *during* constant evaluation — it's not a constant expression
   itself in the way `if constexpr` needs. Use plain `if
   (std::is_constant_evaluated())`, or C++23's `if consteval`. (`if constexpr`
   would always take one branch.)
   </details>

5. **`.rodata`:** why does a `constexpr` lookup table give more deterministic
   startup than a table filled by a global constructor?

   <details><summary>Answer</summary>

   The `constexpr` table's bytes are in the binary image (`.rodata`) — the OS
   maps it, done, zero CPU work at startup. A constructor-filled table runs code
   during static initialization (ordered non-deterministically across TUs, and
   the work itself takes time/cache) before `main` — variable, and a source of
   warm-up jitter.
   </details>

---

## Interview questions

1. `constexpr` / `consteval` / `constinit` — teenon ka exact guarantee?
2. `constexpr` compile-time evaluation kab force hoti?
3. `constinit` init-order fiasco aur init-guard dono kaise fix karta?
4. `consteval` function ka runtime footprint (koi nahi, unless `&fn`)?
5. `constexpr` global vs `constinit` global — const-ness?
6. Compile-time table `.rodata` mein — startup jitter pe kya asar?

---

## Next
→ [`08-ranges-deep.md`](08-ranges-deep.md)
