// 02_all_types.cpp
// ============================================================
// Saare fundamental types -- sizes, ranges, behaviour
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 02_all_types.cpp -o types && ./types
// ============================================================

#include <iostream>
#include <limits>
#include <iomanip>
#include <cstdint>

int main() {
    std::cout << "======================================================\n";
    std::cout << "  APNE SYSTEM PE TYPE SIZES\n";
    std::cout << "======================================================\n";

    // NOTE: yeh sizes STANDARD mein guaranteed NAHI hain!
    // Standard sirf minimums aur relationships guarantee karta hai:
    //   sizeof(char) == 1 <= sizeof(short) <= sizeof(int)
    //                     <= sizeof(long) <= sizeof(long long)
    //
    // Isliye jab exact size chahiye -> <cstdint> use karo (file 09).

    std::cout << std::left << std::setw(16) << "TYPE"
              << std::setw(8) << "BYTES" << "RANGE\n";
    std::cout << "------------------------------------------------------\n";

    std::cout << std::setw(16) << "bool"    << std::setw(8) << sizeof(bool)
              << "true / false\n";

    std::cout << std::setw(16) << "char"    << std::setw(8) << sizeof(char)
              << static_cast<int>(std::numeric_limits<char>::min()) << " to "
              << static_cast<int>(std::numeric_limits<char>::max())
              << (std::numeric_limits<char>::is_signed ? "  (SIGNED)" : "  (UNSIGNED)")
              << "\n";

    std::cout << std::setw(16) << "signed char" << std::setw(8) << sizeof(signed char)
              << static_cast<int>(std::numeric_limits<signed char>::min()) << " to "
              << static_cast<int>(std::numeric_limits<signed char>::max()) << "\n";

    std::cout << std::setw(16) << "unsigned char" << std::setw(8) << sizeof(unsigned char)
              << "0 to " << static_cast<int>(std::numeric_limits<unsigned char>::max())
              << "\n";

    std::cout << std::setw(16) << "short"   << std::setw(8) << sizeof(short)
              << std::numeric_limits<short>::min() << " to "
              << std::numeric_limits<short>::max() << "\n";

    std::cout << std::setw(16) << "int"     << std::setw(8) << sizeof(int)
              << std::numeric_limits<int>::min() << " to "
              << std::numeric_limits<int>::max() << "\n";

    std::cout << std::setw(16) << "unsigned int" << std::setw(8) << sizeof(unsigned)
              << "0 to " << std::numeric_limits<unsigned>::max() << "\n";

    std::cout << std::setw(16) << "long"    << std::setw(8) << sizeof(long)
              << std::numeric_limits<long>::min() << " to "
              << std::numeric_limits<long>::max() << "\n";

    std::cout << std::setw(16) << "long long" << std::setw(8) << sizeof(long long)
              << std::numeric_limits<long long>::min() << " to "
              << std::numeric_limits<long long>::max() << "\n";

    std::cout << std::setw(16) << "float"   << std::setw(8) << sizeof(float)
              << "+/- " << std::numeric_limits<float>::max()
              << "  (~7 digits precision)\n";

    std::cout << std::setw(16) << "double"  << std::setw(8) << sizeof(double)
              << "+/- " << std::numeric_limits<double>::max()
              << "  (~15-16 digits)\n";

    std::cout << std::setw(16) << "long double" << std::setw(8) << sizeof(long double)
              << "platform-dependent\n";

    std::cout << std::setw(16) << "void*"   << std::setw(8) << sizeof(void*)
              << "memory addresses\n";

    std::cout << std::setw(16) << "size_t"  << std::setw(8) << sizeof(std::size_t)
              << "0 to " << std::numeric_limits<std::size_t>::max() << "\n";

    // ============================================================
    //  ⚠️ PLATFORM DIFFERENCES -- yeh dhyaan se dekho
    // ============================================================
    std::cout << "\n======================================================\n";
    std::cout << "  ⚠️  PLATFORM WARNINGS\n";
    std::cout << "======================================================\n";
    std::cout << "long is " << sizeof(long) << " bytes yahan.\n";
    std::cout << "  Linux/macOS x86-64: 8 bytes\n";
    std::cout << "  Windows x86-64:     4 bytes   <- YAHAN FARK HAI!\n";
    std::cout << "\nchar is " << (std::numeric_limits<char>::is_signed ? "SIGNED" : "UNSIGNED")
              << " yahan.\n";
    std::cout << "  x86/x86-64: signed\n";
    std::cout << "  ARM:        unsigned         <- YAHAN BHI FARK!\n";
    std::cout << "\nIsliye portable code mein <cstdint> use karo:\n";
    std::cout << "  int32_t, int64_t, uint8_t -- inki size GUARANTEED hai.\n";

    // ============================================================
    //  VALUES DEKHTE HAIN
    // ============================================================
    std::cout << "\n======================================================\n";
    std::cout << "  VALUES\n";
    std::cout << "======================================================\n";

    bool isOpen = true;
    char grade = 'A';
    short year = 2026;
    int population = 1400000000;          // 1.4 arab -- int mein fit ho jaata hai
    long long marketCap = 3000000000000LL; // 3 trillion -- long long chahiye
    float temperature = 36.6f;
    double pi = 3.14159265358979;

    std::cout << "bool isOpen      = " << std::boolalpha << isOpen << std::noboolalpha << "\n";
    std::cout << "char grade       = " << grade
              << "  (as int: " << static_cast<int>(grade) << ")\n";
    std::cout << "short year       = " << year << "\n";
    std::cout << "int population   = " << population << "\n";
    std::cout << "long long mcap   = " << marketCap << "\n";
    std::cout << "float temp       = " << temperature << "\n";
    std::cout << "double pi        = " << std::setprecision(15) << pi << "\n";

    // ============================================================
    //  PRECISION KA FARK
    // ============================================================
    std::cout << "\n======================================================\n";
    std::cout << "  float vs double PRECISION\n";
    std::cout << "======================================================\n";
    float  fPi = 3.14159265358979f;
    double dPi = 3.14159265358979;

    std::cout << std::setprecision(20);
    std::cout << "float  pi = " << fPi << "\n";
    std::cout << "double pi = " << dPi << "\n";
    std::cout << "\nfloat mein ~7 digits ke baad garbage hai.\n";
    std::cout << "Isliye DEFAULT double use karo, float nahi.\n";

    return 0;
}
