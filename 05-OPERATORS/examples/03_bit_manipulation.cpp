// 03_bit_manipulation.cpp
// ============================================================
// Bit manipulation ka poora toolkit + C++20 <bit>
// ============================================================
//   g++ -std=c++20 -O2 -march=native -Wall -Wextra 03_bit_manipulation.cpp -o bitman
//   ./bitman
//
// ⚠️ -march=native zaroori hai POPCNT/LZCNT/TZCNT instructions ke liye.
//    Uske bina compiler slow fallback code banata hai.
// ============================================================

#include <iostream>
#include <bitset>
#include <bit>
#include <cstdint>
#include <iomanip>
#include <chrono>
#include <vector>
#include <random>

// ============================================================
//  THE TOOLKIT -- yeh 5 functions yaad kar lo
// ============================================================
constexpr bool testBit(std::uint32_t v, int bit) {
    return (v & (1u << bit)) != 0;
}
constexpr std::uint32_t setBit(std::uint32_t v, int bit) {
    return v | (1u << bit);
}
constexpr std::uint32_t clearBit(std::uint32_t v, int bit) {
    return v & ~(1u << bit);
}
constexpr std::uint32_t toggleBit(std::uint32_t v, int bit) {
    return v ^ (1u << bit);
}
// Branchless: bit ko `cond` ki value do
constexpr std::uint32_t assignBit(std::uint32_t v, int bit, bool cond) {
    return (v & ~(1u << bit)) | (static_cast<std::uint32_t>(cond) << bit);
}

// Manual popcount -- sirf comparison ke liye
int manualPopcount(std::uint32_t v) {
    int count = 0;
    while (v) { count += static_cast<int>(v & 1u); v >>= 1; }
    return count;
}

