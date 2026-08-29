// 06_conversions.cpp
// ============================================================
// Implicit conversions ke saare traps
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 06_conversions.cpp -o conv && ./conv
//
// Aur phir strict warnings ke saath (kitni warnings aati hain dekho):
//   g++ -std=c++20 -Wall -Wextra -Wconversion -Wsign-conversion 06_conversions.cpp -o conv2
// ============================================================

#include <iostream>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <bit>
#include <limits>
#include <vector>

int main() {
    // ============================================================
    //  1. FLOAT -> INT: TRUNCATION (round nahi!)
    // ============================================================
    std::cout << "===== 1. FLOAT -> INT (truncation) =====\n";
    std::cout << "(int)3.99   = " << static_cast<int>(3.99)   << "   <- 4 NAHI!\n";
    std::cout << "(int)3.5    = " << static_cast<int>(3.5)    << "\n";
    std::cout << "(int)-3.99  = " << static_cast<int>(-3.99)  << "  <- zero ki taraf, -4 nahi\n";
    std::cout << "\nRound karne ke liye <cmath> use karo:\n";
    std::cout << "  std::round(3.5) = " << std::round(3.5) << "\n";
    std::cout << "  std::floor(3.9) = " << std::floor(3.9) << "\n";
    std::cout << "  std::ceil(3.1)  = " << std::ceil(3.1)  << "\n";

    // ============================================================
    //  2. BADE -> CHHOTE: WRAP
    // ============================================================
    std::cout << "\n===== 2. BADE -> CHHOTE (wrap) =====\n";
    const int big = 300;
    const auto small = static_cast<char>(big);
    std::cout << "int 300 -> char = " << static_cast<int>(small)
              << "   (300 mod 256 = 44)\n";

    const std::int64_t huge = 5000000000LL;
    const auto truncated = static_cast<std::int32_t>(huge);
    std::cout << "int64_t 5000000000 -> int32_t = " << truncated << "\n";

    // ============================================================
    //  3. SIGNED <-> UNSIGNED: SIGN FLIP
    // ============================================================
    std::cout << "\n===== 3. SIGNED <-> UNSIGNED =====\n";
    const int negative = -1;
    std::cout << "int -1 -> unsigned = " << static_cast<unsigned>(negative)
              << "   <- 4294967295!\n";

    const unsigned bigU = 4000000000u;
    std::cout << "unsigned 4000000000 -> int = " << static_cast<int>(bigU)
              << "  <- implementation-defined\n";

    // Comparison ka bug
    std::cout << "\nCOMPARISON BUG:\n";
    const int a = -1;
    const unsigned b = 1;
    // Compiler SIGNED ko UNSIGNED mein convert karta hai
    std::cout << "  int a = -1; unsigned b = 1;\n";
    std::cout << "  a ko unsigned mein: " << static_cast<unsigned>(a) << "\n";
    std::cout << "  isliye (a < b) FALSE hota hai!\n";
    std::cout << "  Sahi comparison: " << std::boolalpha
              << (a < static_cast<int>(b)) << std::noboolalpha << "\n";

    // ============================================================
    //  4. INTEGER PROMOTION
    // ============================================================
    std::cout << "\n===== 4. INTEGER PROMOTION =====\n";
    // Chhote types arithmetic mein automatically `int` ban jaate hain.
    // Kyunki CPU 32-bit words pe efficiently kaam karti hai.
    const char c1 = 100, c2 = 100;
    std::cout << "sizeof(char)      = " << sizeof(char) << "\n";
    std::cout << "sizeof(c1 + c2)   = " << sizeof(c1 + c2)
              << "   <- INT ban gaya!\n";
    std::cout << "c1 + c2           = " << (c1 + c2)
              << " <- 200 (char mein nahi aata, par int mein aa gaya)\n";

    const short s = 1;
    std::cout << "sizeof(short)     = " << sizeof(short) << "\n";
    std::cout << "sizeof(s + s)     = " << sizeof(s + s) << "\n";
    std::cout << "sizeof(+s)        = " << sizeof(+s)
              << "   <- unary + bhi promote karta hai\n";

    // ============================================================
    //  5. PRECISION LOSS: int -> float
    // ============================================================
    std::cout << "\n===== 5. PRECISION LOSS =====\n";
    const std::int32_t bigInt = 16777217;      // 2^24 + 1
    const float asFloat = static_cast<float>(bigInt);
    std::cout << "int   " << bigInt << " -> float = "
              << std::fixed << asFloat << std::defaultfloat << "\n";
    std::cout << "float mein 24 bits mantissa hai -> 2^24 se bade integers exact nahi\n";
    std::cout << "double mein 53 bits -> 2^53 tak safe\n";

    // ============================================================
    //  6. THE CLASSIC AVERAGE BUG
    // ============================================================
    std::cout << "\n===== 6. CLASSIC AVERAGE BUG =====\n";
    const int correct = 45;
    const int total = 60;

    const double wrong = correct / total * 100;                      // ⚠️ 0
    const double right = static_cast<double>(correct) / total * 100; // ✅ 75

    std::cout << "correct/total*100              = " << wrong
              << "   <- integer division PEHLE hui!\n";
    std::cout << "(double)correct/total*100      = " << right << "  ✅\n";

    // ============================================================
    //  7. SAFE BIT REINTERPRETATION
    // ============================================================
    std::cout << "\n===== 7. BITS KO DOBARA DEKHNA =====\n";
    const float f = 1.0f;

    // ❌ GALAT: reinterpret_cast strict aliasing tod deta hai -> UB
    //    int i = *reinterpret_cast<const int*>(&f);

    // ✅ SAHI 1: memcpy (compiler ise optimize kar deta hai, koi cost nahi)
    std::int32_t viaMemcpy;
    std::memcpy(&viaMemcpy, &f, sizeof(f));

    // ✅ SAHI 2: std::bit_cast (C++20) -- aur yeh constexpr bhi hai
    const auto viaBitCast = std::bit_cast<std::int32_t>(f);

    std::cout << "float 1.0f ke bits:\n";
    std::cout << "  memcpy se:     0x" << std::hex << viaMemcpy << std::dec << "\n";
    std::cout << "  bit_cast se:   0x" << std::hex << viaBitCast << std::dec << "\n";
    std::cout << "  (1.0f = 0x3F800000 IEEE-754 mein)\n";
    std::cout << "\nreinterpret_cast se MAT karo -- woh strict aliasing violate karta hai.\n";

    // ============================================================
    //  8. LOOP INDEX BUG
    // ============================================================
    std::cout << "\n===== 8. LOOP INDEX =====\n";
    const std::vector<int> v = {10, 20, 30};

    std::cout << "❌ for (int i = 0; i < v.size(); ++i)\n";
    std::cout << "   (v.size() unsigned hai -> -Wsign-compare warning)\n";

    std::cout << "\n✅ Sahi tareeke:\n";
    std::cout << "   for (std::size_t i = 0; i < v.size(); ++i)\n";
    for (std::size_t i = 0; i < v.size(); ++i) std::cout << "     v[" << i << "] = " << v[i] << "\n";

    std::cout << "   for (const auto& x : v)   <- BEST\n";
    for (const auto& x : v) std::cout << "     " << x << "\n";

    // ============================================================
    //  9. HFT: SILENT TRUNCATION KA KHATRA
    // ============================================================
    std::cout << "\n===== 9. HFT SCENARIO =====\n";
    const double calculatedPrice = 21500.75;
    const auto priceInTicks = static_cast<std::int64_t>(calculatedPrice);

    std::cout << "Strategy ne price calculate kiya: " << calculatedPrice << "\n";
    std::cout << "int64_t mein convert kiya:        " << priceInTicks
              << "   <- .75 CHUPCHAP GAYA\n";
    std::cout << "\nAgar yeh implicit hota (bina cast ke), compiler kuch nahi bolta.\n";
    std::cout << "Aur aap galat price pe order bhej dete.\n";
    std::cout << "\nSAHI approach: prices ko SHURU SE integer ticks mein rakho.\n";
    std::cout << "Kabhi double se convert karne ki nobat hi na aaye.\n";

    return 0;
}
