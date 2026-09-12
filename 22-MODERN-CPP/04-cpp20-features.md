# 04 — C++20: the second big step

## Prerequisites
- [`03-cpp17-features.md`](03-cpp17-features.md)
- Folder 21 file 10 (concepts), folder 19 file 14 (ranges)

## Yeh topic abhi kyun
C++20 C++11 ke baad **sabse bada** release hai — **concepts**, **ranges**,
**coroutines**, **modules** (the "big four"), plus `<=>`, designated init,
`std::span`, `consteval`/`constinit`, `<bit>`, calendar/timezone `<chrono>`, aur
formatting. Har ek ka apna lesson hai; yahan overview + kya kahan.

---

## The big four

| Feature | What / why | Detail |
|---|---|---|
| **Concepts** | named, readable template constraints; one-line errors; subsumption | folder 21 file 10 |
| **Ranges** (`<ranges>`) | lazy composable views (`filter\|transform\|take`), range algorithms + projections | folder 19 file 14, [`08-ranges-deep.md`](08-ranges-deep.md), [`examples/03`](examples/03_ranges_pipelines.cpp) |
| **Coroutines** | `co_await` / `co_yield` / `co_return` — suspendable functions (generators, async) | [`09-coroutines.md`](09-coroutines.md), [`examples/04`](examples/04_coroutines_generator.cpp) |
| **Modules** | `export module` / `import` — replace textual `#include` | [`10-modules.md`](10-modules.md), `examples/05_modules_demo/` |

Toolchain support in 2024–26: concepts, ranges, `<=>` — solid on GCC/Clang/MSVC.
Coroutines — usable, but C++20 ships **no library types** (`std::generator` is
C++23), so you hand-roll or use a library. Modules — improving fast, build-system
integration still the friction point.

---

## Other language features

| Feature | What / why | Detail |
|---|---|---|
| **`operator<=>` (spaceship)** | one three-way comparison → all six relational operators (`= default`) | [`11-spaceship-operator.md`](11-spaceship-operator.md), [`examples/06`](examples/06_spaceship.cpp) |
| **Designated initializers** | `Widget w{.x = 1, .y = 2};` — init by member name (C-compatible subset) | folder 11 |
| **`consteval`** | "immediate function" — *must* run at compile time | folder 21 file 13 |
| **`constinit`** | guarantee static/thread-local init happens at compile time (no init-order fiasco, no runtime guard) | folder 14 |
| **`constexpr` almost everywhere** | `constexpr` `std::vector`/`std::string` (in constexpr context), virtual `constexpr`, `try`/`catch` in `constexpr`, `dynamic_cast` | folder 21 file 13 |
| **Abbreviated function templates** | `void f(auto x)` / `void f(Concept auto x)` — `auto` params on ordinary functions | folder 21 files 02, 10 |
| **Template `<auto N>` and class-type NTTPs** | non-type params of deduced or structural class type | folder 21 file 04 |
| **`[[likely]]` / `[[unlikely]]`** | branch-hint attributes on statements | file 12, folder 06 |
| **`[[no_unique_address]]`** | let an empty member take zero space (EBO for members) | folder 15 |
| **Lambda improvements** | `[]<class T>(T)` explicit template params; `[=, this]`; default-constructible & assignable stateless lambdas; pack capture `[...xs = ...]` | file 06 |
| **`using enum`** | bring an enum's enumerators into scope | folder 11 |
| **`char8_t`, `\u{...}`** | UTF-8 groundwork | folder 10 |
| **Range-`for` with initializer** | `for (auto v = get(); auto& x : v)` | folder 07 |

---

## Other library features

