// 01_lambdas_all.cpp
// ============================================================
// Every lambda feature: captures (value/ref/init/this), mutable,
// generic (auto params), trailing return, constexpr/consteval,
// immediately-invoked, capturing a pack, and the closure's layout.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 01_lambdas_all.cpp -o l && ./l
// ============================================================

#include <algorithm>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct Widget {
    int id = 7;
    auto make_printer() {
        // [this] captures the pointer -> members accessed via this (beware dangling if Widget dies)
        return [this] { std::printf("  Widget %d\n", id); };
        // [*this] (C++17) would capture a COPY of the whole Widget
    }
};

int main() {
    std::printf("=== 1. capture modes ===\n");
    int a = 10, b = 20;
    auto byVal  = [a, b]      { return a + b; };            // copies a, b at creation
    auto byRef  = [&a, &b]    { return a + b; };            // refers to a, b
    auto byAll  = [=]         { return a + b; };            // capture everything used, by value
    auto byAllR = [&]         { return a + b; };            // ... by reference
    a = 100;
    std::printf("  byVal (snapshot 30)   = %d\n", byVal());
    std::printf("  byRef (live 120)      = %d\n", byRef());

    std::printf("\n=== 2. init capture (C++14) -- create a new member ===\n");
    auto counter = [n = 0]() mutable { return ++n; };       // n lives in the closure; mutable to modify it
    std::printf("  %d %d %d\n", counter(), counter(), counter());
    auto owner = [p = std::make_unique<int>(42)] { return *p; };   // MOVE a move-only object into the closure
    std::printf("  moved-in unique_ptr -> %d\n", owner());

    std::printf("\n=== 3. mutable ===\n");
    auto acc = [total = 0](int x) mutable { total += x; return total; };
    std::printf("  %d %d %d\n", acc(1), acc(2), acc(3));     // 1 3 6 -- state persists in the closure

    std::printf("\n=== 4. generic lambda (auto params) ===\n");
    auto add = [](auto x, auto y) { return x + y; };        // operator() is a template
    std::printf("  add(2,3)=%d  add(1.5,2.5)=%.1f  add(str)=%s\n",
                add(2, 3), add(1.5, 2.5), add(std::string("ab"), "cd").c_str());
    auto first = []<class T>(const std::vector<T>& v) { return v.front(); };   // C++20 explicit template param
    std::printf("  first({9,8,7}) = %d\n", first(std::vector<int>{9, 8, 7}));

    std::printf("\n=== 5. trailing return type ===\n");
    auto div = [](double x, double y) -> double { return y == 0 ? 0.0 : x / y; };
    std::printf("  div(7,2) = %.2f\n", div(7, 2));

    std::printf("\n=== 6. constexpr lambda ===\n");
    constexpr auto sq = [](int x) { return x * x; };        // usable in constant expressions
    static_assert(sq(5) == 25);
    std::printf("  sq(9) = %d\n", sq(9));

    std::printf("\n=== 7. IIFE -- immediately invoked, for complex initialization ===\n");
    const int configured = [] {
        int v = 0;
        for (int i = 1; i <= 10; ++i) v += i;
        return v;                                            // 55 -- init a const with a computation
    }();
    std::printf("  configured = %d\n", configured);

    std::printf("\n=== 8. lambda as an algorithm predicate (inlined, zero overhead) ===\n");
    std::vector<int> v{5, 2, 8, 1, 9, 3};
    std::sort(v.begin(), v.end(), [](int x, int y) { return x > y; });
    std::printf("  sorted desc: ");
    std::for_each(v.begin(), v.end(), [](int x) { std::printf("%d ", x); });
    std::printf("\n");

    std::printf("\n=== 9. [this] capture ===\n");
    Widget w;
    auto pr = w.make_printer();
    pr();

    std::printf("\n=== 10. capturing a parameter pack (C++20) ===\n");
    auto sum_all = [](auto... xs) { return (xs + ...); };
    std::printf("  sum_all(1,2,3,4) = %d\n", sum_all(1, 2, 3, 4));

    std::printf("\n=== 11. closure layout ===\n");
    int x1 = 1; double x2 = 2.0;
    auto clo = [x1, x2] { return x1 + x2; };
    std::printf("  sizeof([x1,x2]{...})    = %zu   (a struct with an int + a double + padding)\n", sizeof(clo));
    auto empty = [] { return 42; };
    std::printf("  sizeof([]{...})         = %zu   (empty -> 1)\n", sizeof(empty));
    std::function<int()> f = empty;
    std::printf("  sizeof(std::function)  = %zu   (type-erased wrapper -- folder 19 file 16)\n", sizeof(f));

    std::printf(
        "\n"
        "  A lambda is a compiler-generated class with a captured-state struct and an\n"
        "  operator(). No-capture -> converts to a function pointer, empty (size 1).\n"
        "  Passed as a template parameter (algorithms) -> inlined, zero cost. Stored in\n"
        "  std::function -> type-erased, indirect call (+ maybe a heap alloc).\n");
    return 0;
}
