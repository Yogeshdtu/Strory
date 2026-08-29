// 02_multiple_outputs.cpp
// ============================================================
// cout ko alag alag tareekon se use karna
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 02_multiple_outputs.cpp -o out && ./out
// ============================================================

#include <iostream>

int main() {
    // ---------- Tareeka 1: alag alag statements ----------
    std::cout << "Line ek\n";
    std::cout << "Line do\n";
    std::cout << "Line teen\n";

    std::cout << "\n";  // ek khali line

    // ---------- Tareeka 2: CHAINING ----------
    // `<<` har baar `std::cout` ko WAPAS return karta hai.
    // Isliye aap chain bana sakte ho:
    //
    //   std::cout << "A"   -> returns std::cout
    //   (std::cout) << "B" -> returns std::cout
    //   (std::cout) << "C" -> returns std::cout
    //
    // Left se right chalta hai.
    std::cout << "Line char" << "\n" << "Line paanch" << "\n";

    std::cout << "\n";

    // ---------- Tareeka 3: ek statement, kai lines ----------
    // Compiler ke liye newline ka koi matlab nahi hai. Sirf `;` statement
    // khatam karta hai. Isliye lambi statement ko todna bilkul theek hai --
    // aur padhne mein aasan hai.
    std::cout << "Yeh ek statement hai "
              << "jo kai lines mein "
              << "todi gayi hai.\n";

    std::cout << "\n";

    // ---------- Alag alag types print karna ----------
    // `<<` OVERLOADED hai -- har type ke liye alag version hai.
    // Compiler khud sahi version chunta hai (overload resolution).
    std::cout << "Integer:    " << 42        << "\n";
    std::cout << "Double:     " << 3.14      << "\n";
    std::cout << "Character:  " << 'A'       << "\n";
    std::cout << "String:     " << "namaste" << "\n";
    std::cout << "Bool:       " << true      << "  <- 1 dikhta hai, 'true' nahi\n";
    std::cout << "Expression: " << (2 + 3)   << "\n";

    std::cout << "\n";

    // ---------- OPERATOR PRECEDENCE KA TRAP ----------
    // `<<` ki precedence `+` se KAM hai, isliye `+` pehle chalta hai.
    std::cout << "1 + 2 ka result: " << 1 + 2 << "\n";   // 3 -- pehle jodo, phir print

    // Lekin `<<` ki precedence `<` (comparison) se ZYADA hai!
    // Isliye brackets zaroori hain, warna galat parse hoga.
    // std::cout << 1 < 2;     // ❌ compiler isko (std::cout << 1) < 2 padhega
    std::cout << "1 < 2 ka result: " << (1 < 2) << "  <- brackets zaroori the\n";

    std::cout << "\n";

    // ---------- cerr: error stream ----------
    // cout  -> stdout  (buffered)
    // cerr  -> stderr  (UNBUFFERED -- turant dikhta hai)
    //
    // Yeh isliye alag hain taaki aap normal output aur errors ko alag
    // redirect kar sako:
    //   ./out > output.txt        <- sirf cout file mein jayega
    //   ./out 2> errors.txt       <- sirf cerr file mein jayega
    //   ./out > all.txt 2>&1      <- dono ek file mein
    std::cout << "Yeh cout pe gaya (normal output)\n";
    std::cerr << "Yeh cerr pe gaya (error output)\n";

    return 0;
}
