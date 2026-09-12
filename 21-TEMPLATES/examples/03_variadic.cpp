// 03_variadic.cpp
// ============================================================
// Variadic templates -- parameter packs, recursion, and C++17
// FOLD EXPRESSIONS (the modern way). Plus perfect-forwarding a pack.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 03_variadic.cpp -o v && ./v
// ============================================================

#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// ---- 1. recursion over a pack (the pre-C++17 way) ----
template <class T>
T sumRec(T v) { return v; }                         // base case: one argument left

template <class T, class... Rest>
T sumRec(T first, Rest... rest) {                   // peel one, recurse on the rest
    return first + sumRec(rest...);
}

// ---- 2. fold expressions (C++17) -- no recursion, no base case ----
template <class... Ts>
auto sumFold(Ts... xs) { return (xs + ...); }       // unary right fold: x0 + (x1 + (x2 + ...))

template <class... Ts>
bool allTrue(Ts... xs) { return (xs && ...); }      // fold over &&

template <class... Ts>
void printAll(const Ts&... xs) {
    ((std::printf("%s ", std::string(xs).c_str())), ...);   // fold over the comma operator
    std::printf("\n");
}

template <class... Ts>
std::size_t count(Ts&&...) { return sizeof...(Ts); }  // sizeof... = pack size

// ---- 3. perfect-forwarding a pack into a constructor (like std::make_unique) ----
template <class T, class... Args>
std::unique_ptr<T> makeUnique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));   // forward each element with its category
}

struct Point {
    int x, y;
    Point(int a, int b) : x(a), y(b) { std::printf("  Point(%d,%d) constructed\n", a, b); }
};

// ---- 4. index a pack at compile time via a fold + a counter ----
template <class... Ts>
void enumerate(const Ts&... xs) {
    int i = 0;
    ((std::printf("  [%d] = %d\n", i++, static_cast<int>(xs))), ...);
}

int main() {
    std::printf("=== 1. recursion over a pack ===\n");
    std::printf("  sumRec(1,2,3,4,5)      = %d\n", sumRec(1, 2, 3, 4, 5));
    std::printf("  sumRec(1.5, 2.5)       = %.1f\n", sumRec(1.5, 2.5));

    std::printf("\n=== 2. fold expressions (C++17) ===\n");
    std::printf("  sumFold(1,2,3,4,5)     = %d\n", sumFold(1, 2, 3, 4, 5));
    std::printf("  allTrue(true,true,1)   = %d\n", allTrue(true, true, 1));
    std::printf("  allTrue(true,false)    = %d\n", allTrue(true, false));
    std::printf("  count(1,'a',2.0,\"x\")   = %zu args\n", count(1, 'a', 2.0, "x"));
    std::printf("  printAll: ");
    printAll(std::string("alpha"), std::string("beta"), std::string("gamma"));

    std::printf("\n=== 3. perfect-forward a pack into a constructor ===\n");
    auto p = makeUnique<Point>(3, 4);                // 3,4 forwarded into Point(int,int)
    std::printf("  p->x=%d p->y=%d\n", p->x, p->y);

    std::printf("\n=== 4. enumerate a pack ===\n");
    enumerate(10, 20, 30, 40);

    std::printf(
        "\n"
        "  A fold expression expands `(pack OP ...)` into `x0 OP x1 OP x2 ...` at\n"
        "  compile time -- no recursion, no base-case overload. `sizeof...(Ts)` gives\n"
        "  the count. std::forward<Args>(args)... forwards each element preserving\n"
        "  its value category (this is how make_unique / emplace work).\n");
    return 0;
}
