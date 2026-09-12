# 15 — Utility types: `pair`, `tuple`, `optional`, `variant`, `any`, `expected`

## Prerequisites
- [`14-ranges.md`](14-ranges.md), folder 11 (structs), folder 18 (move)
- `if constexpr`, structured bindings (folder 22 preview)

## Yeh topic abhi kyun
Ye "vocabulary types" hain — function signatures mein bar-bar aate hain:
"maybe a value" (`optional`), "one of these types" (`variant`), "value or error"
(`expected`), "a few values together" (`pair`/`tuple`). Inhe fluently use karna
modern C++ padhne-likhne ke liye zaroori.

`examples/06_optional_variant.cpp` mein `optional` + `variant` + `visit` live hain.

---

## `std::pair<A, B>` and `std::tuple<Ts...>`

```cpp
#include <utility>   // pair
#include <tuple>     // tuple

std::pair<int, std::string> p{1, "one"};
p.first; p.second;
auto p2 = std::make_pair(2, "two");           // deduces types (or CTAD: std::pair{2, "two"})

std::tuple<int, double, std::string> t{1, 2.5, "x"};
std::get<0>(t);                                // by index
std::get<std::string>(t);                      // by type (only if unique in the tuple)
std::tuple_size_v<decltype(t)>;                // 3

// structured bindings -- the good way to unpack:
auto [id, name] = p;
auto& [a, b, c] = t;                           // by reference -- modify in place

std::tuple<int&, int&> refs{x, y};
std::tie(x, y) = std::make_tuple(10, 20);      // assign through -- older idiom
auto [q, r] = std::div(17, 5);                 // many std funcs return small structs -> bind them
```

- `pair` / `tuple` are just structs with generated members — no heap, `sizeof` =
  sum of members + padding.
- Use them for **ad-hoc** grouping (a function returning two things, a map's
  `value_type` which *is* `pair<const K, V>`). For anything with meaning, a
  **named struct** is clearer: `struct Quote { double px; int qty; };` beats
  `std::pair<double,int>` — `.px` reads better than `.first`.
- `std::apply(f, tuple)` — call `f` with the tuple's elements as arguments.
- `std::tuple_cat(t1, t2)` — concatenate.

## `std::optional<T>` — a value that might be absent

```cpp
#include <optional>

std::optional<int> parsePositive(std::string_view s);   // returns nullopt on failure

std::optional<int> o = 42;
std::optional<int> e = std::nullopt;              // or just {}

if (o) use(*o);                                   // contextual bool + deref
if (o.has_value()) use(o.value());               // value() THROWS std::bad_optional_access if empty
int x = o.value_or(-1);                           // default if empty -- no throw
o.reset();                                        // now empty
o.emplace(7);                                     // construct a value in place

// C++23 monadic:
std::optional<int> r = o.and_then(parseNext).transform([](int v){ return v * 2; }).or_else(fallback);
```

- **No heap.** `optional<T>` is `T` + a `bool`, inline. `sizeof(optional<int>)` is
  8 (4 + 1 + padding). The `T` is only constructed when engaged.
- Use for: "not found", "not yet set", "optional parameter", "may fail with no
  error detail". If you need *why* it failed → `std::expected`.
- `optional<T&>` is **not** allowed pre-C++26 — use `T*` for an optional
  reference.

## `std::variant<Ts...>` — a type-safe tagged union

```cpp
#include <variant>

using Value = std::variant<std::nullptr_t, bool, double, std::string>;

Value v = 3.14;                                  // holds double
v.index();                                       // 2
std::holds_alternative<double>(v);               // true
std::get<double>(v);                             // 3.14  -- throws std::bad_variant_access if wrong
double* p = std::get_if<double>(&v);             // nullptr if not a double -- no throw
v = std::string{"hi"};                           // now holds string (old double destroyed)

// exhaustive dispatch with std::visit:
std::string describe(const Value& val) {
    return std::visit([](const auto& x) -> std::string {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T, std::nullptr_t>) return "null";
        else if constexpr (std::is_same_v<T, bool>)      return x ? "true" : "false";
        else if constexpr (std::is_same_v<T, double>)    return std::to_string(x);
        else                                             return x;              // std::string
    }, val);
}
```

- **No heap** (unless an alternative allocates, like `std::string`). Size = size
  of the largest alternative + a small tag, aligned to the strictest alternative.
- `std::visit` is **exhaustive** — if you add an alternative and a generic lambda
  doesn't handle it, you get a compile error (with the "overload set" pattern) or
  it falls through (with `if constexpr` — add an `else`).
- Can be **valueless_by_exception** if an alternative's move/copy throws mid-
  assignment (rare). `v.valueless_by_exception()`.
