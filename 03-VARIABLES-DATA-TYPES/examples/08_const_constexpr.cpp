// 08_const_constexpr.cpp
// ============================================================
// const vs constexpr vs consteval
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 08_const_constexpr.cpp -o cc && ./cc
//
// Verify karo ki constexpr COMPILE TIME pe hua:
//   g++ -std=c++20 -O2 -S 08_const_constexpr.cpp -o - | grep 3628800
//   (agar number literally assembly mein dikhe -> compile time pe ho gaya)
// ============================================================

#include <iostream>
#include <array>
#include <cstdint>

// ============================================================
//  constexpr FUNCTION -- dono jagah kaam karta hai
// ============================================================
// Agar arguments compile time pe pata hon -> compile time pe chalega
// Agar nahi -> runtime pe chalega
constexpr std::int64_t factorial(int n) {
    return (n <= 1) ? 1 : n * factorial(n - 1);
}

constexpr int square(int x) { return x * x; }

// ============================================================
//  consteval (C++20) -- COMPILE TIME PE HI, warna error
// ============================================================
consteval int mustBeCompileTime(int x) { return x * 2; }

// ============================================================
//  COMPILE-TIME LOOKUP TABLE -- HFT ka favourite trick
// ============================================================
// Yeh poora table COMPILE TIME pe ban jaata hai.
// Binary mein already-computed data ki tarah jaata hai.
// Runtime cost: sirf ek array lookup (aur woh cache mein garam rahega).
constexpr std::array<std::uint32_t, 256> makeSquareTable() {
    std::array<std::uint32_t, 256> table{};
    for (std::uint32_t i = 0; i < 256; ++i) {
        table[i] = i * i;
    }
    return table;
}
constexpr auto SQUARE_TABLE = makeSquareTable();

// Compile time pe verify karo
static_assert(SQUARE_TABLE[5]  == 25);
static_assert(SQUARE_TABLE[16] == 256);
static_assert(SQUARE_TABLE[255] == 65025);

// ============================================================
//  HFT-style constants
// ============================================================
constexpr std::size_t   CACHE_LINE_SIZE = 64;
constexpr std::size_t   MAX_ORDERS      = 1'000'000;
constexpr std::int64_t  TICK_SIZE_PAISE = 5;        // NSE: 0.05 rupaye
constexpr std::uint64_t NANOS_PER_SECOND = 1'000'000'000ULL;

// Protocol struct + compile-time validation
struct MarketDataHeader {
    std::uint8_t  messageType;
    std::uint8_t  version;
    std::uint16_t bodyLength;
    std::uint32_t sequenceNumber;
    std::uint64_t timestampNs;
};
static_assert(sizeof(MarketDataHeader) == 16, "Header layout badal gaya!");
static_assert(alignof(MarketDataHeader) == 8);

