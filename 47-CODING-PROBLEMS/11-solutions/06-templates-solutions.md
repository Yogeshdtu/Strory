# 06 — Templates: worked solutions

Poore code — traits, detection, CRTP, variadic builds. Baaki
`06-templates-problems.md` ke `<details>` blocks mein.

---

## B3 — CRTP static polymorphism

```cpp
#include <cstdio>

template <class Derived>
struct Shape {
    double area() const { return static_cast<const Derived*>(this)->area_impl(); }
    void describe() const { std::printf("area = %.3f\n", area()); }   // shared, non-virtual
};

struct Circle : Shape<Circle> {
    double r;
    explicit Circle(double r) : r(r) {}
    double area_impl() const { return 3.14159265358979 * r * r; }
};
struct Square : Shape<Square> {
    double s;
    explicit Square(double s) : s(s) {}
    double area_impl() const { return s * s; }
};

template <class S>
double total_area(const Shape<S>* items, int n) {   // templated over one concrete S
    double t = 0;
    for (int i = 0; i < n; ++i) t += items[i].area();   // inlinable, no vtable
    return t;
}
```

`Shape<Circle>` aur `Shape<Square>` **alag base types** → ek `Shape*` container
nahi ban sakta. Use jab call site pe concrete type pata ho (templated algorithm).
Zero indirection, fully inlinable. Runtime heterogeneity chahiye → virtual + LTO
devirtualization, ya `std::variant<Circle, Square>`.

---

## B8 — Detect `.reserve()`

```cpp
#include <type_traits>
#include <utility>

// C++20 — the easy way
template <class C>
void maybe_reserve(C& c, std::size_t n) {
    if constexpr (requires { c.reserve(n); }) c.reserve(n);
}

// Pre-C++20 — void_t detection idiom
template <class, class = void>
struct has_reserve : std::false_type {};
template <class C>
struct has_reserve<C, std::void_t<decltype(std::declval<C&>().reserve(std::size_t{}))>>
    : std::true_type {};
template <class C>
inline constexpr bool has_reserve_v = has_reserve<C>::value;
// static_assert(has_reserve_v<std::vector<int>>);
// static_assert(!has_reserve_v<std::list<int>>);
```

`requires { expr; }` — C++20 concept-lite, "kya yeh expression well-formed hai".
Pre-20: SFINAE via `void_t` — agar `decltype(...)` valid, partial specialization
match hoti (`true_type`), warna primary (`false_type`).

---

## C1 — Minimal `tuple`

```cpp
#include <utility>

template <class... Ts> struct Tuple {};

template <class Head, class... Tail>
struct Tuple<Head, Tail...> : Tuple<Tail...> {
    Head head;
    Tuple() = default;
    template <class H, class... T>
    explicit Tuple(H&& h, T&&... t)
        : Tuple<Tail...>(std::forward<T>(t)...), head(std::forward<H>(h)) {}
};
template <class... Ts> Tuple(Ts...) -> Tuple<Ts...>;   // CTAD

// get<N> — recurse into the N-th base, return that level's `head`
template <std::size_t N, class Head, class... Tail>
auto& get(Tuple<Head, Tail...>& t) {
    if constexpr (N == 0) return t.head;
    else return get<N - 1>(static_cast<Tuple<Tail...>&>(t));
}

// TupleElement<N, Ts...>::type — the type at index N
template <std::size_t N, class Head, class... Tail>
struct TupleElement { using type = typename TupleElement<N - 1, Tail...>::type; };
template <class Head, class... Tail>
struct TupleElement<0, Head, Tail...> { using type = Head; };
```

Recursive inheritance: har level ek `Head` add karta. `get<N>` `N` bases neeche
`static_cast` karke us level ka `head` return karta. EBO se empty types free.
Real `std::tuple` ~C++ committee-grade — yeh core idea hai.

---

## C3 — `for_each_in_tuple`

```cpp
#include <tuple>
#include <utility>

template <class Tup, class F, std::size_t... I>
void for_each_impl(Tup&& t, F&& f, std::index_sequence<I...>) {
    (f(std::get<I>(std::forward<Tup>(t))), ...);        // fold over comma
}
template <class Tup, class F>
void for_each_in_tuple(Tup&& t, F&& f) {
    for_each_impl(std::forward<Tup>(t), std::forward<F>(f),
                  std::make_index_sequence<std::tuple_size_v<std::decay_t<Tup>>>{});
}

// C++17 one-liner alternative:
//   std::apply([&](auto&&... xs){ (f(xs), ...); }, t);
```

`std::index_sequence<0,1,2,...>` ko ek pack ke roop mein "unpack" karke fold
expression `(f(get<I>(t)), ...)` — har element pe `f`. Yeh pattern har
tuple-algorithm ka base (`transform`, `apply`, structured serialization).

---

## A1 — Generic max, and the dangling-reference trap

```cpp
template <class T>
const T& maxv(const T& a, const T& b) { return a < b ? b : a; }

// TRAP:
//   const int& r = maxv(3, 4);         // r binds to a temporary's lifetime? NO —
//   // the temporaries 3 and 4 die at the end of the full expression.
//   // r is dangling. std::max has the exact same footgun.
//
// Safe: use the value, don't store the reference:
//   int m = maxv(3, 4);                // fine
```

Return-by-`const&` efficient for big `T`, but if the arguments are prvalues and
you *bind a reference* to the result, that reference dangles once the full
expression ends. `std::max({a, b, c})` (initializer_list overload) and
`std::ranges::max` have documented lifetime rules — hand-rolled ones don't.
