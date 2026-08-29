// 07_fixed_width.cpp
// ============================================================
// Fixed-width types -- aur woh HFT mein kyun mandatory hain
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 07_fixed_width.cpp -o fw && ./fw
// ============================================================

#include <iostream>
#include <cstdint>
#include <cstddef>
#include <limits>
#include <iomanip>
#include <cstring>

// ============================================================
//  ❌ GALAT TAREEKA -- platform-dependent sizes
// ============================================================
struct BadMessage {
    long  timestamp;      // Linux: 8 bytes, Windows: 4 bytes  ⚠️
    int   price;          // usually 4, par guaranteed nahi
    short quantity;       // usually 2
    char  side;           // signed? unsigned? platform pe depend
};

// ============================================================
//  ✅ SAHI TAREEKA -- har platform pe same layout
// ============================================================
struct GoodMessage {
    std::uint64_t timestampNs;    // HAMESHA 8 bytes
    std::int64_t  priceInTicks;   // HAMESHA 8 bytes
    std::uint32_t quantity;       // HAMESHA 4 bytes
    std::uint8_t  side;           // HAMESHA 1 byte
};

// ============================================================
//  WIRE PROTOCOL STRUCT -- exact byte layout, koi padding nahi
// ============================================================
// #pragma pack(1) compiler ko bolta hai "koi padding mat daalo".
// Yeh network/file formats ke liye zaroori hai jahan har byte
// ka position fixed hota hai.
//
// ⚠️ Trade-off: packed structs pe unaligned access hota hai, jo
//    kuch platforms pe slow hai (ya ARM pe crash bhi kar sakta hai).
//    Isliye HFT mein aksar bytes ko manually parse karke aligned
//    struct mein daala jaata hai. Folder 38 mein detail.
#pragma pack(push, 1)
struct WireMessage {
    std::uint8_t  messageType;    // offset 0,  1 byte
    std::uint64_t timestampNs;    // offset 1,  8 bytes
    std::uint64_t orderId;        // offset 9,  8 bytes
    std::int32_t  priceInTicks;   // offset 17, 4 bytes
    std::uint32_t quantity;       // offset 21, 4 bytes
    std::uint8_t  side;           // offset 25, 1 byte
};
#pragma pack(pop)

// COMPILE TIME pe verify karo ki layout sahi hai.
// Agar koi member add/remove kare, BUILD FAIL hoga --
// production mein bug nahi jaayega.
static_assert(sizeof(WireMessage) == 26, "Wire layout badal gaya!");
static_assert(offsetof(WireMessage, timestampNs)  == 1);
static_assert(offsetof(WireMessage, orderId)      == 9);
static_assert(offsetof(WireMessage, priceInTicks) == 17);
static_assert(offsetof(WireMessage, quantity)     == 21);
static_assert(offsetof(WireMessage, side)         == 25);

