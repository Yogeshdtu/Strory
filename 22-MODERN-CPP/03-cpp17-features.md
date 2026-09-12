# 03 — C++17: vocabulary types and ergonomics

## Prerequisites
- [`02-cpp14-features.md`](02-cpp14-features.md)
- Folder 19 (STL — `optional`/`variant`/`any` live in file 15)

## Yeh topic abhi kyun
C++17 ne **vocabulary types** (`optional`, `variant`, `any`, `string_view`) aur
bade ergonomic wins (structured bindings, `if constexpr`, fold expressions, CTAD)
diye. Yeh woh version hai jahan modern C++ "readable" ho gaya. Ye sab pichle
folders mein use hue — yahan systematic list.

---

## Language features

| Feature | What / why | Detail |
|---|---|---|
| **Structured bindings** | `auto [a, b] = pair;` — unpack tuple/struct/array into names | [`examples/02`](examples/02_structured_bindings.cpp), folder 19 file 15 |
| **`if` / `switch` with initializer** | `if (auto it = m.find(k); it != m.end())` — scope the temporary to the branch | folder 06 |
| **`if constexpr`** | compile-time branch; discarded branch not instantiated | folder 21 file 07 |
| **Fold expressions** | `(xs + ...)` — variadic reductions without recursion | folder 21 file 06 |
| **CTAD** | `std::pair p{1, 2.0};` — deduce class template args from the constructor | folder 21 file 03 |
| **Inline variables** | `inline constexpr int x = 5;` in a header — one definition across TUs (the `_v` traits) | folder 21 file 08 |
| **`constexpr` lambdas** | a lambda usable in constant expressions | file 06 |
| **Guaranteed copy elision** | a prvalue return is *not* a copy/move — the object is built in place; the type needn't be movable | folder 18 file 11 |
| **Nested namespaces** | `namespace a::b::c { }` | folder 24 |
| **`[[nodiscard]]`, `[[maybe_unused]]`, `[[fallthrough]]`** | warning-control attributes | file 12 |
| **`__has_include`** | preprocessor check for a header's existence | — |
| **Class template `auto` non-type params** (partial) | groundwork for C++20's `template <auto N>` | folder 21 file 04 |
| **Aggregate init with base classes** | `struct D : B { int x; }; D d{ {baseInit}, 1 };` | folder 15 |
| **`u8` char literals, hex floats** | minor literal additions | — |

---

## Library features — the big ones

| Type | What / why | Detail |
|---|---|---|
| **`std::optional<T>`** | "a `T` or nothing" — no heap, `value_or`, safe absence | folder 19 file 15 |
| **`std::variant<Ts...>`** | type-safe tagged union + `std::visit` (jump-table dispatch) | folder 19 file 15 |
| **`std::any`** | holds any type (type-erased, may allocate) — rarely the right tool | folder 19 file 15 |
| **`std::string_view`** | non-owning `{ptr, len}` view of a character range — zero-copy string params | folder 10 |
| **`std::byte`** | `enum class byte : unsigned char` — "raw memory" that isn't `char` arithmetic | folder 14 |
| **`std::filesystem`** | portable paths, directory iteration, file ops | folder 19 file 19 |
| **`std::invoke` / `std::apply`** | uniform call of any Callable / call `f` with a tuple's elements | folder 19 file 16 |
| **`<charconv>` — `std::to_chars` / `from_chars`** | fastest, allocation-free, locale-independent number↔string (~8× `stoi` — folder 10) | folder 10 |
| **Parallel algorithms** (`std::execution::par`) | `std::sort(std::execution::par, ...)` — opt-in parallelism (libstdc++ needs TBB) | folder 19 file 13 |
| **`std::shared_mutex`** | reader-writer lock | folder 26 |
| **`std::scoped_lock`** | deadlock-free multi-mutex RAII lock (`lock_guard` for many mutexes) | folder 26 |
| **`std::clamp`, `std::gcd`, `std::lcm`, `std::sample`, `std::reduce`, `std::*_scan`** | new algorithm/numeric functions | folder 19 files 09, 13 |
| **`std::launder`** | tell the optimizer "this memory now holds a new object" (placement-new edge cases) | folder 25 |
| **`std::uncaught_exceptions()`** | count, not just bool — for `ScopeGuard`-style "did we unwind?" | folder 23 |

