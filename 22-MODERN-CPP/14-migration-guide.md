# 14 — Legacy → modern: a migration guide

## Prerequisites
- Files 01–13 of this folder
- [`examples/08_legacy_to_modern.cpp`](examples/08_legacy_to_modern.cpp)

## Yeh topic abhi kyun
Bahut C++ code aaj bhi C++98/03 idioms mein likha hai. Ise modernize karna =
kam code, kam bug surface, aksar behtar performance. Yeh lesson ek **before/after
table** hai — har purana idiom aur uska modern replacement, `examples/08` ke
saath jo dono versions side-by-side chalata.

---

## Before / after

### Iteration
```cpp
// ❌ C++98
for (std::vector<Trade>::const_iterator it = v.begin(); it != v.end(); ++it)
    use(it->px);

// ✅ modern
for (const auto& t : v) use(t.px);                       // range-for + auto
for (auto& t : v) t.px *= 1.01;                          // by ref to modify
for (auto&& x : someView) ...;                           // forwarding ref for views/generators
```

### Loops that are really algorithms
```cpp
// ❌ manual accumulate / find / transform loops
double sum = 0; for (size_t i = 0; i < v.size(); ++i) if (v[i].sym == s) sum += v[i].px * v[i].qty;

// ✅
auto ns = v | std::views::filter([&](auto& t){ return t.sym == s; })
            | std::views::transform([](auto& t){ return t.px * t.qty; });
double sum = std::accumulate(ns.begin(), ns.end(), 0.0);
// or std::ranges::fold_left (C++23)
```

### Ownership
```cpp
// ❌ raw owning pointer + manual delete (leak on early return / exception)
Widget* w = new Widget(...);
... if (bad) { delete w; return; } ...
delete w;

// ✅
auto w = std::make_unique<Widget>(...);                  // freed on every path, exception-safe
```

### Nullness
```cpp
if (p == NULL) ...       // ❌ NULL is 0, breaks overloads
if (p == 0) ...          // ❌
if (!p) ...              // ✅ (or `p == nullptr`)
```

### Function objects
```cpp
// ❌ a functor struct just to pass a predicate
struct ByLen { bool operator()(const std::string& a, const std::string& b) const { return a.size() < b.size(); } };
std::sort(v.begin(), v.end(), ByLen());

// ✅
std::sort(v.begin(), v.end(), [](auto& a, auto& b){ return a.size() < b.size(); });
std::ranges::sort(v, {}, &std::string::size);            // projection -- even shorter
```

### Typedefs
```cpp
typedef std::map<std::string, std::vector<int> > Table;   // ❌ (and the `> >` space)
using Table = std::map<std::string, std::vector<int>>;    // ✅ alias, reads left-to-right, can be templated
```

### Enums
```cpp
enum Color { RED, GREEN };            // ❌ leaks names, converts to int
enum class Color { Red, Green };      // ✅ scoped, strong; static_cast<int> when you mean it
```

### Map lookup + insert
```cpp
// ❌
std::map<K,V>::iterator it = m.find(k);
if (it == m.end()) m.insert(std::make_pair(k, V()));
else it->second = newV;

// ✅
m.insert_or_assign(k, newV);                             // or m[k] = newV; or try_emplace(k, ...)
for (auto& [key, val] : m) ...;                          // structured bindings
```

### Passing strings
```cpp
void log(const std::string& s);   // ❌ forces the caller to have a std::string (allocation from a literal)
void log(std::string_view s);     // ✅ accepts std::string, literal, substring -- no copy
```

### Passing "a bunch of contiguous T"
```cpp
void f(const std::vector<int>& v);        // ❌ forces a vector
void f(const int* p, size_t n);           // ❌ two args, easy to mismatch
void f(std::span<const int> xs);          // ✅ vector / array / C array / sub-range, one arg, .size(), range-for
```

### Parsing numbers
```cpp
int n = atoi(s.c_str());                  // ❌ no error reporting, UB on overflow
int n = std::stoi(s);                     // ⚠️ throws, allocates, slow, locale
auto [p, ec] = std::from_chars(b, e, n);  // ✅ no alloc, no locale, explicit error, ~8x faster (folder 10)
```

### Comparisons
```cpp
// ❌ six hand-written operators (and get one subtly wrong)
bool operator<(const T&, const T&); bool operator==(...); ... bool operator>=(...);

// ✅
auto operator<=>(const T&) const = default;              // all six, consistent (file 11)
```

