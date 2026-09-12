// 06_bit_flags.cpp
// ============================================================
// Bit flags -- HFT-style order flags system
// ============================================================
// Yeh pattern har systems codebase mein dikhega.
// Faayda: 8 flags EK uint8_t mein -- 8 alag bool (8+ bytes) ki jagah 1 byte.
//         Cache mein zyada objects fit honge.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 06_bit_flags.cpp -o flags && ./flags
// ============================================================

#include <iostream>
#include <bitset>
#include <cstdint>
#include <iomanip>
#include <string>
#include <vector>

// ============================================================
//  ORDER FLAGS -- enum class ke saath type safety
// ============================================================
enum class OrderFlag : std::uint16_t {
    None       = 0,
    IsBuy      = 1u << 0,      // 0000 0000 0000 0001
    IsLimit    = 1u << 1,      // 0000 0000 0000 0010
    IsIOC      = 1u << 2,      // Immediate-Or-Cancel
    IsFOK      = 1u << 3,      // Fill-Or-Kill
    IsHidden   = 1u << 4,      // iceberg / hidden order
    IsPostOnly = 1u << 5,      // maker-only
    IsCancelled= 1u << 6,
    IsFilled   = 1u << 7,
};

// `enum class` type-safe hai, par uske liye operators define karne padte hain.
// `constexpr` isliye taaki yeh compile time pe evaluate ho sakein.
constexpr OrderFlag operator|(OrderFlag a, OrderFlag b) {
    return static_cast<OrderFlag>(
        static_cast<std::uint16_t>(a) | static_cast<std::uint16_t>(b));
}
constexpr OrderFlag operator&(OrderFlag a, OrderFlag b) {
    return static_cast<OrderFlag>(
        static_cast<std::uint16_t>(a) & static_cast<std::uint16_t>(b));
}
constexpr OrderFlag operator~(OrderFlag a) {
    return static_cast<OrderFlag>(~static_cast<std::uint16_t>(a));
}
constexpr OrderFlag& operator|=(OrderFlag& a, OrderFlag b) {
    a = a | b;
    return a;
}
constexpr OrderFlag& operator&=(OrderFlag& a, OrderFlag b) {
    a = a & b;
    return a;
}

// Helper functions
constexpr bool hasFlag(OrderFlag value, OrderFlag flag) {
    return (value & flag) == flag;
}
constexpr OrderFlag setFlag(OrderFlag value, OrderFlag flag) {
    return value | flag;
}
constexpr OrderFlag clearFlag(OrderFlag value, OrderFlag flag) {
    return value & ~flag;
}

constexpr std::uint16_t raw(OrderFlag f) { return static_cast<std::uint16_t>(f); }

std::string describe(OrderFlag f) {
    std::string s;
    auto add = [&](OrderFlag flag, const char* name) {
        if (hasFlag(f, flag)) { if (!s.empty()) s += "|"; s += name; }
    };
    add(OrderFlag::IsBuy,       "BUY");
    add(OrderFlag::IsLimit,     "LIMIT");
    add(OrderFlag::IsIOC,       "IOC");
    add(OrderFlag::IsFOK,       "FOK");
    add(OrderFlag::IsHidden,    "HIDDEN");
    add(OrderFlag::IsPostOnly,  "POST_ONLY");
    add(OrderFlag::IsCancelled, "CANCELLED");
    add(OrderFlag::IsFilled,    "FILLED");
    return s.empty() ? "NONE" : s;
}

// ============================================================
//  MEMORY COMPARISON
// ============================================================
// ⚠️ DHYAN DO: yeh comparison hamesha "flags jeet gaye" nahi dikhata.
//    PADDING ki wajah se chhote structs mein fark chhup jaata hai.
//    Neeche dono cases dikhaye hain -- yeh ek IMPORTANT lesson hai.

// CASE A: sirf 8 flags -- padding sab kha jaati hai
struct OrderBoolsSmall {
    std::uint64_t id;          // 8
    std::int64_t  price;       // 8
    bool isBuy, isLimit, isIOC, isFOK;
    bool isHidden, isPostOnly, isCancelled, isFilled;    // 8 bytes
};                             // total 24 (16 + 8, koi padding waste nahi)

struct OrderFlagsSmall {
    std::uint64_t id;          // 8
    std::int64_t  price;       // 8
    OrderFlag     flags;       // 2
};                             // total 24 (16 + 2 + 6 PADDING) -- koi bachat NAHI!

