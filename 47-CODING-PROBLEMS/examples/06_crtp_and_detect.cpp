// 06_crtp_and_detect.cpp
// ============================================================
// Folder 47 file 06: CRTP static polymorphism (no vtable) + compile-time
// capability detection (requires-expression and the void_t idiom).
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g -O0 06_crtp_and_detect.cpp -o t && ./t
// ============================================================

#include <cassert>
#include <cmath>
#include <cstdio>
#include <list>
#include <type_traits>
#include <utility>
#include <vector>

// ---- B3: CRTP ------------------------------------------------
template <class Derived>
struct Shape {
    double area() const { return static_cast<const Derived*>(this)->area_impl(); }
    bool   bigger_than(double threshold) const { return area() > threshold; }   // shared, non-virtual
};

struct Circle : Shape<Circle> {
    double r;
    explicit Circle(double radius) : r(radius) {}
    double area_impl() const { return 3.14159265358979 * r * r; }
};
struct Square : Shape<Square> {
    double s;
    explicit Square(double side) : s(side) {}
    double area_impl() const { return s * s; }
};

template <class T>
double total_area(const T* items, std::size_t n) {   // T is ONE concrete shape type
    double t = 0.0;
    for (std::size_t i = 0; i < n; ++i) t += items[i].area();   // inlinable, no indirect call
    return t;
}
// NOTE: the param is `const T*` (the concrete type), NOT `const Shape<T>*` —
// indexing through a base pointer would stride by sizeof(base)==1 (the classic
// "array of derived through a base pointer" bug).

// ---- B8: detect .reserve() ---------------------------------
template <class C>
void maybe_reserve(C& c, std::size_t n) {
    if constexpr (requires { c.reserve(n); }) c.reserve(n);
    // else: no-op (e.g. std::list has no reserve)
}

// pre-C++20 void_t detection idiom, for contrast
template <class, class = void>
struct has_reserve : std::false_type {};
template <class C>
struct has_reserve<C, std::void_t<decltype(std::declval<C&>().reserve(std::size_t{}))>>
    : std::true_type {};

int main() {
    // ---- CRTP: sizeof has no vtable pointer ----
    static_assert(sizeof(Circle) == sizeof(double), "CRTP adds no vtable pointer");
    static_assert(sizeof(Square) == sizeof(double), "CRTP adds no vtable pointer");
    static_assert(std::is_trivially_copyable_v<Circle>);

    Circle circles[3] = { Circle{1.0}, Circle{2.0}, Circle{3.0} };
    const double got = total_area(circles, 3);
    const double want = 3.14159265358979 * (1.0 + 4.0 + 9.0);
    assert(std::fabs(got - want) < 1e-9);

    Square sq{4.0};
    assert(std::fabs(sq.area() - 16.0) < 1e-9);
    assert(sq.bigger_than(10.0) && !sq.bigger_than(20.0));

    // ---- capability detection ----
    static_assert(has_reserve<std::vector<int>>::value);
    static_assert(!has_reserve<std::list<int>>::value);

    std::vector<int> v;
    maybe_reserve(v, 128);
    assert(v.capacity() >= 128);                 // reserve was called

    std::list<int> lst;
    maybe_reserve(lst, 128);                      // compiles, does nothing
    assert(lst.empty());

    std::puts("06_crtp_and_detect: ALL PASS");
    return 0;
}

// ============================================================
// TALKING POINTS
//   - CRTP: Base calls Derived's _impl via static_cast<Derived*> — resolved at
//     compile time, fully inlinable, no vtable, no indirect-call mispredict.
//   - Cost: Shape<Circle> and Shape<Square> are DIFFERENT types — no common
//     Shape* container. Use CRTP when the concrete type is known at the call
//     site (a templated algorithm). Runtime heterogeneity -> virtual + LTO
//     devirtualization, or std::variant.
//   - requires{ c.reserve(n); } (C++20) vs the void_t partial-specialization
//     idiom (C++11/17): same idea — "is this expression well-formed?".
// ============================================================