### Formatting
```cpp
printf("%s: %.2f\n", sym.c_str(), px);                   // ❌ UB on a mismatch
std::cout << sym << ": " << std::fixed << std::setprecision(2) << px << "\n";   // ❌ verbose, slow, sticky state
std::print("{}: {:.2f}\n", sym, px);                     // ✅ (C++23) type-safe, fast; std::format for C++20
```

### Compile-time constants
```cpp
#define MAX_ORDERS 4096                    // ❌ no type, no scope, textual
enum { MAX_ORDERS = 4096 };               // ❌ old workaround
inline constexpr int MAX_ORDERS = 4096;   // ✅ typed, scoped, debuggable
```

### `NULL`/`0`/casts
```cpp
(int)x                                    // ❌ C-cast: could be any of 4 casts, unsearchable
static_cast<int>(x)                        // ✅ explicit, greppable
reinterpret_cast<...> / const_cast<...>    // ✅ loud where you really mean it
```

---

## Strategy for an existing codebase

1. **Turn on warnings** — `-Wall -Wextra -Wshadow -Wconversion` and a modern
   standard (`-std=c++20`). Fix what it flags; a lot of modernization is just
   "the compiler now tells you".
2. **`clang-tidy` with `modernize-*`** — auto-fixes range-for, `auto`,
   `nullptr`, `override`, `using`, `make_unique`, `emplace`, default member init,
   `= default`. Run it, review the diff.
3. **Bottom-up** — leaf utilities first (string params → `string_view`, raw
   pointers → smart pointers / `span`), then callers.
4. **Don't rewrite for its own sake** — modernize a file when you're touching it
   anyway. A working, tested C++98 module that nobody edits can wait.
5. **Keep behaviour identical** — modernization is a refactor. Have the tests
   green before and after; `examples/08` runs both versions and they produce the
   same output.
6. **Measure the hot path** — most modern idioms are neutral-to-faster, but
   verify (`std::function` for a hot callback is a *regression* — folder 19 file
   16).

---

## Andar kya hota hai

- Most swaps are **zero-cost or a win**: `auto`/range-for compile to the same
  code as the explicit loop; `make_unique` inlines to `new` + ctor;
  `enum class`/`using`/`nullptr`/`static_cast` are compile-time; `<=>` is the same
  comparison written once.
- Real performance *improvements*: `string_view`/`span` params remove copies and
  allocations at call sites; `from_chars` is ~8× `stoi` (folder 10);
  `emplace_back`/`try_emplace` save a move; guaranteed copy elision (C++17) makes
  value returns free.
- The one common *regression*: replacing a templated callback or a raw function
  pointer with `std::function` on a hot path — indirect call + possible
  allocation (folder 19 file 16). Modernize the *style*, keep the *dispatch
  mechanism* the hot path needs.
- `clang-tidy modernize-*` transforms are AST-level and behaviour-preserving;
  they're safe to apply broadly and review as a diff.

> **HFT relevance:** modernizing a trading codebase is mostly upside — fewer
> raw-pointer lifetime bugs, `string_view`/`span` killing copy/alloc at
> boundaries, `from_chars` in parsers, `<=>` for key types, `constexpr`/`enum
> class` replacing macros, RAII replacing manual cleanup. Do it opportunistically
> (when you touch a file), warnings-first, `clang-tidy`-assisted, tests green
> before/after. The one guardrail: on the **measured hot path**, keep
> compile-time dispatch (templates / CRTP / `variant` — folder 21 file 16) and
> don't let a "cleaner" `std::function` or a `std::regex` sneak in. Everywhere
> else, modern C++ is smaller, safer code that compiles to the same or better
> machine code.

---

## Hands-on

```bash
./build.ps1 22-MODERN-CPP/examples/08_legacy_to_modern.cpp
```

`examples/08` has `namespace legacy` (C++98 idioms) and `namespace modern`
(C++17/20) computing the same results. Read both `run()`s and list every idiom
that changed. Then take one C++98-style file from an earlier folder's `examples/`
and modernize it; diff the assembly at `-O2` (should be equivalent or better).

---

## ⚠️ Traps

### Trap 1 — `std::function` for a hot callback during modernization
```cpp
// Replacing `void (*cb)(const Tick&)` with `std::function<void(const Tick&)>` on the tick path = a regression.
// Keep the function pointer / template it.
```

