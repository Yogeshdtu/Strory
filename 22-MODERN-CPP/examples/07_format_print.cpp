// 07_format_print.cpp
// ============================================================
// std::format (C++20) -- type-safe, positional, formatted output.
// Alignment, precision, bases, a custom formatter. std::print is
// C++23 (may be missing here) -> we use std::format + fputs.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 07_format_print.cpp -o fp && ./fp
// ============================================================

#include <cstdio>
#include <format>
#include <string>
#include <vector>

// ---- a custom type + its std::formatter specialization ----
struct Price { long ticks; int tickSize; };

template <>
struct std::formatter<Price> {
    // parse the format-spec after the ':' (we accept an optional 'c' for "compact")
    bool compact = false;
    constexpr auto parse(std::format_parse_context& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it == 'c') { compact = true; ++it; }
        return it;                                    // must point at '}'
    }
    auto format(const Price& p, std::format_context& ctx) const {
        double val = static_cast<double>(p.ticks) * p.tickSize / 10000.0;
        return compact ? std::format_to(ctx.out(), "{:.2f}", val)
                       : std::format_to(ctx.out(), "${:.4f} ({} ticks)", val, p.ticks);
    }
};

static void print(const std::string& s) { std::fputs(s.c_str(), stdout); }

int main() {
    print(std::format("=== 1. basics ===\n"));
    print(std::format("  int={} float={} str={} bool={}\n", 42, 3.14159, "hi", true));
    print(std::format("  positional: {0} {1} {0}\n", "A", "B"));            // A B A

    print(std::format("\n=== 2. alignment & width ===\n"));
    print(std::format("  |{:<10}|{:>10}|{:^10}|\n", "left", "right", "center"));
    print(std::format("  |{:*<10}|{:0>10}|{:->10}|\n", "pad", 42, "x"));    // fill chars

    print(std::format("\n=== 3. numbers ===\n"));
    print(std::format("  {:.3f}  {:e}  {:g}\n", 3.14159265, 123456.0, 0.0001));
    print(std::format("  hex={:#x} oct={:#o} bin={:#b} dec={:d}\n", 255, 255, 255, 255));
    print(std::format("  {:+}  {: }  {:08.2f}\n", 7, 7, 3.5));               // sign control, zero-pad
    print(std::format("  {:>8}  {:>8}\n", 1234567, -42));

    print(std::format("\n=== 4. a table ===\n"));
    struct Row { const char* sym; double px; long qty; };
    Row rows[] = {{"AAPL", 191.24, 1500}, {"MSFT", 402.11, 300}, {"NVDA", 875.50, 42}};
    print(std::format("  {:<6}{:>12}{:>10}\n", "sym", "price", "qty"));
    for (auto& r : rows)
        print(std::format("  {:<6}{:>12.2f}{:>10}\n", r.sym, r.px, r.qty));

    print(std::format("\n=== 5. custom formatter ===\n"));
    Price p{1912400, 1};
    print(std::format("  default: {}\n", p));
    print(std::format("  compact: {:c}\n", p));

    print(std::format("\n=== 6. format into a preallocated buffer (no allocation) ===\n"));
    {
        char buf[64];
        auto res = std::format_to_n(buf, sizeof(buf) - 1, "px={:.2f} qty={}", 101.25, 300);
        *res.out = '\0';
        print(std::format("  {} (wrote {} chars)\n", buf, res.size));
    }

    print(
        "\n"
        "  std::format: type-safe (a bad type is a COMPILE error, not UB like printf),\n"
        "  positional args, rich spec mini-language, and custom formatters via a\n"
        "  std::formatter<T> specialization. std::format_to_n writes into your buffer\n"
        "  with no allocation. std::print / std::println are C++23.\n");
    return 0;
}
