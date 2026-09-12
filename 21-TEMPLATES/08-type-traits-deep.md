# 08 — `<type_traits>` deep, and writing your own

## Prerequisites
- [`05-specialization.md`](05-specialization.md), [`07-if-constexpr.md`](07-if-constexpr.md)
- Folder 19 file 21 (the practical trait catalog)

## Yeh topic abhi kyun
Folder 19 file 21 mein traits **use** kiye. Yahan dekhte hain woh **bante kaise
hain** — `integral_constant`, `true_type`/`false_type`, partial specialization,
`void_t` — aur apne traits kaise likhein. Yeh SFINAE (file 09) aur concepts
(file 10) ka andar-ka mechanism hai.

---

## The building blocks

### `std::integral_constant<T, v>` — a compile-time constant as a type
```cpp
template <class T, T v>
struct integral_constant {
    static constexpr T value = v;
    using value_type = T;
    using type = integral_constant;
    constexpr operator T() const noexcept { return v; }      // implicit conversion to the value
    constexpr T operator()() const noexcept { return v; }
};

using true_type  = std::integral_constant<bool, true>;
using false_type = std::integral_constant<bool, false>;
```

Every predicate trait ultimately inherits from `true_type` or `false_type`, so it
gets `::value`, `::type`, and the conversions for free.

### The two trait shapes
```cpp
// PREDICATE trait -> ::value (a bool)          -> the _v alias
std::is_integral<int>::value;   // true
std::is_integral_v<int>;        // inline constexpr bool ..._v = ...::value;

// TRANSFORMATION trait -> ::type (a type)      -> the _t alias
std::remove_reference<int&>::type;   // int
std::remove_reference_t<int&>;       // using ..._t = typename ...::type;
```

---

## Writing a predicate trait — partial specialization

```cpp
// primary: "not a pointer"
template <class T>
struct is_pointer : std::false_type {};

// partial spec: "T* IS a pointer"  -- matches any pointer, still generic in T
template <class T>
struct is_pointer<T*> : std::true_type {};

template <class T>
inline constexpr bool is_pointer_v = is_pointer<T>::value;
```

