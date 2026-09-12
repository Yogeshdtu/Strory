# 05 — C++23: polish

## Prerequisites
- [`04-cpp20-features.md`](04-cpp20-features.md)
- Folder 19 file 15 (`std::expected`), folder 23 preview (error handling)

## Yeh topic abhi kyun
C++23 ek **consolidation** release hai — C++20 ki cheezein complete karta:
`std::expected` (finally a value-or-error type), `std::print` (a real
`printf`-replacement), `std::generator` (the coroutine library type C++20 forgot),
`std::mdspan`, deducing `this`, aur ranges ko `std::ranges::to` + naye adaptors
se poora karta. Toolchain support **partial** hai (GCC 14+/Clang 17+ mostly);
is repo ka default `-std=c++20` hai, to ye discussed hai, sab compile-verified
nahi.

---

## Library features

| Feature | What / why | Detail |
|---|---|---|
| **`std::expected<T, E>`** | value **or** error, no exceptions, no out-param; `.value()`, `.error()`, `and_then`/`transform`/`transform_error` monadic chain | folder 19 file 15, folder 23 |
| **`std::print` / `std::println`** | `std::print("x={}\n", x);` — `std::format` straight to a stream, faster than iostreams, type-safe unlike `printf` | [`13-format-and-print.md`](13-format-and-print.md) |
| **`std::generator<T>`** | the standard coroutine generator — `std::generator<int> fib() { ... co_yield a; ... }` (C++20 made you hand-roll it) | [`09-coroutines.md`](09-coroutines.md), [`examples/04`](examples/04_coroutines_generator.cpp) |
| **`std::mdspan`** | non-owning multi-dimensional array view over contiguous storage (row/column-major layouts, submdspan) | — |
| **`std::flat_map` / `std::flat_set`** | sorted-vector-backed map/set — the "cache-friendly `std::map`" from folder 20 file 17, standardized | folder 19 file 25, folder 20 file 17 |
| **`std::ranges::to<C>()`** | materialize a view into a container in one call: `v \| views::filter(...) \| std::ranges::to<std::vector>()` | folder 19 file 14 |
| **New range adaptors** | `views::zip`, `views::enumerate`, `views::adjacent`, `views::chunk`, `views::slide`, `views::chunk_by`, `views::join_with`, `views::cartesian_product` | folder 19 file 14 |
| **`std::ranges::fold_left` / `fold_right`** | the ranges version of `accumulate` with better constraints | folder 19 file 13 |
| **`std::byteswap`** | reverse byte order → `BSWAP` (endianness conversion) | folder 19 file 22 |
| **`std::stacktrace`** | capture and print a call stack as a value | folder 45 |
| **`std::move_only_function`** | like `std::function` but supports move-only callables (a move-only closure) | folder 19 file 16 |
| **`std::string::contains`, `std::ranges::contains`** | the obvious "is X in here" | folder 10, folder 19 file 09 |
| **`import std;`** | one module for the whole standard library (huge compile-time win, toolchain-dependent) | [`10-modules.md`](10-modules.md) |

---

## Language features

