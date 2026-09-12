# 04 — Initialization order & the static init order fiasco

## Prerequisites
- `03-storage-duration.md`, `24-COMPILATION-LINKING` file 06
- [`examples/02_static_init_fiasco/`](examples/02_static_init_fiasco/)

## Yeh topic abhi kyun
Do globals, do alag TUs, ek dusre pe depend — aur jo pehle init hoga woh
**unspecified** hai. Nateeja: ek object jo abhi construct nahi hua use karna → UB,
jo intermittent crashes / garbage deta hai, aur sirf ek particular link order pe
manifest hota. Yeh senior-C++ ka pakka topic hai, aur iske fixes clean hain.

---

## Order rules — kya guaranteed hai

### Ek TU ke andar (namespace-scope non-local objects)
**Source order.** `a.cxx` mein `A ga; B gb;` → `ga` pehle, `gb` baad.

### Ek object ke andar (members / bases)
- Base classes → declaration order.
- Non-static data members → **declaration order** (member-init-list ka order NAHI —
  folder 15 ka trap).
- Phir constructor body.

### Local objects
Runtime order (jaise likhe hain, jab control pahunche).

### Function-local `static`
**First time control us declaration se guzarta hai** (lazy), thread-safe.

### ❌ Cross-TU namespace-scope objects
**UNSPECIFIED.** `a.cxx` ka `ga` aur `b.cxx` ka `gb` — kaun pehle? Standard kuch
nahi kehta. Link order, TU order, phase of the moon.

---

## The fiasco

```cpp
// logger.cxx
BadLogger g_bad_logger;                 // std::string member inside

// config.cxx
BadConfig::BadConfig() {
    g_bad_logger.log("config ctor");    // ⚠️ g_bad_logger constructed yet?
}
BadConfig g_bad_config;
```

Agar linker `config.cxx` ke initializers ko pehle chalata hai:
- `g_bad_logger` ke bytes zero hain (`.bss`) par uska **constructor nahi chala** →
  `std::string tag` member abhi construct nahi hua → `.c_str()` garbage / crash.

`examples/02_static_init_fiasco` — **do link orders**, do alag outputs:

```
link order A (config.o logger.o):
  BadLogger::BadLogger()  (ab log() safe)
  BADLOG[1/ready]: BadConfig ctor: engine      <- worked

link order B (logger.o config.o):
  BADLOG[1/(null)]: BadConfig ctor: engine      <- tag is "(null)"!  UB visible
  BadLogger::BadLogger()  (ab log() safe)
```

Same source, alag link order → alag behaviour. **Yeh IFNDR / UB hai.**

Destruction ka mirror bhi: globals reverse-of-construction order mein destruct
hote, cross-TU unspecified → ek global jo doosre ko apne dtor mein use kare →
destruction-order fiasco.

---

## Fix 1 — construct-on-first-use (Meyers singleton)

```cpp
Logger& logger() {
    static Logger instance;        // first call pe init, thread-safe guard
    return instance;
}
Config& config() {
    static Config instance;        // Config::Config() logger() ko call karta ->
    return instance;               //   logger ki static PEHLE fully-constructed
}
```

- Har global ko ek **function** ke peeche chhupao, andar ek function-local static.
- Init tabhi hota jab pehli baar use ho — **order use se determine hota**, not
  link order.
- `config()`'s static ctor `logger()` call karta → `logger()`'s static `config()`
  se pehle construct. Chain automatically ordered.
- `examples/02` ka FIXED side dono link orders mein identical output.

**Caveat:** destruction order abhi bhi reverse-of-construction — agar A ka dtor B
ko use kare aur B pehle construct hua tha, B pehle destruct hoga → A ka dtor
dangling B use karega. Fixes: never-destroy singletons (`static Logger* p = new
Logger; return *p;` — leak by design), ya `[[clang::no_destroy]]`, ya dtors ko
dependency-free rakho.

## Fix 2 — `constinit` (C++20)

