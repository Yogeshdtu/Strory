# 01 — C++11: the foundation of modern C++

## Prerequisites
- Folders 12–18 (pointers, references, move, RAII), folder 19 (STL)
- This folder is a **systematic audit** — you've met most of these in context; here they're catalogued standard-by-standard.

## Yeh topic abhi kyun
C++11 modern C++ ka **turning point** tha — move semantics, `auto`, lambdas,
`nullptr`, smart pointers, threads, range-`for`. Iske baad har standard
incremental hai. Yeh lesson: C++11 ke saare mukhya features ek jagah, ek line ka
"kya aur kyun", aur kahan detail padha.

---

## Language features

| Feature | What / why | Detail |
|---|---|---|
| **`auto`** | deduce a variable's type from its initializer — no spelling long iterator/template types | folder 03 file 12 |
| **`decltype`** | the type of an expression, for generic return types (`decltype(a+b)`) | folder 21 file 02 |
| **rvalue refs `T&&` + move semantics** | steal resources instead of copying → O(1) transfers | folder 18 files 05–08 |
| **`std::move` / `std::forward`** | cast to rvalue / conditional cast for perfect forwarding | folder 18 files 08, 12 |
| **Lambdas** | inline anonymous function objects with captured state | file 06, [`examples/01`](examples/01_lambdas_all.cpp) |
| **`nullptr`** | a real null-pointer literal (type `std::nullptr_t`) — not `0`/`NULL` | folder 12 file 04 |
| **Range-based `for`** | `for (auto& x : container)` — no iterators, works on any range | folder 07, folder 19 file 01 |
| **`constexpr`** | compute at compile time; C++11 was restrictive (single return), relaxed since C++14 | folder 08, folder 21 file 13 |
| **Uniform / brace init `{}`** | one syntax for all initialization; catches narrowing | folder 03 file 09 |
| **`= default` / `= delete`** | explicitly request or forbid a special member | folder 15, folder 18 file 09 |
| **`override` / `final`** | compiler-checked virtual override; seal a class/method | folder 16 |
| **`enum class`** | scoped, strongly-typed enumerations | folder 11 file 07 |
| **`static_assert`** | compile-time assertion with a message | folder 21 files 07, 13 |
| **Variadic templates** | templates with `class... Ts` parameter packs | folder 21 file 06 |
| **`noexcept`** | promise/query "won't throw"; enables move optimizations | folder 18 file 13, folder 23 |
| **Delegating & inheriting constructors** | a ctor calls another; `using Base::Base;` | folder 15 |
| **In-class member initializers** | `int x = 0;` at the declaration | folder 15 |
| **Trailing return type** | `auto f() -> T` — return type after the params | folder 21 file 02 |
| **Raw string literals** | `R"(no \ escaping)"` — regex/paths without backslash hell | folder 19 file 20 |
| **`alignas` / `alignof`** | query/control alignment | folder 11, folder 28 (false sharing) |
| **User-defined literals** | `42_km`, `"x"s` — a suffix operator | folder 19 file 17 (`chrono` literals) |
| **Attributes `[[...]]`** | `[[noreturn]]` in C++11; more added later | file 12 |
| **`thread_local`** | per-thread storage duration | folder 14, folder 26 |

---

## Library features

| Feature | What / why | Detail |
|---|---|---|
| **`std::unique_ptr` / `shared_ptr` / `weak_ptr`** | RAII ownership; `unique_ptr` zero-overhead | folder 17 |
| **`std::thread`, `std::mutex`, `std::atomic`, `std::condition_variable`, `std::future`** | the concurrency library (`<thread>` etc.) | folders 26–27 |
| **`std::array`** | fixed-size array with a container interface, no decay | folder 19 file 03 |
| **`std::unordered_map` / `unordered_set`** | hash containers, average O(1) | folder 19 file 06 |
| **`std::tuple`** | fixed heterogeneous aggregate | folder 19 file 15 |
| **`std::function`** | type-erased callable wrapper (has a cost — folder 19 file 16) | folder 19 file 16 |
| **`std::chrono`** | type-safe durations / time points / clocks | folder 19 file 17 |
| **`std::regex`** | regular expressions (slow — folder 19 file 20) | folder 19 file 20 |
| **`std::initializer_list`** | the `{1,2,3}` init mechanism | folder 15 |
| **Move-aware containers** | `std::vector` etc. move elements on reallocation if `noexcept` | folder 18 file 13 |
| **`std::to_string` / `std::stoi`** family | basic string↔number (superseded by `<charconv>` for speed) | folder 10 |
| **`<type_traits>`, `<random>`, `<ratio>`, `<system_error>`** | new utility headers | folders 19, 21 |

