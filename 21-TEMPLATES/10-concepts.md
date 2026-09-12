# 10 — Concepts (C++20)

## Prerequisites
- [`09-sfinae.md`](09-sfinae.md), [`08-type-traits-deep.md`](08-type-traits-deep.md)
- Folder 19 file 14 (ranges use concepts everywhere)

## Yeh topic abhi kyun
Concepts C++20 ka bada feature hai — **named, readable constraints** jo SFINAE ki
jagah lete. Ek concept ek `bool` hai types ke upar; usse aap templates ko
constrain karte ho, aur galat call pe **ek line ka error** milta (SFINAE ke page
bhar noise ki jagah). STL (`<ranges>`, `<iterator>`) ab poori tarah concepts pe
likhi hai.

`examples/06_concepts.cpp` sab dikhata hai.

---

## Defining a concept

```cpp
#include <concepts>

// 1. from other concepts / traits
template <class T>
concept Numeric = std::integral<T> || std::floating_point<T>;

// 2. with a requires-expression -- "these expressions must be valid"
template <class T>
concept Addable = requires (T a, T b) {
    a + b;                                    // a + b must compile
    { a += b } -> std::same_as<T&>;           // a += b must compile AND yield T&
    { a.size() } -> std::convertible_to<std::size_t>;   // compound requirement
    typename T::value_type;                   // T::value_type must name a type
    requires sizeof(T) <= 64;                 // nested requirement -- a bool constant expr
};
```

A `requires (params) { ... }` expression is `true` iff every requirement inside is
satisfied. It's checked, not executed — the body is never run.

Requirement kinds:
- **simple**: `a + b;` — the expression must be well-formed.
- **type**: `typename T::iterator;` — must name a valid type.
- **compound**: `{ expr } -> ConceptOnTheResult;` — expr valid *and* its type
  satisfies the concept.
- **nested**: `requires bool-constant-expr;` — an extra compile-time predicate.

---

## Using a concept — four syntaxes

```cpp
// (a) type-constraint in the template parameter list
template <Numeric T>
T twice(T x) { return x + x; }

// (b) trailing requires-clause (for compound conditions)
template <class T> requires Numeric<T> && (!std::same_as<T, bool>)
T thrice(T x) { return x + x + x; }

// (c) requires-clause after the parameter list
template <class T>
T quad(T x) requires Numeric<T> { return x * 4; }

// (d) constrained `auto` -- abbreviated function template
auto half(Numeric auto x) { return x / 2; }
void sink(std::ranges::range auto&& r);      // takes any range
```

All four mean the same "this template only participates in overload resolution
when the constraint holds".

---

## The payoff: readable errors

```cpp
twice(std::string("x"));
```

**SFINAE (`enable_if`) error:** "no matching function for call to `twice`" +
every rejected candidate + a nested "`enable_if_t<false, ...>` has no member
`type`" buried three levels deep.

**Concepts error:**
```
error: no matching function for call to 'twice(std::string)'
note: constraints not satisfied
note:   the required expression 'x + x' is invalid    (or: 'Numeric<std::string>' evaluated to false)
```

One or two lines pointing at *which* requirement failed.

---

## Subsumption — the more-constrained overload wins

If two candidates both match, the one whose constraints **subsume** (logically
imply) the other's is preferred:

```cpp
template <std::integral T>                          void p(T);   // (1)
template <std::integral T> requires (sizeof(T) >= 4) void p(T);  // (2) -- constraints of (2) imply (1)'s

p(short{});   // (1) -- sizeof(short) < 4, only (1) is viable
p(int{});     // (2) -- both viable, (2) is more constrained -> chosen
```

This replaces SFINAE's fragile "make the conditions mutually exclusive" dance
(`enable_if<A>` + `enable_if<!A>`). With concepts you can have overlapping
constraints and the compiler picks the tightest.

