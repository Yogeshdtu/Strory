// 05_sfinae.cpp
// ============================================================
// SFINAE -- "Substitution Failure Is Not An Error". A template
// that fails to substitute is quietly removed from the overload
// set instead of being a hard error. Uses: enable_if, detection idiom.
// (C++20 concepts -- file 06 -- replace most of this; know it for old code.)
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 05_sfinae.cpp -o sf && ./sf
// ============================================================

#include <cstdio>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// ---- 1. enable_if: two overloads, mutually exclusive by a trait ----
template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
const char* classify(T) { return "integral"; }

template <class T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
const char* classify(T) { return "floating point"; }

template <class T, std::enable_if_t<!std::is_arithmetic_v<T>, int> = 0>
const char* classify(const T&) { return "non-arithmetic"; }

// ---- 2. enable_if as the RETURN type ----
template <class T>
std::enable_if_t<std::is_pointer_v<T>, bool>
isNull(T p) { return p == nullptr; }

// ---- 3. the detection idiom: does T have a `.size()` member? ----
template <class, class = void>
struct has_size : std::false_type {};

template <class T>
struct has_size<T, std::void_t<decltype(std::declval<const T&>().size())>>
    : std::true_type {};

template <class T>
inline constexpr bool has_size_v = has_size<T>::value;

// use it to pick an implementation (via if constexpr -- SFINAE + modern branch)
template <class C>
std::size_t elementCount(const C& c) {
    if constexpr (has_size_v<C>) return c.size();
    else                        { std::size_t n = 0; for (auto it = std::begin(c); it != std::end(c); ++it) ++n; return n; }
}

// ---- 4. expression SFINAE: only enabled if `a + b` is well-formed ----
template <class A, class B>
auto tryAdd(const A& a, const B& b) -> decltype(a + b) {   // substitution fails if no operator+
    return a + b;
}
const char* tryAdd(...) { return "(not addable)"; }        // C-varargs fallback -- lowest priority

struct NoAdd {};

int main() {
    std::printf("=== 1. enable_if overload set ===\n");
    std::printf("  classify(42)          : %s\n", classify(42));
    std::printf("  classify(3.14)        : %s\n", classify(3.14));
    std::printf("  classify(std::string) : %s\n", classify(std::string("x")));

    std::printf("\n=== 2. enable_if on the return type (pointers only) ===\n");
    int x = 0; int* p = &x; int* np = nullptr;
    std::printf("  isNull(&x)     = %d\n", isNull(p));
    std::printf("  isNull(nullptr)= %d\n", isNull(np));
    // isNull(42);   // ERROR: no matching overload -- 42 isn't a pointer, SFINAE removed the template

    std::printf("\n=== 3. detection idiom: has .size() ? ===\n");
    std::printf("  has_size_v<std::vector<int>> = %d\n", has_size_v<std::vector<int>>);
    std::printf("  has_size_v<int>              = %d\n", has_size_v<int>);
    int carr[5] = {1,2,3,4,5};
    std::printf("  elementCount(vector{1,2,3}) = %zu\n", elementCount(std::vector<int>{1,2,3}));
    std::printf("  elementCount(int[5])        = %zu  (falls back to iterating)\n", elementCount(carr));

    std::printf("\n=== 4. expression SFINAE ===\n");
    std::printf("  tryAdd(2, 3)          = %d\n", tryAdd(2, 3));
    std::printf("  tryAdd(string, cstr)  = \"%s\"\n", std::string(tryAdd(std::string("a"), "b")).c_str());
    std::printf("  tryAdd(NoAdd, NoAdd)  = %s\n", tryAdd(NoAdd{}, NoAdd{}));

    std::printf(
        "\n"
        "  SFINAE: when substituting template args produces an ill-formed SIGNATURE\n"
        "  (not a body error), that candidate is silently dropped, not a compile error.\n"
        "  enable_if gates overloads; void_t + declval detects members/expressions.\n"
        "  C++20 concepts (file 06) do all of this with readable syntax and errors.\n");
    return 0;
}