---

## The workhorses

```cpp
// optional -- may-fail without an error channel
std::optional<int> parsePort(std::string_view s);
int port = parsePort(cfg).value_or(8080);

// variant -- a closed set of message types
using Msg = std::variant<Add, Cancel, Trade>;
std::visit(Overloaded{ [](const Add& a){...}, [](const Cancel& c){...}, [](const Trade& t){...} }, msg);

// string_view -- read-only string params, no copy
void log(std::string_view line);          // accepts std::string, const char*, a substring -- all without allocating

// structured bindings + if-init
if (auto [it, inserted] = table.try_emplace(key, value); !inserted)
    it->second = value;

// from_chars -- parse without allocation or locale
int n; auto [ptr, ec] = std::from_chars(buf, buf + len, n);
if (ec == std::errc{}) use(n);
```

---

## Andar kya hota hai

- `optional` / `variant` are **inline storage** — `optional<T>` is `T` + a
  `bool`; `variant<Ts...>` is aligned storage for the largest alternative + a
  small tag. No allocation (folder 19 file 15). `std::visit` builds a compile-time
  jump table → one predictable indirect call.
- `string_view` is two words (ptr + len). Passing it by value is the idiom; it
  never touches the heap. The danger is lifetime — it doesn't own the chars.
- **Guaranteed copy elision**: `return T{...};` (a prvalue) constructs the result
  *directly* in the caller's storage — there is no temporary, so `T` doesn't even
  need a copy/move constructor. This changed the language semantics, not just an
  optimization (folder 18 file 11).
- `if constexpr` and fold expressions are compile-time; structured bindings
  introduce aliases (not new objects); CTAD runs function-template deduction on
  synthesized constructors.
- `from_chars` is a hand-written digit loop — no `strtol`, no locale, no
  allocation → ~8× `std::stoi` and no error-prone `errno` dance (folder 10).

> **HFT relevance:** C++17 is where the vocabulary the hot path actually uses
> landed: **`std::variant` + `std::visit`** for parsed-message dispatch
> (allocation-free, jump-table, callee inlines — folder 19 files 15, 26),
> **`std::optional`** for "found / not found" with no heap, **`std::string_view`**
> and **`std::byte`** for zero-copy views into a receive buffer, **`<charconv>`**
> for the fastest possible numeric parsing in a market-data decoder,
> **`if constexpr`** for type-dependent fast paths (folder 21 file 07),
> **guaranteed copy elision** so factory functions returning big value types cost
> nothing. `std::scoped_lock` for deadlock-safe multi-lock in the control plane.
> `std::any` and `std::filesystem` stay off the hot path.

---

## Hands-on

```bash
./build.ps1 22-MODERN-CPP/examples/02_structured_bindings.cpp
./build.ps1 19-STL/examples/06_optional_variant.cpp
./build.ps1 10-STRINGS/examples/05_fast_parsing.cpp     # from_chars vs stoi
```

Write: a parser returning `std::optional<Quote>`; a `std::variant<int, double,
std::string>` + an `Overloaded` visitor; a function taking `std::string_view`
called with a `std::string`, a literal, and a `substr`.

---

## ⚠️ Traps

### Trap 1 — `string_view` outliving its data
```cpp
std::string_view sv = std::string("temp");   // ⚠️ the temporary dies at the semicolon -> sv dangles
```

### Trap 2 — `std::get<T>` / `*optional` without checking
```cpp
int x = std::get<int>(v);   // ⚠️ throws if v isn't an int. holds_alternative / get_if first
int y = *opt;               // ⚠️ UB if empty. `if (opt)` / value_or
```