int main() {
    std::cout << "===== FIXED WIDTH TYPES =====\n";
    std::cout << std::left << std::setw(14) << "TYPE"
              << std::setw(8) << "BYTES" << "RANGE\n";
    std::cout << "-------------------------------------------------------------\n";

    std::cout << std::setw(14) << "int8_t"   << std::setw(8) << sizeof(std::int8_t)
              << +std::numeric_limits<std::int8_t>::min() << " to "
              << +std::numeric_limits<std::int8_t>::max() << "\n";
    std::cout << std::setw(14) << "uint8_t"  << std::setw(8) << sizeof(std::uint8_t)
              << "0 to " << +std::numeric_limits<std::uint8_t>::max() << "\n";
    std::cout << std::setw(14) << "int16_t"  << std::setw(8) << sizeof(std::int16_t)
              << std::numeric_limits<std::int16_t>::min() << " to "
              << std::numeric_limits<std::int16_t>::max() << "\n";
    std::cout << std::setw(14) << "int32_t"  << std::setw(8) << sizeof(std::int32_t)
              << std::numeric_limits<std::int32_t>::min() << " to "
              << std::numeric_limits<std::int32_t>::max() << "\n";
    std::cout << std::setw(14) << "uint32_t" << std::setw(8) << sizeof(std::uint32_t)
              << "0 to " << std::numeric_limits<std::uint32_t>::max() << "\n";
    std::cout << std::setw(14) << "int64_t"  << std::setw(8) << sizeof(std::int64_t)
              << std::numeric_limits<std::int64_t>::min() << " to "
              << std::numeric_limits<std::int64_t>::max() << "\n";
    std::cout << std::setw(14) << "uint64_t" << std::setw(8) << sizeof(std::uint64_t)
              << "0 to " << std::numeric_limits<std::uint64_t>::max() << "\n";
    std::cout << std::setw(14) << "size_t"   << std::setw(8) << sizeof(std::size_t)
              << "0 to " << std::numeric_limits<std::size_t>::max() << "\n";
    std::cout << std::right;

    // ============================================================
    //  ⚠️ uint8_t PRINTING TRAP
    // ============================================================
    std::cout << "\n===== uint8_t PRINTING TRAP =====\n";
    const std::uint8_t value = 65;
    std::cout << "std::cout << value        -> " << value
              << "   <- CHARACTER! (uint8_t = unsigned char)\n";
    std::cout << "std::cout << +value       -> " << +value
              << "  <- number (unary + promote karta hai)\n";
    std::cout << "std::cout << (int)value   -> " << static_cast<int>(value)
              << "  <- number\n";
    std::cout << "\nYeh sirf 8-bit types ke saath hota hai. int32_t/int64_t theek hain.\n";

    // ============================================================
    //  PLATFORM-DEPENDENT vs FIXED
    // ============================================================
    std::cout << "\n===== STRUCT LAYOUT COMPARISON =====\n";
    std::cout << "BadMessage  (long/int/short/char): " << sizeof(BadMessage)
              << " bytes  <- alag platform pe alag!\n";
    std::cout << "GoodMessage (fixed-width):         " << sizeof(GoodMessage)
              << " bytes  <- HAR platform pe same\n";
    std::cout << "WireMessage (packed, no padding):  " << sizeof(WireMessage)
              << " bytes  <- exact wire layout\n";

    std::cout << "\nGoodMessage (natural alignment ke saath):\n";
    std::cout << "  offsetof(timestampNs)  = " << offsetof(GoodMessage, timestampNs)  << "\n";
    std::cout << "  offsetof(priceInTicks) = " << offsetof(GoodMessage, priceInTicks) << "\n";
    std::cout << "  offsetof(quantity)     = " << offsetof(GoodMessage, quantity)     << "\n";
    std::cout << "  offsetof(side)         = " << offsetof(GoodMessage, side)         << "\n";
    std::cout << "  (21 bytes data, par sizeof = " << sizeof(GoodMessage)
              << " -- padding ki wajah se)\n";

    std::cout << "\nWireMessage (packed):\n";
    std::cout << "  offsetof(timestampNs)  = " << offsetof(WireMessage, timestampNs)  << "\n";
    std::cout << "  offsetof(orderId)      = " << offsetof(WireMessage, orderId)      << "\n";
    std::cout << "  offsetof(priceInTicks) = " << offsetof(WireMessage, priceInTicks) << "\n";
    std::cout << "  (koi padding nahi -- har byte exactly wahan hai jahan spec kehta hai)\n";

    // ============================================================
    //  PRACTICAL: ek message parse karo
    // ============================================================
    std::cout << "\n===== MESSAGE PARSING DEMO =====\n";

    // Ek fake "wire" buffer -- jaise network se aaya ho
    unsigned char buffer[sizeof(WireMessage)] = {};

    WireMessage outgoing{};
    outgoing.messageType  = 'A';               // 'A' = Add Order
    outgoing.timestampNs  = 1735689600000000000ULL;
    outgoing.orderId      = 987654321;
    outgoing.priceInTicks = 2150075;           // Rs 21500.75 (0.01 tick size)
    outgoing.quantity     = 100;
    outgoing.side         = 'B';               // Buy

    // Struct ko bytes mein copy karo (serialize)
    std::memcpy(buffer, &outgoing, sizeof(outgoing));

    // Bytes se wapas struct mein (deserialize)
    // NOTE: memcpy hi sahi tareeka hai -- pointer cast strict aliasing tod sakta hai.
    WireMessage incoming{};
    std::memcpy(&incoming, buffer, sizeof(incoming));

    std::cout << "Parsed message:\n";
    std::cout << "  type      : " << static_cast<char>(incoming.messageType) << "\n";
    std::cout << "  timestamp : " << incoming.timestampNs << " ns\n";
    std::cout << "  orderId   : " << incoming.orderId << "\n";
    std::cout << "  price     : " << incoming.priceInTicks << " ticks"
              << "  (= Rs " << (incoming.priceInTicks / 100) << "."
              << std::setfill('0') << std::setw(2) << (incoming.priceInTicks % 100)
              << std::setfill(' ') << ")\n";
    std::cout << "  quantity  : " << incoming.quantity << "\n";
    std::cout << "  side      : " << static_cast<char>(incoming.side) << "\n";

    std::cout << "\nRaw bytes (hex):\n  ";
    for (std::size_t i = 0; i < sizeof(buffer); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(buffer[i]) << " ";
        if ((i + 1) % 13 == 0) std::cout << "\n  ";
    }
    std::cout << std::dec << std::setfill(' ') << "\n";

    // ============================================================
    //  KYUN YEH MATTER KARTA HAI
    // ============================================================
    std::cout << "\n===== KYUN FIXED-WIDTH ZAROORI HAI =====\n";
    std::cout << "1. LAYOUT: wire protocols mein har byte ka position fixed hai.\n";
    std::cout << "   Agar `long` use karo, Linux pe 8 bytes aur Windows pe 4 --\n";
    std::cout << "   parser galat offsets se padhega -> galat prices -> paisa doobega.\n\n";
    std::cout << "2. OVERFLOW: uint64_t order IDs practically kabhi overflow nahi honge.\n";
    std::cout << "   int32_t 147 million orders mein hi overflow ho jaata.\n\n";
    std::cout << "3. CACHE PLANNING: aap exactly jaan sakte ho ki kitne objects\n";
    std::cout << "   ek 64-byte cache line mein aayenge. `int` ke saath yeh possible nahi.\n\n";
    std::cout << "4. static_assert: compile time pe layout verify ho jaata hai.\n";
    std::cout << "   Koi galti kare to BUILD FAIL -- production bug nahi.\n";

    return 0;
}