| Feature | What / why | Detail |
|---|---|---|
| **Deducing `this`** (`explicit object parameter`) | `auto size(this Self&& self)` — one member function template covers const/non-const/`&`/`&&` and enables recursive lambdas, CRTP-without-CRTP | — |
| **`if consteval`** | branch cleanly on compile-time vs runtime evaluation (better than `std::is_constant_evaluated()`) | folder 21 file 13 |
| **`[[assume(expr)]]`** | tell the optimizer `expr` is true (undefined behaviour if it isn't) — for provable invariants | file 12 |
| **`static operator()` / `static operator[]`** | a stateless functor's call operator can be `static` → no `this` to pass | — |
| **Multidimensional `operator[]`** | `m[i, j]` — real multi-arg subscript (for `mdspan` etc.) | — |
| **`#elifdef` / `#warning`** | preprocessor conveniences | — |
| **Narrowing contextual conversion, `auto(x)` decay-copy, `[[unlikely]]` on labels** | small fixes | — |

---

## `std::expected` — the one you'll reach for

```cpp
#include <expected>   // -std=c++23

std::expected<int, ParseError> parsePort(std::string_view s) {
    int n;
    auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), n);
    if (ec != std::errc{}) return std::unexpected(ParseError::NotANumber);
    if (n < 1 || n > 65535) return std::unexpected(ParseError::OutOfRange);
    return n;
}

auto r = parsePort(cfg);
if (r) use(*r);
else   log(r.error());

int port = parsePort(cfg).value_or(8080);

// monadic chaining -- no nested ifs
auto result = parsePort(a)
            .and_then(openSocket)          // called only on success; may itself return unexpected
            .transform(wrapInSession)      // maps the value
            .transform_error(annotate);    // maps the error
```

No heap (it's a `variant<T, E>` with an ergonomic API), no exceptions, composes.
The answer to "error handling without exceptions" (folder 23). Pre-C++23:
`tl::expected`, `absl::StatusOr`, `boost::outcome`.

---

## Andar kya hota hai

- `std::expected<T, E>` is inline storage for `T` **or** `E` + a discriminator —
  like `optional` but the "empty" arm carries an `E` (folder 19 file 15). `.value()`
  on an error throws `std::bad_expected_access`; `*r` is UB if it holds an error.
- `std::print` calls `std::vformat` into a buffer then writes it — no `printf`
  format-string parsing at runtime for the common case, no iostream `<<` chain,
  and a mismatched `{}`/argument is a **compile error**.
- `std::generator<T>` is a proper coroutine type with an input-range interface —
  same frame-allocation cost as any coroutine (file 09), but now you get
  `for (int x : fib())` and it composes with ranges.
- Deducing `this` collapses the "write it 4 times for const/non-const/lvalue/
  rvalue" member-function problem into one template, and the object parameter is
  a normal parameter → recursive lambdas (`auto fib = [](this auto self, int n){
  return n < 2 ? n : self(n-1) + self(n-2); };`).
- `import std;` parses the entire standard library once into a module → large
  builds drop significantly, where the toolchain supports it.

> **HFT relevance:** **`std::expected`** is the headline — a zero-overhead,
> allocation-free, exception-free value-or-error channel for parsers, validators,
> and any hot-path function that can fail (folder 23). C++20 codebases use
> `tl::expected` / a hand-rolled equivalent until the toolchain moves.
> **`std::flat_map` / `flat_set`** standardize the sorted-vector container HFT
> already hand-rolls (folder 20 file 17). **`std::move_only_function`** stores a
> move-only closure without `shared_ptr` gymnastics. **`std::byteswap`** is the
> one-instruction wire↔host conversion. **`std::mdspan`** for non-owning views of
> 2D data (a correlation matrix, a book snapshot grid). **`import std;`** cuts
> build time. Deducing `this` trims member-function boilerplate. `std::print` is
> nicer logging but stays off the tick path (any formatted I/O does).

---

## Hands-on

```bash
# this repo builds -std=c++20 -- to try C++23 bits:
g++ -std=c++23 -Wall -Wextra 22-MODERN-CPP/examples/04_coroutines_generator.cpp -o co23
# (if your GCC has <expected> / <generator>, swap the hand-rolled Generator for std::generator)
```

Sketch: rewrite a parser to return `std::expected<Config, Error>` with a monadic
chain; a `std::flat_map` price-level structure; a recursive lambda via deducing
`this`.

---

## ⚠️ Traps

### Trap 1 — assuming your toolchain has C++23 library bits
```cpp
#include <expected>   // GCC < 12 / libstdc++ without it -> not found. Check __cpp_lib_expected
```

### Trap 2 — `std::expected` `.value()` on an error
```cpp
auto r = parse(s);
int x = r.value();   // ⚠️ throws std::bad_expected_access if r holds an error. Check `if (r)` / value_or
```

### Trap 3 — `std::generator` still allocates a frame
```cpp
// It's ergonomic, not free -- same coroutine-frame cost. Not a hot-loop replacement for a plain scan
```

### Trap 4 — deducing `this` and forgetting it's a template
```cpp
struct S { void f(this S& self); };   // `f` is now a template -- can't be virtual, and takes the object explicitly
```

### Trap 5 — `[[assume(x)]]` with a false assumption
```cpp
[[assume(ptr != nullptr)]];   // ⚠️ if ptr CAN be null here -> UB. Only for genuinely provable invariants
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "C++23 is widely available" | Partial — check `__cpp_lib_*` / `__cpp_*` feature-test macros per feature |
| "`std::expected` allocates" | Inline `variant<T,E>`-like storage — no heap |
| "`std::generator` fixes coroutine overhead" | It's the missing *library type*; the frame cost is unchanged |
| "`std::print` is just `printf`" | Type-safe (bad `{}` = compile error), no format parsing for the common path, `std::format` spec |
| "Deducing `this` is a `this` rename" | The object becomes an explicit parameter → one function covers const/ref/rvalue; enables recursive lambdas |

---

## Exercises

1. **expected chain:** `parse` → `validate` → `build`, each returning
   `std::expected<_, Error>`. Compose them without nested `if`s.

   <details><summary>Answer</summary>

   `auto r = parse(input).and_then(validate).and_then(build);` — `and_then` runs
   the next step only on success and propagates the first error. `if (r)
   use(*r); else handle(r.error());`
   </details>

2. **expected vs optional:** when do you use `std::expected<T, E>` over
   `std::optional<T>`?

   <details><summary>Answer</summary>

   When the caller needs to know **why** it failed (a reason to report / branch
   on), not just "no value". `optional` is "present or absent"; `expected` is
   "value or a specific error".
   </details>

3. **flat_map:** why standardize `std::flat_map` when `std::map` exists?

   <details><summary>Answer</summary>

   `std::map` is a node-per-element tree → `log n` cache misses per lookup.
   `std::flat_map` is a sorted `std::vector` (keys) + a parallel vector (values)
   → contiguous, ~2 cache misses, `O(log n)` binary search, but `O(n)` insert.
   The cache-friendly trade-off from folder 20 file 17, now in the standard.
   </details>

4. **Deducing this — recursive lambda:** write a factorial as a lambda that can
   call itself.

   <details><summary>Answer</summary>

   `auto fact = [](this auto self, int n) -> long { return n <= 1 ? 1 : n *
   self(n - 1); };` — `self` is the lambda itself, passed as the explicit object
   parameter. Pre-C++23 you needed a `std::function` or a Y-combinator trick.
   </details>

5. **feature test:** how do you write code that uses `std::expected` if available
   and falls back otherwise?

   <details><summary>Answer</summary>

   `#if __cpp_lib_expected >= 202211L` → `#include <expected>` and use
   `std::expected`; `#else` → `#include "tl/expected.hpp"` and alias
   `template <class T, class E> using expected = tl::expected<T, E>;`.
   </details>

---

## Interview questions

1. C++23 ka theme (consolidation) — kaunse C++20 gaps bharta?
2. `std::expected<T,E>` — kya solve karta (vs exceptions, vs out-param, vs optional)?
3. `std::generator` — C++20 mein kya karna padta tha, cost change hua? (nahi)
4. `std::flat_map` `std::map` se kyun (cache) — trade-off?
5. Deducing `this` — member function boilerplate kaise kam, recursive lambda kaise?
6. `import std;` ka fayda?

---

## Next
→ [`06-lambdas-deep.md`](06-lambdas-deep.md)
