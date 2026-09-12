// 04_if_constexpr.cpp
// ============================================================
// `if constexpr` -- compile-time branching. The DISCARDED branch
// is not instantiated, so it may contain code that wouldn't compile
// for the current T. Replaces tag dispatch and most SFINAE.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 04_if_constexpr.cpp -o ic && ./ic
// ============================================================

#include <cstdio>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

// ---- 1. one function, compile-time-chosen behaviour per type ----
template <class T>
void describe(const T& v) {
    if constexpr (std::is_integral_v<T>) {
        std::printf("  integral: %lld\n", static_cast<long long>(v));
    } else if constexpr (std::is_floating_point_v<T>) {
        std::printf("  float   : %.3f\n", static_cast<double>(v));
    } else if constexpr (std::is_pointer_v<T>) {
        std::printf("  pointer : %p -> deref %d\n", static_cast<const void*>(v), *v);   // *v only compiled for pointer T
    } else if constexpr (std::is_same_v<T, std::string>) {
        std::printf("  string  : \"%s\" (len %zu)\n", v.c_str(), v.size());
    } else {
        std::printf("  other\n");
    }
}

// ---- 2. a serialize() that memcpy's trivially-copyable types, else calls .bytes() ----
struct Trivial { int a; double b; };
struct Custom  { std::string s; std::vector<std::byte> bytes() const {
    return { reinterpret_cast<const std::byte*>(s.data()),
             reinterpret_cast<const std::byte*>(s.data()) + s.size() }; } };

template <class T>
std::vector<std::byte> serialize(const T& v) {
    if constexpr (std::is_trivially_copyable_v<T>) {
        std::vector<std::byte> out(sizeof(T));
        std::memcpy(out.data(), &v, sizeof(T));         // fast path -- not compiled for Custom
        return out;
    } else {
        return v.bytes();                               // general path -- not compiled for Trivial
    }
}

// ---- 3. recursion terminated by `if constexpr` (compile-time) ----
template <std::size_t N>
constexpr std::size_t factorial() {
    if constexpr (N <= 1) return 1;
    else                  return N * factorial<N - 1>();
}

int main() {
    std::printf("=== 1. describe() picks a branch at compile time ===\n");
    int i = 42; double d = 3.14159; std::string s = "hello"; int* p = &i;
    describe(i);
    describe(d);
    describe(p);
    describe(s);
    describe('c');    // integral

    std::printf("\n=== 2. serialize(): trivial -> memcpy, custom -> .bytes() ===\n");
    auto a = serialize(Trivial{7, 2.5});
    auto b = serialize(Custom{"payload"});
    std::printf("  serialize(Trivial) -> %zu bytes\n", a.size());
    std::printf("  serialize(Custom)  -> %zu bytes\n", b.size());

    std::printf("\n=== 3. compile-time factorial via if constexpr ===\n");
    std::printf("  factorial<5>()  = %zu\n", factorial<5>());
    std::printf("  factorial<10>() = %zu\n", factorial<10>());
    static_assert(factorial<6>() == 720);

    std::printf(
        "\n"
        "  `if constexpr (cond)`: the FALSE branch is parsed but NOT instantiated for\n"
        "  the current T -- so `*v` is fine even when T isn't a pointer, and memcpy is\n"
        "  fine even when T has a std::string. This replaces tag dispatch and a lot of\n"
        "  SFINAE with a plain if.\n");
    return 0;
}