The whole standard `<type_traits>` predicate set is this pattern (plus compiler
intrinsics for the ones that can't be expressed in C++ — see below).

More examples:
```cpp
template <class T> struct is_const           : std::false_type {};
template <class T> struct is_const<const T>  : std::true_type {};

template <class> struct is_std_vector : std::false_type {};
template <class T, class A> struct is_std_vector<std::vector<T, A>> : std::true_type {};

template <class T> struct rank : std::integral_constant<std::size_t, 0> {};
template <class T> struct rank<T[]>  : std::integral_constant<std::size_t, 1 + rank<T>::value> {};
template <class T, std::size_t N> struct rank<T[N]> : std::integral_constant<std::size_t, 1 + rank<T>::value> {};
```

---

## Writing a transformation trait

```cpp
template <class T> struct remove_const          { using type = T; };
template <class T> struct remove_const<const T> { using type = T; };
template <class T> using  remove_const_t = typename remove_const<T>::type;

template <class T> struct add_pointer { using type = T*; };   // (real one handles ref/void edge cases)
```

`std::conditional` — the type-level `?:`:
```cpp
template <bool B, class T, class F> struct conditional          { using type = T; };
template <class T, class F>         struct conditional<false, T, F> { using type = F; };
template <bool B, class T, class F> using conditional_t = typename conditional<B, T, F>::type;

using index_t = std::conditional_t<(MAX <= UINT32_MAX), std::uint32_t, std::uint64_t>;
```

---

## The detection idiom — `std::void_t`

`std::void_t<...>` maps any well-formed list of types to `void`. If any type in
the list is **ill-formed**, substitution fails → SFINAE → the partial
specialization is dropped.

```cpp
template <class...> using void_t = void;   // that's the whole definition

// does T have a member `.size()` returning something?
template <class, class = void>
struct has_size : std::false_type {};

template <class T>
struct has_size<T, std::void_t<decltype(std::declval<const T&>().size())>>
    : std::true_type {};

template <class T> inline constexpr bool has_size_v = has_size<T>::value;
```

- `std::declval<T>()` — an "imaginary" `T` value usable only in unevaluated
  contexts (`decltype`, `sizeof`). Lets you write `declval<T>().foo()` without
  needing a real, constructible `T`.
- If `T().size()` is valid, `decltype(...)` is a type, `void_t<...>` is `void`,
  the partial spec matches (it's more specialized) → `true_type`.
- If not, the partial spec's `void_t<...>` argument is ill-formed → SFINAE drops
  it → the primary (`false_type`) is used.

C++20 replaces this with `requires` (file 10):
```cpp
template <class T> concept HasSize = requires (const T& t) { t.size(); };
```

---

## Compiler intrinsics

Some traits **can't** be written in pure C++ — the compiler must inspect its own
internal representation:

```cpp
std::is_trivially_copyable_v<T>       // -> __is_trivially_copyable(T)
std::is_base_of_v<B, D>              // -> __is_base_of(B, D)
std::is_constructible_v<T, Args...>  // -> __is_constructible(T, Args...)
std::is_enum_v<T>, std::is_union_v<T>, std::underlying_type_t<E>, std::is_aggregate_v<T>, ...
```

libstdc++'s `<type_traits>` forwards these to `__builtin` / `__is_*` intrinsics.
All still **zero runtime cost** — resolved during compilation, no code emitted.

---

## Andar kya hota hai

- A trait instantiation produces a small `struct` with a `static constexpr`
  member. The optimizer never sees it — `is_integral_v<T>` is a compile-time
  `bool`, used in `if constexpr` / `static_assert` / `enable_if`, and leaves no
  trace in the binary.
- Partial-specialization matching runs the same "most specialized wins" ordering
  as class-template specialization (file 05): `is_pointer<T*>` is more
  specialized than `is_pointer<T>`, so pointer types get `true_type`.
- `void_t` works because SFINAE applies during the substitution of the partial
  specialization's template arguments: an ill-formed `decltype(...)` inside
  `void_t<...>` is a *substitution failure*, not a hard error, so that candidate
  is silently removed and the primary is used.
- `declval<T>()` is declared but never defined; using it outside an unevaluated
  context is a link error by design — it exists purely to give an expression a
  type.

> **HFT relevance:** hand-written traits gate the **compile-time fast paths** in
> low-latency generic code: a lock-free queue `static_assert`s a `is_trivially_
> copyable` + `has_unique_object_representations` trait so a slot can be published
> with one store; a serializer detects a `has_serialize_member` trait to pick
> `memcpy` vs field-by-field (`if constexpr`, file 07); a container detects
> `is_contiguous_range` to take a `memmove` path. `std::conditional_t` picks the
> narrowest index type for a fixed-capacity structure. It's all `constexpr` →
> zero runtime cost, and getting the traits right (mark moves `noexcept`, keep
> PODs trivially copyable) is a measurable perf lever (folder 18 file 13). C++20
> concepts (file 10) are the modern spelling — but the trait machinery underneath
> is exactly this.

---

## Hands-on

```bash
./build.ps1 21-TEMPLATES/examples/05_sfinae.cpp    # has_size via void_t + declval
./build.ps1 21-TEMPLATES/examples/04_if_constexpr.cpp
```

Write: `is_std_vector<T>`, `is_std_array<T>`, `element_type_t<T>` (the value type
of a container or pointer), and `has_reserve<T>` (detection idiom). Verify with
`static_assert`.

---

## ⚠️ Traps

### Trap 1 — using a trait as a value without `::value` / `_v`
```cpp
if constexpr (std::is_integral<T>) ...   // ❌ that's a type -> always "truthy" in a bool context is a different bug. _v or ::value
```

### Trap 2 — `declval` in an evaluated context
```cpp
auto x = std::declval<T>();   // ❌ declval is never defined -> link error. Only in decltype/sizeof/noexcept
```

### Trap 3 — forgetting the primary needs a default 2nd param for void_t
```cpp
template <class T> struct has_size : std::false_type {};   // ❌ needs `template <class, class = void>`
template <class T> struct has_size<T, std::void_t<...>> : std::true_type {};
```

### Trap 4 — `is_same` ignoring cv/ref
```cpp
std::is_same_v<int, const int>   // false. std::is_same_v<std::remove_cvref_t<T>, int>
```

### Trap 5 — expecting a trait to have runtime cost / be "checked at runtime"
```cpp
// Traits are pure compile-time. There is no runtime type query here (that's RTTI / dynamic_cast).
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Traits do runtime type inspection" | Compile-time only — RTTI/`typeid` is the runtime thing |
| "`std::is_integral<T>` is a bool" | It's a *type* (inherits `true_type`/`false_type`); use `_v` / `::value` |
| "You can write every trait in C++" | Some need compiler intrinsics (`is_trivially_copyable`, `is_base_of`, …) |
| "`void_t` is complicated magic" | `template <class...> using void_t = void;` — the SFINAE does the work |
| "`declval<T>()` makes a `T`" | It only gives an expression of type `T` for `decltype` — never a real object |

---

## Exercises

1. **is_const:** implement `is_const<T>` and test it on `int`, `const int`,
   `const int*`, `int* const`.

   <details><summary>Answer</summary>

   `template <class T> struct is_const : std::false_type {}; template <class T>
   struct is_const<const T> : std::true_type {};`. `int` → false. `const int` →
   true. `const int*` → false (the *pointer* isn't const, the pointee is).
   `int* const` → true (top-level const on the pointer).
   </details>

2. **is_specialization_of:** write `is_vector<T>` true iff `T` is some
   `std::vector<...>`.

   <details><summary>Answer</summary>

   `template <class> struct is_vector : std::false_type {}; template <class T,
   class A> struct is_vector<std::vector<T, A>> : std::true_type {};` — partial
   spec matches the two-parameter `std::vector`.
   </details>

3. **Detection idiom:** write `has_push_back<T>` (does `T` have `.push_back(value_type)`?).

   <details><summary>Answer</summary>

   `template <class, class = void> struct has_push_back : std::false_type {};
   template <class T> struct has_push_back<T, std::void_t<decltype(
   std::declval<T&>().push_back(std::declval<typename T::value_type>()))>> :
   std::true_type {};`
   </details>

4. **conditional_t:** define `smallest_uint_t<N>` = the smallest unsigned type
   that can hold `N` (`uint8_t`/`uint16_t`/`uint32_t`/`uint64_t`).

   <details><summary>Answer</summary>

   Nested `std::conditional_t`: `conditional_t<(N <= 0xFF), uint8_t,
   conditional_t<(N <= 0xFFFF), uint16_t, conditional_t<(N <= 0xFFFFFFFF),
   uint32_t, uint64_t>>>`.
   </details>

5. **Why an intrinsic:** why can't `std::is_trivially_copyable<T>` be written in
   pure C++?

   <details><summary>Answer</summary>

   "Trivially copyable" depends on internal properties the language gives no
   introspection hook for — whether every copy/move ctor and the destructor are
   trivial and there are no virtual functions/bases. The compiler knows this from
   its own AST; a pure-C++ trait has no way to query it, so it forwards to
   `__is_trivially_copyable(T)`.
   </details>

---

## Interview questions

1. `integral_constant` / `true_type` / `false_type` — traits inhe kaise use karte?
2. Predicate trait partial specialization se kaise banta (`is_pointer<T*>`)?
3. `void_t` detection idiom — SFINAE kahan hota, `declval` kya deta?
4. `std::conditional_t` — type-level `?:`, ek use case?
5. Kaunse traits compiler intrinsics maangte, kyun?
6. Traits ki runtime cost (zero) — RTTI se kya fark?

---

## Next
→ [`09-sfinae.md`](09-sfinae.md)
