# 21 — `<type_traits>`: compile-time introspection

## Prerequisites
- [`20-regex.md`](20-regex.md), folder 21 preview (templates), folder 18 (`noexcept` move, `is_nothrow_move_constructible`)
- `constexpr`, `if constexpr`

## Yeh topic abhi kyun
`<type_traits>` woh toolkit hai jisse template code **types ke baare mein sawaal**
poochta hai at compile time: "kya yeh integer hai?", "kya iska move ctor noexcept
hai?", "kya yeh trivially copyable hai (memcpy kar sakta hoon)?". Ye SFINAE,
`if constexpr`, aur concepts ka foundation hai — aur `std::vector` jaise
containers isi se decide karte hain memcpy karein ya loop.

`#include <type_traits>`. Sab traits compile-time — zero runtime cost.

---

## Shape of a trait

```cpp
std::is_integral<int>::value        // true   -- the classic form (a struct with a static bool)
std::is_integral_v<int>             // true   -- the _v variable template (C++17) -- prefer this
std::is_same_v<int, int32_t>        // platform-dependent (true on most)

std::remove_reference<int&>::type   // int    -- transformation traits produce a ::type
std::remove_reference_t<int&>       // int    -- the _t alias (C++14) -- prefer this
```

Two kinds:
- **Predicate traits** → `::value` / `_v` (a `bool` or integral constant).
- **Transformation traits** → `::type` / `_t` (a modified type).

---

## The ones you actually use

### Category
```cpp
std::is_integral_v<T>          std::is_floating_point_v<T>     std::is_arithmetic_v<T>
std::is_pointer_v<T>           std::is_enum_v<T>               std::is_class_v<T>
std::is_array_v<T>             std::is_const_v<T>              std::is_reference_v<T>
std::is_void_v<T>              std::is_signed_v<T>             std::is_unsigned_v<T>
```

### Relationships
```cpp
std::is_same_v<A, B>
std::is_base_of_v<Base, Derived>
std::is_convertible_v<From, To>
std::is_constructible_v<T, Args...>          std::is_assignable_v<T&, U>
```

### The performance-relevant ones
```cpp
std::is_trivially_copyable_v<T>              // memcpy is a legal copy -> containers/algorithms take the fast path
std::is_trivially_destructible_v<T>          // no ~T() loop needed on clear/erase
std::is_standard_layout_v<T>                 // safe for C interop / reinterpret to bytes / offsetof
std::is_trivial_v<T>                         // trivial ctor + trivially copyable
std::is_nothrow_move_constructible_v<T>      // vector reallocation MOVES instead of COPIES (folder 18 file 13)
std::is_nothrow_swappable_v<T>
std::has_unique_object_representations_v<T>  // no padding -> hashable/comparable as raw bytes
std::alignment_of_v<T>   /  alignof(T)
```

### Transformations
```cpp
std::remove_reference_t<T>     std::remove_cv_t<T>      std::remove_cvref_t<T>   // C++20
std::decay_t<T>               // what a by-value parameter / auto would deduce (strips ref, cv, array->ptr, fn->ptr)
std::add_pointer_t<T>         std::underlying_type_t<Enum>
std::conditional_t<cond, IfTrue, IfFalse>
std::common_type_t<A, B>      std::invoke_result_t<F, Args...>
```

### Building your own
```cpp
std::integral_constant<int, 5>          // ::value == 5
std::true_type / std::false_type       // integral_constant<bool, true/false>
std::void_t<...>                        // maps anything to void -- the classic detection-idiom helper
std::enable_if_t<cond, T>               // present only if cond -> SFINAE (concepts are the modern replacement)
```

---

## Where they're used

### `if constexpr` — compile-time branch (the modern way)
```cpp
template <class T>
void store(std::byte* dst, const T& v) {
    if constexpr (std::is_trivially_copyable_v<T>) {
        std::memcpy(dst, &v, sizeof v);        // fast path -- the else branch isn't even compiled for such T
    } else {
        new (dst) T(v);                        // general path
    }
}
```

