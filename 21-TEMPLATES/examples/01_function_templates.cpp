// 01_function_templates.cpp
// ============================================================
// Function templates -- deduction, explicit args, overloading,
// non-type params, `auto` return, and why they are zero-overhead.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 01_function_templates.cpp -o ft && ./ft
// ============================================================

#include <array>
#include <cstdio>
#include <string>
#include <type_traits>
#include <vector>

// ---- 1. the basic template + argument deduction ----
template <class T>
T myMax(T a, T b) { return a < b ? b : a; }        // T deduced from the arguments

// ---- 2. multiple type params + a trailing-return-type ----
template <class A, class B>
auto addMixed(A a, B b) -> decltype(a + b) {        // return type = whatever a + b is
    return a + b;
}

// ---- 3. non-type template parameter (a compile-time value) ----
template <class T, std::size_t N>
T sum(const std::array<T, N>& arr) {                // N is part of the type -> known at compile time
    T s{};
    for (std::size_t i = 0; i < N; ++i) s += arr[i];
    return s;
}

// ---- 4. overloading a template with a more specific non-template ----
template <class T>
const char* kind(const T&) { return "generic"; }
const char* kind(int)       { return "int (exact non-template wins)"; }
const char* kind(double)    { return "double"; }

// ---- 5. explicit template argument (deduction can't see the return type) ----
template <class To, class From>
To narrow_cast(From v) { return static_cast<To>(v); }

int main() {
    std::printf("=== 1. deduction ===\n");
    std::printf("  myMax(3, 7)         = %d\n", myMax(3, 7));
    std::printf("  myMax(2.5, 1.5)     = %.1f\n", myMax(2.5, 1.5));
    std::printf("  myMax<double>(3, 2) = %.1f  (explicit T=double, ints convert)\n", myMax<double>(3, 2));
    // myMax(3, 2.5);   // ERROR: T can't be both int and double -- deduction is not conversion

    std::printf("\n=== 2. mixed types, deduced return ===\n");
    std::printf("  addMixed(3, 2.5)      -> %.1f  (double)\n", addMixed(3, 2.5));
    std::printf("  addMixed(2, 3)        -> %d    (int)\n", addMixed(2, 3));
    auto s = addMixed(std::string("ab"), "cd");
    std::printf("  addMixed(string, cstr)-> \"%s\"\n", s.c_str());

    std::printf("\n=== 3. non-type parameter ===\n");
    std::array<int, 4> a4{1, 2, 3, 4};
    std::array<double, 3> d3{0.5, 0.25, 0.25};
    std::printf("  sum(a4) = %d   (N=4 baked into the type)\n", sum(a4));
    std::printf("  sum(d3) = %.2f (N=3)\n", sum(d3));

    std::printf("\n=== 4. overload resolution: exact non-template beats template ===\n");
    std::printf("  kind(42)    : %s\n", kind(42));
    std::printf("  kind(3.14)  : %s\n", kind(3.14));
    std::printf("  kind(\"hi\")  : %s\n", kind("hi"));

    std::printf("\n=== 5. explicit arg where deduction can't help ===\n");
    std::printf("  narrow_cast<int>(3.9)      = %d\n", narrow_cast<int>(3.9));
    std::printf("  narrow_cast<char>(65)      = %c\n", narrow_cast<char>(65));

    std::printf(
        "\n"
        "  A template is a CODE GENERATOR: `myMax<int>` and `myMax<double>` are two\n"
        "  separate functions the compiler stamps out. Each is fully optimized and\n"
        "  inlinable -- no runtime type info, no indirection. Cost is compile time +\n"
        "  binary size, not speed.\n");
    return 0;
}