**Important:** subsumption works on the *structure* of atomic constraints, not on
their runtime truth. `requires (sizeof(T) >= 4)` subsumes nothing unless the
exact same atomic constraint appears elsewhere. Build concepts from named
sub-concepts so subsumption can see the relationships.

---

## The standard concepts you'll use

```cpp
#include <concepts>
std::same_as<T, U>              std::convertible_to<From, To>
std::integral<T>               std::floating_point<T>       std::signed_integral<T>
std::derived_from<D, B>
std::equality_comparable<T>    std::totally_ordered<T>      std::three_way_comparable<T>
std::default_initializable<T>  std::copyable<T>   std::movable<T>   std::regular<T>
std::invocable<F, Args...>     std::predicate<F, Args...>

#include <iterator>   // std::input_iterator, std::random_access_iterator, std::sentinel_for, ...
#include <ranges>     // std::ranges::range, std::ranges::sized_range, std::ranges::view, ...
```

`std::ranges::sort(v)` is constrained on `std::ranges::random_access_range<R> &&
std::sortable<iterator_t<R>, ...>` — pass a `std::list` and you get "constraint
`random_access_range` not satisfied", not a wall of iterator-arithmetic errors.

---

## Andar kya hota hai

- A concept is a compile-time predicate. `Numeric<int>` is a `constexpr bool`,
  usable in `static_assert`, `if constexpr`, and `requires` clauses.
- Constraint checking happens during overload resolution: unsatisfied constraint
  → candidate removed from the set (same *effect* as SFINAE, better ergonomics).
- **Constraint normalization**: the compiler breaks each `requires`-clause into a
  conjunction/disjunction of **atomic constraints**, then compares candidates by
  whether one set of atomics implies another (subsumption). Named concepts keep
  atomics identical across uses so implication is detectable.
- `requires`-expressions are checked in an unevaluated context — no code runs,
  `a + b` just has to type-check. `requires (T a, T b)` introduces `a`, `b` as
  never-constructed placeholders (like `declval`).
- Zero runtime cost — it's all resolved at compile time; the selected template is
  a normal function.

> **HFT relevance:** in modern low-latency C++, concepts are how you write a
> **single generic hot-path function** that *only* accepts the types it can
> handle fast — `void publish(TriviallyCopyable auto&& msg)`,
> `sum(std::ranges::contiguous_range auto&& r)` — with a one-line error if
> someone passes the wrong thing (a `std::list`, a type with a non-trivial move).
> They replace the `enable_if` soup that made template-heavy trading code
> unreadable and slow to compile. Subsumption lets you layer a "fast path" and a
> "general path" overload with overlapping constraints and trust the compiler to
> pick the tighter one. The STL you build on (`<ranges>`, `<iterator>`) is
> concept-constrained, so your generic algorithms compose with clear diagnostics.
> Still zero runtime cost — a concept is a compile-time gate, nothing more.

---

## Hands-on

```bash
./build.ps1 21-TEMPLATES/examples/06_concepts.cpp
```

Write: a `Hashable` concept (`std::hash<T>{}(t)` valid), a `Container` concept
(`begin`/`end`/`size`), and a `sum(Container auto&& c)` that requires the element
type be `Numeric`. Trigger a failed constraint and read the error. Then add a
more-constrained `sum` overload for `contiguous_range` and confirm subsumption
picks it for a `std::vector`.

---

## ⚠️ Traps

### Trap 1 — `requires requires`
```cpp
template <class T> requires requires (T a) { a.f(); }   // ✅ valid but confusing: requires-clause + requires-expression.
// Name it: `template <class T> concept HasF = requires (T a) { a.f(); };` then `requires HasF<T>`.
```

### Trap 2 — expecting subsumption from unrelated atomic constraints
```cpp
template <class T> requires (std::is_integral_v<T>)          void f(T);
template <class T> requires (std::is_integral_v<T> && ...)   void f(T);
// If the first atom isn't literally the SAME constraint expression, no subsumption -> ambiguous. Use named concepts.
```

