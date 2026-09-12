// 01_first_functions.cpp
// ============================================================
// Function basics -- define, call, parameters, return, void
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 01_first_functions.cpp -o ff && ./ff
// ============================================================
// Yeh dikhata hai:
//   1. sabse chhota function; call kaise hota hai
//   2. parameters (input) aur return value (output)
//   3. void function -- kuch return nahi karta
//   4. ek function doosre ko call karta hai
//   5. declare-before-use -- compiler ko pehle batana padta hai
//   6. DRY -- copy-paste hatana
// ============================================================

#include <iostream>
#include <string>
#include <string_view>

// ------------------------------------------------------------
//  1. Sabse chhota useful function: do int lo, sum do
// ------------------------------------------------------------
int add(int a, int b) {          // `int` return type | `add` naam | (int a, int b) parameters
    return a + b;                // `return` -> caller ko value bhejo, function khatam
}

// ------------------------------------------------------------
//  2. void -- kaam karta hai, value nahi lautta
// ------------------------------------------------------------
void greet(std::string_view name) {
    std::cout << "  Namaste, " << name << "!\n";
    // koi return nahi -- ya bare `return;` likh sakte ho jaldi nikalne ko
}

// ------------------------------------------------------------
//  3. Ek function doosre ko call karta hai
// ------------------------------------------------------------
int square(int x) {
    return x * x;
}
int sumOfSquares(int a, int b) {
    return square(a) + square(b);   // square() do baar call
}

// ------------------------------------------------------------
//  4. DECLARE-BEFORE-USE: yahan sirf declaration (prototype).
//     Definition neeche main() ke baad hai.
// ------------------------------------------------------------
bool isPrime(int n);                 // "aisa function exist karta hai" -- compiler khush

// ------------------------------------------------------------
//  5. DRY -- pehle (galat): same 4 lines 3 baar. Ab: ek function.
// ------------------------------------------------------------
void printBox(std::string_view title) {
    const std::string bar(title.size() + 4, '=');
    std::cout << "  " << bar << "\n";
    std::cout << "  = " << title << " =\n";
    std::cout << "  " << bar << "\n";
}

int main() {
    // ============================================================
    //  1. CALL -- naam + (arguments)
    // ============================================================
    std::cout << "===== 1. add() =====\n";
    int result = add(3, 4);          // 3, 4 = ARGUMENTS ; a, b = PARAMETERS
    std::cout << "  add(3, 4) = " << result << "\n";
    std::cout << "  add(add(1,2), add(3,4)) = " << add(add(1, 2), add(3, 4)) << "\n";

    // ============================================================
    //  2. void function
    // ============================================================
    std::cout << "\n===== 2. greet() (void) =====\n";
    greet("Yogesh");
    greet("World");
    // int x = greet("X");   // ❌ ERROR -- void kuch return nahi karta

    // ============================================================
    //  3. Function calling function
    // ============================================================
    std::cout << "\n===== 3. sumOfSquares() =====\n";
    std::cout << "  sumOfSquares(3, 4) = " << sumOfSquares(3, 4)
              << "   (9 + 16)\n";

    // ============================================================
    //  4. isPrime() -- declaration upar thi, definition neeche
    // ============================================================
    std::cout << "\n===== 4. isPrime() (defined after main) =====\n";
    std::cout << "  ";
    for (int n = 2; n <= 20; ++n)
        if (isPrime(n)) std::cout << n << " ";
    std::cout << "\n";

    // ============================================================
    //  5. DRY -- ek function, 3 calls
    // ============================================================
    std::cout << "\n===== 5. printBox() -- DRY =====\n";
    printBox("SECTION A");
    printBox("A longer heading");

    return 0;
}

// ------------------------------------------------------------
//  isPrime ki DEFINITION -- main ke baad, par declaration pehle thi
// ------------------------------------------------------------
bool isPrime(int n) {
    if (n < 2) return false;             // guard clause (folder 06)
    for (int d = 2; d * d <= n; ++d)     // sqrt(n) tak check kaafi hai
        if (n % d == 0) return false;
    return true;
}