### `static_assert` — enforce a contract
```cpp
template <class T>
struct RingBuffer {
    static_assert(std::is_trivially_copyable_v<T>, "RingBuffer<T> needs a trivially copyable T for lock-free publish");
    static_assert(std::is_nothrow_move_constructible_v<T> || std::is_copy_constructible_v<T>);
};
```

### SFINAE / concepts — constrain overloads
```cpp
// old:
template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
void f(T x);

// C++20:
template <std::integral T> void f(T x);
void f(std::integral auto x);
```

### Container/algorithm internals
`std::vector` reallocation calls `std::move_if_noexcept` — checks
`is_nothrow_move_constructible` to decide move vs copy (measured ~3x in
`18-COPY-MOVE/examples/08`). `std::copy` on a `is_trivially_copyable` contiguous
range → `memmove`. `std::vector::~vector` / `clear()` skip the destructor loop
when `is_trivially_destructible`.

---

## Andar kya hota hai

- Most traits are **compiler intrinsics** — `std::is_trivially_copyable`,
  `std::is_base_of`, `std::is_constructible` can't be written in pure C++, so
  libstdc++ forwards to `__is_trivially_copyable(T)` etc. Zero cost: they're
  resolved during template instantiation, produce a `bool` constant, and leave no
  trace in the binary.
- `_v` / `_t` are just `inline constexpr bool ..._v = ...::value;` and `using
  ..._t = typename ...::type;` — syntactic sugar that removes `typename` and
  `::value` noise.
- `if constexpr (cond)` — the **discarded** branch is parsed but **not
  instantiated**, so it may contain code that wouldn't compile for the current
  `T`. This is what makes trait-driven fast paths clean (no tag dispatch, no
  extra overloads).
- `void_t` detection idiom: `template<class,class=void> struct has_foo :
  std::false_type{}; template<class T> struct has_foo<T,
  std::void_t<decltype(std::declval<T>().foo())>> : std::true_type{};` — the
  partial specialization is chosen only if `T().foo()` is well-formed. Concepts
  (`requires { t.foo(); }`) replace this in C++20.

> **HFT relevance:** traits are how generic low-latency components stay both
> flexible and fast **with no runtime branch**: a lock-free queue
> `static_assert`s `is_trivially_copyable` so a slot can be published with a
> single store; a serializer uses `if constexpr (is_trivially_copyable_v<T> &&
> has_unique_object_representations_v<T>)` to `memcpy` a struct wholesale and
> falls back to field-by-field otherwise; `is_nothrow_move_constructible` on your
> types is what keeps `std::vector` growth from silently copying. Getting these
> right (mark moves `noexcept`, keep POD types trivially copyable) is a real,
> measurable perf lever, not pedantry.

---

## Hands-on

```cpp
// traits.cpp
#include <type_traits>
#include <cstdio>
#include <string>

struct Pod   { int a; double b; };
struct Heavy { std::string s; };

template <class T>
void report(const char* name) {
    std::printf("%-8s trivially_copyable=%d  nothrow_move_ctor=%d  standard_layout=%d\n",
        name,
        std::is_trivially_copyable_v<T>,
        std::is_nothrow_move_constructible_v<T>,
        std::is_standard_layout_v<T>);
}

int main() {
    report<int>("int");
    report<Pod>("Pod");
    report<Heavy>("Heavy");
    report<std::string>("string");
    static_assert(std::is_trivially_copyable_v<Pod>);
    static_assert(!std::is_trivially_copyable_v<Heavy>);
}
```
```bash
g++ -std=c++20 -O2 -Wall traits.cpp -o traits && ./traits
```

---

## ⚠️ Traps

### Trap 1 — forgetting `_v` / `_t` → verbose or wrong
```cpp
if (std::is_integral<T>) ...        // ❌ that's a type, always "truthy". std::is_integral_v<T>, or ::value
```

### Trap 2 — `is_same` with cv/ref differences
```cpp
std::is_same_v<int, const int>      // false. Use std::is_same_v<std::remove_cvref_t<T>, int>
```

### Trap 3 — `if constexpr` without `constexpr` → both branches must compile
```cpp
if (std::is_pointer_v<T>) return *x;   // ⚠️ plain if: `*x` must compile even for non-pointer T. Use `if constexpr`
```

