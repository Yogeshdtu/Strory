// 06_concepts.cpp
// ============================================================
// C++20 concepts -- named, readable constraints that replace SFINAE.
// requires-clauses, requires-expressions, subsumption (best-match),
// and the clean compiler errors you get vs enable_if.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 06_concepts.cpp -o cc && ./cc
// ============================================================

#include <concepts>
#include <cstdio>
#include <string>
#include <vector>

// ---- 1. define a concept with a requires-expression ----
template <class T>
concept Numeric = std::integral<T> || std::floating_point<T>;

template <class T>
concept Addable = requires (T a, T b) {
    { a + b } -> std::convertible_to<T>;      // a + b must compile AND be convertible to T
};

template <class T>
concept Sized = requires (const T& c) {
    { c.size() } -> std::convertible_to<std::size_t>;
};

// ---- 2. constrain a function three equivalent ways ----
template <Numeric T>                                   // (a) type-constraint on the template param
T doubleIt(T x) { return x + x; }

template <class T> requires Addable<T>                 // (b) trailing requires-clause
T addSelf(T x) { return x + x; }

auto triple(Numeric auto x) { return x + x + x; }      // (c) constrained `auto` parameter

// ---- 3. subsumption: the MORE constrained overload wins ----
template <class T> requires std::integral<T>
const char* best(T) { return "integral overload"; }

template <class T> requires (std::integral<T> && sizeof(T) >= 4)
const char* best(T) { return "wide-integral overload (more constrained -> preferred)"; }

// ---- 4. a generic algorithm constrained on a concept ----
template <Sized C>
void report(const C& c) { std::printf("  size() = %zu\n", c.size()); }

int main() {
    std::printf("=== 1. Numeric / Addable / Sized concepts ===\n");
    static_assert(Numeric<int> && Numeric<double> && !Numeric<std::string>);
    static_assert(Addable<int> && Addable<std::string>);
    static_assert(Sized<std::vector<int>> && Sized<std::string> && !Sized<int>);
    std::printf("  (all static_asserts passed)\n");

    std::printf("\n=== 2. three ways to constrain ===\n");
    std::printf("  doubleIt(21)        = %d\n", doubleIt(21));
    std::printf("  addSelf(string(ab)) = \"%s\"\n", addSelf(std::string("ab")).c_str());
    std::printf("  triple(1.5)         = %.1f\n", triple(1.5));
    // doubleIt(std::string("x"));   // ERROR: constraint "Numeric<std::string>" not satisfied -- CLEAR message

    std::printf("\n=== 3. subsumption picks the more constrained overload ===\n");
    std::printf("  best(short{})  : %s\n", best(short{}));       // sizeof(short)=2 -> plain integral overload
    std::printf("  best(int{})    : %s\n", best(int{}));         // sizeof(int)>=4 -> wide overload wins
    std::printf("  best(long{})   : %s\n", best(long{}));

    std::printf("\n=== 4. constrained algorithm ===\n");
    report(std::vector<int>{1, 2, 3, 4});
    report(std::string("hello"));
    // report(42);   // ERROR: constraint Sized<int> not satisfied

    std::printf(
        "\n"
        "  A concept is a named bool over types, checkable with static_assert and\n"
        "  usable as `template <Concept T>`, `requires Concept<T>`, or `Concept auto`.\n"
        "  Overload resolution prefers the MORE constrained candidate (subsumption).\n"
        "  Failing a constraint gives a one-line error, not a wall of SFINAE noise.\n");
    return 0;
}
