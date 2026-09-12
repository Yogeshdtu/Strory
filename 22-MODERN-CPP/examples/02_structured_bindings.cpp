// 02_structured_bindings.cpp
// ============================================================
// Structured bindings (C++17): unpack a pair / tuple / array /
// struct into named variables. With `if`-with-initializer, map
// iteration, and the by-value vs by-reference distinction.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 02_structured_bindings.cpp -o sb && ./sb
// ============================================================

#include <array>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <tuple>

struct Quote { double px; std::uint32_t qty; std::string sym; };

std::tuple<int, double, std::string> makeRow() { return {1, 2.5, "AAPL"}; }

int main() {
    std::printf("=== 1. unpack a tuple ===\n");
    auto [id, price, name] = makeRow();
    std::printf("  id=%d price=%.2f name=%s\n", id, price, name.c_str());

    std::printf("\n=== 2. unpack a struct (public members, in order) ===\n");
    Quote q{101.25, 300, "MSFT"};
    auto [p, n, s] = q;                         // copies
    std::printf("  px=%.2f qty=%u sym=%s\n", p, n, s.c_str());

    std::printf("\n=== 3. by REFERENCE -- modify the source ===\n");
    auto& [rp, rn, rs] = q;
    rp = 999.99; rn = 1;
    std::printf("  after auto& binding + assign: q.px=%.2f q.qty=%u\n", q.px, q.qty);

    std::printf("\n=== 4. unpack a std::array ===\n");
    std::array<int, 3> arr{7, 8, 9};
    auto [x, y, z] = arr;
    std::printf("  %d %d %d\n", x, y, z);

    std::printf("\n=== 5. map iteration -- [key, value] ===\n");
    std::map<std::string, int> counts{{"a", 3}, {"b", 5}, {"c", 2}};
    for (const auto& [key, val] : counts)
        std::printf("  %s -> %d\n", key.c_str(), val);
    for (auto& [key, val] : counts) val *= 10;   // modify values in place
    std::printf("  after *=10: b -> %d\n", counts["b"]);

    std::printf("\n=== 6. if-with-initializer + structured binding ===\n");
    if (auto [it, inserted] = counts.insert({"d", 1}); inserted)
        std::printf("  inserted d -> %d\n", it->second);
    if (auto [it, inserted] = counts.insert({"d", 99}); !inserted)
        std::printf("  d already present -> %d (insert did nothing)\n", it->second);

    std::printf("\n=== 7. std::tie -- assign into EXISTING variables ===\n");
    int a2 = 0; double b2 = 0; std::string c2;
    std::tie(a2, b2, c2) = makeRow();
    std::printf("  a2=%d b2=%.2f c2=%s\n", a2, b2, c2.c_str());
    int lo, rem;
    std::tie(lo, rem) = std::pair{17 / 5, 17 % 5};   // ignore parts with std::ignore
    std::printf("  17/5 = %d rem %d\n", lo, rem);

    std::printf(
        "\n"
        "  `auto [a,b,c] = expr;` introduces names that ALIAS members of a hidden copy\n"
        "  (or, with `auto&`, of the source). Not new objects -- you can't capture them\n"
        "  in a lambda pre-C++20, and their types come from the source. Great for tuple\n"
        "  returns, map iteration, and `if (auto [it, ok] = m.insert(...); ok)`.\n");
    return 0;
}