### Trap 4 — assuming `int` is `int32_t`
```cpp
static_assert(std::is_same_v<int, std::int32_t>);   // ⚠️ true in practice, not guaranteed. Don't rely on it
```

### Trap 5 — `is_trivially_copyable` but with padding, hashed as bytes
```cpp
struct S { char c; int i; };   // trivially copyable but has 3 padding bytes -> memcmp/hash of raw bytes is unreliable. Check has_unique_object_representations_v
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Traits cost something at runtime" | Zero — compile-time constants / intrinsics, nothing in the binary |
| "`std::is_integral<T>` is a bool" | It's a *type* (with `::value`). Use `std::is_integral_v<T>` |
| "`if` and `if constexpr` are interchangeable" | Only `if constexpr` discards (doesn't instantiate) the dead branch |
| "`is_trivially_copyable` ⇒ safe to `memcmp`" | Padding bytes may differ — need `has_unique_object_representations_v` too |
| "`std::decay_t` just removes `const`" | Removes ref + cv, and decays array→ptr, function→ptr (like by-value deduction) |

---

## Exercises

1. **Fast path:** write `template<class T> void append(std::vector<std::byte>&
   buf, const T& v)` that `memcpy`s when `T` is trivially copyable and calls
   `v.serialize(buf)` otherwise, with **no runtime branch** for the trivial case.

   <details><summary>Answer</summary>

   `if constexpr (std::is_trivially_copyable_v<T>) { auto n = buf.size();
   buf.resize(n + sizeof v); std::memcpy(buf.data() + n, &v, sizeof v); } else {
   v.serialize(buf); }` — the discarded branch isn't instantiated.
   </details>

2. **remove_cvref:** why does generic code often start with `using U =
   std::remove_cvref_t<T>;` before checking traits?

   <details><summary>Answer</summary>

   `T` deduced from a forwarding reference can be `const X&`, `X&&`, etc.
   `is_same_v<T, X>` / `is_integral_v<T>` would be `false`/wrong for those.
   Stripping cv + ref first gives the underlying type the checks care about.
   </details>

3. **Detect a member:** using `void_t` (or a C++20 `requires`), write
   `has_reserve<T>` that's true if `T` has a `.reserve(size_t)`.

   <details><summary>Answer</summary>

   C++20: `template<class T> concept has_reserve = requires (T t, std::size_t n)
   { t.reserve(n); };`. Pre-C++20: `template<class,class=void> struct has_reserve
   : std::false_type{}; template<class T> struct has_reserve<T,
   std::void_t<decltype(std::declval<T&>().reserve(std::size_t{}))>> :
   std::true_type{};`
   </details>

4. **noexcept move check:** how would you assert at compile time that your type
   `Order` won't be *copied* during `std::vector<Order>` growth?

   <details><summary>Answer</summary>

   `static_assert(std::is_nothrow_move_constructible_v<Order>);` — `std::vector`
   uses `move_if_noexcept`, which picks the move ctor only when it's `noexcept`;
   otherwise it copies (for the strong guarantee).
   </details>

5. **conditional_t:** define `index_t` as `std::uint32_t` if `N <= UINT32_MAX`
   else `std::uint64_t`, at compile time.

   <details><summary>Answer</summary>

   `using index_t = std::conditional_t<(N <= std::numeric_limits<std::uint32_t>
   ::max()), std::uint32_t, std::uint64_t>;`
   </details>

---

## Interview questions

1. Predicate trait vs transformation trait — `_v` vs `_t`?
2. `if constexpr` normal `if` se kaise alag (dead branch instantiation)?
3. `is_trivially_copyable` container/algorithm ko kya batata (fast path)?
4. `is_nothrow_move_constructible` `std::vector` growth ko kaise affect karta?
5. `std::decay_t` kya-kya strip karta?
6. `void_t` detection idiom — kaise kaam karta, C++20 mein kya replace karta?

---

## Next
→ [`22-bit-utilities.md`](22-bit-utilities.md)
