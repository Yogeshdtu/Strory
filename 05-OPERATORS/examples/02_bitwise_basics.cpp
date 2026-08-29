// 02_bitwise_basics.cpp
// ============================================================
// Har bitwise operator ka demo, binary ke saath
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 02_bitwise_basics.cpp -o bits && ./bits
// ============================================================

#include <iostream>
#include <bitset>
#include <iomanip>
#include <cstdint>

// Helper: number ko binary ke saath print karo
void show(const char* label, std::uint8_t v) {
    std::cout << "  " << std::setw(16) << std::left << label
              << std::bitset<8>(v) << "  = " << std::setw(3) << std::right
              << static_cast<int>(v) << "\n";
}

int main() {
    const std::uint8_t a = 0b1100;      // 12
    const std::uint8_t b = 0b1010;      // 10

    std::cout << "===== OPERANDS =====\n";
    show("a =", a);
    show("b =", b);

    // ============================================================
    //  AND (&) -- dono bits 1 hon to 1
    // ============================================================
    std::cout << "\n===== AND (&) -- dono 1 hon to 1 =====\n";
    show("a =", a);
    show("b =", b);
    std::cout << "  ----------------\n";
    show("a & b =", static_cast<std::uint8_t>(a & b));
    std::cout << "\n  Use: MASKING -- specific bits nikalna\n";
    const std::uint16_t value = 0xABCD;
    std::cout << "    0xABCD & 0x00FF = 0x" << std::hex
              << (value & 0x00FF) << std::dec << "   (low byte)\n";
    std::cout << "    0xABCD & 0xFF00 = 0x" << std::hex
              << (value & 0xFF00) << std::dec << "   (high byte)\n";

    // ============================================================
    //  OR (|) -- koi ek bit 1 ho to 1
    // ============================================================
    std::cout << "\n===== OR (|) -- koi ek 1 ho to 1 =====\n";
    show("a =", a);
    show("b =", b);
    std::cout << "  ----------------\n";
    show("a | b =", static_cast<std::uint8_t>(a | b));
    std::cout << "\n  Use: FLAGS combine karna\n";

    // ============================================================
    //  XOR (^) -- bits ALAG hon to 1
    // ============================================================
    std::cout << "\n===== XOR (^) -- bits ALAG hon to 1 =====\n";
    show("a =", a);
    show("b =", b);
    std::cout << "  ----------------\n";
    show("a ^ b =", static_cast<std::uint8_t>(a ^ b));

    std::cout << "\n  XOR ki PROPERTIES (yeh yaad rakho):\n";
    std::cout << "    a ^ a     = " << static_cast<int>(a ^ a)
              << "     <- apne saath XOR = 0\n";
    std::cout << "    a ^ 0     = " << static_cast<int>(a ^ 0)
              << "    <- 0 ke saath XOR = wahi\n";
    std::cout << "    a ^ b ^ b = " << static_cast<int>(a ^ b ^ b)
              << "    <- do baar XOR = wapas original  ← MAGIC\n";

    // ============================================================
    //  NOT (~) -- saare bits ulte
    // ============================================================
    std::cout << "\n===== NOT (~) -- saare bits ulte =====\n";
    show("a =", a);
    show("~a =", static_cast<std::uint8_t>(~a));
    std::cout << "\n  ⚠️ SIGNED ke saath dhyaan:\n";
    const int signedA = 12;
    std::cout << "    int x = 12;  ~x = " << (~signedA)
              << "   (two's complement: ~x == -x - 1)\n";

    // ============================================================
    //  LEFT SHIFT (<<) -- multiply by 2^n
    // ============================================================
    std::cout << "\n===== LEFT SHIFT (<<) -- multiply by 2^n =====\n";
    const std::uint8_t s = 0b00000101;      // 5
    show("s =", s);
    show("s << 1 =", static_cast<std::uint8_t>(s << 1));
    show("s << 2 =", static_cast<std::uint8_t>(s << 2));
    show("s << 3 =", static_cast<std::uint8_t>(s << 3));
    std::cout << "\n  x << n  ==  x * 2^n   (jab tak overflow na ho)\n";

    std::cout << "\n  Use: BIT MASKS banana\n";
    for (int i = 0; i < 5; ++i) {
        std::cout << "    1u << " << i << " = " << std::setw(2) << (1u << i)
                  << "   " << std::bitset<8>(1u << i) << "\n";
    }

    std::cout << "\n  ⚠️ UB TRAPS:\n";
    std::cout << "    x << 32   -> UB agar x 32-bit hai (shift >= width)\n";
    std::cout << "    x << -1   -> UB (negative shift)\n";

    // ============================================================
    //  RIGHT SHIFT (>>) -- divide by 2^n
    // ============================================================
    std::cout << "\n===== RIGHT SHIFT (>>) -- divide by 2^n =====\n";
    const std::uint8_t r = 0b00010100;      // 20
    show("r =", r);
    show("r >> 1 =", static_cast<std::uint8_t>(r >> 1));
    show("r >> 2 =", static_cast<std::uint8_t>(r >> 2));

    std::cout << "\n  ⚠️ SIGNED vs UNSIGNED mein ALAG behaviour:\n";
    const std::uint32_t unsignedVal = 0x80000000u;
    const std::int32_t  signedVal   = -8;

    std::cout << "    unsigned 0x80000000 >> 1 = 0x" << std::hex
              << (unsignedVal >> 1) << std::dec
              << "   (LOGICAL -- 0 aata hai)\n";
    std::cout << "    signed   -8 >> 1         = " << (signedVal >> 1)
              << "           (ARITHMETIC -- sign bit copy)\n";

    std::cout << "\n  ⚠️ >> DIVISION KE BARABAR NAHI HAI (negative numbers mein):\n";
    std::cout << "    -7 / 2   = " << (-7 / 2)  << "   (zero ki taraf)\n";
    std::cout << "    -7 >> 1  = " << (-7 >> 1) << "   (neeche ki taraf)\n";
    std::cout << "    ✅ Shift-as-division sirf UNSIGNED pe safe hai\n";

    // ============================================================
    //  ⚠️ & vs &&
    // ============================================================
    std::cout << "\n===== ⚠️ & vs && =====\n";
    std::cout << "  &  = BITWISE  -- bit by bit, dono operands ALWAYS evaluate\n";
    std::cout << "  && = LOGICAL  -- bool result, SHORT-CIRCUIT karta hai\n";
    std::cout << "\n  bool ke liye result same aata hai, par:\n";
    std::cout << "    if (ptr != nullptr &  ptr->x > 5)   <- ⚠️ CRASH (dono chalenge)\n";
    std::cout << "    if (ptr != nullptr && ptr->x > 5)   <- ✅ SAFE\n";
    std::cout << "\n  RULE: booleans ke liye HAMESHA && aur || use karo.\n";

    // ============================================================
    //  MODULO BY POWER OF 2
    // ============================================================
    std::cout << "\n===== FAST MODULO (power of 2) =====\n";
    std::cout << "  x % 8  ==  x & 7        (jab 8 power of 2 ho)\n";
    std::cout << "  Speed:  division 20-40 cycles,  AND 1 cycle\n\n";
    for (unsigned i = 0; i < 12; i += 3) {
        std::cout << "    i=" << std::setw(2) << i
                  << "   i % 8 = " << (i % 8)
                  << "   i & 7 = " << (i & 7u) << "\n";
    }
    std::cout << "\n  ⚠️ Sirf NON-NEGATIVE values ke liye:\n";
    std::cout << "    -7 % 8 = " << (-7 % 8) << "\n";
    std::cout << "    -7 & 7 = " << (-7 & 7) << "    <- ALAG!\n";
    std::cout << "\n  HFT: ring buffers ka size HAMESHA power of 2 rakhte hain,\n";
    std::cout << "  sirf isliye ki index wrap `& (size-1)` se ho, `% size` se nahi.\n";

    return 0;
}
