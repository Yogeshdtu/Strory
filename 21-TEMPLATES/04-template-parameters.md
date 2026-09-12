# 04 — Template parameters in depth

## Prerequisites
- [`03-class-templates.md`](03-class-templates.md)
- Folder 03 (integer types, constexpr)

## Yeh topic abhi kyun
Template parameter sirf `class T` nahi hota. Teen kinds hain: **type**, **non-type
(value)**, aur **template template**. Non-type parameters HFT mein bahut kaam ke
— fixed-size buffers, compile-time config, unrolled loops. Aur default arguments,
parameter packs, aur `auto` NTTPs (C++17/20) ne inhe aur flexible banaya.

---

## The three kinds

```cpp
template <class T>                       // 1. TYPE parameter
template <std::size_t N>                 // 2. NON-TYPE parameter (a value)
template <template <class> class C>      // 3. TEMPLATE TEMPLATE parameter
```

### 1. Type parameters — `class T` / `typename T`
The common case. `class` and `typename` are identical here (historical; `typename`
reads better). Can have a default: `template <class T = int>`.

### 2. Non-type parameters (NTTPs) — a compile-time value
```cpp
template <std::size_t N>        struct Array   { int data[N]; };
template <int Lo, int Hi>       struct Range    { static constexpr int span = Hi - Lo; };
template <bool Debug>           struct Logger   { void log(const char*) { if constexpr (Debug) { /*...*/ } } };
template <const char* Name>     struct Named    { /* pointer with linkage */ };
```

Allowed NTTP types:
- integral and enumeration types
- pointer / lvalue reference to an object or function **with linkage**
- `std::nullptr_t`
- (C++17) `auto` — deduce the NTTP's type from the argument
- (C++20) floating-point types, and **structural** literal class types (all
  members public and structural — so you can pass a small `struct` as an NTTP)

```cpp
template <auto V> struct C { static constexpr auto value = V; };
C<42>       // V is int, value == 42
C<'x'>      // V is char
C<&global>  // V is int*

// C++20: a struct NTTP
struct Cfg { int width; bool wrap; };
template <Cfg c> struct Widget { static constexpr int w = c.width; };
Widget<Cfg{80, true}> w;
```

The argument **must be a constant expression**. `std::array<T, n>` where `n` is a
runtime `int` doesn't compile — that's the line between `std::array` and
`std::vector`.

### 3. Template template parameters — a parameter that is a template
```cpp
template <class T, template <class...> class Container = std::vector>
class Stack {
    Container<T> c_;
public:
    void push(const T& x) { c_.push_back(x); }
};
Stack<int>              s1;   // Container = std::vector
Stack<int, std::deque>  s2;   // Container = std::deque
```
Use `template <class...>` (variadic) for the parameter so it matches `std::vector`
(which has a hidden allocator parameter) without you spelling every argument.
Niche — usually passing `Container<T>` as a plain type parameter is simpler.

---

## Defaults, packs, and ordering

```cpp
template <class T, class Alloc = std::allocator<T>, std::size_t Block = 4096>
class Pool { /* ... */ };

Pool<int> p;                    // Alloc and Block defaulted
Pool<int, MyAlloc<int>> q;      // Block defaulted
Pool<int, MyAlloc<int>, 8192> r;
```

- Defaults, once one is given, all following parameters must also have defaults
  (like function default args).
- A **parameter pack** (`class... Ts`, `auto... Vs`) must be **last** (for class
  templates; function templates are more flexible — file 06).
- `std::size_t... Is` — a pack of non-type parameters (used in `std::index_sequence`
  tricks, file 13).

```cpp
template <class... Ts>              struct Tuple;           // type pack
template <int... Ns>               struct IntList;          // NTTP pack
template <template <class> class... Cs> struct Adaptors;    // template template pack
```

---

## `typename` and `template` disambiguators

Inside a template, a **dependent** name (one that depends on a template parameter)
that names a *type* needs `typename`; one that names a *member template* needs
`template` (file 14):

```cpp
template <class C>
void f(C& c) {
    typename C::value_type x;               // C::value_type is a type -> `typename` required
    auto it = c.template get<0>();          // get is a member template -> `template` required
}
```

Without `typename`, the compiler assumes `C::value_type` is a *value* (a static
member) and mis-parses.

---

## Andar kya hota hai

- An NTTP is substituted as a literal constant. `Array<4>::data` is `int[4]` — a
  fixed-size member; `sizeof(Array<4>) == 16`. The value participates in constant
  folding, loop unrolling, and `constexpr` evaluation.
- `template <auto V>` deduces `decltype(V)` from the argument — one template
  covers `int`, `char`, `enum`, pointer NTTPs.
- A C++20 struct NTTP is stored as an anonymous static constant with linkage;
  two `Widget<Cfg{80,true}>` in different TUs are the **same type** (structural
  equality of the NTTP), so the linker merges them.
- Template template parameters are matched structurally against a template's
  parameter list — the reason `template <class T> class C` won't accept
  `std::vector` (2 params) unless you write `template <class...> class C`.
- Defaults are substituted at the point of use, not definition — so a default
  like `std::allocator<T>` can refer to an earlier parameter `T`.

