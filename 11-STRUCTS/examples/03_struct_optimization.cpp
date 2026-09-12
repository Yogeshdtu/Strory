// 03_struct_optimization.cpp
// ============================================================
// Member reordering -- struct ka size aadha (padding hataake)
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 03_struct_optimization.cpp -o so && ./so
// ============================================================
// Rule of thumb: members ko DESCENDING alignment order mein rakho
// (bade pehle: pointers/double/int64 -> int32 -> int16 -> char/bool).
// Isse beech ki padding minimum hoti hai.
// ============================================================

#include <cstddef>
#include <cstdint>
#include <iostream>

// ---- V1: "natural" order jaise socha (galat) ----
struct EventV1 {
    bool         active;      // 1
    // 7 padding
    std::int64_t timestamp;   // 8
    std::uint16_t code;       // 2
    // 2 padding
    std::int32_t value;       // 4
    bool         flagged;     // 1
    // 7 tail padding
    double       weight;      // 8   -- wait, iske liye 8-align chahiye -> upar aur padding
};                            // -> bloated

// ---- V2: descending alignment order ----
struct EventV2 {
    std::int64_t timestamp;   // 8
    double       weight;      // 8
    std::int32_t value;       // 4
    std::uint16_t code;       // 2
    bool         active;      // 1
    bool         flagged;     // 1
    // 0 padding -- 8+8+4+2+1+1 = 24, already 8-multiple
};

// ---- Real-ish: an order book entry ----
struct LevelBad {
    bool          valid;      // 1  (+7 pad)
    std::int64_t  price;      // 8
    std::int32_t  orderCount; // 4  (+4 pad, kyunki qty ko 8-align chahiye)
    std::int64_t  qty;        // 8
    char          side;       // 1  (+7 tail pad)
};   // 40 bytes

struct LevelGood {
    std::int64_t  price;      // 8
    std::int64_t  qty;        // 8
    std::int32_t  orderCount; // 4
    char          side;       // 1
    bool          valid;      // 1
    // 2 tail pad -> 24 bytes
};

template <typename T>
void row(const char* name, std::size_t usefulBytes) {
    const std::size_t sz = sizeof(T);
    std::cout << "  " << name
              << "  sizeof = " << sz
              << "  align = "  << alignof(T)
              << "  wasted = " << (sz - usefulBytes) << " bytes"
              << "  (" << (100 * (sz - usefulBytes) / sz) << "% padding)\n";
}

int main() {
    std::cout << "===== Event: bool + int64 + uint16 + int32 + bool + double =====\n";
    row<EventV1>("V1 (mixed order)     ", 24);
    row<EventV2>("V2 (descending align)", 24);

    std::cout << "\n===== Order book Level: int64 x2 + int32 + char + bool =====\n";
    row<LevelBad>("LevelBad  (bad order) ", 22);
    row<LevelGood>("LevelGood (good order)", 22);

    std::cout << "\n===== cache line impact (64 bytes) =====\n";
    std::cout << "  LevelBad  (" << sizeof(LevelBad)  << " B): "
              << 64 / sizeof(LevelBad)  << " levels per cache line\n";
    std::cout << "  LevelGood (" << sizeof(LevelGood) << " B): "
              << 64 / sizeof(LevelGood) << " levels per cache line\n";
    std::cout << "  -> Good " << static_cast<double>(sizeof(LevelBad)) / static_cast<double>(sizeof(LevelGood))
              << "x zyada dense: same data ke liye "
              << static_cast<double>(sizeof(LevelBad)) / static_cast<double>(sizeof(LevelGood))
              << "x kam memory + cache lines.\n";

    std::cout <<
        "\n"
        "  RULE: struct members DESCENDING alignment order.\n"
        "  Aur: -Wpadded se check, ya `pahole` (Linux) / static_assert(sizeof(T) == N).\n"
        "  ⚠️ Kabhi API/wire compatibility member order fix kar deti hai --\n"
        "     tab explicit padding fields ya alag layout struct.\n";

    return 0;
}
