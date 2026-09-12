// 01_cout_basics.cpp
// ============================================================
// std::cout ka poora tour -- chaining, types, precedence, traps
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 01_cout_basics.cpp -o cout && ./cout
// ============================================================

#include <iostream>
#include <string>

// Custom type ke liye apna operator<<.
// Yeh operator overloading ka pehla practical example hai -- folder 15 mein poora.
struct Point {
    int x;
    int y;
};

// `os` ko REFERENCE se lete hain (streams copy nahi ho sakte).
// Aur `os` ko WAPAS return karte hain -- isi se chaining kaam karti hai.
std::ostream& operator<<(std::ostream& os, const Point& p) {
    return os << "(" << p.x << ", " << p.y << ")";
}

int main() {
    std::cout << "===== CHAINING KAISE KAAM KARTI HAI =====\n";
    // Har `<<` `std::cout` ko WAPAS return karta hai:
    //   std::cout << "A"   -> returns std::cout&
    //   (std::cout) << "B" -> returns std::cout&
    //   ...
    // Left-to-right chalta hai.
    std::cout << "A" << "B" << "C" << "\n";

    std::cout << "\n===== ALAG TYPES =====\n";
    // `<<` OVERLOADED hai -- har type ke liye alag version hai.
    // Compiler overload resolution karke sahi chunta hai.
    std::cout << "int:     " << 42 << "\n";
    std::cout << "double:  " << 3.14 << "\n";
    std::cout << "char:    " << 'A' << "\n";
    std::cout << "cstring: " << "hello" << "\n";
    std::cout << "string:  " << std::string("world") << "\n";
    std::cout << "bool:    " << true << "   <- 1 dikha, 'true' nahi\n";
    std::cout << "bool:    " << std::boolalpha << true << std::noboolalpha
              << " <- boolalpha ke saath\n";
    std::cout << "custom:  " << Point{1, 2} << "  <- apna operator<<\n";

    std::cout << "\n===== ⚠️ PRECEDENCE TRAP =====\n";
    // `<<` ki precedence:
    //   KAM  hai `+ - * / %` se  -> woh pehle chalte hain  ✅
    //   ZYADA hai `< > == && ||` se -> unhe BRACKETS chahiye ⚠️
    std::cout << "1 + 2      = " << 1 + 2 << "   (+ pehle chala, theek hai)\n";
    std::cout << "2 * 3      = " << 2 * 3 << "   (theek hai)\n";
    std::cout << "10 % 3     = " << 10 % 3 << "   (theek hai)\n";

    // std::cout << 1 < 2;
    // ❌ Yeh COMPILE ERROR hai! Compiler ise aise padhta hai:
    //      (std::cout << 1) < 2
    //    aur `ostream < int` ka koi operator nahi hai.
    std::cout << "(1 < 2)    = " << (1 < 2) << "   <- BRACKETS zaroori the\n";
    std::cout << "(3 == 3)   = " << (3 == 3) << "   <- yahan bhi\n";

    std::cout << "\n===== ⚠️ POINTER TRAP =====\n";
    int value = 5;
    int* intPtr = &value;
    const char* charPtr = "text";

    // `int*` -> address print hota hai
    std::cout << "int*:          " << intPtr << "  <- address\n";
    // `char*` -> STRING print hoti hai! (special overload hai)
    std::cout << "const char*:   " << charPtr << "  <- STRING, address nahi!\n";
    // Address chahiye to void* mein cast karo
    std::cout << "(void*)char*:  " << static_cast<const void*>(charPtr)
              << "  <- ab address\n";

    std::cout << "\n===== ⚠️ uint8_t TRAP =====\n";
    // uint8_t actually `unsigned char` ka alias hai.
    // cout ke liye woh ek CHARACTER hai, number nahi.
    unsigned char smallNum = 65;
    std::cout << "unsigned char 65:      " << smallNum
              << "   <- 'A' dikha!\n";
    std::cout << "+smallNum:             " << +smallNum
              << "  <- unary + promote karta hai\n";
    std::cout << "static_cast<int>:      " << static_cast<int>(smallNum) << "\n";

    std::cout << "\n===== MEMBER FUNCTIONS =====\n";
    // put() -- ek raw character
    std::cout.put('X');
    std::cout.put('\n');

    // write() -- exactly N bytes, embedded '\0' bhi
    std::cout.write("Hello", 5);
    std::cout.put('\n');

    // Fark dekho: operator<< '\0' pe RUK jaata hai, write() nahi
    const char data[] = {'A', '\0', 'B'};
    std::cout << "operator<<: [" << data << "]  <- \\0 pe ruk gaya\n";
    std::cout << "write(3):   [";
    std::cout.write(data, 3);
    std::cout << "]  <- teenon bytes (beech mein \\0 hai)\n";

    std::cout << "\n===== STREAM STATE =====\n";
    std::cout << "good(): " << std::cout.good() << "\n";
    std::cout << "fail(): " << std::cout.fail() << "\n";
    std::cout << "bad():  " << std::cout.bad()  << "\n";

    std::cout << "\n===== ADJACENT LITERALS =====\n";
    // Do string literals side-by-side hon to compiler unhe JOD deta hai.
    // Yeh COMPILE TIME pe hota hai -- ek hi operator<< call hoti hai.
    std::cout << "Yeh " "ek " "hi " "string " "hai\n";

    return 0;
}
