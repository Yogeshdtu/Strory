// 06_spaceship.cpp
// ============================================================
// The three-way comparison operator `<=>` (C++20):
// - `= default` generates all six relational operators
// - comparison categories (strong / weak / partial ordering)
// - custom <=> for domain rules; mixed-type comparison
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 06_spaceship.cpp -o sp && ./sp
// ============================================================

#include <algorithm>
#include <cctype>
#include <cmath>
#include <compare>
#include <cstdio>
#include <string>
#include <vector>

// ---- 1. defaulted <=> -> lexicographic over members, all 6 operators for free ----
struct Version {
    int major, minor, patch;
    auto operator<=>(const Version&) const = default;    // also generates ==, !=, <, <=, >, >=
};

// ---- 2. custom <=> : compare by a derived key, not member-wise ----
struct Price {
    long ticks;                                          // integer price in ticks
    // strong_ordering: total order, and equal means substitutable
    std::strong_ordering operator<=>(const Price& o) const { return ticks <=> o.ticks; }
    bool operator==(const Price& o) const { return ticks == o.ticks; }   // needed: not auto-generated with a custom <=>
};

// ---- 3. partial_ordering : some values are unordered (like NaN) ----
struct Measurement {
    double value;
    std::partial_ordering operator<=>(const Measurement& o) const { return value <=> o.value; }
    bool operator==(const Measurement& o) const { return value == o.value; }
};

// ---- 4. weak_ordering : equivalent but not identical (case-insensitive name) ----
struct CIString {
    std::string s;
    std::weak_ordering operator<=>(const CIString& o) const {
        auto lower = [](std::string x){ for (char& c : x) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return x; };
        return lower(s) <=> lower(o.s);
    }
    bool operator==(const CIString& o) const { return (*this <=> o) == 0; }
};

static const char* name(std::strong_ordering o) {
    return o < 0 ? "less" : o > 0 ? "greater" : "equal";
}

int main() {
    std::printf("=== 1. defaulted <=> gives all 6 operators ===\n");
    Version a{1, 2, 3}, b{1, 3, 0};
    std::printf("  {1,2,3} <  {1,3,0} : %d\n", a < b);
    std::printf("  {1,2,3} == {1,2,3} : %d\n", (a == Version{1, 2, 3}));
    std::printf("  {1,2,3} >= {1,3,0} : %d\n", a >= b);
    std::vector<Version> vs{{2, 0, 0}, {1, 5, 9}, {1, 5, 0}, {2, 0, 1}};
    std::sort(vs.begin(), vs.end());                     // uses the generated operator<
    std::printf("  sorted: ");
    for (auto& v : vs) std::printf("%d.%d.%d ", v.major, v.minor, v.patch);
    std::printf("\n");

    std::printf("\n=== 2. custom <=> by key ===\n");
    Price p1{100}, p2{105}, p3{100};
    std::printf("  100 vs 105 : %s\n", name(p1 <=> p2));
    std::printf("  100 vs 100 : %s\n", name(p1 <=> p3));
    std::printf("  p1 < p2    : %d\n", p1 < p2);

    std::printf("\n=== 3. partial_ordering (NaN is unordered) ===\n");
    Measurement m1{1.0}, m2{2.0}, mn{std::nan("")};
    auto c1 = m1 <=> m2;
    auto c2 = m1 <=> mn;
    std::printf("  1.0 <=> 2.0 : %s\n", c1 < 0 ? "less" : "?");
    std::printf("  1.0 <=> NaN : %s\n", c2 == std::partial_ordering::unordered ? "unordered" : "ordered");
    std::printf("  (1.0 < NaN) = %d, (1.0 >= NaN) = %d   -- both false: no order\n", m1 < mn, m1 >= mn);

    std::printf("\n=== 4. weak_ordering (case-insensitive equivalence) ===\n");
    CIString x{"Hello"}, y{"HELLO"}, w{"World"};
    std::printf("  \"Hello\" == \"HELLO\" : %d  (equivalent, not identical)\n", x == y);
    std::printf("  \"Hello\" <  \"World\" : %d\n", x < w);

    std::printf(
        "\n"
        "  `T operator<=>(const T&) const = default;` -> member-wise lexicographic\n"
        "  comparison + all six relational operators. A custom <=> returns a category:\n"
        "   strong_ordering  : total order, equal == substitutable (ints, keys)\n"
        "   weak_ordering    : total order, but 'equal' means equivalent (case-insensitive)\n"
        "   partial_ordering : some pairs are unordered (floating point + NaN)\n"
        "  With a custom <=> you must still write == yourself (it isn't auto-derived).\n");
    return 0;
}
