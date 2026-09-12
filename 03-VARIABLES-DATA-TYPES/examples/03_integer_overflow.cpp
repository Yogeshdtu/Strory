// 03_integer_overflow.cpp
// ============================================================
// Integer overflow, signed/unsigned traps, division traps
// ============================================================
// YEH IS FOLDER KA SABSE IMPORTANT EXAMPLE HAI.
// In bugs se real production systems toote hain aur real paisa doobta hai.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 03_integer_overflow.cpp -o ovf && ./ovf
//
// Aur phir UBSan ke saath (signed overflow pakadne ke liye):
//   g++ -std=c++20 -fsanitize=undefined -g 03_integer_overflow.cpp -o ovf_san
//   ./ovf_san
// ============================================================

#include <iostream>
#include <limits>
#include <cstdint>
#include <vector>

int main() {
    // ============================================================
    //  1. UNSIGNED OVERFLOW -- yeh WELL-DEFINED hai
    // ============================================================
    std::cout << "===== 1. UNSIGNED OVERFLOW (defined) =====\n";

    // Standard kehta hai: unsigned arithmetic modulo 2^N hoti hai.
    // Matlab wrap around GUARANTEED hai -- yeh UB nahi hai.
    std::uint8_t small = 255;                      // 8-bit max
    std::cout << "uint8_t max        = " << +small << "\n";
    small = static_cast<std::uint8_t>(small + 1);  // 256 mod 256 = 0
    std::cout << "max + 1            = " << +small << "  <- wrap to 0\n";

    unsigned int u = 0;
    std::cout << "unsigned 0 - 1     = " << (u - 1) << "  <- wrap to max!\n";
    std::cout << "  (yeh " << std::numeric_limits<unsigned>::max() << " hai)\n";

    // Yeh predictable hai, isliye hash functions, checksums,
    // aur circular buffers ismein isi pe chalte hain.

    // ============================================================
    //  2. SIGNED OVERFLOW -- yeh UNDEFINED BEHAVIOUR hai ⚠️
    // ============================================================
    std::cout << "\n===== 2. SIGNED OVERFLOW (UNDEFINED!) =====\n";
    std::cout << "int max = " << std::numeric_limits<int>::max() << "\n";
    std::cout << "int max + 1 = UNDEFINED BEHAVIOUR -- yahan demo NAHI kar rahe.\n";
    std::cout << "\nKya ho sakta hai:\n";
    std::cout << "  - shayad wrap around ho (-2147483648)\n";
    std::cout << "  - shayad program crash ho\n";
    std::cout << "  - shayad compiler poora code hi HATA de\n";
    std::cout << "  - shayad kuch aur ho\n";
    std::cout << "\nUBSan se pakdo: g++ -fsanitize=undefined\n";

    // GALAT -- kabhi mat likhna:
    //   int x = std::numeric_limits<int>::max();
    //   x = x + 1;                                   // ⚠️ UB

    // SAHI tareeke:
    int a = 2000000000;
    int b = 2000000000;

    // (a) Bada type use karo
    std::int64_t safeSum = static_cast<std::int64_t>(a) + b;
    std::cout << "\nSAHI: int64_t mein karo -> " << safeSum << "\n";

    // (b) Compiler builtin se check karo
    int result;
    if (__builtin_add_overflow(a, b, &result)) {
        std::cout << "SAHI: __builtin_add_overflow ne OVERFLOW detect kiya\n";
    }

    // (c) Pehle se check karo
    if (a > 0 && b > std::numeric_limits<int>::max() - a) {
        std::cout << "SAHI: manual check ne overflow detect kiya\n";
    }

    // ============================================================
    //  3. UB KA ASLI KHATRA -- compiler code hata deta hai
    // ============================================================
    std::cout << "\n===== 3. UB SE COMPILER KYA KARTA HAI =====\n";
    std::cout << "Yeh function dekho:\n";
    std::cout << "    bool check(int x) { return x + 1 > x; }\n";
    std::cout << "\nMath mein yeh HAMESHA true hai.\n";
    std::cout << "Par x = INT_MAX pe overflow hoga -> UB.\n";
    std::cout << "Compiler sochta hai: 'UB nahi ho sakta, matlab yeh hamesha true hai'\n";
    std::cout << "-> aur poora check HATA deta hai (-O2 pe).\n";
    std::cout << "\n-O0 aur -O2 pe alag output mil sakta hai. Yeh LEGAL hai.\n";

    // ============================================================
    //  4. SIGNED/UNSIGNED COMPARISON -- classic bug
    // ============================================================
    std::cout << "\n===== 4. SIGNED/UNSIGNED COMPARISON =====\n";

    int negative = -1;
    unsigned int positive = 1;

    // Jab signed aur unsigned compare hote hain (same rank ke),
    // SIGNED ko UNSIGNED mein convert kiya jaata hai.
    // -1 unsigned mein 4294967295 ban jaata hai!
    std::cout << "int a = -1, unsigned b = 1;\n";
    std::cout << "  a < b  ka result: "
              << (negative < static_cast<int>(positive) ? "true (sahi)" : "")
              << "\n";
    std::cout << "  (-1) ko unsigned mein: "
              << static_cast<unsigned int>(negative) << "  <- YEH problem hai\n";
    std::cout << "\nIsliye: comparisons mein signed aur unsigned MIX MAT KARO.\n";
    std::cout << "-Wsign-compare warning ON rakho.\n";

    // ============================================================
    //  5. THE EMPTY VECTOR INFINITE LOOP
    // ============================================================
    std::cout << "\n===== 5. EMPTY CONTAINER TRAP =====\n";
    std::vector<int> empty;
    std::cout << "vector khali hai, size() = " << empty.size() << "\n";
    std::cout << "empty.size() - 1 = " << (empty.size() - 1) << "  <- 0 - 1 wrap!\n";
    std::cout << "\nYeh loop INFINITE hoga:\n";
    std::cout << "    for (size_t i = v.size() - 1; i >= 0; --i)\n";
    std::cout << "Kyunki:\n";
    std::cout << "  (a) 0 - 1 = " << (empty.size() - 1) << " (wrap around)\n";
    std::cout << "  (b) size_t kabhi negative nahi ho sakta, to i >= 0 HAMESHA true\n";
    std::cout << "\nSAHI tareeke:\n";
    std::cout << "  for (const auto& x : v)                  // range-based (best)\n";
    std::cout << "  for (size_t i = 0; i < v.size(); ++i)    // forward\n";
    std::cout << "  for (auto i = std::ssize(v)-1; i>=0; --i) // C++20 signed size\n";

    // ============================================================
    //  6. INTEGER DIVISION TRAPS
    // ============================================================
    std::cout << "\n===== 6. INTEGER DIVISION =====\n";
    std::cout << "7 / 2      = " << (7 / 2)      << "   <- 3.5 NAHI!\n";
    std::cout << "7 % 2      = " << (7 % 2)      << "   (remainder)\n";
    std::cout << "-7 / 2     = " << (-7 / 2)     << "  <- zero ki taraf, -4 nahi\n";
    std::cout << "-7 % 2     = " << (-7 % 2)     << "\n";
    std::cout << "7.0 / 2    = " << (7.0 / 2)    << " <- ek double hai to double division\n";

    std::cout << "\nCLASSIC BUG:\n";
    int correct = 45, total = 60;
    double wrongPct = correct / total * 100;                          // ⚠️ 0!
    double rightPct = static_cast<double>(correct) / total * 100;     // ✅ 75
    std::cout << "  correct/total*100                  = " << wrongPct
              << "   <- integer division PEHLE hui\n";
    std::cout << "  (double)correct/total*100          = " << rightPct << "  <- sahi\n";

    // Division by zero:
    //   int  x = 5 / 0;      // ⚠️ UB -- usually crash (SIGFPE)
    //   double y = 5.0/0.0;  // ✅ inf (IEEE-754 mein defined)
    std::cout << "\n5.0 / 0.0  = " << (5.0 / 0.0) << "   <- float division defined hai\n";
    std::cout << "5 / 0      = UB (crash) <- integer division UB hai\n";

    // ============================================================
    //  7. HFT SCENARIO
    // ============================================================
    std::cout << "\n===== 7. HFT SCENARIO =====\n";
    std::cout << "Ek exchange ek din mein ~1 arab orders bhejta hai.\n";

    std::int32_t  orderId32 = 2000000000;
    std::uint64_t orderId64 = 2000000000;

    std::cout << "\nAgar order ID int32_t hai:\n";
    std::cout << "  max = " << std::numeric_limits<std::int32_t>::max() << "\n";
    std::cout << "  current = " << orderId32 << "\n";
    std::cout << "  ~147 million orders baad OVERFLOW -> duplicate IDs -> DISASTER\n";

    std::cout << "\nAgar order ID uint64_t hai:\n";
    std::cout << "  max = " << std::numeric_limits<std::uint64_t>::max() << "\n";
    std::cout << "  current = " << orderId64 << "\n";
    std::cout << "  1 arab orders/din pe -> ~50 arab saal lagenge overflow mein\n";
    std::cout << "\nIsliye HFT mein IDs aur timestamps HAMESHA uint64_t hote hain.\n";

    return 0;
}