> **HFT relevance:** NTTPs are the mechanism behind every **fixed-capacity,
> zero-allocation** hot-path structure — `RingBuffer<T, 1024>`, `FlatMap<K, V,
> MAX>`, `PriceLevels<TICKS>` — the size is a compile-time constant so the buffer
> is inline/static and the compiler unrolls and bounds-proves loops over it. A
> `template <bool Enabled>` or `template <Cfg c>` parameter compiles feature
> flags / tuning **in or out** with zero runtime cost (`if constexpr (Enabled)` —
> file 07). C++20 struct NTTPs let a whole config block ride in the type. Keep
> the combinatorial explosion of `<T, N, Policy...>` instantiations in check —
> that's the tax.

---

## Hands-on

```bash
./build.ps1 21-TEMPLATES/examples/02_class_templates.cpp   # non-type param (Cap)
./build.ps1 21-TEMPLATES/examples/04_if_constexpr.cpp      # bool NTTP -> compiled-out code
```

Write: `template <std::size_t N> struct Vec { double d[N]; };` with a
`dot(const Vec&)` and check the loop unrolls at `-O2` (`./build.ps1 asm`); a
`template <auto... Vs> constexpr auto sum_v = (Vs + ...);`.

---

## ⚠️ Traps

### Trap 1 — runtime value as an NTTP
```cpp
std::size_t n = getSize();
Buffer<n> b;   // ❌ n isn't a constant expression. constexpr n, or use a runtime container
```

### Trap 2 — default parameter without defaults after it
```cpp
template <class T = int, class U> struct S;   // ❌ U has no default but follows one that does
```

### Trap 3 — missing `typename` on a dependent type
```cpp
template <class C> void f() { C::iterator it; }   // ❌ parsed as `C::iterator * it` ... use `typename C::iterator it;`
```

### Trap 4 — template template param arity mismatch
```cpp
template <class T, template <class> class C> struct S;
S<int, std::vector> s;   // ❌ std::vector is template<class, class>. Use `template <class...> class C`
```

### Trap 5 — pack not last (class template)
```cpp
template <class... Ts, class Last> struct S;   // ❌ pack must be the final parameter for class templates
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Template params are always types" | Also values (NTTPs) and templates (template template params) |
| "NTTP can be any type" | Integral/enum/pointer-with-linkage/nullptr; +float & structural class in C++20; must be constexpr |
| "`class` and `typename` differ in a param list" | Identical there; `typename` also disambiguates dependent types in bodies |
| "Template template param `template<class> class C` matches `std::vector`" | No — `std::vector` has an allocator param; use `template <class...> class C` |
| "Defaults can go anywhere" | Once one param has a default, all after it must too |

---

## Exercises

1. **NTTP vs runtime:** when is `std::array<T, N>` the right choice over
   `std::vector<T>`, in terms of the `N` parameter?

   <details><summary>Answer</summary>

   When `N` is known at compile time and fixed — then storage is inline (no
   heap), `sizeof` is exact, and loops unroll. `std::vector` when the size is a
   runtime value or must change.
   </details>

2. **auto NTTP:** write `template <auto V> struct Tag {};` and instantiate it
   with an `int`, a `char`, and an enum value. What is `decltype(V)` each time?

   <details><summary>Answer</summary>

   `Tag<42>` → `decltype(V)` is `int`. `Tag<'a'>` → `char`. `Tag<Color::Red>` →
   `Color`. One template, any integral/enum/pointer NTTP.
   </details>

3. **typename fix:** `template <class M> void keys(const M& m) { M::key_type k;
   ... }` won't compile. Why and fix.

   <details><summary>Answer</summary>

   `M::key_type` is a dependent name; the compiler assumes it's a value, so
   `M::key_type k;` mis-parses. Fix: `typename M::key_type k;`.
   </details>

4. **Struct NTTP (C++20):** pass a `struct Params { int lanes; bool simd; };` as
   a template argument and read a member.

   <details><summary>Answer</summary>

   `template <Params P> struct Kernel { static constexpr int lanes = P.lanes; };`
   then `Kernel<Params{8, true}> k;`. Requires `Params` to be a *structural*
   type (all bases/members public and structural).
   </details>

5. **Bool NTTP:** design a `template <bool Metrics> class Engine` where metrics
   collection has **zero cost** when `Metrics == false`.

   <details><summary>Answer</summary>

   Guard every metrics call with `if constexpr (Metrics) { ++counter; }` and make
   the counter member conditional (or `[[no_unique_address]]` an empty struct).
   With `Metrics == false` the branches aren't instantiated → no code, no member.
   </details>

---

## Interview questions

1. Template parameter ke 3 kinds — kaun kya?
2. NTTP mein kaunse types allowed (C++17/20 additions)?
3. `class` vs `typename` param list mein — aur body mein `typename` kyun?
4. Template template parameter kyun `template <class...> class C` likhte?
5. Default template args ke ordering rules?
6. NTTP compile-time hone se optimizer ko kya milta (unroll, bounds)?

---

## Next
→ [`05-specialization.md`](05-specialization.md)