int main() {
    // ============================================================
    //  1. THE TOOLKIT
    // ============================================================
    std::cout << "===== 1. BIT TOOLKIT =====\n";
    std::uint32_t v = 0b00000000;

    std::cout << "  shuru:          " << std::bitset<8>(v) << "\n";
    v = setBit(v, 2);
    std::cout << "  setBit(2):      " << std::bitset<8>(v) << "\n";
    v = setBit(v, 5);
    std::cout << "  setBit(5):      " << std::bitset<8>(v) << "\n";
    v = toggleBit(v, 2);
    std::cout << "  toggleBit(2):   " << std::bitset<8>(v) << "   (tha 1, ab 0)\n";
    v = toggleBit(v, 0);
    std::cout << "  toggleBit(0):   " << std::bitset<8>(v) << "   (tha 0, ab 1)\n";
    v = clearBit(v, 5);
    std::cout << "  clearBit(5):    " << std::bitset<8>(v) << "\n";
    std::cout << "  testBit(0):     " << testBit(v, 0) << "\n";
    std::cout << "  testBit(5):     " << testBit(v, 5) << "\n";
    v = assignBit(v, 7, true);
    std::cout << "  assignBit(7,1): " << std::bitset<8>(v) << "   (branchless)\n";

    // ============================================================
    //  2. POWER OF 2
    // ============================================================
    std::cout << "\n===== 2. POWER OF 2 CHECK =====\n";
    std::cout << "  Trick: (v != 0) && ((v & (v-1)) == 0)\n";
    std::cout << "  Kyun? Power of 2 mein exactly EK bit set hota hai.\n";
    std::cout << "        v-1 us bit ko clear karke neeche sab set kar deta hai.\n\n";

    for (std::uint32_t n : {0u, 1u, 2u, 3u, 4u, 7u, 8u, 16u, 100u, 1024u}) {
        const bool manual = (n != 0) && ((n & (n - 1)) == 0);
        const bool stdWay = std::has_single_bit(n);
        std::cout << "    " << std::setw(5) << n
                  << "  " << std::bitset<11>(n)
                  << "   manual=" << manual
                  << "  std::has_single_bit=" << stdWay
                  << (manual == stdWay ? "  ✅" : "  ❌ MISMATCH") << "\n";
    }

    // ============================================================
    //  3. LOWEST SET BIT
    // ============================================================
    std::cout << "\n===== 3. LOWEST SET BIT =====\n";
    const std::uint32_t x = 0b10110000;
    std::cout << "  x            = " << std::bitset<8>(x) << "\n";
    std::cout << "  x & -x       = " << std::bitset<8>(x & (~x + 1))
              << "   (lowest set bit nikala)\n";
    std::cout << "  x & (x-1)    = " << std::bitset<8>(x & (x - 1))
              << "   (lowest set bit clear kiya)\n";
    std::cout << "  countr_zero  = " << std::countr_zero(x)
              << "          (lowest set bit ka index)\n";

    // ============================================================
    //  4. SET BITS PE ITERATE -- sparse bitsets ke liye FAST
    // ============================================================
    std::cout << "\n===== 4. SET BITS PE ITERATE =====\n";
    std::uint32_t bits = 0b10010100;
    std::cout << "  bits = " << std::bitset<8>(bits) << "\n";
    std::cout << "  Set bits: ";
    while (bits) {
        const int idx = std::countr_zero(bits);
        std::cout << idx << " ";
        bits &= (bits - 1);              // lowest set bit clear karo
    }
    std::cout << "\n  (sirf SET bits jitni iterations -- saare 32 nahi)\n";

    // ============================================================
    //  5. C++20 <bit> LIBRARY
    // ============================================================
    std::cout << "\n===== 5. C++20 <bit> =====\n";
    const std::uint32_t val = 0b00101100;      // 44
    std::cout << "  val = " << std::bitset<8>(val) << " (" << val << ")\n\n";
    std::cout << "    std::popcount(val)      = " << std::popcount(val)
              << "    (kitne 1 bits)\n";
    std::cout << "    std::countl_zero(val)   = " << std::countl_zero(val)
              << "   (leading zeros)\n";
    std::cout << "    std::countr_zero(val)   = " << std::countr_zero(val)
              << "    (trailing zeros)\n";
    std::cout << "    std::bit_width(val)     = " << std::bit_width(val)
              << "    (represent karne ko kitne bits)\n";
    std::cout << "    std::has_single_bit(val)= " << std::has_single_bit(val)
              << "    (power of 2?)\n";
    std::cout << "    std::bit_ceil(val)      = " << std::bit_ceil(val)
              << "   (agla power of 2)\n";
    std::cout << "    std::bit_floor(val)     = " << std::bit_floor(val)
              << "   (pichla power of 2)\n";
    std::cout << "    std::rotl(val, 2)       = " << std::bitset<8>(std::rotl(std::uint8_t(val), 2))
              << "\n";
    std::cout << "    std::rotr(val, 2)       = " << std::bitset<8>(std::rotr(std::uint8_t(val), 2))
              << "\n";

    // ============================================================
    //  6. ALIGNMENT TRICKS -- memory pools mein use hote hain
    // ============================================================
    std::cout << "\n===== 6. ALIGNMENT =====\n";
    auto alignUp = [](std::size_t value, std::size_t alignment) {
        return (value + alignment - 1) & ~(alignment - 1);
    };
    auto alignDown = [](std::size_t value, std::size_t alignment) {
        return value & ~(alignment - 1);
    };
    auto isAligned = [](std::size_t value, std::size_t alignment) {
        return (value & (alignment - 1)) == 0;
    };

    std::cout << "  (alignment power of 2 hona chahiye)\n";
    for (std::size_t n : {1u, 63u, 64u, 65u, 128u, 200u}) {
        std::cout << "    n=" << std::setw(4) << n
                  << "  alignUp(n,64)=" << std::setw(4) << alignUp(n, 64)
                  << "  alignDown(n,64)=" << std::setw(4) << alignDown(n, 64)
                  << "  isAligned=" << isAligned(n, 64) << "\n";
    }
    std::cout << "\n  HFT: memory pools mein har allocation ko 64-byte\n";
    std::cout << "  (cache line) boundary pe align karte hain.\n";

    // ============================================================
    //  7. ENDIANNESS SWAP
    // ============================================================
    std::cout << "\n===== 7. ENDIANNESS SWAP =====\n";
    const std::uint32_t original = 0x12345678;

    auto manualSwap32 = [](std::uint32_t vv) -> std::uint32_t {
        return ((vv & 0x000000FFu) << 24) | ((vv & 0x0000FF00u) <<  8) |
               ((vv & 0x00FF0000u) >>  8) | ((vv & 0xFF000000u) >> 24);
    };

    std::cout << std::hex;
    std::cout << "  original            = 0x" << original << "\n";
    std::cout << "  manual swap         = 0x" << manualSwap32(original)
              << "   (8-10 instructions)\n";
    std::cout << "  __builtin_bswap32   = 0x" << __builtin_bswap32(original)
              << "   (1 instruction -- BSWAP)\n";
    std::cout << std::dec;
    std::cout << "  (C++23 mein std::byteswap bhi hai)\n";
    std::cout << "\n  HFT: market data BIG-endian, x86 LITTLE-endian.\n";
    std::cout << "  Har message field swap karna padta hai -> BSWAP 1 cycle.\n";

    // ============================================================
    //  8. BENCHMARK: manual vs std::popcount
    // ============================================================
    std::cout << "\n===== 8. BENCHMARK: popcount =====\n";
    constexpr int N = 3000000;

    std::mt19937 rng(42);
    std::vector<std::uint32_t> data(10000);
    for (auto& d : data) d = rng();

    auto t1 = std::chrono::steady_clock::now();
    long long sum1 = 0;
    for (int i = 0; i < N; ++i) sum1 += manualPopcount(data[static_cast<std::size_t>(i) % data.size()]);
    auto t2 = std::chrono::steady_clock::now();

    long long sum2 = 0;
    for (int i = 0; i < N; ++i) sum2 += std::popcount(data[static_cast<std::size_t>(i) % data.size()]);
    auto t3 = std::chrono::steady_clock::now();

    auto ms = [](auto p, auto q) {
        return std::chrono::duration<double, std::milli>(q - p).count();
    };

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  " << N << " popcounts:\n";
    std::cout << "    manual loop      : " << std::setw(8) << ms(t1, t2) << " ms\n";
    std::cout << "    std::popcount    : " << std::setw(8) << ms(t2, t3) << " ms";
    if (ms(t2, t3) > 0.0) std::cout << "   <- " << (ms(t1,t2)/ms(t2,t3)) << "x faster";
    std::cout << "\n";
    std::cout << "  (results match? " << (sum1 == sum2 ? "haan ✅" : "NAHI ❌") << ")\n";
    std::cout << "\n  std::popcount POPCNT instruction mein compile hota hai (~3 cycles).\n";
    std::cout << "  Manual loop 32 iterations chalata hai.\n";
    std::cout << "  ⚠️ -march=native ke bina fallback code banta hai -- fark kam dikhega.\n";

    return 0;
}
