# 02 — C++14: filling the gaps

## Prerequisites
- [`01-cpp11-features.md`](01-cpp11-features.md)

## Yeh topic abhi kyun
C++14 ek **chhota, incremental** release tha — C++11 ke rough edges theek karne
wala. Koi bada naya paradigm nahi, par har din kaam aane wale fixes: generic
lambdas, `make_unique`, relaxed `constexpr`, return type deduction.

---

## Language features

| Feature | What / why |
|---|---|
| **Generic lambdas** (`[](auto x){}`) | lambda parameters can be `auto` → `operator()` becomes a template. The `std::visit` generic-lambda pattern (folder 19 file 15) needs this. |
| **`auto` return type deduction** | `auto f() { return x; }` — no trailing `-> decltype(...)` needed. `decltype(auto)` preserves references (folder 21 file 02). |
| **Relaxed `constexpr`** | C++11 `constexpr` functions were one `return` statement; C++14 allows loops, locals, mutation, multiple statements → real compile-time computation without recursive templates (folder 21 file 13). |
| **Variable templates** | `template <class T> constexpr T pi = T(3.14159...);` — the mechanism behind `std::is_integral_v` (the `_v` aliases arrived in C++17 but the feature is C++14). |
| **Init-capture / generalized lambda capture** | `[n = 0]` / `[p = std::move(ptr)]` — create a new closure member, move-capture a move-only object (folder 21 file... [`examples/01`](examples/01_lambdas_all.cpp)). |
| **Binary literals + digit separators** | `0b1010`, `1'000'000` — readability. |
| **`[[deprecated]]`** | mark an API deprecated with an optional message. |
| **Aggregate init with NSDMI** | aggregates with in-class member initializers can still be brace-initialized (a C++11 inconsistency, fixed). |

---

## Library features

| Feature | What / why |
|---|---|
| **`std::make_unique<T>(args...)`** | the C++11 omission — `unique_ptr` had no factory, only `shared_ptr` did. Prefer `make_unique` over `unique_ptr<T>(new T(...))` (exception safety in argument evaluation, one place naming `T`). |
| **`std::exchange(obj, newVal)`** | set `obj` to `newVal`, return the old value. The idiom for move constructors: `ptr_(std::exchange(o.ptr_, nullptr))` (folder 18). |
| **Transparent comparators** (`std::less<>`, `std::greater<>`) | heterogeneous lookup in `std::map`/`std::set` — `m.find("literal")` without constructing a `std::string` key (folder 19 file 05). |
| **`std::integer_sequence` / `index_sequence`** | compile-time index packs for tuple iteration (folder 21 file 13). |
| **`std::quoted`** | stream manipulator that adds/strips quotes and escapes. |
| **`cbegin`/`cend` etc. as free functions**, `std::rbegin`/`rend` | round out the non-member iterator helpers. |
| **`std::shared_timed_mutex`** | reader-writer lock (`shared_mutex` without the "timed" is C++17). |
| **Chrono / string_view groundwork** | `""s`, `""ms` user-defined literals for `std::string` and `std::chrono`. |

---

## The two you'll use most