```cpp
constinit int g_start_mode = 1;                    // ✅ compile-time init, no order issue
constinit Level g_levels[8] = { /* constexpr */ }; // ✅
constinit std::string g_name = "engine";           // ❌ std::string ctor not constexpr -> compile error
```

- Asserts **constant initialization** — value binary mein baked, no runtime
  initializer, no guard, no fiasco.
- Compile error agar initializer constant nahi → guarantee ka enforcement.
- **Not `const`** — value baad mein change ho sakti.
- Sirf un globals ke liye jo genuinely const-initializable hon.

## Fix 3 — no non-trivial globals

Dependencies ko **explicitly pass** karo. `main` mein sab construct karo, phir
inject. Testable, deterministic, no magic.

---

## `constinit` vs `constexpr` vs `const` (recap, folder 22)

| | Kya guarantee | Mutable after? | Storage |
|---|---|---|---|
| `const` | value change nahi hoga | no | anywhere |
| `constexpr` | **compile-time usable** (const + const-init) | no | `.rodata` |
| `constinit` | **compile-time initialized** (no runtime init) | **yes** | `.data`/`.bss` |

`constinit` = "no init-order fiasco, no init guard" without forcing immutability.

---

## Andar kya hota hai

- Dynamic-init global → compiler `.text` mein ek init fn, `.init_array` (ya
  `.ctors`) mein uska pointer. C runtime (`__libc_csu_init` / CRT) `main` se pehle
  `.init_array` ke sab pointers call karta — **is TU ka order source order, TUs ke
  beech ka order linker ke `.init_array` merge order pe** (unspecified per standard).
- `constinit`/const-init → koi `.init_array` entry nahi; value seedha section mein.
- Function-local static → `_ZGVZ...` guard variable; first call `__cxa_guard_acquire`
  (atomic), ctor, `__cxa_guard_release`, `__cxa_atexit(dtor)`.
- **Dynamic-init reads another dynamic-init global** → compiler can't reorder to
  fix it (different TUs). No diagnostic. It's on you.

---

## > **HFT relevance**
> - **`constinit` for all const/config globals** — tick sizes, capacities, table
>   data, feature flags. Deterministic startup, no guard, no fiasco.
> - **Construct-on-first-use for the few genuinely-stateful singletons** (a global
>   logger, a metrics registry) — but be deliberate about destruction order
>   (often: never destroy them; process exit reclaims everything).
> - **Avoid non-trivial global constructors** — they run before `main` in
>   unspecified order and can do surprising work (allocation, file I/O, syscalls).
>   A trading engine's startup should be an explicit, ordered sequence in `main`,
>   not a scatter of global ctors.
> - The fiasco is a real "works on my machine / crashes in CI after a link-order
>   change" bug — exactly the kind of nondeterminism you can't afford.

---

## Hands-on

```bash
cd 25-OBJECT-MODEL/examples/02_static_init_fiasco && ./build.sh
```

Two link orders, two outputs — one shows `tag = (null)` (the `std::string`
member's ctor hadn't run). The FIXED side is identical both ways. Then:
- Add `constinit int g_mode = some_runtime_fn();` → compile error (proves the
  guarantee).
- Add `constinit int g_mode = 7;` → compiles, no `.init_array` entry (`objdump -h`
  / `readelf -x .init_array`).

---

## ⚠️ Traps

### Trap 1 — cross-TU global dependency
```cpp
// a.cxx:  extern Logger g_log;  Config g_cfg(g_log);   // ⚠️ g_log constructed?
```
Construct-on-first-use or `constinit`.

### Trap 2 — member-init-list order ≠ init order
```cpp
struct S { int a, b; S(int x) : b(x), a(b) {} };   // ⚠️ a init FIRST (decl order), b garbage
```
Members init in **declaration order**; `-Wreorder` warns.

### Trap 3 — `constinit` ≠ `const`
`constinit int x = 5; x = 10;` is legal. `constinit` is about *when*, not *whether
it can change*.

### Trap 4 — destruction-order fiasco
A static local's destructor uses another static that was constructed later (so
destroyed earlier). Keep singleton dtors dependency-free, or never destroy.