// CASE B: 24 flags (realistic order state) -- ab fark dikhta hai
struct OrderBoolsBig {
    std::uint64_t id;          // 8
    std::int64_t  price;       // 8
    std::uint32_t quantity;    // 4
    bool flags[24];            // 24 bytes -- har flag ek byte
};

struct OrderFlagsBig {
    std::uint64_t id;          // 8
    std::int64_t  price;       // 8
    std::uint32_t quantity;    // 4
    std::uint32_t flags;       // 4 bytes -- 32 flags tak
};

int main() {
    std::cout << "===== 1. FLAGS BANANA =====\n";

    OrderFlag order = OrderFlag::IsBuy | OrderFlag::IsLimit | OrderFlag::IsIOC;

    std::cout << "  order = IsBuy | IsLimit | IsIOC\n";
    std::cout << "  raw value : " << raw(order) << "\n";
    std::cout << "  binary    : " << std::bitset<8>(raw(order)) << "\n";
    std::cout << "  describe  : " << describe(order) << "\n";

    // ============================================================
    //  2. TEST
    // ============================================================
    std::cout << "\n===== 2. FLAGS TEST KARNA =====\n";
    std::cout << std::boolalpha;
    std::cout << "  hasFlag(IsBuy)      = " << hasFlag(order, OrderFlag::IsBuy) << "\n";
    std::cout << "  hasFlag(IsLimit)    = " << hasFlag(order, OrderFlag::IsLimit) << "\n";
    std::cout << "  hasFlag(IsFOK)      = " << hasFlag(order, OrderFlag::IsFOK)
              << "   <- set nahi hai\n";
    std::cout << "  hasFlag(IsHidden)   = " << hasFlag(order, OrderFlag::IsHidden) << "\n";

    // ============================================================
    //  3. SET / CLEAR / TOGGLE
    // ============================================================
    std::cout << "\n===== 3. SET / CLEAR =====\n";
    std::cout << "  shuru:              " << std::bitset<8>(raw(order))
              << "  " << describe(order) << "\n";

    order = setFlag(order, OrderFlag::IsHidden);
    std::cout << "  + IsHidden:         " << std::bitset<8>(raw(order))
              << "  " << describe(order) << "\n";

    order |= OrderFlag::IsPostOnly;                 // compound assignment
    std::cout << "  |= IsPostOnly:      " << std::bitset<8>(raw(order))
              << "  " << describe(order) << "\n";

    order = clearFlag(order, OrderFlag::IsIOC);
    std::cout << "  - IsIOC:            " << std::bitset<8>(raw(order))
              << "  " << describe(order) << "\n";

    // ============================================================
    //  4. MULTIPLE FLAGS EK SAATH
    // ============================================================
    std::cout << "\n===== 4. MULTIPLE FLAGS =====\n";
    const OrderFlag timeInForce = OrderFlag::IsIOC | OrderFlag::IsFOK;
    OrderFlag o2 = OrderFlag::IsBuy | OrderFlag::IsIOC;

    std::cout << "  o2 = BUY|IOC\n";
    std::cout << "  Koi time-in-force flag hai? "
              << ((o2 & timeInForce) != OrderFlag::None) << "\n";
    std::cout << "  DONO IOC aur FOK hain?      "
              << hasFlag(o2, timeInForce) << "   <- nahi, sirf IOC\n";

    // Sab time-in-force flags clear karo
    o2 = clearFlag(o2, timeInForce);
    std::cout << "  Sab TIF clear karke:        " << describe(o2) << "\n";

    // ============================================================
    //  5. COMPILE-TIME EVALUATION
    // ============================================================
    std::cout << "\n===== 5. COMPILE TIME =====\n";
    // `constexpr` functions ki wajah se yeh sab COMPILE TIME pe ho jaata hai
    constexpr OrderFlag compileTimeFlags =
        OrderFlag::IsBuy | OrderFlag::IsLimit | OrderFlag::IsPostOnly;
    static_assert(hasFlag(compileTimeFlags, OrderFlag::IsBuy));
    static_assert(hasFlag(compileTimeFlags, OrderFlag::IsLimit));
    static_assert(!hasFlag(compileTimeFlags, OrderFlag::IsIOC));
    static_assert(raw(compileTimeFlags) == 0b00100011);

    std::cout << "  static_assert se compile time pe verify ho gaya  ✅\n";
    std::cout << "  raw = " << raw(compileTimeFlags)
              << "  binary = " << std::bitset<8>(raw(compileTimeFlags)) << "\n";
    std::cout << "  Runtime pe ZERO cost -- values binary mein hi hain.\n";

    // ============================================================
    //  6. MEMORY COMPARISON -- yeh HFT ka asli point hai
    // ============================================================
    std::cout << "\n===== 6. MEMORY: bool[] vs FLAGS =====\n";

    std::cout << "  CASE A -- sirf 8 flags:\n";
    std::cout << "    sizeof(OrderBoolsSmall) = " << sizeof(OrderBoolsSmall) << " bytes\n";
    std::cout << "    sizeof(OrderFlagsSmall) = " << sizeof(OrderFlagsSmall) << " bytes\n";
    std::cout << "    Bachat: "
              << (static_cast<long>(sizeof(OrderBoolsSmall)) -
                  static_cast<long>(sizeof(OrderFlagsSmall)))
              << " bytes\n";
    std::cout << "\n    ⚠️ KOI BACHAT NAHI! Kyun?\n";
    std::cout << "       8 bools exactly 8 bytes lete hain, aur woh 16-byte ke\n";
    std::cout << "       baad wale slot mein fit ho jaate hain.\n";
    std::cout << "       Flags version mein 2 bytes ke baad 6 bytes PADDING lagti hai\n";
    std::cout << "       (kyunki struct ka alignment 8 hai).\n";
    std::cout << "       -> Dono 24 bytes. Padding ne fark kha liya.\n";

    std::cout << "\n  CASE B -- 24 flags (realistic order state):\n";
    std::cout << "    sizeof(OrderBoolsBig)   = " << sizeof(OrderBoolsBig) << " bytes\n";
    std::cout << "    sizeof(OrderFlagsBig)   = " << sizeof(OrderFlagsBig) << " bytes\n";
    const long saving = static_cast<long>(sizeof(OrderBoolsBig)) -
                        static_cast<long>(sizeof(OrderFlagsBig));
    std::cout << "    Bachat: " << saving << " bytes per order ("
              << std::fixed << std::setprecision(0)
              << (100.0 * static_cast<double>(saving) / static_cast<double>(sizeof(OrderBoolsBig)))
              << "%)  ✅\n";

    constexpr std::size_t kCacheLine = 64;
    std::cout << "\n    Ek 64-byte CACHE LINE mein kitne fit honge:\n";
    std::cout << "      OrderBoolsBig: " << (kCacheLine / sizeof(OrderBoolsBig)) << "\n";
    std::cout << "      OrderFlagsBig: " << (kCacheLine / sizeof(OrderFlagsBig)) << "\n";

    std::cout << "\n  ═══ DO LESSONS ═══\n";
    std::cout << "  1. Bit flags tabhi bachate hain jab flags KAAFI HON.\n";
    std::cout << "     2-3 flags ke liye padding waise hi kha jaayegi.\n";
    std::cout << "  2. Struct size ka andaza mat lagao -- sizeof() se MEASURE karo.\n";
    std::cout << "     Padding aksar aapki expectation todh deti hai.\n";
    std::cout << "     (Folder 11 mein padding poora padhenge.)\n";

    std::cout << "\n  HFT MEIN YEH KYUN MATTER KARTA HAI:\n";
    std::cout << "  Order book mein lakhon orders hote hain. Har order chhota hoga to:\n";
    std::cout << "    - Zyada orders ek cache line mein fit honge\n";
    std::cout << "    - Kam cache misses -> kam RAM access (~100ns each)\n";
    std::cout << "  Folder 32 (cache) aur 39 (order book) mein poora.\n";

    // ============================================================
    //  7. ⚠️ PRECEDENCE REMINDER
    // ============================================================
    std::cout << "\n===== 7. ⚠️ PRECEDENCE REMINDER =====\n";
    std::cout << "  Bitwise operators ki precedence == se KAM hai:\n";
    std::cout << "    if (flags & FLAG == 0)     ⚠️ GALAT -- (FLAG == 0) pehle\n";
    std::cout << "    if ((flags & FLAG) == 0)   ✅ SAHI\n";
    std::cout << "\n  Isliye humne `hasFlag()` helper banaya -- brackets ek jagah,\n";
    std::cout << "  aur call sites saaf rehte hain.\n";

    return 0;
}
