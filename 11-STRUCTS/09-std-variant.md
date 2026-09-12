# 09 — `std::variant`

## Prerequisites
- [`08-unions.md`](08-unions.md)
- `08-FUNCTIONS/08-function-overloading.md`, `06-CONDITIONS/04-switch-statement.md`

## Yeh topic abhi kyun
`std::variant<A, B, C>` = "in mein se **exactly ek**" — ek **type-safe tagged
union**. Active type khud track karta hai, sahi destructor chalata hai, galat
access pe `throw` karta hai, aur `std::string` jaise non-trivial types safely
handle karta hai. Raw union + manual tag ka safe replacement.

---

## Basic

```cpp
#include <variant>

std::variant<int, double, std::string> v;   // default: holds first alternative (int{} = 0)

v = 42;                                       // now holds int
v = 3.14;                                     // now holds double (old int destroyed)
v = std::string("hello");                     // now holds std::string

v.index()                                     // 2  (which alternative -- 0-based)
std::holds_alternative<std::string>(v)        // true
```

`sizeof(variant)` ≈ largest alternative + a small discriminant + alignment.

---

## Access — `get`, `get_if`, `visit`

```cpp
// std::get<T> -- throws std::bad_variant_access if not holding T
try {
    int i = std::get<int>(v);
} catch (const std::bad_variant_access&) { /* v holds something else */ }

// std::get_if<T> -- returns T* or nullptr (no throw) -- prefer this for checks
if (const std::string* s = std::get_if<std::string>(&v)) {
    use(*s);
}

// std::visit -- dispatch on the active alternative (exhaustive, no throw)
std::visit([](const auto& value) { std::cout << value; }, v);
```

### `std::visit` with a handler set

```cpp
struct Handler {
    void operator()(int i)                const { /* ... */ }
    void operator()(double d)             const { /* ... */ }
    void operator()(const std::string& s) const { /* ... */ }
};
std::visit(Handler{}, v);

// or the "overloaded" idiom (inline lambdas):
template <class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;   // deduction guide (pre-C++20)

std::visit(overloaded{
    [](int i)                { /* ... */ },
    [](double d)             { /* ... */ },
    [](const std::string& s) { /* ... */ },
}, v);
```

⚠️ `std::visit` is **exhaustive** — if you miss an alternative, it won't compile
(unlike a `switch` with a missing `case`).

---

## `std::variant` vs raw union vs virtual (polymorphism)

| | `std::variant` | raw `union` + tag | `virtual` (base class ptr) |
|---|---|---|---|
| Type-safe | ✅ | ❌ (manual) | ✅ |
| Closed set (all types known) | ✅ | ✅ | ❌ (open — any subclass) |
| Non-trivial members | ✅ (handled) | ❌ (manual lifetime) | ✅ |
| Storage | inline (no heap) | inline | heap (usually) + vptr |
| Dispatch cost | branch on index (inlinable) | your `switch` | indirect call (vtable) |
| Add a new type | edit the `variant` + all `visit`s (compiler flags misses) | edit everything | just add a subclass |

**`std::variant` for a fixed, known set of alternatives** — it's the value-type,
allocation-free, cache-friendly choice. **`virtual`** when the set is open /
extensible (folder 16).

---

## Common uses

```cpp
// A parsed event -- one of a few message types
using Event = std::variant<Quote, Trade, Reject>;
std::vector<Event> stream;
for (const Event& e : stream) std::visit(EventPrinter{}, e);

// "value or error" -- though std::expected (C++23) / std::optional are usually better
std::variant<Result, ErrorCode> compute();

// state machine -- each state is a struct
using State = std::variant<Idle, Connecting, Connected, Failed>;
```

---

## Gotchas

```cpp
// valueless_by_exception -- if an assignment throws mid-change, variant can be "empty"
if (v.valueless_by_exception()) { /* rare -- only if a move/copy threw */ }

// duplicate alternatives need the index, not the type
std::variant<int, int> w;   // std::get<int>(w) -- ambiguous. std::get<0>(w)

// v = {} does NOT reset -- assigns from an empty init-list (may not compile)
v = 0;   // to "reset", assign a concrete value / std::monostate
```

`std::monostate` — an empty alternative for "no value yet":
```cpp
std::variant<std::monostate, Quote, Trade> v;   // default: monostate (valid "empty")
```