- Use for: a small **closed** set of types (a config value, a protocol message
  kind, an AST node). For an **open** set → `std::any` or virtual dispatch.

## `std::any` — holds anything (type-erased)

```cpp
#include <any>
std::any a = 42;
a = std::string{"hi"};
int* p = std::any_cast<int>(&a);                 // nullptr (a holds string now)
std::string s = std::any_cast<std::string>(a);   // throws std::bad_any_cast if wrong

if (a.has_value()) use(a.type().name());
```

- **May allocate** (small-object optimization for tiny/trivial types; heap
  otherwise).
- No `visit` — you must `any_cast` to a concrete type you name. You need to
  *know* the type to get it out.
- Rarely the right tool. `variant` (closed set) or an interface (open set) is
  almost always better. `any` fits truly heterogeneous plugin/property-bag
  scenarios.

## `std::expected<T, E>` (C++23) — value or error

```cpp
#include <expected>   // needs -std=c++23  (this repo is c++20 -> discussed, not compiled)

std::expected<int, ParseError> parse(std::string_view s);

auto r = parse(input);
if (r) use(*r);                                  // has value
else   log(r.error());                           // has error E

int x = r.value_or(0);
auto chained = parse(a).and_then(parseB).transform(scale).transform_error(annotate);
```

- **No heap** — like a `variant<T, E>` with an ergonomic API. `sizeof` ≈
  `max(sizeof T, sizeof E)` + tag.
- The modern answer to "how do I return an error without exceptions and without
  out-params": `expected<Value, Error>`. Composes with `and_then` / `transform`.
- Pre-C++23: `tl::expected`, `absl::StatusOr`, `boost::outcome`, or
  `std::variant<T, E>` + a helper, or `std::optional<T>` if you don't need `E`.

---

## Picking between them

| Situation | Type |
|---|---|
| Return 2–3 unrelated values, no names needed | `std::pair` / `std::tuple` (prefer a named struct if it recurs) |
| A value that may be absent, no reason needed | `std::optional<T>` |
| A value **or** an error you want to inspect | `std::expected<T, E>` (C++23) / `variant<T,E>` / `StatusOr` |
| One of a fixed, known set of types | `std::variant<...>` + `std::visit` |
| One of an open / unknown set of types | an interface (virtual) — or `std::any` as a last resort |

All of `optional` / `variant` / `expected` are **stack** types — no allocation,
cache-friendly, `constexpr`-friendly. That's the point vs a `unique_ptr<Base>`
hierarchy.

---

## Andar kya hota hai

- `optional<T>` = `struct { union { char none; T val; }; bool engaged; };` —
  placement-new `T` into the storage on assignment, call `~T()` on `reset`.
  Trivial `T` → the whole thing is trivially copyable.
- `variant<Ts...>` = aligned storage sized for the largest `T` + a `size_t`-ish
  index. `std::get` checks the index and `reinterpret_cast`s. `std::visit` builds
  (at compile time) a **jump table** of function pointers, one per alternative,
  indexed by `.index()` — O(1) dispatch, not a chain of `if`s.
- `any` = a pointer to a heap block (or an inline buffer) plus a pointer to a
  manager function that knows how to copy/destroy/type-id the stored object.
  `any_cast` compares `typeid`.
- `expected<T,E>` = like `optional` but the "empty" arm stores an `E` instead of
  nothing.

> **HFT relevance:** `optional` / `variant` / `expected` matter because they're
> **allocation-free and contiguous** — a `std::variant<Add, Cancel, Trade>` for a
> parsed market-data message sits in a `std::vector` with no indirection, and
> `std::visit`'s jump-table dispatch is one indirect call (predictable, often
> better than a `virtual` through a scattered vtable). `expected<T, Err>` (or a
> hand-rolled equivalent on C++20) is the standard "no exceptions on the hot
> path" error channel. `std::any` is avoided — it can allocate and needs a
> `typeid` compare. Prefer named structs over `tuple`/`pair` in interfaces so
> the code reads.

---

## Hands-on

```bash
./build.ps1 19-STL/examples/06_optional_variant.cpp
```

`std::optional<int> parsePositive`, a JSON-ish `std::variant<nullptr_t, bool,
double, std::string>` with a generic-lambda `std::visit`, `holds_alternative` /
`get` / `get_if`, and `sizeof` of each. Add an overload-set visitor
(`struct Overloaded : Ts... { using Ts::operator()...; };`) as an alternative to
`if constexpr`.

---

## ⚠️ Traps