int main() {
    // ============================================================
    //  1. const -- "runtime pe badlega nahi"
    // ============================================================
    std::cout << "===== 1. const =====\n";
    const int maxRetries = 3;
    std::cout << "const int maxRetries = " << maxRetries << "\n";
    // maxRetries = 5;              // ❌ COMPILE ERROR
    std::cout << "maxRetries = 5;  -> COMPILE ERROR ✅ (compiler ne bacha liya)\n";

    // const runtime value bhi ho sakti hai
    int userInput = 42;             // socho yeh user se aaya
    const int frozenValue = userInput;      // ✅ legal
    std::cout << "const int x = runtimeValue;  -> ✅ legal (const runtime ho sakta hai)\n";
    std::cout << "  frozenValue = " << frozenValue << "\n";

    // ============================================================
    //  2. constexpr -- "COMPILE TIME pe pata hai"
    // ============================================================
    std::cout << "\n===== 2. constexpr =====\n";
    constexpr int compileTimeMax = 100;
    // constexpr int bad = userInput;       // ❌ ERROR: compile time pe pata nahi
    std::cout << "constexpr int x = 100;           -> ✅\n";
    std::cout << "constexpr int x = runtimeValue;  -> ❌ ERROR\n";

    // constexpr array size ke liye use ho sakta hai
    int arr[compileTimeMax];
    std::cout << "int arr[compileTimeMax];  -> ✅ (array size ke liye chahiye)\n";
    std::cout << "  sizeof(arr) = " << sizeof(arr) << " bytes\n";

    // ============================================================
    //  3. constexpr FUNCTION -- dono jagah
    // ============================================================
    std::cout << "\n===== 3. constexpr FUNCTIONS =====\n";

    constexpr auto f10 = factorial(10);     // COMPILE TIME pe calculate hua
    static_assert(f10 == 3628800);          // COMPILE TIME pe verify hua

    int n = 5;
    auto fRuntime = factorial(n);           // RUNTIME pe calculate hua

    std::cout << "constexpr auto f = factorial(10);  -> " << f10
              << "  (COMPILE TIME pe)\n";
    std::cout << "auto f = factorial(runtimeN);      -> " << fRuntime
              << "     (RUNTIME pe)\n";
    std::cout << "\nSAME function, dono jagah kaam karta hai.\n";
    std::cout << "Verify: g++ -O2 -S ... | grep 3628800\n";
    std::cout << "  (agar number assembly mein dikhe -> compile time pe hua)\n";

    // ============================================================
    //  4. consteval -- compile time pe HI
    // ============================================================
    std::cout << "\n===== 4. consteval (C++20) =====\n";
    constexpr int ce = mustBeCompileTime(21);
    std::cout << "consteval int f(int x) { return x*2; }\n";
    std::cout << "  constexpr int a = f(21);      -> " << ce << "  ✅\n";
    std::cout << "  int b = f(runtimeValue);      -> ❌ COMPILE ERROR\n";
    std::cout << "\nconstexpr = 'compile time pe HO SAKTA hai'\n";
    std::cout << "consteval = 'compile time pe HONA HI CHAHIYE'\n";

    // ============================================================
    //  5. COMPILE-TIME LOOKUP TABLE
    // ============================================================
    std::cout << "\n===== 5. COMPILE-TIME LOOKUP TABLE =====\n";
    std::cout << "constexpr auto SQUARE_TABLE = makeSquareTable();\n";
    std::cout << "  SQUARE_TABLE[5]   = " << SQUARE_TABLE[5]   << "\n";
    std::cout << "  SQUARE_TABLE[16]  = " << SQUARE_TABLE[16]  << "\n";
    std::cout << "  SQUARE_TABLE[255] = " << SQUARE_TABLE[255] << "\n";
    std::cout << "\nPoora table COMPILE TIME pe bana. Binary mein data ki tarah hai.\n";
    std::cout << "Runtime cost: sirf ek array lookup (~1-4 cycles agar cache mein ho).\n";
    std::cout << "Yeh HFT mein bahut use hota hai -- expensive calculations pehle hi kar lo.\n";

    // ============================================================
    //  6. const POINTERS -- interview favourite
    // ============================================================
    std::cout << "\n===== 6. const POINTERS =====\n";
    int value = 10;
    int other = 20;

    // Trick: `const` uske LEFT wali cheez pe apply hota hai.
    //        Agar left mein kuch nahi, to RIGHT wali pe.
    //        Ya: declaration ko RIGHT SE LEFT padho.

    const int* p1 = &value;         // "p1 is a pointer to a const int"
    // *p1 = 20;                    // ❌ data const hai
    p1 = &other;                    // ✅ pointer badal sakte hain

    int* const p2 = &value;         // "p2 is a const pointer to an int"
    *p2 = 20;                       // ✅ data badal sakte hain
    // p2 = &other;                 // ❌ pointer const hai

    const int* const p3 = &value;   // dono const
    // *p3 = 30;                    // ❌
    // p3 = &other;                 // ❌

    std::cout << "const int* p        -> DATA const,    pointer badal sakta hai\n";
    std::cout << "int* const p        -> POINTER const, data badal sakta hai\n";
    std::cout << "const int* const p  -> dono const\n";
    std::cout << "\nPadhne ka trick: RIGHT SE LEFT padho.\n";
    std::cout << "  const int* p  ->  p is a Pointer to an int which is Const\n";
    std::cout << "  int* const p  ->  p is a Const Pointer to an int\n";
    std::cout << "\nvalue ab = " << value << " (p2 se badla)\n";
    (void)p1; (void)p3;

    // ============================================================
    //  7. HFT CONSTANTS
    // ============================================================
    std::cout << "\n===== 7. HFT-STYLE CONSTANTS =====\n";
    std::cout << "CACHE_LINE_SIZE   = " << CACHE_LINE_SIZE  << " bytes\n";
    std::cout << "MAX_ORDERS        = " << MAX_ORDERS       << "\n";
    std::cout << "TICK_SIZE_PAISE   = " << TICK_SIZE_PAISE  << " paise (Rs 0.05)\n";
    std::cout << "NANOS_PER_SECOND  = " << NANOS_PER_SECOND << "\n";
    std::cout << "\nsizeof(MarketDataHeader) = " << sizeof(MarketDataHeader)
              << " bytes (static_assert se verified)\n";
    std::cout << "\nAgar koi developer header struct badal de, BUILD FAIL hoga.\n";
    std::cout << "Bug production mein nahi jaayega. Yeh compile-time safety hai.\n";

    // ============================================================
    //  8. #define KE MUKABLE
    // ============================================================
    std::cout << "\n===== 8. #define vs constexpr =====\n";
    std::cout << "#define MAX 100\n";
    std::cout << "  ❌ type safety nahi (sirf text replacement)\n";
    std::cout << "  ❌ scope nahi (file ke end tak sab jagah)\n";
    std::cout << "  ❌ debugger mein nahi dikhta\n";
    std::cout << "  ❌ namespace mein nahi daal sakte\n";
    std::cout << "\nconstexpr int MAX = 100;\n";
    std::cout << "  ✅ type-safe\n";
    std::cout << "  ✅ properly scoped\n";
    std::cout << "  ✅ debugger mein dikhta hai\n";
    std::cout << "  ✅ namespace mein daal sakte ho\n";
    std::cout << "  ✅ same performance (compile time pe resolve ho jaata hai)\n";

    return 0;
}
