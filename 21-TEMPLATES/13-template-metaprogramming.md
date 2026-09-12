# 13 — Template metaprogramming (TMP)

## Prerequisites
- [`06-variadic-templates.md`](06-variadic-templates.md), [`08-type-traits-deep.md`](08-type-traits-deep.md)
- [`07-if-constexpr.md`](07-if-constexpr.md), folder 08 (`constexpr`)

## Yeh topic abhi kyun
TMP = **compile time par computation** — types aur values pe. Kabhi yeh recursive
template instantiation se karte the (slow, cryptic). Ab **`constexpr` functions**
+ **`if constexpr`** + **fold expressions** se zyaadaatar TMP normal code jaisa
likha jaata. Yeh lesson: kya possible hai, purana vs naya style, aur kab TMP
worth hai (kam — HFT compile-time tables ke liye).

---

## What "compute at compile time" means

Two domains:
- **Value computation** — produce a number/array/string the compiler bakes in
  (`constexpr` functions, `consteval`).
- **Type computation** — produce a *type* from other types (traits,
  `std::conditional_t`, type lists).

The payoff: zero runtime cost (the answer is a constant / a chosen type), and the
compiler can catch errors (out-of-range table index) before the program runs.

---

## Value TMP — then vs now

### Old: recursive templates (avoid)
```cpp
template <unsigned N> struct Fact { static constexpr unsigned value = N * Fact<N - 1>::value; };
template <>           struct Fact<0> { static constexpr unsigned value = 1; };
Fact<5>::value;   // 120 -- computed by instantiating Fact<5>, Fact<4>, ... Fact<0>
```
Every step is a separate class instantiation → slow compiles, unreadable errors,
a specialization for the base case.

### New: `constexpr` functions (do this)
```cpp
constexpr unsigned fact(unsigned n) {
    unsigned r = 1;
    for (unsigned i = 2; i <= n; ++i) r *= i;   // loops, locals, mutation -- all fine in constexpr since C++14
    return r;
}
static_assert(fact(5) == 120);
constexpr auto f10 = fact(10);                   // computed at compile time

// consteval -> MUST run at compile time (C++20)
consteval unsigned factC(unsigned n) { /* same body */ return /*...*/ 1; }
```
Reads like normal code, compiles fast, real error messages.

### `constexpr` containers (C++20)
```cpp
constexpr auto make_sieve() {
    std::array<bool, 100> s{};
    s.fill(true); s[0] = s[1] = false;
    for (std::size_t i = 2; i * i < s.size(); ++i)
        if (s[i]) for (std::size_t j = i * i; j < s.size(); j += i) s[j] = false;
    return s;
}
constexpr auto primes = make_sieve();           // a 100-bool lookup table, zero runtime work to build

// std::vector / std::string are constexpr-constructible in C++20 (must not escape constexpr eval)
constexpr int sumFirstN(int n) {
    std::vector<int> v(n); std::iota(v.begin(), v.end(), 1);
    return std::accumulate(v.begin(), v.end(), 0);
}
```

---

## Type TMP

### Type lists
```cpp
template <class... Ts> struct TypeList {};

// length
template <class L> struct Length;
template <class... Ts> struct Length<TypeList<Ts...>>
    : std::integral_constant<std::size_t, sizeof...(Ts)> {};

// index
template <std::size_t I, class L> struct At;
template <std::size_t I, class Head, class... Tail>
struct At<I, TypeList<Head, Tail...>> : At<I - 1, TypeList<Tail...>> {};
template <class Head, class... Tail>
struct At<0, TypeList<Head, Tail...>> { using type = Head; };

// contains
template <class T, class L> struct Contains;
template <class T, class... Ts>
struct Contains<T, TypeList<Ts...>>
    : std::bool_constant<(std::is_same_v<T, Ts> || ...)> {};   // fold over ||
```

`std::tuple<Ts...>` + `std::tuple_element_t` / `std::tuple_size_v` are the
standard type list; `std::variant` carries one too.

