// endianness.cpp
// ============================================================
// LESSON 10 ka example: little vs big endian
// ============================================================
// HFT relevance: market data protocols aksar BIG endian (network byte order)
// hote hain, jabki x86 servers LITTLE endian hote hain. Agar aap byte swap
// bhool gaye, aapki price ki value bilkul galat ho jayegi.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra endianness.cpp -o endianness && ./endianness
// ============================================================

#include <iostream>
#include <bit>       // std::endian (C++20)
#include <cstdint>
#include <cstring>   // std::memcpy
#include <iomanip>   // std::setw, std::setfill

int main() {
    // ---------- 1. Apna system kaunsa hai? ----------
    std::cout << "===== SYSTEM ENDIANNESS =====\n";
    if constexpr (std::endian::native == std::endian::little) {
        std::cout << "Yeh system LITTLE endian hai (x86/ARM ka default)\n";
    } else if constexpr (std::endian::native == std::endian::big) {
        std::cout << "Yeh system BIG endian hai\n";
    } else {
        std::cout << "Yeh system MIXED endian hai (bahut rare)\n";
    }
    // NOTE: `if constexpr` COMPILE TIME pe decide hota hai -- runtime pe koi check nahi.
    //       Yeh C++17 ka feature hai. Folder 21 mein detail mein padhenge.

    // ---------- 2. Bytes ko actually dekho ----------
    std::cout << "\n===== MEMORY MEIN BYTES =====\n";
    std::uint32_t value = 0x12345678;   // 4 bytes: 12, 34, 56, 78

    // Ek uint32_t ke bytes ko byte-by-byte padhne ke liye hum uske
    // address ko unsigned char* ki tarah dekhte hain.
    // (unsigned char* se kisi bhi object ke bytes padhna LEGAL hai -- yeh
    //  strict aliasing rule ka ek exception hai. Folder 25 mein detail.)
    const auto* bytes = reinterpret_cast<const unsigned char*>(&value);

    std::cout << "Value: 0x12345678\n";
    std::cout << "Memory mein bytes: ";
    for (std::size_t i = 0; i < sizeof(value); ++i) {
        std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(bytes[i]) << " ";
    }
    std::cout << std::dec << "\n";
    std::cout << "Little endian pe: 0x78 0x56 0x34 0x12 (ulta)\n";
    std::cout << "Big endian pe:    0x12 0x34 0x56 0x78 (seedha)\n";

    // ---------- 3. Byte swap karna ----------
    std::cout << "\n===== BYTE SWAP =====\n";

    // Manual byte swap (samajhne ke liye)
    // Har byte ko uski nayi jagah shift karke, OR se jodte hain.
    auto swap32 = [](std::uint32_t v) -> std::uint32_t {
        return ((v & 0x000000FFu) << 24) |   // sabse chhota byte -> sabse bada
               ((v & 0x0000FF00u) <<  8) |
               ((v & 0x00FF0000u) >>  8) |
               ((v & 0xFF000000u) >> 24);    // sabse bada byte -> sabse chhota
    };

    std::uint32_t original = 0x12345678;
    std::uint32_t swapped  = swap32(original);
    std::cout << std::hex;
    std::cout << "Original: 0x" << original << "\n";
    std::cout << "Swapped:  0x" << swapped  << "\n";
    std::cout << std::dec;

    // Practice mein compiler intrinsics use karo -- woh ek hi CPU instruction
    // (x86 pe BSWAP) mein kaam kar dete hain:
    //   __builtin_bswap32(v)   -- GCC/Clang
    //   _byteswap_ulong(v)     -- MSVC
    //   std::byteswap(v)       -- C++23 (standard tareeka)
    //   ntohl(v) / htonl(v)    -- network byte order ke liye (<arpa/inet.h>)

    // ---------- 4. HFT scenario: galat parsing ka nateeja ----------
    std::cout << "\n===== HFT SCENARIO =====\n";
    // Exchange ne bheja: price = 10000 (big endian, wire pe)
    // Wire bytes: 00 00 27 10
    unsigned char wire[4] = {0x00, 0x00, 0x27, 0x10};

    // GALAT: seedha memcpy (little-endian machine pe bytes ulte lag jaayenge)
    std::uint32_t wrong;
    std::memcpy(&wrong, wire, 4);
    // NOTE: memcpy hi sahi tareeka hai bytes ko type mein daalne ka.
    //       Pointer cast karke padhna strict aliasing tod sakta hai (folder 25).

    // SAHI: bytes ko explicitly big-endian maan ke jodo
    std::uint32_t correct = (static_cast<std::uint32_t>(wire[0]) << 24) |
                            (static_cast<std::uint32_t>(wire[1]) << 16) |
                            (static_cast<std::uint32_t>(wire[2]) <<  8) |
                            (static_cast<std::uint32_t>(wire[3])      );

    std::cout << "Exchange ne bheja tha price: 10000\n";
    std::cout << "Galat parse (raw memcpy):    " << wrong   << "  <-- DISASTER\n";
    std::cout << "Sahi parse (big-endian):     " << correct << "  <-- correct\n";
    std::cout << "\nIsi tarah ki galti se real paisa doobta hai.\n";

    return 0;
}