### Trap 5 — assuming `.bss` zero means "initialized"
Zeroed bytes ≠ constructed object for a non-trivial type. `std::string` at all-zero
bytes is not a valid empty string.

### Trap 6 — first-use static in a signal handler / early ctor
The first call might be from a context where the guard's lock isn't safe. Prefer
`constinit` or explicit init for anything touched that early.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "globals init in a fixed order" | Within a TU: source order. Across TUs: **unspecified** |
| "`.bss` zeroed = object ready" | Non-trivial type needs its ctor; zero bytes aren't a valid object |
| "member-init-list sets init order" | Declaration order does; the list just supplies values |
| "`constinit` makes it const" | No — guarantees compile-time init; still mutable |
| "the fiasco is rare/theoretical" | A link-order change in CI can surface it as intermittent crashes |
| "construct-on-first-use fixes destruction too" | Only construction order; destruction can still fiasco |

---

## Exercises

1. **Predict:** `struct P { int a, b; P() : b(1), a(b + 1) {} };` — values of `a`,
   `b` after `P p;`?

   <details><summary>Answer</summary>

   `a` is initialized **first** (declaration order), from `b + 1` where `b` is
   still garbage → `a` is garbage. Then `b = 1`. The member-init-list order
   (`b, a`) is a red herring; `-Wreorder` warns.
   </details>

2. **Fiasco or not:** `a.cxx`: `int g_x = 42;`  `b.cxx`: `int g_y = g_x + 1;`.
   Is `g_y` reliably `43`?

   <details><summary>Answer</summary>

   No. `g_x` is dynamically... actually `g_x = 42` is *constant* initialization
   (baked in `.data`), and constant init happens before any dynamic init, so
   `g_y` (dynamic init) sees `g_x == 42` → `g_y == 43` reliably here. Change
   `g_x` to `int g_x = compute();` (dynamic init) and it becomes a genuine
   fiasco.
   </details>

3. **Fix it:** `Registry g_registry;` in `reg.cxx`, used by a global constructor
   in `plugin.cxx`. Intermittent startup crash. Fix without touching `plugin.cxx`.

   <details><summary>Answer</summary>

   `Registry& registry() { static Registry r; return r; }` and have everyone
   (including `plugin.cxx` via the same header) call `registry()`. First use
   triggers construction, guaranteed before anyone touches it.
   </details>

4. **`constinit`:** which compile? `constinit int a = 5;`, `constinit int b =
   std::time(nullptr);`, `constinit const char* c = "x";`, `constinit std::string
   d = "x";`

   <details><summary>Answer</summary>

   `a`, `c` — compile (constant initialization). `b` — error (`std::time` isn't
   `constexpr`). `d` — error (`std::string`'s ctor isn't `constexpr` in a way that
   makes this constant-initializable at namespace scope).
   </details>

5. **Destruction fiasco:** sketch a two-singleton case where construction is fine
   but destruction is UB.

   <details><summary>Answer</summary>

   `Pool& pool()` constructed first (someone calls it early), `Logger& logger()`
   constructed later. `Pool::~Pool()` calls `logger().log("draining")`. At exit,
   destruction is reverse of construction → `logger` destroyed first → `Pool`'s
   dtor uses a destroyed `Logger`. Fix: never destroy the singletons, or make
   `Pool::~Pool()` not touch other singletons.
   </details>

---

## Interview questions

1. Init order guarantees: within a TU, within an object, across TUs.
2. Static initialization order fiasco — kya, kyun UB, kaise reproduce.
3. Construct-on-first-use — kaise kaam karta, ek limitation (destruction).
4. `constinit` — kya guarantee, `const`/`constexpr` se farq.
5. Member-init-list order vs actual init order — kaunsa jeetta?
6. Dynamic-init global kab chalta (`.init_array`)?
7. Destruction-order fiasco — kya, kaise bachein?

---

## Next
→ [`05-temporaries.md`](05-temporaries.md)
