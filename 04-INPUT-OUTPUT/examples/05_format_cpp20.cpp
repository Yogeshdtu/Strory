// 05_format_cpp20.cpp
// ============================================================
// std::format (C++20) -- modern, type-safe formatting
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 05_format_cpp20.cpp -o fmt && ./fmt
//
// ZARURAT: GCC 13+, Clang 17+, ya MSVC 19.29+
// Agar nahi hai -> fmtlib use karo (API bilkul same hai):
//   https://github.com/fmtlib/fmt
// ============================================================

#include <iostream>
#include <string>
#include <cstdint>
#include <version>

// Compiler support check
#if defined(__cpp_lib_format) && __cpp_lib_format >= 201907L
  #include <format>
  #define HAS_FORMAT 1
#else
  #define HAS_FORMAT 0
#endif

#if HAS_FORMAT

// ============================================================
//  Custom type ke liye formatter
//  (templates folder 21 mein -- yahan bas pattern dekho)
// ============================================================
struct Price {
    std::int64_t ticks;      // 1 tick = 0.01 rupaye
};

template <>
struct std::formatter<Price> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();          // koi custom spec support nahi kar rahe
    }

    auto format(const Price& p, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "Rs {}.{:02}",
                              p.ticks / 100, p.ticks % 100);
    }
};

