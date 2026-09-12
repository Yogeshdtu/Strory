# 17 — Exercises: templates & generic programming

## Prerequisites
- All of folder 21 (files 01–16)

## Yeh file kya hai
Practice — output prediction, "find the bug", puzzles, aur chhote implementation
tasks. Har ek ka answer `<details>` mein. Compile karke check karo: `./build.ps1
file.cpp`. Benchmarks ke liye `./build.ps1 fast`.

---

## Part A — Output / behaviour prediction

### A1
```cpp
template <class T> const char* who(T)        { return "template"; }
const char* who(int)                          { return "int"; }
const char* who(double)                       { return "double"; }
int main() {
    std::printf("%s %s %s\n", who(1), who(1.5f), who('c'));
}
```
<details><summary>Answer</summary>

`int template template`.
- `who(1)` — arg is `int`. `who<int>(int)` and the non-template `who(int)` are
  both exact matches → **non-template wins** → `"int"`.
- `who(1.5f)` — arg is `float`. `who<float>(float)` is exact; `who(int)` needs a
  conversion, `who(double)` a promotion → **template wins** → `"template"`.
- `who('c')` — arg is `char`. `who<char>(char)` is exact; the non-templates need
  a promotion/conversion → **template wins** → `"template"`.
</details>

### A2
```cpp
template <class T> T mid(T a, T b) { return (a + b) / 2; }
int main() {
    std::printf("%d %.2f\n", mid(3, 6), mid(3.0, 6.0));
}
```
<details><summary>Answer</summary>

`4 4.50`. `mid(3, 6)` → `T = int`, `(3+6)/2 = 4` (integer div). `mid(3.0, 6.0)`
→ `T = double`, `4.5`. `mid(3, 6.0)` would fail (deduction conflict).
</details>

### A3
```cpp
template <class... Ts> auto f(Ts... xs) { return (xs + ...); }
int main() { std::printf("%d\n", f(1, 2, 3, 4)); }   // and: f()?
```
<details><summary>Answer</summary>

`f(1,2,3,4)` → `10` (unary right fold `1 + (2 + (3 + 4))`). `f()` → **compile
error** — a unary fold over `+` on an empty pack is ill-formed. `(0 + ... + xs)`
would make `f()` return 0.
</details>

### A4
```cpp
template <class T>
void g(T v) {
    if constexpr (std::is_pointer_v<T>) std::printf("deref %d\n", *v);
    else                                std::printf("val %d\n", v);
}
int main() { int x = 7; g(x); g(&x); }
```
<details><summary>Answer</summary>

`val 7` then `deref 7`. `if constexpr` discards the `*v` branch for `T = int` (so
it compiles) and the `v` branch for `T = int*`.
</details>

### A5
```cpp
template <class T> struct N { static const char* s; };
template <class T> const char* N<T>::s = "generic";
template <> const char* N<int>::s = "int";
int main() { std::printf("%s %s\n", N<double>::s, N<int>::s); }
```
<details><summary>Answer</summary>

`generic int`. `N<double>` uses the primary's static member definition; `N<int>`
uses the explicit specialization of the static member.
</details>

---

## Part B — Find the bug

### B1
```cpp
template <class C>
typename C::value_type first(const C& c) { return c[0]; }
int main() { std::vector<int> v{1,2,3}; std::printf("%d\n", first(v)); int a[3]={9,8,7}; first(a); }
```
<details><summary>Answer</summary>

`first(v)` is fine. `first(a)` fails: a C array `int[3]` has no `::value_type` →
substitution failure in the return type → SFINAE removes the only candidate → "no
matching function". Add an overload for arrays, or take `std::span`.
</details>

### B2
```cpp
template <class T> struct Derived : Base<T> {
    void f() { helper(); }        // Base<T> has void helper();
};
```
<details><summary>Answer</summary>

`helper()` is an unqualified name from a **dependent base** → not visible in phase
1 → error. Fix: `this->helper();` or `Base<T>::helper();` or `using
Base<T>::helper;`.
</details>

### B3
```cpp
template <class T, std::enable_if_t<std::is_integral_v<T>>* = 0>
void h(T);
template <class T, std::enable_if_t<std::is_floating_point_v<T>>* = 0>
void h(T);
```
<details><summary>Answer</summary>

`std::enable_if_t<cond>` with no second arg is `void`; `void* = 0` as a template
param — legal, but both overloads then have the *same* signature `template <class
T, void*> void h(T)` and can clash / be a redefinition depending on usage. Use
`std::enable_if_t<cond, int> = 0` (an `int` NTTP with a default) so the two
overloads differ.
</details>