### `std::index_sequence` — iterate a pack by index
```cpp
template <class Tuple, std::size_t... Is>
void printTuple(const Tuple& t, std::index_sequence<Is...>) {
    ((std::cout << std::get<Is>(t) << ' '), ...);              // fold over the indices
}
template <class... Ts>
void printTuple(const std::tuple<Ts...>& t) {
    printTuple(t, std::index_sequence_for<Ts...>{});            // makes <0, 1, ..., sizeof...(Ts)-1>
}
```
`std::make_index_sequence<N>` / `std::index_sequence_for<Ts...>` generate the
compile-time index pack; you fold over `Is...` to touch each element.

---

## When TMP is worth it (and when it isn't)

**Worth it:**
- **Compile-time lookup tables** — CRC tables, sine tables, gamma/error-code
  maps, a perfect hash of a fixed symbol set. Built once by the compiler, `.rodata`
  at runtime, zero init cost.
- **Compile-time validation** — `static_assert` that a wire struct is exactly 32
  bytes, that an enum covers all cases, that a config satisfies invariants.
- **Generating boilerplate** — reflection-lite (list a struct's fields), a
  serializer that iterates them, a visitor.
- **Dimensional analysis / units** — `std::chrono` is TMP (`duration<Rep,
  Period>` with `std::ratio` arithmetic).

**Not worth it:**
- Anything a `constexpr` function does more clearly.
- Deep recursive-template computation "because it's clever" — it tanks compile
  time and nobody can maintain it.
- Runtime data — TMP only knows what the compiler knows.

**Rule:** reach for `constexpr` + `if constexpr` + folds first. Drop to
recursive-template type manipulation only for genuine type-level work
(type lists, trait plumbing), and keep it small.

---

## Andar kya hota hai

- A `constexpr` function is compiled to *normal* runtime code **and** can be
  evaluated by the compiler's constant-expression interpreter when the arguments
  are constants. `constexpr auto x = fact(10);` runs the interpreter at compile
  time; `fact(runtimeN)` runs the emitted machine code.
- Recursive template instantiation makes the compiler build (and cache) a class
  per step — `Fact<5>` forces `Fact<4>`, …, `Fact<0>`. For deep recursion this
  is `O(N)` instantiations with real memory + time cost, and the recursion depth
  hits the compiler's instantiation limit (~900 by default) long before a
  `constexpr` loop would.
- `std::make_index_sequence<N>` is itself implemented with a `O(log N)`
  template recursion (a doubling trick) so it doesn't blow the depth limit.
- `consteval` functions leave **no runtime code** (unless address-taken) — they
  are pure compile-time.
- A `constexpr` table (`std::array`) ends up in `.rodata` — the program starts
  with it already populated, no static-initializer runs.

> **HFT relevance:** the useful TMP in trading systems is **compile-time table
> generation** and **compile-time validation**. A CRC/checksum table, a
> tick-size-per-instrument map, a fixed-symbol perfect hash, an FSM transition
> table — built by a `constexpr` function into a `std::array`, so at runtime it's
> a zero-cost `.rodata` lookup with no startup initialization (which matters for
> deterministic warm-up). `static_assert` locks wire-struct sizes/offsets and
> enum coverage at build time so a protocol mismatch is a compile error, not a
> production incident. `std::chrono`'s `duration`/`ratio` machinery gives
> unit-safe time arithmetic with no runtime cost (folder 19 file 17). Deep
> recursive TMP for its own sake is avoided — it wrecks build times, and build
> time is developer latency.

---

## Hands-on

```bash
./build.ps1 21-TEMPLATES/examples/04_if_constexpr.cpp   # compile-time factorial
```

Write: a `constexpr` function that builds a 256-entry CRC-8 table into a
`std::array`, and `static_assert` a known value; a `TypeList` with `Length`,
`At<I>`, `Contains<T>`; a `printTuple` using `index_sequence`.

---

## ⚠️ Traps

### Trap 1 — recursive-template computation where a `constexpr` loop works
```cpp
template <int N> struct Sum { static constexpr int v = N + Sum<N-1>::v; };   // ⚠️ N instantiations, hits the depth limit.
constexpr int sum(int n) { int s = 0; for (int i = 1; i <= n; ++i) s += i; return s; }   // ✅
```

### Trap 2 — expecting `constexpr` to *force* compile-time evaluation
```cpp
constexpr int f(int);
int r = f(x);   // may run at RUNTIME if x isn't constant. Use `consteval`, or assign to a `constexpr` variable
```

### Trap 3 — a `constexpr` container escaping the evaluation
```cpp
constexpr std::vector<int> v{1,2,3};   // ❌ (pre-C++23) -- the allocation can't persist past constexpr eval. Use std::array
```

### Trap 4 — instantiation depth limit
```cpp
Fib<1000>::value;   // ⚠️ ~1000 nested instantiations -> "template instantiation depth exceeds maximum". constexpr loop instead
```

### Trap 5 — TMP for runtime data
```cpp
// You can't metaprogram over values only known at runtime. TMP sees compile-time constants and types.
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "TMP = recursive templates" | Modern TMP is mostly `constexpr` functions + `if constexpr` + folds |
| "`constexpr` means it runs at compile time" | It *can* — force it with `consteval` or by assigning to a `constexpr` variable |
| "Recursive-template math is fast to compile" | `O(N)` instantiations, hits the depth limit; a `constexpr` loop is faster and clearer |
| "You can build a `constexpr std::vector` table" | Use `std::array` — dynamic allocation can't escape constexpr eval (pre-C++23) |
| "TMP can process runtime input" | Only compile-time constants and types |

---

## Exercises

1. **Rewrite:** convert `template <int N> struct Pow2 { static constexpr long v =
   2 * Pow2<N-1>::v; }; template <> struct Pow2<0> { static constexpr long v = 1;
   };` to a `constexpr` function.

   <details><summary>Answer</summary>

   `constexpr long pow2(int n) { long v = 1; for (int i = 0; i < n; ++i) v *= 2;
   return v; }` — or `1L << n`. `static_assert(pow2(10) == 1024);`
   </details>

2. **Compile-time table:** write a `constexpr std::array<int, 10> squares()` and
   use `squares()[7]` as a compile-time constant.

   <details><summary>Answer</summary>

   `constexpr std::array<int,10> squares() { std::array<int,10> a{}; for (int i =
   0; i < 10; ++i) a[i] = i*i; return a; } constexpr auto sq = squares();
   static_assert(sq[7] == 49);`
   </details>

3. **TypeList::Contains:** implement it with a fold and test
   `Contains<int, TypeList<double, int, char>>::value`.

   <details><summary>Answer</summary>

   `template <class T, class... Ts> struct Contains<T, TypeList<Ts...>> :
   std::bool_constant<(std::is_same_v<T, Ts> || ...)> {};` → `Contains<int,
   TypeList<double,int,char>>::value == true`.
   </details>

4. **index_sequence:** write `sum_tuple(const std::tuple<Ts...>&)` that adds all
   elements.

   <details><summary>Answer</summary>

   `template <class Tup, std::size_t... Is> auto s(const Tup& t,
   std::index_sequence<Is...>) { return (std::get<Is>(t) + ...); } template
   <class... Ts> auto sum_tuple(const std::tuple<Ts...>& t) { return s(t,
   std::index_sequence_for<Ts...>{}); }`
   </details>

5. **consteval:** why use `consteval` for a function that builds a CRC table
   instead of `constexpr`?

   <details><summary>Answer</summary>

   `consteval` **guarantees** the call is evaluated at compile time — it's a hard
   error if it ever runs at runtime. For a table you never want built at runtime,
   this documents and enforces the intent; `constexpr` merely *allows*
   compile-time evaluation.
   </details>

---

## Interview questions

1. Modern TMP kaise dikhta (`constexpr` fn + `if constexpr` + folds) vs purana (recursive templates)?
2. `constexpr` compile-time evaluation force kaise karein (`consteval` / `constexpr` variable)?
3. Recursive-template computation ke problems (depth limit, compile time)?
4. Compile-time lookup table ka fayda (`.rodata`, zero init)?
5. `std::index_sequence` — pack ko index se iterate kaise?
6. TMP kis kaam ke liye worth hai (tables, validation), kis ke liye nahi?

---

## Next
→ [`14-two-phase-lookup.md`](14-two-phase-lookup.md)