---

## Andar kya hota hai

- `std::variant` = an aligned byte buffer sized for the largest alternative + an
  `index` (usually 1 byte, sometimes folded). No heap.
- Assignment → destroy the current alternative (if non-trivial), construct the new
  one in place, update `index`.
- `std::get_if<T>` → `index == index_of<T> ? reinterpret the buffer as T* : nullptr`.
- `std::visit` → effectively a jump table on `index` to the right handler
  instantiation; `-O2` inlines small visitors → a `switch`-like branch, no
  indirect call.
- `std::bad_variant_access` is thrown by `std::get` on mismatch (a real
  `throw` — has a cost on that path).

> **HFT relevance:** `std::variant` is the go-to for closed event/message sets in
> HFT app logic: `variant<Quote, Trade, Reject, ...>` over a feed, dispatched via
> `std::visit` (inlined branch, no vtable, no heap — beats `virtual` for a fixed
> set). It's cache-friendly (inline storage) and the exhaustiveness check catches
> "forgot to handle the new message type" at compile time. On the very hottest
> decode path, a hand-rolled tagged union of PODs (no `std::string`) + a raw
> `switch` shaves the last bit. `virtual` is reserved for genuinely open
> hierarchies. Folders 16, 36, 38.

---

## Hands-on

`examples/06_variant.cpp` — `Event = variant<Quote, Trade, Reject>`, `index()`,
`get`/`get_if`, `visit`, stream processing:

```bash
./build.ps1 11-STRUCTS/examples/06_variant.cpp
```

---

## ⚠️ Traps

### Trap 1 — `std::get<T>` without checking
```cpp
int i = std::get<int>(v);   // ⚠️ throws if v isn't int. get_if, or holds_alternative first
```

### Trap 2 — non-exhaustive `visit`
```cpp
std::visit(overloaded{ [](int){}, [](double){} }, v);   // ❌ won't compile if v can hold std::string
```
(This is a feature — but surprising if you expected a "default".)

### Trap 3 — duplicate alternatives + `get<T>`
```cpp
std::variant<int, int> v;  std::get<int>(v);   // ❌ ambiguous. std::get<0>(v)
```

### Trap 4 — expecting `variant` to be "empty" by default
```cpp
std::variant<Quote, Trade> v;   // holds a default-constructed Quote, NOT empty. Use std::monostate
```

### Trap 5 — `variant` for an open/extensible set
```cpp
using Shape = std::variant<Circle, Square>;   // ⚠️ adding Triangle = edit every visit. virtual if open
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`variant` heap-allocates" | Inline storage — largest alternative + tag |
| "`std::get<T>` returns null on mismatch" | Throws — `std::get_if` returns null |
| "`visit` has a default case" | Exhaustive — missing alternative = compile error |
| "`variant` default-constructs empty" | Holds the first alternative; `std::monostate` for empty |
| "`variant` replaces `virtual` always" | Only for a closed set; `virtual` for open hierarchies |

---

## Exercises

1. **Basics:** `std::variant<int, std::string> v;` — assign int, then string.
   `v.index()` each time. `holds_alternative` checks.

2. **`get_if` dispatch:** `std::vector<std::variant<int, double>>` — sum the ints
   and the doubles separately using `get_if`.

3. **`visit` + overloaded:** the `overloaded` idiom to print each alternative of
   `variant<Quote, Trade, Reject>` differently.

4. **Exhaustiveness:** remove one handler from your `visit` — what's the compile
   error? Add a `std::monostate` alternative — now what?

5. **State machine:** `variant<Idle, Running, Done>` with a `tick(State&)` that
   `visit`s and transitions. Drive it.

6. **variant vs virtual:** implement "area of a shape" for `{Circle, Square}` both
   ways (`std::variant` + `visit`, and a base class + `virtual`). Compare
   `sizeof`, and the assembly of the dispatch (`-O2 -S`).

---

## Interview questions

1. `std::variant` vs raw union — 3 safety differences?
2. `std::get` vs `std::get_if` — error behaviour?
3. `std::visit` — exhaustiveness? `switch` se fark?
4. `std::variant` vs `virtual` polymorphism — closed vs open set, cost?
5. `std::monostate` kya hai, kab chahiye?
6. `std::variant` storage — heap ya inline?

---

## Next
→ [`10-enums.md`](10-enums.md)