### B4
```cpp
template <class Derived>
struct Shape { double area() const { return static_cast<const Derived*>(this)->area_impl(); } };
struct Circle : Shape<Square> { double area_impl() const; };   // typo
```
<details><summary>Answer</summary>

`Circle` passes `Square` as `Derived`. `static_cast<const Square*>(this)` on a
`Circle` object is a lie → UB when `area()` is called. Guard with `friend
Derived;` + a private `Shape` constructor so only the real `Derived` can
instantiate the base.
</details>

### B5
```cpp
template <int N> struct Fib { static constexpr long v = Fib<N-1>::v + Fib<N-2>::v; };
template <> struct Fib<0> { static constexpr long v = 0; };
template <> struct Fib<1> { static constexpr long v = 1; };
constexpr long x = Fib<900>::v;
```
<details><summary>Answer</summary>

~900 nested template instantiations → hits the compiler's instantiation-depth
limit ("template instantiation depth exceeds maximum"). Rewrite as a `constexpr`
function with a loop: `constexpr long fib(int n){ long a=0,b=1; for(int i=0;i<n;
++i){ long c=a+b; a=b; b=c; } return a; }`.
</details>

---

## Part C — Implementation tasks

### C1 — `min_of` variadic
Write `min_of(a, b, c, ...)` returning the smallest, as a fold.
<details><summary>Answer</summary>

`template <class T, class... Ts> T min_of(T first, Ts... rest) { T m = first;
((m = rest < m ? rest : m), ...); return m; }` — fold over comma, updating `m`.
(Requires all args the same type `T`; add `std::common_type_t` for mixed.)
</details>

### C2 — `is_specialization_of`
`is_specialization_of<T, std::vector>::value` — true iff `T` is some
`std::vector<...>`.
<details><summary>Answer</summary>

`template <class T, template <class...> class Tmpl> struct
is_specialization_of : std::false_type {}; template <template <class...> class
Tmpl, class... Args> struct is_specialization_of<Tmpl<Args...>, Tmpl> :
std::true_type {};`
</details>

### C3 — `Overloaded` visitor helper
Write the `Overloaded` struct so `std::visit(Overloaded{ [](int){...},
[](std::string const&){...} }, var)` works.
<details><summary>Answer</summary>

`template <class... Ts> struct Overloaded : Ts... { using Ts::operator()...; };
template <class... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;` (the deduction
guide is needed pre-C++20). Inheriting from each lambda + `using` their
`operator()` makes one callable with all overloads.
</details>

### C4 — `constexpr` CRC-8 table
Write a `constexpr std::array<std::uint8_t, 256> crc8_table()` (poly `0x07`) and
`static_assert` `table()[0] == 0` and `table()[1] == 0x07`.
<details><summary>Answer</summary>

`constexpr std::array<std::uint8_t,256> t() { std::array<std::uint8_t,256> a{};
for (int i = 0; i < 256; ++i) { std::uint8_t c = (std::uint8_t)i; for (int k = 0;
k < 8; ++k) c = (c & 0x80) ? (std::uint8_t)((c << 1) ^ 0x07) : (std::uint8_t)(c
<< 1); a[(size_t)i] = c; } return a; }` — built by the compiler into `.rodata`.
</details>

### C5 — CRTP `Comparable`
A mixin `Comparable<Derived>` that provides `<`, `<=`, `>`, `>=`, `==`, `!=` from
a single `Derived::cmp(other)` returning `<0 / 0 / >0`.
<details><summary>Answer</summary>

`template <class D> struct Comparable { friend bool operator<(const D& a, const
D& b){ return a.cmp(b) < 0; } friend bool operator==(const D& a, const D& b){
return a.cmp(b) == 0; } /* and the rest in terms of these */ };` — hidden
friends, found by ADL, inlined.
</details>

### C6 — `has_reserve` concept and use
A concept `Reservable` and a function `prepare(C& c, size_t n)` that calls
`c.reserve(n)` if possible, else does nothing.
<details><summary>Answer</summary>

`template <class C> concept Reservable = requires (C& c, std::size_t n) {
c.reserve(n); }; template <class C> void prepare(C& c, std::size_t n) { if
constexpr (Reservable<C>) c.reserve(n); }`
</details>

---

## Part D — Discussion

