// bits_demo.cpp
// ============================================================
// LESSON 10 ka example: bits, bytes, binary, hex, overflow
// ============================================================
//   g++ -std=c++20 -Wall -Wextra bits_demo.cpp -o bits_demo && ./bits_demo
// ============================================================

#include <iostream>
#include <bitset>    // std::bitset -- number ko binary mein dikhane ke liye
#include <climits>   // INT_MAX, INT_MIN jaise constants
#include <cstdint>   // uint8_t, int32_t jaise fixed-width types

int main() {
    std::cout << "===== BINARY REPRESENTATION =====\n";

    int x = 13;
    // std::bitset<N>(value) -- value ko N bits mein binary ke roop mein dikhata hai.
    // Yeh sirf DISPLAY ke liye hai; memory mein kuch nahi badalta.
    std::cout << "13 (8 bits):  " << std::bitset<8>(x)  << "\n";
    std::cout << "13 (32 bits): " << std::bitset<32>(x) << "\n";

    std::cout << "\n===== TWO'S COMPLEMENT =====\n";
    int neg = -5;
    // Dhyaan do: -5 mein saare upar wale bits 1 hain. Yeh two's complement hai.
    std::cout << " 5: " << std::bitset<8>(5)   << "\n";
    std::cout << "-5: " << std::bitset<8>(neg) << "\n";
    std::cout << "-1: " << std::bitset<8>(-1)  << "  <- saare bits 1!\n";

    std::cout << "\n===== HEX =====\n";
    // std::hex se aage ka output hex mein aayega. std::dec se wapas decimal.
    std::cout << "255 in hex: 0x" << std::hex << 255 << std::dec << "\n";
    std::cout << "0xFF in decimal: " << 0xFF << "\n";
    std::cout << "0b1010 in decimal: " << 0b1010 << "  (binary literal, C++14 se)\n";

    std::cout << "\n===== TYPE SIZES (bytes) =====\n";
    // sizeof ek COMPILE-TIME operator hai -- yeh runtime pe kuch nahi karta.
    // Compiler ise ek constant number se replace kar deta hai.
    std::cout << "char:      " << sizeof(char)      << "\n";
    std::cout << "short:     " << sizeof(short)     << "\n";
    std::cout << "int:       " << sizeof(int)       << "\n";
    std::cout << "long:      " << sizeof(long)      << "\n";
    std::cout << "long long: " << sizeof(long long) << "\n";
    std::cout << "float:     " << sizeof(float)     << "\n";
    std::cout << "double:    " << sizeof(double)    << "\n";
    std::cout << "pointer:   " << sizeof(void*)     << "  <- 64-bit system pe 8\n";

    std::cout << "\n===== RANGES =====\n";
    std::cout << "int  max: " << INT_MAX  << "\n";
    std::cout << "int  min: " << INT_MIN  << "\n";
    std::cout << "uint max: " << UINT_MAX << "\n";

    std::cout << "\n===== UNSIGNED OVERFLOW (defined behaviour) =====\n";
    // uint8_t = 8-bit unsigned = 0 se 255
    // 255 + 1 = 256, par 8 bits mein 256 nahi samata -> wrap around hoke 0 ban jaata hai.
    // Unsigned types ke liye yeh WELL-DEFINED hai (modulo 2^N arithmetic).
    std::uint8_t small = 255;
    std::cout << "uint8_t 255 hai:   " << static_cast<int>(small) << "\n";
    small = static_cast<std::uint8_t>(small + 1);
    std::cout << "uint8_t 255+1 hai: " << static_cast<int>(small) << "  <- wrap around!\n";
    // NOTE: static_cast<int> isliye lagaya kyunki cout uint8_t ko CHARACTER samajhta hai,
    //       number nahi. Yeh ek classic beginner gotcha hai.

    std::cout << "\n===== SIGNED OVERFLOW = UNDEFINED BEHAVIOUR! =====\n";
    std::cout << "int max + 1 UB hai -- yahan demo NAHI kar rahe.\n";
    std::cout << "Compiler UB maan ke code optimize kar sakta hai, aur\n";
    std::cout << "result kuch bhi ho sakta hai. Isse hamesha bachna hai.\n";
    // GALAT: int i = INT_MAX; i = i + 1;   <-- UB, kabhi mat likhna

    std::cout << "\n===== CHARACTERS ARE NUMBERS =====\n";
    char c = 'A';
    std::cout << "'A' ka number: " << static_cast<int>(c) << "\n";
    std::cout << "'a' ka number: " << static_cast<int>('a') << "\n";
    std::cout << "'a' - 'A' = "    << ('a' - 'A') << "  <- case conversion ka raaz\n";
    std::cout << "'7' - '0' = "    << ('7' - '0') << "  <- char ko digit banane ka raaz\n";

    return 0;
}
