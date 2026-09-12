// 04_float_traps.cpp
// ============================================================
// Floating point ke saare traps
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 04_float_traps.cpp -o traps && ./traps
// ============================================================

#include <iostream>
#include <iomanip>
#include <cmath>
#include <limits>
#include <cstdint>
#include <algorithm>

// Floats ko compare karne ka SAHI tareeka
bool nearlyEqual(double a, double b,
                 double relEps = 1e-9, double absEps = 1e-12) {
    const double diff = std::fabs(a - b);
    // Pehle absolute check -- dono ~0 ke paas ho sakte hain
    if (diff <= absEps) return true;
    // Phir relative check -- bade numbers ke liye
    return diff <= relEps * std::max(std::fabs(a), std::fabs(b));
}

int main() {
    std::cout << "===== 1. THE CLASSIC: 0.1 + 0.2 =====\n";
    std::cout << std::setprecision(20);
    std::cout << "0.1        = " << 0.1        << "\n";
    std::cout << "0.2        = " << 0.2        << "\n";
    std::cout << "0.1 + 0.2  = " << (0.1 + 0.2) << "\n";
    std::cout << "0.3        = " << 0.3        << "\n";
    std::cout << "\n0.1 + 0.2 == 0.3 ? " << std::boolalpha << (0.1 + 0.2 == 0.3)
              << std::noboolalpha << "\n";
    std::cout << "\nYEH BUG NAHI HAI. Yeh binary floating point ki NATURE hai.\n";
    std::cout << "0.1 binary mein infinite repeating hai -- 52 bits mein kaat diya jaata hai.\n";
    std::cout << "Python, Java, JavaScript -- sab mein yahi hota hai.\n";

    std::cout << std::setprecision(6);

    // ============================================================
    std::cout << "\n===== 2. SAHI COMPARISON =====\n";
    std::cout << std::boolalpha;
    std::cout << "0.1 + 0.2 == 0.3          -> " << (0.1 + 0.2 == 0.3) << "  ❌\n";
    std::cout << "nearlyEqual(0.1+0.2, 0.3) -> " << nearlyEqual(0.1 + 0.2, 0.3) << "  ✅\n";
    std::cout << std::noboolalpha;

    // ============================================================
    std::cout << "\n===== 3. ACCUMULATION ERROR =====\n";
    float  sumF = 0.0f;
    double sumD = 0.0;
    const int N = 1000000;
    for (int i = 0; i < N; ++i) {
        sumF += 0.1f;
        sumD += 0.1;
    }
    std::cout << std::setprecision(15);
    std::cout << "0.1 ko " << N << " baar joda:\n";
    std::cout << "  Expected: 100000\n";
    std::cout << "  float:    " << sumF << "   <- BAHUT off!\n";
    std::cout << "  double:   " << sumD << "\n";
    std::cout << "\nHar addition mein chhota error tha. 10 lakh baar mein woh jud gaya.\n";
    std::cout << "Yahi wajah thi ki 1991 mein Patriot missile system fail hua tha.\n";
    std::cout << std::setprecision(6);

    // ============================================================
    std::cout << "\n===== 4. NaN KI AJEEB PROPERTIES =====\n";
    const double nan = std::numeric_limits<double>::quiet_NaN();
    std::cout << std::boolalpha;
    std::cout << "nan == nan  -> " << (nan == nan) << "   <- FALSE! apne barabar bhi nahi\n";
    std::cout << "nan != nan  -> " << (nan != nan) << "   <- TRUE\n";
    std::cout << "nan <  1.0  -> " << (nan < 1.0)  << "\n";
    std::cout << "nan >  1.0  -> " << (nan > 1.0)  << "   <- dono false!\n";
    std::cout << "std::isnan(nan) -> " << std::isnan(nan) << "  <- YEH use karo\n";
    std::cout << "\nTrick: if (x != x) matlab x NaN hai\n";
    std::cout << "⚠️ NaN std::sort ko TOD deta hai (strict weak ordering violate karta hai)\n";
    std::cout << std::noboolalpha;

    // ============================================================
    std::cout << "\n===== 5. INFINITY =====\n";
    const double inf = std::numeric_limits<double>::infinity();
    std::cout << " 5.0 / 0.0 = " << (5.0 / 0.0)  << "   <- DEFINED (IEEE-754)\n";
    std::cout << "-5.0 / 0.0 = " << (-5.0 / 0.0) << "\n";
    std::cout << " 0.0 / 0.0 = " << (0.0 / 0.0)  << "   <- NaN\n";
    std::cout << "inf + 1    = " << (inf + 1)    << "\n";
    std::cout << "inf - inf  = " << (inf - inf)  << "   <- NaN\n";
    std::cout << "\n(int division by zero UB hai -- crash karti hai. Float nahi.)\n";

    // ============================================================
    std::cout << "\n===== 6. PRECISION LOSS: int -> float =====\n";
    const std::int32_t bigInt = 16777217;      // 2^24 + 1
    const float asFloat = static_cast<float>(bigInt);
    std::cout << "int   16777217 (2^24 + 1) = " << bigInt << "\n";
    std::cout << "float mein convert kiya    = " << std::setprecision(10)
              << asFloat << "   <- 1 KHO GAYA!\n";
    std::cout << "barabar hain? " << std::boolalpha
              << (static_cast<float>(bigInt) == static_cast<float>(bigInt - 1))
              << std::noboolalpha << "  <- 16777216 aur 16777217 float mein SAME hain\n";
    std::cout << "\nfloat mein 24 bits mantissa hai -> 2^24 tak hi exact integers.\n";
    std::cout << "double mein 53 bits -> 2^53 tak exact.\n";
    std::cout << "Isliye uint64_t order IDs ko double mein MAT daalo!\n";
    std::cout << std::setprecision(6);

    // ============================================================
    std::cout << "\n===== 7. 💰 FINANCE: float MAT USE KARO =====\n";

    // ❌ GALAT TAREEKA
    const double priceD = 100.10;
    const double qtyD = 3;
    const double totalD = priceD * qtyD;
    std::cout << std::setprecision(20);
    std::cout << "GALAT (double):\n";
    std::cout << "  100.10 * 3 = " << totalD << "\n";
    std::cout << "  Expected:    300.30\n";
    std::cout << "  Exactly 300.30? " << std::boolalpha << (totalD == 300.30)
              << std::noboolalpha << "\n";

    // ✅ SAHI TAREEKA -- integer paise
    const std::int64_t priceInPaise = 10010;      // Rs 100.10
    const std::int64_t qty = 3;
    const std::int64_t totalInPaise = priceInPaise * qty;
    std::cout << "\nSAHI (integer paise):\n";
    std::cout << "  10010 paise * 3 = " << totalInPaise << " paise\n";
    std::cout << "  = Rs " << (totalInPaise / 100) << "." 
              << std::setfill('0') << std::setw(2) << (totalInPaise % 100)
              << std::setfill(' ') << "\n";
    std::cout << "  EXACT hai. Koi error nahi.\n";
    std::cout << std::setprecision(6);

    std::cout << "\nHFT RULE: prices ko HAMESHA integer ticks/paise mein rakho.\n";
    std::cout << "  - Correctness: exchange prices discrete ticks mein hote hain\n";
    std::cout << "  - Speed: integer add = 1 cycle, float divide = 15-40 cycles\n";
    std::cout << "  - Determinism: koi rounding surprise nahi\n";

    // ============================================================
    std::cout << "\n===== 8. EPSILON =====\n";
    std::cout << std::setprecision(20);
    std::cout << "double epsilon = " << std::numeric_limits<double>::epsilon() << "\n";
    std::cout << "  (1.0 se agla representable number ka fark)\n";
    std::cout << "float epsilon  = " << std::numeric_limits<float>::epsilon() << "\n";
    std::cout << "\n⚠️ min() vs lowest() TRAP:\n";
    std::cout << "  double min():    " << std::numeric_limits<double>::min()
              << "  <- sabse chhota POSITIVE!\n";
    std::cout << "  double lowest(): " << std::numeric_limits<double>::lowest()
              << "  <- sabse chhota  ✅ YEH use karo\n";

    return 0;
}