1. **Template vs virtual vs variant:** you have a `Strategy` abstraction. Give
   the three implementations and the exact criterion that picks each.
   <details><summary>Answer sketch</summary>

   **Template** (`run<Strat>(...)`): each call site knows its concrete strategy
   at compile time → fully inlined, `sizeof` no vptr; no heterogeneous container.
   **`std::variant<S1,S2,S3>` + `visit`**: the live strategy is a single runtime
   value from a small closed set → jump-table dispatch, allocation-free,
   inline-inside-visit. **`virtual`**: open/large set, or you need a
   `vector<unique_ptr<IStrategy>>` of mixed types, or it's cold (control plane).
   Measured (`examples/07`,`08`): template/CRTP ~0.5–1.1 ns, variant ~1.1 ns,
   virtual (real boundary) ~2.4–2.5 ns.
   </details>

2. **Build-time budget:** your trading system's incremental build went from 40s
   to 4 minutes as template use grew. Diagnose and fix.
   <details><summary>Answer sketch</summary>

   Profile with `-ftime-trace` / `-ftime-report`. Likely culprits: a fat class
   template instantiated in hundreds of TUs (→ factor T-independent bulk into a
   non-template base + `extern template` the common instantiations),
   recursive-template metaprogramming in a hot header (→ `constexpr` functions),
   an exploding set of `<Policy...>` combinations (→ shrink / default), a giant
   header included everywhere (→ split, PCH, or a C++20 module). Erase types at
   cold boundaries.
   </details>

3. **SFINAE → concepts:** why did C++20 concepts get adopted so fast in
   performance-oriented codebases despite zero runtime difference?
   <details><summary>Answer sketch</summary>

   The runtime is identical (both are compile-time gates), but concepts fix
   *developer* costs: one-line "constraint not satisfied" errors instead of pages
   of SFINAE noise, named reusable constraints, subsumption (overlapping
   constraints resolve to the tighter overload without the mutually-exclusive
   `enable_if<A>`/`enable_if<!A>` dance), no phantom-parameter fragility. In a
   template-heavy codebase, error legibility and refactor safety are worth a lot.
   </details>

---

## Challenge

Yeh open-ended projects hain — inka koi "answer key" nahi. Jo naapo, wahi likho
(Rule 2); jo compile error aaye, uska asli text paste karo.

### Challenge 1 — `Price<Scale>`: zero-cost fixed-point type
Ek `template <std::int64_t Scale> struct Price { std::int64_t ticks; };` banao
(non-type template parameter, file 04):
- `+`, `-`, `<=>` sirf **same `Scale`** ke beech chalein. `Price<100>{} + Price<10000>{}`
  compile hi na ho — ek `concept SameScale` se (file 10).
- `double` mein conversion sirf explicit function se (`to_double()`), implicit nahi.
- `std::formatter<Price<Scale>>` specialization jo `12345` ticks + `Scale=100` ko `123.45`
  print kare (folder 22 file 13).
- `static_assert` se compile-time tests (file 13).

**Zero-cost prove karo:** `Price<100>` ke 1M elements ka sum aur raw `std::int64_t` ka sum
`-O2 -S` se compile karo. Dono ka assembly compare karo (`diff`). Same aaya to "zero-cost"
ka saboot hai; alag aaya to kya alag hai aur kyun — woh likho.

### Challenge 2 — compile-time message dispatcher vs `virtual`
3 message types (`Add`, `Cancel`, `Trade`) — har ek ek struct. Ek variadic
`Dispatcher<Handlers...>` banao (file 06) jo message ke type-byte se constexpr-built table
ke through sahi handler call kare (file 12, 13). Phir:
1. Wahi kaam `virtual` base class se likho.
2. 4M messages ka ns/msg `-O2` pe dono ka naapo — `36-LOW-LATENCY-CPP/examples/07_dispatch_comparison.cpp`
   ka method follow karo (HOMO aur HETERO data dono).
3. **Bloat naapo (file 15):** 3 message types vs 30 message types — binary size (`size` /
   file size) aur compile time. Templates ki kimat kahan dikhi?

### Challenge 3 — SFINAE vs concepts: error message ki ladai
`SpscQueue<T, N>` jiska `N` power of 2 hona chahiye aur `T` `std::semiregular`. Do versions:
`std::enable_if` (file 09) aur `requires` (file 10). Dono ko jaan-boojh kar galat use karo
(`N = 100`, aur ek non-copyable `T`). Dono error messages poore paste karo, lines gino, aur
batao ek naye teammate ke liye kaunsa samajhna aasaan hai — aur kyun.

---

## Next
→ [`../22-MODERN-CPP/00-README.md`](../22-MODERN-CPP/00-README.md)
