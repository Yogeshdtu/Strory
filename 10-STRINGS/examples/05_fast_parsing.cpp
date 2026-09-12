// 05_fast_parsing.cpp
// ============================================================
// Number parsing -- std::from_chars vs std::stoi vs atoi vs stringstream
// ============================================================
//   BENCHMARK -> -O2 ZAROORI.
//   g++ -std=c++20 -O2 05_fast_parsing.cpp -o fp && ./fp
// ============================================================
// std::from_chars (C++17):
//   - NO allocation, NO locale, NO exceptions
//   - strict: "42abc" -> parses 42, tells you it stopped at 'a'
//   - fastest standard integer/float parser
// vs:
//   std::stoi     -> throws, builds a std::string sometimes, locale-aware
//   std::atoi     -> no error reporting, UB on overflow
//   std::stringstream -> allocates, locale, VERY slow
// ============================================================

#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

int main() {
    // test data: 1 lakh number strings
    std::vector<std::string> data;
    data.reserve(100'000);
    for (int i = 0; i < 100'000; ++i)
        data.push_back(std::to_string((i * 2654435761u) % 1'000'000));

    constexpr int REPS = 60;
    auto ms = [](auto a, auto b) {
        return std::chrono::duration<double, std::milli>(b - a).count();
    };

    long long sumFC = 0, sumStoi = 0, sumAtoi = 0, sumSS = 0;   // ek pass ka sum

    // ---- from_chars ----
    auto t0 = std::chrono::steady_clock::now();
    for (int r = 0; r < REPS; ++r) {
        long long s = 0;
        for (const std::string& str : data) {
            int v = 0;
            std::from_chars(str.data(), str.data() + str.size(), v);
            s += v;
        }
        sumFC = s;
    }
    auto t1 = std::chrono::steady_clock::now();

    // ---- stoi ----
    for (int r = 0; r < REPS; ++r) {
        long long s = 0;
        for (const std::string& str : data) s += std::stoi(str);
        sumStoi = s;
    }
    auto t2 = std::chrono::steady_clock::now();

    // ---- atoi ----
    for (int r = 0; r < REPS; ++r) {
        long long s = 0;
        for (const std::string& str : data) s += std::atoi(str.c_str());
        sumAtoi = s;
    }
    auto t3 = std::chrono::steady_clock::now();

    // ---- stringstream ----  (kam reps -- bahut slow hai)
    constexpr int SS_REPS = 6;
    for (int r = 0; r < SS_REPS; ++r) {
        long long s = 0;
        for (const std::string& str : data) {
            std::istringstream iss(str);
            int v = 0; iss >> v;
            s += v;
        }
        sumSS = s;
    }
    auto t4 = std::chrono::steady_clock::now();

    const double fcMs   = ms(t0, t1) / REPS;
    const double stoiMs = ms(t1, t2) / REPS;
    const double atoiMs = ms(t2, t3) / REPS;
    const double ssMs   = ms(t3, t4) / SS_REPS;

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "100k number strings, per-pass time:\n\n";
    std::cout << "  std::from_chars   : " << std::setw(8) << fcMs   << " ms   (1.0x baseline)\n";
    std::cout << "  std::stoi         : " << std::setw(8) << stoiMs << " ms   ("
              << std::setprecision(1) << (stoiMs / fcMs) << "x)\n" << std::setprecision(3);
    std::cout << "  std::atoi         : " << std::setw(8) << atoiMs << " ms   ("
              << std::setprecision(1) << (atoiMs / fcMs) << "x)\n" << std::setprecision(3);
    std::cout << "  std::stringstream : " << std::setw(8) << ssMs   << " ms   ("
              << std::setprecision(1) << (ssMs / fcMs) << "x)  <- allocates + locale\n"
              << std::setprecision(3);

    const bool match = (sumFC == sumStoi) && (sumFC == sumAtoi) && (sumFC == sumSS);
    std::cout << "  (per-pass checksums " << (match ? "match" : "DIFFER") << ")\n";

    // ---- from_chars: strict + error-reporting demo ----
    std::cout << "\n===== from_chars strictness =====\n";
    for (std::string_view s : {"42", "42abc", "abc", "  7", "999999999999999999"}) {
        int v = 0;
        auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
        std::cout << "  \"" << s << "\" -> ";
        if (ec == std::errc{})                         std::cout << "value " << v
                                                                  << ", stopped at index " << (ptr - s.data());
        else if (ec == std::errc::invalid_argument)    std::cout << "invalid (no digits)";
        else if (ec == std::errc::result_out_of_range) std::cout << "out of range for int";
        std::cout << "\n";
    }

    std::cout <<
        "\n  HFT: market data / FIX / order entry parsing = HOT PATH.\n"
        "  std::from_chars (ya hand-rolled) -- allocation-free, exception-free,\n"
        "  branch-light. std::stoi / stringstream hot path se BAN. Folder 38.\n";

    return 0;
}