---

## Why C++11 mattered

- **Move semantics** made returning big objects by value cheap → RAII everywhere,
  fewer raw pointers, `std::vector<BigThing>` practical.
- **`auto` + lambdas + range-`for`** made STL algorithms usable without pages of
  iterator boilerplate.
- **Smart pointers** gave a real answer to "who deletes this?".
- **The memory model + `<atomic>`** gave C++ a *defined* concurrency semantics
  for the first time (folder 27).

Post-C++11 standards refine: C++14 fills gaps (generic lambdas, `make_unique`),
C++17 adds vocabulary types + structured bindings + `if constexpr`, C++20 is the
next big step (concepts, ranges, coroutines, modules, `<=>`), C++23 polishes
(`std::expected`, `std::print`, `std::mdspan`).

---

## Andar kya hota hai

- Most C++11 features are **zero runtime cost**: `auto`, `constexpr`,
  `static_assert`, `enum class`, `override`, `nullptr`, uniform init, variadic
  templates, `noexcept` — all compile-time. `auto x = f();` generates the same
  code as `T x = f();`.
- Move semantics is the one with runtime *impact* — and it's a *reduction*: a
  move ctor does a few pointer copies + nulls instead of an allocation + deep
  copy (folder 18).
- Lambdas are compiler-generated classes; a no-capture lambda is empty (size 1)
  and converts to a function pointer; passed to a template it inlines to nothing
  ([`examples/01`](examples/01_lambdas_all.cpp): `sizeof([]{}) == 1`).
- `std::unique_ptr` with the default deleter is exactly one pointer (`sizeof ==
  8`) via EBO (folder 17).

> **HFT relevance:** C++11 is the baseline every trading codebase builds on. The
> load-bearing pieces: **move semantics** (cheap object transfer, RAII resource
> wrappers), **`std::atomic` + the memory model** (the foundation of lock-free
> queues and Seqlocks — folders 27–28), **`std::chrono`** (nanosecond
> timestamping), **`std::unique_ptr`** (the objects that *are* heap-allocated, at
> startup), **`constexpr`** (compile-time tables), **`enum class` + `alignas` +
> `static_assert`** (wire-format structs locked at build time). Everything from
> C++14 on is refinement on top of this base.

---

## Hands-on

```bash
./build.ps1 22-MODERN-CPP/examples/01_lambdas_all.cpp
./build.ps1 22-MODERN-CPP/examples/08_legacy_to_modern.cpp
```

`08` is a C++98-style program next to its modern rewrite — spot every C++11+
feature that replaced a C++98 idiom.

---

## ⚠️ Traps

### Trap 1 — `auto` dropping const/ref
```cpp
const std::string& r = get();
auto x = r;   // ⚠️ x is std::string (a COPY) -- auto strips const & ref. `const auto& x = r;`
```

### Trap 2 — `{}` init and `std::initializer_list` ambiguity
```cpp
std::vector<int> v{5};    // one element: 5      (initializer_list wins)
std::vector<int> v(5);    // five elements: 0 0 0 0 0
```

### Trap 3 — `nullptr` vs `0` vs `NULL` in overloads
```cpp
void f(int); void f(char*);
f(NULL);     // ⚠️ NULL is often `0` -> calls f(int). f(nullptr) -> f(char*)
```