### Trap 3 — a `requires`-expression that always passes
```cpp
template <class T> concept Ok = requires { true; };   // ⚠️ `true;` is always well-formed -> Ok<anything> is true. Test real expressions.
```

### Trap 4 — compound requirement return-type check confusion
```cpp
{ a.size() } -> std::size_t;   // ❌ RHS must be a CONCEPT, not a type. -> std::same_as<std::size_t> or std::convertible_to<std::size_t>
```

### Trap 5 — thinking concepts add runtime cost
```cpp
// A concept is a compile-time bool. The selected function is normal code -- no checks at runtime.
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Concepts are runtime type checks" | Compile-time predicates that gate overload resolution — zero runtime cost |
| "`{ expr } -> Type` checks the return type against a type" | RHS must be a **concept**: `-> std::same_as<Type>` / `std::convertible_to<Type>` |
| "Concepts and SFINAE do different things" | Same effect (remove a candidate); concepts add readability, subsumption, one-line errors |
| "Overlapping constraints are ambiguous" | With named concepts, subsumption picks the more-constrained overload |
| "`requires (T a)` constructs a `T`" | `a` is an unconstructed placeholder used only to form expressions to check |

---

## Exercises

1. **Write a concept:** `Stringy<T>` — `T` is convertible to `std::string_view`.

   <details><summary>Answer</summary>

   `template <class T> concept Stringy = std::convertible_to<T,
   std::string_view>;` — covers `std::string`, `const char*`, `std::string_view`,
   and user types with the conversion.
   </details>

2. **Compound requirement:** a `Counter<T>` concept — `T` has `++t` yielding
   `T&`, and `t.value()` convertible to `long long`.

   <details><summary>Answer</summary>

   `template <class T> concept Counter = requires (T t) { { ++t } ->
   std::same_as<T&>; { t.value() } -> std::convertible_to<long long>; };`
   </details>

3. **Subsumption:** you have `template <std::integral T> void h(T);` and want a
   second overload just for `T` where `sizeof(T) == 8`. Write it so the compiler
   prefers it for `long long` without ambiguity.

   <details><summary>Answer</summary>

   `template <class T> concept Int64 = std::integral<T> && sizeof(T) == 8;` then
   `template <Int64 T> void h(T);`. `Int64`'s atomics include `std::integral<T>`,
   so it subsumes the plain `std::integral` overload → chosen for `long long`,
   the plain one for `int`.
   </details>

4. **Constrain a range algorithm:** write `auto total(std::ranges::range auto&&
   r)` that additionally requires the element type be `std::integral`.

   <details><summary>Answer</summary>

   `template <std::ranges::range R> requires std::integral<std::ranges::range_
   value_t<R>> auto total(R&& r) { long long s = 0; for (auto x : r) s += x;
   return s; }` — a `std::vector<std::string>` now gives "constraint not
   satisfied".
   </details>

5. **SFINAE → concept:** convert `template <class T, std::enable_if_t<
   has_size_v<T>, int> = 0> void f(T&);` (from file 09's detection idiom).

   <details><summary>Answer</summary>

   `template <class T> concept Sized = requires (const T& t) { { t.size() } ->
   std::convertible_to<std::size_t>; };` then `template <Sized T> void f(T&);` —
   no `has_size` trait, no phantom parameter, readable error.
   </details>

---

## Interview questions

1. Concept kya hai — ek `bool` over types? Define karne ke tareeke?
2. `requires`-expression ki 4 requirement kinds?
3. Concept constraint fail hone pe error SFINAE se kaise behtar?
4. Subsumption — more-constrained overload kaise jeetta, named concepts kyun zaroori?
5. Compound requirement `{ expr } -> Concept` — RHS concept kyun, type kyun nahi?
6. Concepts ki runtime cost (zero) — kya replace karte SFINAE se?

---

## Next
→ [`11-crtp-deep.md`](11-crtp-deep.md)