| Feature | What / why | Detail |
|---|---|---|
| **`std::span<T>`** | non-owning `{ptr, len}` view of contiguous elements — the array analogue of `string_view` | folder 19 file 03 |
| **`<bit>`** | `bit_cast`, `popcount`, `countl/r_zero`, `has_single_bit`, `bit_ceil`, `rotl`/`rotr`, `endian` | folder 19 file 22 |
| **`<numbers>`** | `std::numbers::pi`, `e`, `sqrt2`, ... as typed constants | — |
| **`std::format`** | type-safe formatted strings (`std::print` is C++23) | [`13-format-and-print.md`](13-format-and-print.md), [`examples/07`](examples/07_format_print.cpp) |
| **`<chrono>` calendar & time zones** | `year_month_day`, `zoned_time`, `std::chrono::current_zone()` | folder 19 file 17 |
| **`std::jthread`** | a `std::thread` that auto-joins in its destructor + carries a `stop_token` | folder 26 |
| **`<stop_token>`** | cooperative cancellation | folder 26 |
| **`<latch>`, `<barrier>`, `<semaphore>`** | new synchronization primitives | folder 26 |
| **`std::atomic_ref`**, `atomic<float>`, `wait`/`notify` on atomics | atomics improvements | folder 27 |
| **`std::erase` / `std::erase_if`** | free functions — the erase-remove idiom in one call | folder 19 file 10 |
| **`std::ssize`, `std::to_address`, `std::midpoint`, `std::lerp`, `std::cmp_less` (safe integer compare)** | small utilities | folders 03, 19 |
| **`std::bind_front`** | partial application without `bind`'s quirks | folder 19 file 16 |
| **`std::is_constant_evaluated()`** | branch on "am I running at compile time?" | folder 21 file 13 |
| **`std::source_location`** | `__FILE__`/`__LINE__`/`__func__` as a value (for logging) | — |

---

## Andar kya hota hai

- **Concepts / `<=>` / `<bit>` / `constexpr` extensions** — compile-time only,
  zero runtime cost. `<=>` generates the six operators as inline functions;
  `<bit>` maps to single CPU instructions (folder 19 file 22).
- **Ranges views** — lazy, non-owning; at `-O2` a `filter|transform` pipeline
  fuses to one loop, sometimes *faster* than a hand loop with a conditional
  accumulate ([`examples/03`](examples/03_ranges_pipelines.cpp): ~6.5 ms vs ~10
  ms). No intermediate containers.