### Trap 3 — structured binding by value when you wanted a reference
```cpp
for (auto [k, v] : myMap) v = 0;   // ⚠️ modifies a copy. `for (auto& [k, v] : myMap)`
```

### Trap 4 — `from_chars` not checking `ec`
```cpp
int n; std::from_chars(b, e, n);   // ⚠️ `n` untouched on failure -> garbage. Check the returned `ec`
```

### Trap 5 — assuming `std::execution::par` just works
```cpp
std::sort(std::execution::par, v.begin(), v.end());   // libstdc++: needs -ltbb linked, else falls back / errors
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`std::optional` / `variant` allocate" | Inline storage — no heap (unless an alternative like `std::string` does) |
| "`string_view` copies the string" | It's `{ptr, len}` — shares the chars; must not outlive them |
| "Guaranteed copy elision is an optimization" | It's a language rule — a prvalue return builds in place, no move needed |
| "`if constexpr` is a fast runtime `if`" | Compile-time; the dead branch isn't even instantiated |
| "`from_chars` is `std::stoi` with a nicer name" | No allocation, no locale, no exceptions — ~8× faster |

---

## Exercises

1. **variant dispatch:** `using V = std::variant<int, std::string>;` — write a
   `describe(const V&)` that returns `"int:<n>"` or `"str:<s>"` using `std::visit`
   + a generic lambda + `if constexpr`.

   <details><summary>Answer</summary>

   `std::visit([](const auto& x) -> std::string { using T = std::decay_t<
   decltype(x)>; if constexpr (std::is_same_v<T,int>) return "int:" +
   std::to_string(x); else return "str:" + x; }, v);`
   </details>

2. **optional chain:** `std::optional<Config> load(); std::optional<int>
   getPort(const Config&);` — get the port or 0, no exceptions.

   <details><summary>Answer</summary>

   C++17: `int port = 0; if (auto c = load()) if (auto p = getPort(*c)) port =
   *p;`. C++23: `load().and_then(getPort).value_or(0)`.
   </details>

3. **string_view safety:** which is safe? (a) `sv = someString;` (b) `sv =
   getString();` (returns by value) (c) `sv = "literal";`

   <details><summary>Answer</summary>

   (a) safe while `someString` lives. (b) **dangling** — the returned temporary
   dies at the end of the statement. (c) safe — string literals have static
   storage duration.
   </details>

4. **Guaranteed elision:** `struct NoMove { NoMove(int); NoMove(const NoMove&) =
   delete; NoMove(NoMove&&) = delete; }; NoMove make() { return NoMove{5}; }
   auto x = make();` — does this compile in C++17? C++14?

   <details><summary>Answer</summary>

   C++17: **yes** — the prvalue `NoMove{5}` is constructed directly in `x`'s
   storage; no copy/move is involved, so their deletion is irrelevant. C++14:
   **no** — the return would (conceptually) move, and the move ctor is deleted.
   </details>

5. **from_chars:** parse `"12345abc"` into an `int` and report where parsing
   stopped.

   <details><summary>Answer</summary>

   `int n; auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), n);` →
   `n == 12345`, `ec == std::errc{}`, `ptr` points at `'a'` (`ptr - s.data() ==
   5`). No exception, no allocation.
   </details>

---

## Interview questions

1. C++17 ke vocabulary types — `optional` / `variant` / `any` / `string_view`, allocate karte?
2. Structured bindings — kya introduce karte (aliases vs new objects)?
3. Guaranteed copy elision — optimization ya language rule? Movable na hone pe?
4. `if constexpr` + fold expressions — C++11/14 mein kya karna padta tha?
5. `<charconv>` `std::stoi` se kyun fast (no alloc / locale / exceptions)?
6. `std::visit` dispatch — kaise implement hota (jump table)?

---

## Next
→ [`04-cpp20-features.md`](04-cpp20-features.md)