### `std::make_unique`
```cpp
auto p = std::make_unique<Widget>(id, name);          // vs unique_ptr<Widget>(new Widget(id, name))
auto arr = std::make_unique<int[]>(n);                // dynamic array
```
Benefits: one mention of `Widget`, no bare `new`, and (the historical reason)
exception safety when the `unique_ptr` is one argument among several in a call —
`f(std::make_unique<A>(), mayThrow())` can't leak, `f(std::unique_ptr<A>(new A),
mayThrow())` could (evaluation-order gap, tightened in C++17 but the guidance
stands).

### Generic lambdas
```cpp
auto printer = [](const auto& x) { std::cout << x << '\n'; };   // works for any streamable type
std::visit([](auto&& v) { handle(v); }, myVariant);             // one lambda, all alternatives
std::sort(v.begin(), v.end(), [](const auto& a, const auto& b) { return a.key < b.key; });
```
`operator()` is a member template → the closure adapts per call site, still fully
inlined.

---

## Andar kya hota hai

- Generic lambda: the compiler generates a closure whose `operator()` is a
  template; each distinct argument-type combination instantiates it. Same
  zero-overhead story as a hand-written function-object template.
- Relaxed `constexpr` means the compiler's constant-expression interpreter now
  handles loops and mutable locals → a `constexpr` factorial is a normal loop,
  not a chain of `Fact<N>` class instantiations (folder 21 file 13). Fewer
  instantiations, faster builds, real error messages.
- `std::exchange` compiles to "load old, store new, return old" — a few
  instructions, inlined. It exists to make move constructors a one-liner and
  correct (steal *and* null the source in one expression).
- `std::make_unique` is a thin `template` wrapper around `new`; at `-O2` it
  inlines to exactly the `new` + `unique_ptr` construction you'd write by hand.

> **HFT relevance:** nothing paradigm-shifting, but the quality-of-life fixes are
> everywhere in modern trading code: **`std::make_unique`** for the startup-time
> object graph, **generic lambdas** in `std::visit` message dispatch and
> algorithm predicates (inlined, zero cost), **relaxed `constexpr`** for building
> lookup tables with readable loops instead of template recursion (folder 21 file
> 13), **`std::exchange`** in every hand-rolled move constructor for resource
> wrappers, **transparent comparators** to avoid a `std::string` allocation on
> every `std::map` lookup in warm-path code.

---

## Hands-on

```bash
./build.ps1 22-MODERN-CPP/examples/01_lambdas_all.cpp    # generic lambdas, init-capture
./build.ps1 21-TEMPLATES/examples/04_if_constexpr.cpp    # relaxed constexpr
```

Write: a `constexpr` function with a loop that builds a small table (C++11
couldn't); a generic lambda used with three different types; a move constructor
using `std::exchange`.

---

## ⚠️ Traps

### Trap 1 — `auto` return deduction across multiple `return`s
```cpp
auto f(bool b) { if (b) return 1; else return 2.0; }   // ❌ int vs double -> "inconsistent deduction"
```

### Trap 2 — generic lambda hiding a type error until instantiation
```cpp
auto f = [](auto x){ return x.size() + 1; };   // fine until you call f(42) -> error deep in the closure
```

### Trap 3 — `make_unique` for a type with a custom deleter
```cpp
std::make_unique<FILE>(...);   // ❌ can't pass a deleter. Use unique_ptr<FILE, decltype(&fclose)>(fopen(...), &fclose)
```

### Trap 4 — init-capture by value copies, not references
```cpp
auto f = [v = bigVector]{ ... };   // ⚠️ copies bigVector into the closure. [v = std::move(bigVector)] to move
```

### Trap 5 — `std::exchange` argument order
```cpp
ptr_ = std::exchange(o.ptr_, nullptr);   // ✅ ptr_ gets o.ptr_'s OLD value; o.ptr_ becomes nullptr
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "C++14 was a major release" | Small, corrective — generic lambdas, `make_unique`, relaxed `constexpr` |
| "`make_unique` is just sugar" | Also fixes an exception-safety gap and removes the bare `new` |
| "C++11 `constexpr` already supports loops" | No — C++11 was single-return; loops/mutation are C++14 |
| "Generic lambdas have overhead" | `operator()` is a template — inlines like any function object |
| "`std::exchange` is niche" | It's the canonical steal-and-null for move constructors |

---

## Exercises

1. **make_unique benefit:** why is `process(std::make_unique<A>(),
   compute())` leak-safe while `process(std::unique_ptr<A>(new A), compute())`
   historically wasn't?

   <details><summary>Answer</summary>

   With bare `new`, the compiler could evaluate `new A`, then `compute()` (which
   throws), then the `unique_ptr` ctor — the `A` leaks. `make_unique` bundles
   `new A` and the `unique_ptr` construction into one call that can't be
   interleaved. (C++17 tightened argument evaluation, but `make_unique` is still
   the idiom.)
   </details>

2. **constexpr loop:** write a C++14 `constexpr` `sum_to(n)` and use it as an
   array size.

   <details><summary>Answer</summary>

   `constexpr int sum_to(int n){ int s = 0; for (int i = 1; i <= n; ++i) s += i;
   return s; }` then `int arr[sum_to(10)];` (= `int arr[55]`). C++11 would have
   needed `n <= 1 ? 0 : n + sum_to(n-1)`.
   </details>

3. **Generic lambda instantiation:** `auto f = [](auto a, auto b){ return a + b;
   };` called with `(1,2)`, `(1.0,2.0)`, `(1,2.0)`. How many `operator()`
   instantiations?

   <details><summary>Answer</summary>

   Three: `<int,int>`, `<double,double>`, `<int,double>`. Same as a function
   template.
   </details>

4. **Move ctor with exchange:** write the move constructor for `struct Buf { char*
   p; size_t n; };` using `std::exchange`.

   <details><summary>Answer</summary>

   `Buf(Buf&& o) noexcept : p(std::exchange(o.p, nullptr)), n(std::exchange(o.n,
   0)) {}` — each member steals the source's value and resets it in one
   expression.
   </details>

5. **Transparent comparator:** what does `std::map<std::string, int, std::less<>>`
   give you that `std::map<std::string, int>` doesn't?

   <details><summary>Answer</summary>

   Heterogeneous lookup: `m.find("literal")` / `m.find(sv)` without constructing
   a temporary `std::string` key. `std::less<>` (note the empty `<>`) opts the
   container into the transparent-comparator overloads.
   </details>

---

## Interview questions

1. C++14 ka scope — C++11 ke kaunse gaps bhare?
2. `std::make_unique` — 2 wajah (exception safety, no bare `new`)?
3. Relaxed `constexpr` (C++14) — C++11 se kya alag?
4. Generic lambda `operator()` template kyun, cost kya?
5. `std::exchange` — move constructor mein kaise use?
6. Transparent comparator `std::less<>` — `std::map` lookup mein kya bachata?

---

## Next
→ [`03-cpp17-features.md`](03-cpp17-features.md)