- **Coroutines** — the compiler splits the function into a state machine and
  allocates a **coroutine frame** (heap, unless the allocation is elided when the
  frame's lifetime is provably bounded). `co_yield` = store value in the promise,
  suspend; resume = jump back in. There *is* a real cost (the frame, the
  suspend/resume bookkeeping) — coroutines are for *structure*, not raw speed.
- **Modules** — the interface is compiled once into a binary artifact (GCC
  `.gcm`), not re-textually-included per TU. Faster incremental builds, no macro
  leakage, no include-order fragility.
- **`std::jthread`** — RAII join + a `stop_source`/`stop_token` pair; the
  destructor requests stop and joins.

> **HFT relevance:** the compile-time features are pure win on the hot path —
> **concepts** constrain generic infra with clear errors, **`<=>`** removes
> comparison boilerplate, **`<bit>`** gives one-instruction bit ops for masks and
> free-lists, **`consteval`/`constinit`** move table building and static init to
> compile time (deterministic warm-up), **`std::span`** is the standard
> zero-copy buffer slice. **Ranges** give readable data massaging with no
> allocation (cheap adaptors freely; `filter`/`join` avoided in the innermost
> loop). **Coroutines** are used for *offline/async* structure (backtest event
> streams, async gateway I/O) — not the tick path, because of the frame
> allocation and suspend/resume overhead. **Modules** cut build time where the
> toolchain cooperates. `std::jthread` + `stop_token` clean up thread lifecycle
> in the control plane.

---

## Hands-on

```bash
./build.ps1 22-MODERN-CPP/examples/03_ranges_pipelines.cpp
./build.ps1 22-MODERN-CPP/examples/04_coroutines_generator.cpp
./build.ps1 22-MODERN-CPP/examples/06_spaceship.cpp
./build.ps1 fast 22-MODERN-CPP/examples/03_ranges_pipelines.cpp    # pipeline vs hand loop
cd 22-MODERN-CPP/examples/05_modules_demo && ./build.sh            # modules (separate build)
```

---

## ⚠️ Traps

### Trap 1 — expecting `std::generator` in C++20
```cpp
#include <generator>   // ❌ C++23. In C++20 you hand-roll a Generator (examples/04) or use a library
```

### Trap 2 — modules with your existing build system
```cpp
// `import geometry;` needs the module built first, in dependency order. CMake support is recent; plan for friction
```

### Trap 3 — custom `<=>` and forgetting `==`
```cpp
struct P { int v; std::strong_ordering operator<=>(const P&) const = default; };   // OK -- default gives == too
struct Q { int v; std::strong_ordering operator<=>(const Q& o) const { return v <=> o.v; } };  // ⚠️ must ALSO define ==
```

### Trap 4 — coroutine capturing a reference that outlives the caller
```cpp
Generator<int> g(const std::vector<int>& v) { for (int x : v) co_yield x; }
auto gen = g(makeVector());   // ⚠️ the temporary vector is gone before you pull -> dangling
```

### Trap 5 — `[=]` capturing `this` implicitly (deprecated in C++20)
```cpp
auto f = [=]{ return member; };   // ⚠️ captures `this` by pointer implicitly -> write `[=, this]` or `[this]` / `[*this]`
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "C++20 coroutines are fast — use them on the hot path" | They allocate a frame + suspend/resume bookkeeping; for *structure*, not the tick path |
| "Ranges pipelines are slower than hand loops" | At `-O2` they fuse; often equal, sometimes faster ([`examples/03`](examples/03_ranges_pipelines.cpp)) |
| "Modules are drop-in for `#include`" | Real build-order + tooling work; benefits are compile time + hygiene |
| "`= default` `<=>` always gives `==`" | Only the *defaulted* one does; a *custom* `<=>` needs an explicit `==` |
| "Concepts add runtime cost" | Compile-time constraints — zero runtime footprint |

---

## Exercises

1. **Big four:** name each of concepts / ranges / coroutines / modules and the
   pre-C++20 pain it addresses.

   <details><summary>Answer</summary>

   Concepts → unreadable SFINAE + page-long template errors. Ranges → iterator
   pair boilerplate + intermediate containers for multi-step transforms.
   Coroutines → hand-written state machines / callback hell for generators and
   async. Modules → textual `#include` (re-parse per TU, macro leakage,
   include-order fragility, header ODR traps).
   </details>

2. **`<=>` default:** `struct V { int a, b; auto operator<=>(const V&) const =
   default; };` — what does `V{1,2} < V{1,3}` do?

   <details><summary>Answer</summary>

   Member-wise lexicographic: compare `a` (1 == 1, tie), then `b` (2 <=> 3 →
   less) → `true`. The defaulted `<=>` also generated `==`, `!=`, `<=`, `>`, `>=`.
   </details>

3. **Coroutine cost:** why is a `Generator<int>` coroutine not a good choice for
   the innermost market-data parse loop?

   <details><summary>Answer</summary>

   Each coroutine has a heap-allocated frame (unless elided) and every `co_yield`
   / resume does suspend/restore bookkeeping — real per-element overhead. The
   parse loop wants a plain contiguous scan. Coroutines pay for themselves where
   the *structure* (lazy pull, async) is worth it, not in a hot tight loop.
   </details>

4. **span vs vector param:** rewrite `double sum(const std::vector<double>& v)`
   so it also accepts a `std::array`, a C array, and a sub-range — without a
   template.

   <details><summary>Answer</summary>

   `double sum(std::span<const double> xs) { double s = 0; for (double x : xs) s
   += x; return s; }` — all contiguous ranges convert to `span` implicitly;
   `sum(std::span<const double>{v}.subspan(2, 4))` for a slice.
   </details>

5. **constinit:** you have `static Registry g_registry;` and hit the static
   init-order fiasco. How does `constinit` help (and when can't it)?

   <details><summary>Answer</summary>

   `constinit Registry g_registry{...};` forces **compile-time** initialization —
   no runtime init, so no ordering dependency and no init guard. It only works if
   `Registry`'s constructor is `constexpr`-evaluable with the given args; a
   `Registry` that must do runtime work (open a file, read config) can't be
   `constinit`.
   </details>

---

## Interview questions

1. C++20 ke "big four" — kaunse, har ek kya solve karta?
2. Ranges pipeline `-O2` pe hand loop se slow? (nahi — `examples/03` numbers)
3. Coroutine frame — kahan allocate, hot path pe kyun avoid?
4. `<=>` `= default` — kitne operators generate, custom `<=>` ke saath `==`?
5. Modules `#include` se kaise alag — build/hygiene fayde?
6. `consteval` vs `constexpr` vs `constinit` — teenon ka fark?

---

## Next
→ [`05-cpp23-features.md`](05-cpp23-features.md)