### Trap 4 — moved-from object reuse
```cpp
auto b = std::move(a);
a.size();   // ⚠️ a is "valid but unspecified" -- only assign to it or destroy it (folder 18)
```

### Trap 5 — capturing by reference in a lambda that outlives the referent
```cpp
std::function<int()> f;
{ int local = 5; f = [&local]{ return local; }; }
f();   // ⚠️ local is gone -> UB. Capture by value / init-capture
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`auto` is slower / dynamic" | Pure compile-time deduction — identical code to naming the type |
| "C++11 features add runtime cost" | Almost all are compile-time; move semantics *reduces* cost |
| "`nullptr` is just `#define nullptr 0`" | A keyword of type `std::nullptr_t` — participates correctly in overload resolution |
| "A lambda is a `std::function`" | A lambda is a unique compiler-generated class; `std::function` can *hold* one |
| "Uniform init `{}` is always equivalent to `()`" | `{}` prefers `initializer_list` ctors — `vector<int>{5}` ≠ `vector<int>(5)` |

---

## Exercises

1. **auto pitfall:** `const std::vector<int>& v = get(); auto copy = v; auto&
   ref = v;` — types and cost of `copy` and `ref`?

   <details><summary>Answer</summary>

   `copy` is `std::vector<int>` — a full deep copy (allocation + element copy).
   `ref` is `const std::vector<int>&` — an alias, no copy. `auto` strips
   cv/ref; `auto&` / `const auto&` keeps them.
   </details>

2. **Move win:** `std::vector<std::string> make(); auto v = make();` — with and
   without move semantics, what happens on the assignment?

   <details><summary>Answer</summary>

   With move (C++11+): the returned temporary's buffer is stolen — a few pointer
   copies, O(1). Without (C++98): a deep copy of the whole vector and every
   string. (RVO often elides even the move.)
   </details>

3. **Spot the C++11:** in `08_legacy_to_modern.cpp`'s `modern::run`, list five
   C++11+ features and the C++98 idiom each replaced.

   <details><summary>Answer</summary>

   Aggregate/brace init `{...}` (vs `push_back(Trade()); .back().x = ...`),
   range-`for` (vs iterator loops), `auto` (vs spelled-out iterator types),
   lambdas (vs functor structs), `nullptr` (vs `0`/`NULL`), `map::operator[]`
   for count (vs `find`/`insert`), structured bindings (C++17, vs `it->first`/
   `it->second`).
   </details>

4. **`enum class`:** why does `enum class Color { Red };` not implicitly convert
   to `int`, and why is that good?

   <details><summary>Answer</summary>

   Scoped enums have no implicit conversion to their underlying type → you can't
   accidentally use `Color::Red` in arithmetic or pass it where an `int` is
   expected. Prevents a whole class of bugs that unscoped `enum` allowed; cast
   explicitly (`static_cast<int>(c)`) when you really want the value.
   </details>

5. **noexcept move:** why does declaring your move constructor `noexcept` matter
   for `std::vector<YourType>`?

   <details><summary>Answer</summary>

   `std::vector` reallocation uses `std::move_if_noexcept` — it moves elements
   only if the move ctor is `noexcept`; otherwise it **copies** (to preserve the
   strong exception guarantee). Non-`noexcept` move → your "move-optimized" type
   gets copied on every growth (measured ~3× in folder 18 file 13).
   </details>

---

## Interview questions

1. C++11 ke 5 sabse impactful features aur kyun?
2. `auto` ki runtime cost (zero) — const/ref kyun strip hoti?
3. Move semantics ne kya badla (return by value, RAII)?
4. `nullptr` vs `0` / `NULL` — overload resolution ka fark?
5. `{}` init `std::initializer_list` ko prefer karti — `vector<int>{5}` vs `vector<int>(5)`?
6. `<atomic>` + memory model C++11 mein kyun bada deal tha?

---

## Next
→ [`02-cpp14-features.md`](02-cpp14-features.md)
