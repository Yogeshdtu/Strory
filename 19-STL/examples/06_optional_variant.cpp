// 06_optional_variant.cpp
// ============================================================
// std::optional, std::variant + std::visit  (std::expected -- C++23, lesson mein)
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 06_optional_variant.cpp -o ov && ./ov
// ============================================================
//   std::optional<T> : "T ya kuch nahi" -- a value or nullopt. No heap allocation.
//   std::variant<A,B,C> : "in mein se exactly ek". Tagged union. sizeof = max + tag.
//   std::visit(visitor, variant) : active alternative pe visitor call -- exhaustive.
//   (std::expected<T,E> : "T ya error E" -- C++23; niche comment + lesson 15.)
// ============================================================

#include <cstdio>
#include <optional>
#include <string>
#include <variant>
#include <vector>

// ---- optional: parse that may fail ----
std::optional<int> parsePositive(const std::string& s) {
    try {
        int v = std::stoi(s);
        if (v <= 0) return std::nullopt;          // "no value"
        return v;
    } catch (...) {
        return std::nullopt;
    }
}

// ---- variant: a value that is one of several types ----
using Json = std::variant<std::nullptr_t, bool, double, std::string>;

std::string describe(const Json& j) {
    return std::visit([](const auto& x) -> std::string {   // generic visitor -- one lambda handles all
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T, std::nullptr_t>) return "null";
        else if constexpr (std::is_same_v<T, bool>)      return x ? "bool:true" : "bool:false";
        else if constexpr (std::is_same_v<T, double>)    return "number:" + std::to_string(x);
        else                                             return "string:\"" + x + "\"";
    }, j);
}

int main() {
    std::printf("=== std::optional ===\n");
    {
        for (const char* in : {"42", "-3", "abc", "100"}) {
            auto r = parsePositive(in);
            if (r) std::printf("  parsePositive(\"%s\") = %d\n", in, *r);
            else   std::printf("  parsePositive(\"%s\") = <nullopt>\n", in);
        }
        std::optional<int> o;
        std::printf("  o.has_value()      = %d\n", o.has_value());
        std::printf("  o.value_or(-1)     = %d\n", o.value_or(-1));       // default if empty
        o = 7;
        std::printf("  after o = 7: *o    = %d\n", *o);
        // *o when empty -> UB;  o.value() when empty -> throws std::bad_optional_access
    }

    std::printf("\n=== std::variant + std::visit ===\n");
    {
        std::vector<Json> doc{ nullptr, true, 3.14, std::string("hello") };
        for (const auto& j : doc)
            std::printf("  %s   (index=%zu)\n", describe(j).c_str(), j.index());

        Json j = 2.5;
        std::printf("\n  holds_alternative<double> : %d\n", std::holds_alternative<double>(j));
        std::printf("  get<double>              : %g\n", std::get<double>(j));
        if (auto* p = std::get_if<double>(&j)) std::printf("  get_if<double>           : %g\n", *p);
        // std::get<bool>(j) when j holds double -> throws std::bad_variant_access
        j = std::string("switched");
        std::printf("  after j = string: index  : %zu -> %s\n", j.index(), describe(j).c_str());
    }

    std::printf("\n=== sizeof ===\n");
    std::printf("  sizeof(std::optional<int>)   = %zu   (int + a bool flag + padding)\n",
                sizeof(std::optional<int>));
    std::printf("  sizeof(Json variant)          = %zu   (largest alt (std::string) + tag)\n",
                sizeof(Json));

    std::printf(
        "\n"
        "  optional<T> : maybe-a-value. value_or / has_value / *. No allocation.\n"
        "  variant<...>: closed set of types + tag. visit = exhaustive dispatch\n"
        "                (folder 16: virtual dispatch ka alternative -- often faster).\n"
        "  expected<T,E> (C++23): 'value or error' -- exceptions ke bina error handling:\n"
        "     std::expected<Config,ParseError> load(...);  if (auto r = load(); r) use(*r);\n"
        "     else handle(r.error());\n");
    return 0;
}