### Trap 1 — `*optional` / `get<T>(variant)` without checking
```cpp
int x = *o;                 // ⚠️ UB if o is empty. if (o) / o.value_or(...)
auto d = std::get<double>(v);   // ⚠️ throws if v isn't a double. std::get_if / holds_alternative first
```

### Trap 2 — `optional` of a reference
```cpp
std::optional<int&> o;   // ❌ ill-formed (pre-C++26). Use int* for an optional reference
```

### Trap 3 — non-exhaustive `std::visit` with `if constexpr`
```cpp
std::visit([](auto& x){ using T = std::decay_t<decltype(x)>;
    if constexpr (std::is_same_v<T,int>) ...; }, v);   // ⚠️ other alternatives -> function returns nothing / wrong. Add an else
```

### Trap 4 — `tuple`/`pair` where a struct belongs
```cpp
std::pair<double,double> getBidAsk();   // ⚠️ .first/.second at call sites -- which is which? struct BidAsk { double bid, ask; };
```

### Trap 5 — assuming `std::any` never allocates
```cpp
std::any a = BigStruct{};   // ⚠️ heap allocation (exceeds the SBO). any_cast<BigStruct>(a) copies it out
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`std::optional<T>` allocates" | Never — it's `T` + a `bool`, inline |
| "`.value()` and `*opt` behave the same on empty" | `.value()` throws `bad_optional_access`; `*opt` is UB |
| "`std::variant` needs the heap" | No — inline storage sized for the biggest alternative + a tag |
| "`std::visit` is a runtime `if` chain" | Compile-time jump table — O(1) indirect call |
| "`std::any` ≈ `std::variant`" | `any` is open-set + type-erased + may allocate; `variant` is closed-set + inline |

---

## Exercises

1. **optional chain:** `std::optional<Config> loadConfig(); std::optional<int>
   getPort(const Config&);` — get the port or `8080`, no exceptions, one
   expression (C++23 monadic ok, else nested `if`).

   <details><summary>Answer</summary>

   C++23: `int port = loadConfig().and_then(getPort).value_or(8080);`. C++20:
   `int port = 8080; if (auto c = loadConfig()) if (auto p = getPort(*c)) port =
   *p;`
   </details>

2. **variant size:** `std::variant<char, double, std::array<int,4>>` — what's its
   `sizeof` roughly, and why?

   <details><summary>Answer</summary>

   ≈ `sizeof(std::array<int,4>)` (16) + a tag, rounded to the alignment of the
   strictest member (8 for `double`) → 24. It reserves room for the largest
   alternative at all times.
   </details>

3. **visit exhaustiveness:** show how the "overload set" visitor makes adding a
   new `variant` alternative a **compile error** until you handle it.

   <details><summary>Answer</summary>

   `template<class...Ts> struct Overloaded : Ts... { using Ts::operator()...; };`
   then `std::visit(Overloaded{ [](int){...}, [](std::string const&){...} }, v);`.
   If `v` gains a `double` alternative, there's no matching `operator()` →
   `std::visit` fails to compile. (An `[](auto&){}` catch-all would silence it —
   omit it for exhaustiveness.)
   </details>

4. **struct vs pair:** rewrite `std::tuple<std::string,double,std::uint32_t>
   makeQuote()` as something a caller can read, and show the call site both ways.

   <details><summary>Answer</summary>

   `struct Quote { std::string sym; double px; std::uint32_t qty; }; Quote
   makeQuote();`. Call: `auto q = makeQuote(); use(q.sym, q.px);` vs the tuple's
   `auto [s, p, n] = makeQuote();` (positional — easy to swap `p`/`n` silently).
   </details>

5. **expected on C++20:** you can't `#include <expected>`. Give a `Result<T>`
   alias and a usage pattern that provides value-or-error without exceptions.

   <details><summary>Answer</summary>

   `template<class T> using Result = std::variant<T, Error>;` with helpers `bool
   ok(const Result<T>&)` = `std::holds_alternative<T>`, `T& val(...)`,`Error&
   err(...)`. Or adopt `tl::expected`. Callers `if (ok(r)) use(val(r)); else
   handle(err(r));`.
   </details>

---

## Interview questions

1. `std::optional<T>` heap use karta? Layout kya, `sizeof`?
2. `.value()` vs `operator*` empty optional pe — kya fark?
3. `std::variant` andar — storage, tag, `std::visit` dispatch kaise?
4. `std::variant` vs `std::any` — closed vs open set, allocation?
5. `std::expected<T,E>` kya solve karta (vs exceptions, vs out-param)?
6. `std::tuple`/`std::pair` ke bajaye named struct kab, kyun?

---

## Next
→ [`16-functional.md`](16-functional.md)