int main() {
    std::cout << "===== 1. BASIC =====\n";
    std::cout << std::format("Hello, {}!\n", "World");
    std::cout << std::format("{} + {} = {}\n", 2, 3, 2 + 3);

    std::cout << "\n===== 2. ARGUMENT INDEX =====\n";
    // Arguments ko reorder kar sakte ho -- printf mein yeh possible nahi
    std::cout << std::format("{0} {1} {0}\n", "A", "B");
    std::cout << std::format("{1} pehle, {0} baad mein\n", "X", "Y");

    std::cout << "\n===== 3. ALIGNMENT / FILL =====\n";
    std::cout << std::format("[{:<10}]  left\n",   42);
    std::cout << std::format("[{:>10}]  right\n",  42);
    std::cout << std::format("[{:^10}]  center\n", 42);
    std::cout << std::format("[{:*>10}] fill with *\n", 42);
    std::cout << std::format("[{:05}]      zero-pad\n", 42);

    std::cout << "\n===== 4. NUMBER BASES =====\n";
    std::cout << std::format("dec:    {:d}\n",  255);
    std::cout << std::format("bin:    {:b}\n",  255);
    std::cout << std::format("oct:    {:o}\n",  255);
    std::cout << std::format("hex:    {:x}\n",  255);
    std::cout << std::format("HEX:    {:X}\n",  255);
    std::cout << std::format("0xhex:  {:#x}\n", 255);
    std::cout << std::format("0bbin:  {:#b}\n", 5);

    std::cout << "\n===== 5. FLOATING POINT =====\n";
    const double pi = 3.14159265358979;
    std::cout << std::format("default:   {}\n",      pi);
    std::cout << std::format("{{:.2f}}:    {:.2f}\n",   pi);
    std::cout << std::format("{{:.4f}}:    {:.4f}\n",   pi);
    std::cout << std::format("{{:e}}:      {:e}\n",     pi);
    std::cout << std::format("{{:.2e}}:    {:.2e}\n",   pi);
    std::cout << std::format("{{:10.2f}}:  [{:10.2f}]\n", pi);
    std::cout << std::format("{{:<10.2f}}: [{:<10.2f}]\n", pi);

    std::cout << "\n===== 6. SIGN =====\n";
    std::cout << std::format("{:+}   always sign\n", 42);
    std::cout << std::format("{: }   space for positive\n", 42);
    std::cout << std::format("{:+}   negative\n", -42);

    std::cout << "\n===== 7. STRINGS =====\n";
    std::cout << std::format("[{:>12}]  right\n", "abc");
    std::cout << std::format("[{:<12}]  left\n",  "abc");
    std::cout << std::format("[{:.3}]        truncate to 3\n", "abcdefgh");

    std::cout << "\n===== 8. BOOL =====\n";
    // ⚠️ format mein bool DEFAULT se true/false print hota hai (cout ke ulta!)
    std::cout << std::format("{}    <- default (cout mein 1 aata)\n", true);
    std::cout << std::format("{:d}       <- {{:d}} se number\n", true);

    std::cout << "\n===== 9. DYNAMIC WIDTH/PRECISION =====\n";
    const int w = 12;
    const int p = 3;
    std::cout << std::format("[{:{}.{}f}]  <- width aur precision runtime se\n",
                             pi, w, p);

    std::cout << "\n===== 10. TABLE =====\n";
    std::cout << std::format("{:<12}{:>12}{:>8}{:>14}\n",
                             "Symbol", "Price", "Qty", "Value");
    std::cout << std::string(46, '-') << "\n";

    struct Row { const char* sym; double price; int qty; };
    const Row rows[] = {
        {"NIFTY",     21500.50, 100},
        {"BANKNIFTY", 46200.25,  50},
        {"RELIANCE",   2890.75, 200},
    };
    for (const auto& r : rows) {
        std::cout << std::format("{:<12}{:>12.2f}{:>8}{:>14.2f}\n",
                                 r.sym, r.price, r.qty, r.price * r.qty);
    }

    std::cout << "\n===== 11. CUSTOM FORMATTER =====\n";
    std::cout << std::format("Order price: {}\n", Price{2150050});
    std::cout << std::format("Bid: {} | Ask: {}\n", Price{2150000}, Price{2150100});

    std::cout << "\n===== 12. HEX DUMP =====\n";
    const unsigned char data[] = {0x41, 0x42, 0x00, 0x27, 0x10, 0xFF};
    std::string dump;
    for (unsigned char b : data) {
        dump += std::format("{:02x} ", b);
    }
    std::cout << dump << "\n";

    std::cout << "\n===== 13. COMPILE-TIME SAFETY =====\n";
    std::cout << "Yeh sab COMPILE ERRORS hain (printf mein runtime UB hote):\n";
    std::cout << "  std::format(\"{} {}\", 1)       <- kam arguments\n";
    std::cout << "  std::format(\"{:d}\", \"text\")   <- galat type\n";
    std::cout << "  std::format(\"{:.2f}\", 42)     <- int pe float spec\n";
    std::cout << "\nprintf mein yeh sab CHUPCHAP compile ho jaate hain aur\n";
    std::cout << "runtime pe crash ya garbage dete hain.\n";

    // Uncomment karke error dekho:
    // std::cout << std::format("{} {}\n", 1);
    // std::cout << std::format("{:d}\n", "text");

    std::cout << "\n===== 14. MANIPULATORS SE COMPARISON =====\n";
    std::cout << "Manipulators (verbose, sticky state):\n";
    std::cout << "  cout << fixed << setprecision(2) << setw(12) << price;\n";
    std::cout << "  cout << defaultfloat << setprecision(6);   // reset zaroori\n";
    std::cout << "\nstd::format (ek line, koi state nahi):\n";
    std::cout << "  cout << format(\"{:>12.2f}\", price);\n";

    std::cout << "\n===== 15. HFT NOTE =====\n";
    std::cout << "std::format ek std::string RETURN karta hai\n";
    std::cout << "  -> potential HEAP ALLOCATION\n";
    std::cout << "  -> hot path mein yeh problem hai\n";
    std::cout << "\nAlternatives:\n";
    std::cout << "  std::format_to_n(buffer, size, ...)  <- pre-allocated buffer\n";
    std::cout << "  std::print(...)  (C++23)             <- seedha stream mein\n";
    std::cout << "  Ya best: hot path mein format karo hi mat --\n";
    std::cout << "  raw values queue mein daalo, logger thread format kare.\n";

    // format_to_n demo -- koi heap allocation nahi
    char buf[64];
    auto result = std::format_to_n(buf, sizeof(buf) - 1, "{:.2f}", pi);
    *result.out = '\0';
    std::cout << "\nformat_to_n (no allocation): " << buf << "\n";

    return 0;
}

#else   // HAS_FORMAT == 0

int main() {
    std::cout << "⚠️  <format> is compiler mein available nahi hai.\n\n";
    std::cout << "Chahiye: GCC 13+, Clang 17+, ya MSVC 19.29+\n";
    std::cout << "Check karo: g++ --version\n\n";
    std::cout << "Alternatives:\n";
    std::cout << "  1. Compiler upgrade karo\n";
    std::cout << "  2. fmtlib use karo (API bilkul same hai):\n";
    std::cout << "       sudo apt install libfmt-dev\n";
    std::cout << "       #include <fmt/format.h>\n";
    std::cout << "       fmt::format(\"{}\", x);\n";
    std::cout << "  3. Tab tak <iomanip> use karo (file 07 dekho)\n";
    return 0;
}

#endif
