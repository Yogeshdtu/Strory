// 01_arithmetic.cpp
// ============================================================
// Arithmetic operators aur unke 4 classic traps
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 01_arithmetic.cpp -o arith && ./arith
// ============================================================

#include <iostream>
#include <cmath>
#include <limits>
#include <cstdint>
#include <vector>
#include <chrono>
#include <iomanip>

int main() {
    // ============================================================
    //  ⚠️ TRAP 1: INTEGER DIVISION
    // ============================================================
    std::cout << "===== TRAP 1: INTEGER DIVISION =====\n";
    // Do integers ka division HAMESHA integer deta hai.
    // Decimal part TRUNCATE hota hai -- round nahi, ZERO ki taraf kat jaata hai.
    std::cout << "  7 / 2      = " << (7 / 2)     << "    <- 3.5 NAHI!\n";
    std::cout << "  9 / 2      = " << (9 / 2)     << "    <- 4.5 -> 4\n";
    std::cout << " -7 / 2      = " << (-7 / 2)    << "   <- zero ki taraf, -4 NAHI\n";
    std::cout << "  1 / 2      = " << (1 / 2)     << "    <- 0!\n";

    std::cout << "\n  Decimal chahiye to:\n";
    const int a = 7, b = 2;
    std::cout << "  7.0 / 2                    = " << (7.0 / 2) << "\n";
    std::cout << "  static_cast<double>(a) / b = "
              << (static_cast<double>(a) / b) << "\n";
    std::cout << "  static_cast<double>(a / b) = "
              << static_cast<double>(a / b)
              << "    ⚠️ BAHUT LATE! division pehle ho gaya\n";

    // THE CLASSIC BUG
    std::cout << "\n  THE CLASSIC BUG:\n";
    const int correct = 45, total = 60;
    const double wrongPct = correct / total * 100;                      // ⚠️ 0
    const double rightPct = static_cast<double>(correct) / total * 100; // ✅ 75
    const double alsoRight = correct * 100.0 / total;                   // ✅ 75
    std::cout << "    correct/total*100           = " << wrongPct
              << "    ⚠️ integer division pehle hui\n";
    std::cout << "    (double)correct/total*100   = " << rightPct << "   ✅\n";
    std::cout << "    correct*100.0/total         = " << alsoRight << "   ✅\n";

    // ============================================================
    //  ⚠️ TRAP 2: MODULO AUR NEGATIVE NUMBERS
    // ============================================================
    std::cout << "\n===== TRAP 2: MODULO =====\n";
    // C++ mein % ka sign DIVIDEND (left operand) ka hota hai.
    // Python mein DIVISOR ka hota hai -- isliye alag answer aata hai.
    std::cout << "   7 %  3 = " << ( 7 %  3) << "\n";
    std::cout << "  -7 %  3 = " << (-7 %  3) << "   <- Python mein 2 aata hai!\n";
    std::cout << "   7 % -3 = " << ( 7 % -3) << "\n";
    std::cout << "  -7 % -3 = " << (-7 % -3) << "\n";

    std::cout << "\n  Guarantee: (a/b)*b + (a%b) == a\n";
    std::cout << "    (-7/3)*3 + (-7%3) = " << ((-7/3)*3 + (-7%3))
              << "  ✅ == -7\n";

    // Ring buffer bug
    std::cout << "\n  ⚠️ RING BUFFER BUG:\n";
    const int size = 10;
    for (int current = 0; current < 3; ++current) {
        const int bad  = (current - 1) % size;
        const int good = (current + size - 1) % size;
        std::cout << "    current=" << current
                  << "  (current-1)%size = " << std::setw(3) << bad
                  << (bad < 0 ? "  ⚠️ NEGATIVE -> out of bounds!" : "")
                  << "   sahi = " << good << "\n";
    }

    // % sirf integers ke liye
    std::cout << "\n  7.5 % 2 -> COMPILE ERROR (% sirf integers ke liye)\n";
    std::cout << "  std::fmod(7.5, 2.0) = " << std::fmod(7.5, 2.0) << "  ✅\n";

    // ============================================================
    //  ⚠️ TRAP 3: DIVISION BY ZERO
    // ============================================================
    std::cout << "\n===== TRAP 3: DIVISION BY ZERO =====\n";
    std::cout << "  int:   5 / 0     -> UNDEFINED BEHAVIOUR (crash/SIGFPE)\n";
    std::cout << "  int:   5 % 0     -> UNDEFINED BEHAVIOUR\n";
    std::cout << "  float: 5.0 / 0.0 -> " << (5.0 / 0.0)
              << "     ✅ DEFINED (IEEE-754)\n";
    std::cout << "  float:-5.0 / 0.0 -> " << (-5.0 / 0.0) << "\n";
    std::cout << "  float: 0.0 / 0.0 -> " << (0.0 / 0.0)  << "     (NaN)\n";
    std::cout << "\n  Isliye HAMESHA check karo: if (divisor != 0)\n";

    // ============================================================
    //  ⚠️ TRAP 4: OVERFLOW
    // ============================================================
    std::cout << "\n===== TRAP 4: OVERFLOW =====\n";
    const int big1 = 2000000000;
    const int big2 = 2000000000;

    std::cout << "  int max = " << std::numeric_limits<int>::max() << "\n";
    std::cout << "  2000000000 + 2000000000 = 4000000000 -> int mein nahi samata\n";
    std::cout << "  Signed overflow = UNDEFINED BEHAVIOUR (yahan demo nahi kar rahe)\n";

    std::cout << "\n  SAFE tareeke:\n";
    // 1. Bada type
    const std::int64_t safe = static_cast<std::int64_t>(big1) + big2;
    std::cout << "    1. int64_t mein:           " << safe << "  ✅\n";

    // 2. Compiler builtin
    int result = 0;
    const bool overflowed = __builtin_add_overflow(big1, big2, &result);
    std::cout << "    2. __builtin_add_overflow: "
              << (overflowed ? "OVERFLOW detect hua ✅" : "no overflow") << "\n";

    // 3. Manual check
    const bool willOverflow =
        (big1 > 0 && big2 > std::numeric_limits<int>::max() - big1);
    std::cout << "    3. manual check:           "
              << (willOverflow ? "OVERFLOW detect hua ✅" : "no overflow") << "\n";

    // Unsigned wrap -- yeh DEFINED hai
    unsigned int u = std::numeric_limits<unsigned int>::max();
    std::cout << "\n  Unsigned WRAP (defined behaviour):\n";
    std::cout << "    uint max     = " << u << "\n";
    std::cout << "    uint max + 1 = " << (u + 1u) << "   <- 0, modulo 2^32\n";

    // ============================================================
    //  DIVISION KI COST
    // ============================================================
    std::cout << "\n===== DIVISION KI COST =====\n";
    // Benchmark ka rule: sirf WOHI cheez measure karo jo measure karni hai.
    // Isliye loop mein aur kuch mehnga nahi rakhenge (koi % nahi, koi indexing nahi).
    //
    // `volatile` divisor isliye taaki compiler use compile-time constant
    // maan ke khud reciprocal-multiply mein na badal de.
    constexpr int N = 50000000;
    volatile double divisorV = 3.0;

    auto t1 = std::chrono::steady_clock::now();
    double sum1 = 0.0;
    {
        const double d = divisorV;           // ek baar padho
        double x = 1.0;
        for (int i = 0; i < N; ++i) {
            sum1 += x / d;                   // DIVISION har iteration
            x += 1.0;
        }
    }
    auto t2 = std::chrono::steady_clock::now();

    double sum2 = 0.0;
    {
        const double inv = 1.0 / divisorV;   // reciprocal EK BAAR
        double x = 1.0;
        for (int i = 0; i < N; ++i) {
            sum2 += x * inv;                 // MULTIPLY har iteration
            x += 1.0;
        }
    }
    auto t3 = std::chrono::steady_clock::now();

    auto ms = [](auto x, auto y) {
        return std::chrono::duration<double, std::milli>(y - x).count();
    };

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  " << N << " operations:\n";
    std::cout << "    x / d          : " << std::setw(8) << ms(t1, t2) << " ms\n";
    std::cout << "    x * (1/d)      : " << std::setw(8) << ms(t2, t3) << " ms";
    if (ms(t2, t3) > 0.0) {
        std::cout << "   <- " << (ms(t1, t2) / ms(t2, t3)) << "x faster";
    }
    std::cout << "\n";
    std::cout << "  (sums: " << sum1 << " vs " << sum2 << " -- "
              << (sum1 == sum2 ? "exactly barabar" : "thodi si rounding difference")
              << ")\n";

    std::cout << "\n  Approximate cycle costs:\n";
    std::cout << "    integer add/sub :  1 cycle\n";
    std::cout << "    integer mul     :  3 cycles\n";
    std::cout << "    float add/mul   :  4 cycles\n";
    std::cout << "    float DIVIDE    : 15-40 cycles   <- SABSE MEHNGI\n";
    std::cout << "    integer DIVIDE  : 20-40 cycles   <- YEH BHI\n";
    std::cout << "\n  Isliye: loop mein division ho to reciprocal ek baar nikaal ke\n";
    std::cout << "  multiply karo. (Compiler yeh khud nahi kar sakta kyunki\n";
    std::cout << "  floating point mein x/d aur x*(1/d) bilkul same nahi hote --\n";
    std::cout << "  rounding thodi alag hoti hai. -ffast-math se karta hai.)\n\n";

    return 0;
}