### Trap 2 — `string_view` / `span` params that outlive their data
```cpp
std::string_view sv = std::string("x");   // ⚠️ dangles. `string_view` params are for the call, not for storing
```

### Trap 3 — `auto` changing a type subtly
```cpp
auto x = map["k"];   // ⚠️ copies the value. `auto& x = map["k"];` for a reference
auto n = v.size();   // n is size_t -- fine, but `int n = ...` would warn/narrow
```

### Trap 4 — modernizing without tests
```cpp
// range-for + erase, or a projection that changes tie-breaking order, can silently alter behaviour. Tests green before & after
```

### Trap 5 — `= default` `<=>` on a type that shouldn't be member-wise comparable
```cpp
struct Cache { Data d; mutable Stats s; auto operator<=>(const Cache&) const = default; };
// ⚠️ now `s` (a cache/derived field) participates in comparison. Write a custom <=> over `d` only
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Modernizing risks performance" | Almost all swaps are neutral-to-faster; the exception is `std::function`/`std::regex` on a hot path |
| "You should rewrite everything at once" | Opportunistic, file-by-file, tests green before/after; `clang-tidy` assists |
| "`auto` everywhere is always safe" | It strips cv/ref; `auto&` / `const auto&` when you need a reference or const |
| "`string_view`/`span` are drop-in for `const string&`/`const vector&`" | As **parameters** yes; never store them past the call |
| "`= default` `<=>` is always the right comparison" | Only if member-wise lexicographic is the actual rule — else custom `<=>` |

---

## Exercises

1. **Modernize:** `for (std::map<std::string,int>::iterator it = m.begin(); it !=
   m.end(); ++it) { it->second *= 2; }`

   <details><summary>Answer</summary>

   `for (auto& [key, val] : m) val *= 2;` — range-for + structured binding, by
   reference to modify.
   </details>

2. **Ownership:** `Foo* f = new Foo(); setup(f); if (!ok) { delete f; return; }
   registry.add(f);` — modernize, keeping "registry takes ownership".

   <details><summary>Answer</summary>

   `auto f = std::make_unique<Foo>(); setup(f.get()); if (!ok) return;   // f
   auto-freed`. Then `registry.add(std::move(f));` where `add` takes
   `std::unique_ptr<Foo>` (sink parameter — folder 17 file 08).
   </details>

3. **Params:** modernize `double sum(const std::vector<double>& v)` and `void
   log(const std::string& s)` for wider, cheaper call sites.

   <details><summary>Answer</summary>

   `double sum(std::span<const double> v)` (accepts vector/array/C array/slice,
   no copy) and `void log(std::string_view s)` (accepts string/literal/substr, no
   allocation from a literal).
   </details>

4. **Comparison:** `struct Ver { int a, b, c; };` has hand-written `operator<`
   and `operator==` only (so `>` doesn't compile). Modernize.

   <details><summary>Answer</summary>

   `auto operator<=>(const Ver&) const = default;` — replaces both, generates all
   six consistently, and now `Ver` works in `std::set`, `std::sort`,
   `std::lower_bound` with no missing operators.
   </details>

5. **Spot the regression:** a refactor changes `template <class F> void
   for_each_tick(span<const Tick>, F&&)` to `void for_each_tick(span<const Tick>,
   std::function<void(const Tick&)>)`. Why is that bad on the hot path?

   <details><summary>Answer</summary>

   The templated version inlines the callback and vectorizes the loop.
   `std::function` type-erases it → an indirect, non-inlined call per tick + a
   possible `operator new` when the closure is assigned → a real per-tick
   regression (folder 19 file 16). Keep the template.
   </details>

---

## Interview questions

1. C++98 iterator loop → modern — 2 tareeke (range-for, algorithm/ranges)?
2. Raw owning pointer → kya (smart pointer), exception safety kaise milti?
3. `const std::string&` / `const std::vector&` params → `string_view` / `span` — kya milta, kya risk?
4. 6 hand-written comparison operators → `= default` `<=>` — kab safe?
5. Modernization ki strategy (warnings, clang-tidy, opportunistic, tests)?
6. Modernization mein kaunsa change hot path pe regression ho sakta?

---

## Next
→ [`15-exercises.md`](15-exercises.md)
