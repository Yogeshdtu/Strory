// 02_padding_demo.cpp
// ============================================================
// PADDING -- compiler struct members ke beech gaps daalta hai (alignment)
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 02_padding_demo.cpp -o pad && ./pad
//   Padding warnings:  g++ -std=c++20 -Wpadded -mno-ms-bitfields 02_padding_demo.cpp -o /dev/null
//   (MinGW pe -mms-bitfields default on hai -- us mode mein -Wpadded sirf TAIL padding
//    batata hai; beech ki padding dekhne ke liye -mno-ms-bitfields, sirf audit ke liye)
// ============================================================
// Rule: har member apne size ke multiple address pe start hona chahiye
// (natural alignment). double -> 8-byte boundary, int -> 4, short -> 2, char -> 1.
// struct ka size uske SABSE BADE member ke alignment ka multiple hota hai.
// Compiler gaps (padding bytes) daal ke yeh ensure karta hai.
// ============================================================

#include <cstddef>
#include <cstdint>
#include <iostream>

struct Bad {          // members chhote-bade mixed -> zyada padding
    char        a;    // offset 0        (1 byte)
    // 7 bytes PADDING (b ko 8-align chahiye)
    double      b;    // offset 8        (8 bytes)
    char        c;    // offset 16       (1 byte)
    // 3 bytes PADDING (d ko 4-align chahiye)
    std::int32_t d;   // offset 20       (4 bytes)
    // total 24 bytes  (14 kaam ke, 10 padding)
};

struct Good {         // wahi members, DESCENDING size order -> minimum padding
    double      b;    // offset 0
    std::int32_t d;   // offset 8
    char        a;    // offset 12
    char        c;    // offset 13
    // 2 bytes tail PADDING (size ko 8 ka multiple banane ke liye)
    // total 16 bytes  (14 kaam ke, 2 padding)  -- Bad se 8 bytes chhota
};

struct ThreeChars { char a, b, c; };     // 3 bytes, align 1, NO padding

template <typename T>
void report(const char* name) {
    std::cout << "  " << name
              << "  sizeof = " << sizeof(T)
              << "  alignof = " << alignof(T) << "\n";
}

int main() {
    std::cout << "===== struct sizes =====\n";
    report<Bad>("Bad ");
    report<Good>("Good");
    report<ThreeChars>("ThreeChars");

    // ============================================================
    //  offsetof -- har member kis byte pe hai
    // ============================================================
    std::cout << "\n===== member offsets =====\n";
    std::cout << "  Bad::a  @ " << offsetof(Bad, a) << "\n";
    std::cout << "  Bad::b  @ " << offsetof(Bad, b) << "   (7 bytes padding after a)\n";
    std::cout << "  Bad::c  @ " << offsetof(Bad, c) << "\n";
    std::cout << "  Bad::d  @ " << offsetof(Bad, d) << "   (3 bytes padding after c)\n";
    std::cout << "\n";
    std::cout << "  Good::b @ " << offsetof(Good, b) << "\n";
    std::cout << "  Good::d @ " << offsetof(Good, d) << "\n";
    std::cout << "  Good::a @ " << offsetof(Good, a) << "\n";
    std::cout << "  Good::c @ " << offsetof(Good, c) << "   (2 bytes TAIL padding after this)\n";

    // ============================================================
    //  raw bytes -- padding dikhega (garbage / zero)
    // ============================================================
    std::cout << "\n===== raw bytes of a Bad instance =====\n";
    Bad x{};
    x.a = 0x11; x.b = 1.0; x.c = 0x33; x.d = 0x44444444;
    const auto* p = reinterpret_cast<const unsigned char*>(&x);
    std::cout << "  ";
    for (std::size_t i = 0; i < sizeof(Bad); ++i) {
        std::cout << std::hex;
        if (p[i] < 16) std::cout << '0';
        std::cout << static_cast<int>(p[i]) << ' ';
        std::cout << std::dec;
    }
    std::cout << "\n  (11 = a, phir padding, phir b ke 8 bytes (00..00 f0 3f), etc.)\n";

    std::cout <<
        "\n"
        "  * struct size = members ka jod + padding.\n"
        "  * Member order matter karta hai -- Bad(24) vs Good(16), SAME members.\n"
        "  * -Wpadded compiler se poocho kahan padding daali (build note dekho).\n"
        "  * HFT: cache line 64 bytes. Chhota struct = zyada records per line =\n"
        "    kam cache misses. Member reorder = free win. Lesson 05.\n";

    return 0;
}
