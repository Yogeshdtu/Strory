// 01_references_basics.cpp
// ============================================================
// Reference = alias. Copy vs reference vs pointer -- side by side.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 01_references_basics.cpp -o rb && ./rb
// ============================================================
// int  x = 10;
// int& r = x;     // r ek ALIAS hai x ka -- naya object NAHI. r aur x ek hi memory.
// int  c = x;     // c ek COPY hai   -- alag memory, alag zindagi.
// int* p = &x;    // p ek POINTER hai -- x ka address rakhta hai, *p se pahuncho.
// ============================================================

#include <iostream>

int main() {
    // ============================================================
    //  1. reference ek ALIAS hai -- ek object ke do naam
    // ============================================================
    std::cout << "===== 1. reference = alias (same object, doosra naam) =====\n";
    int x = 10;
    int& r = x;                 // r ko ABHI bind karna zaroori -- baad mein nahi
    std::cout << "  x = " << x << ", r = " << r << "\n";
    std::cout << "  &x = " << &x << "\n";
    std::cout << "  &r = " << &r << "   <- BILKUL same address (r koi nayi jagah nahi ghera)\n";

    r = 99;                     // r ko likho -> x badal gaya
    std::cout << "  r = 99;  ->  x ab = " << x << "\n";
    x = 7;                      // x ko likho -> r wahi dikhata hai
    std::cout << "  x = 7;   ->  r ab = " << r << "\n";

    // ============================================================
    //  2. copy alag object hai
    // ============================================================
    std::cout << "\n===== 2. copy = alag object =====\n";
    int c = x;                  // c mein x ki value copy hui
    c = 500;
    std::cout << "  c = 500;  ->  c = " << c << ", x still = " << x
              << "   (copy independent hai)\n";

    // ============================================================
    //  3. reference REBIND nahi hoti
    // ============================================================
    std::cout << "\n===== 3. reference rebind nahi hoti =====\n";
    int y = 1234;
    r = y;                      // yeh 'r ko rebind' NAHI hai -- yeh 'x = y' hai!
    std::cout << "  r = y;  ->  x ab = " << x << "   (r abhi bhi x ka alias, y ka nahi)\n";
    std::cout << "  &r = " << &r << " (still x)   &y = " << &y << "\n";

    // ============================================================
    //  4. pointer REBIND kar sakta hai
    // ============================================================
    std::cout << "\n===== 4. pointer rebind kar sakta hai =====\n";
    int* p = &x;
    std::cout << "  p -> x,  *p = " << *p << "\n";
    p = &y;                     // ab p, y ko point karta hai -- reference yeh nahi kar sakti
    std::cout << "  p = &y,  *p = " << *p << "\n";

    // ============================================================
    //  5. syntax: reference ko use karte waqt * / & nahi chahiye
    // ============================================================
    std::cout << "\n===== 5. syntax bojh =====\n";
    int sum_via_ref = r + 1;        // seedha -- naam jaisa
    int sum_via_ptr = *p + 1;       // deref (*) lagana padta hai
    std::cout << "  r + 1   = " << sum_via_ref << "   (reference: normal variable jaisa)\n";
    std::cout << "  *p + 1  = " << sum_via_ptr << "   (pointer: * chahiye)\n";

    // ============================================================
    //  6. reference ka koi 'null' nahi hota
    // ============================================================
    std::cout << "\n===== 6. null =====\n";
    std::cout << "  pointer nullptr ho sakta hai; reference hamesha kisi object se bandhi hoti hai.\n";
    // int& bad;            // compile ERROR -- uninitialized reference allowed nahi
    // int& n = nullptr;    // compile ERROR -- reference ko object chahiye, address nahi

    std::cout <<
        "\n"
        "  int& r = x;   -- r aur x ek hi memory (&r == &x)\n"
        "  r = ...       -- hamesha x ko likhta hai (rebind nahi)\n"
        "  int  c = x;   -- alag memory (copy)\n"
        "  int* p = &x;  -- rebindable, nullable, * chahiye\n";
    return 0;
}
