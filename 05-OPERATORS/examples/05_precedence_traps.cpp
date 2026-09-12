// 05_precedence_traps.cpp
// ============================================================
// Precedence ke 5 classic traps
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wparentheses 05_precedence_traps.cpp -o prec
//   ./prec
//
// Compile karte waqt WARNINGS zaroor padho -- compiler in bugs ko pakadta hai.
// ============================================================

#include <iostream>
#include <cstdint>
#include <bitset>
#include <iomanip>

int main() {
    std::cout << std::boolalpha;

    // ============================================================
    //  ⚠️ TRAP 1: BITWISE OPERATORS KI PRECEDENCE BAHUT KAM HAI
    // ============================================================
    std::cout << "===== TRAP 1: BITWISE PRECEDENCE =====\n";
    // `==` ki precedence `&` se ZYADA hai.
    // Yeh C ka historical design mistake hai -- Dennis Ritchie ne khud maana tha.
    // Par tab tak bahut code likha ja chuka tha, isliye badla nahi ja saka.

    constexpr std::uint32_t FLAG = 0b0100;
    const std::uint32_t flags = 0b0110;

    std::cout << "  flags = " << std::bitset<4>(flags)
              << ",  FLAG = " << std::bitset<4>(FLAG) << "\n\n";

    // ⚠️ GALAT -- yeh `flags & (FLAG == 0)` ban jaata hai
    const bool wrong = (flags & FLAG == 0);
    std::cout << "  flags & FLAG == 0     -> " << wrong << "\n";
    std::cout << "     parse: flags & (FLAG == 0) = flags & 0 = 0 = false\n";
    std::cout << "     ⚠️ HAMESHA false aayega -- chahe flag set ho ya na ho!\n\n";

    // ✅ SAHI
    const bool right = ((flags & FLAG) == 0);
    std::cout << "  (flags & FLAG) == 0   -> " << right << "  ✅\n";
    std::cout << "  (flags & FLAG) != 0   -> " << ((flags & FLAG) != 0)
              << "   ✅ flag SET hai\n";

    std::cout << "\n  RULE: & | ^ use karo to HAMESHA brackets lagao.\n";

    // ============================================================
    //  ⚠️ TRAP 2: SHIFT AUR ARITHMETIC
    // ============================================================
    std::cout << "\n===== TRAP 2: SHIFT PRECEDENCE =====\n";
    // `+` ki precedence `<<` se ZYADA hai.
    std::cout << "  1 << 2 + 3     -> " << (1 << 2 + 3)
              << "   ⚠️ (1 << 5), kyunki + pehle chala\n";
    std::cout << "  (1 << 2) + 3   -> " << ((1 << 2) + 3) << "    ✅\n";
    std::cout << "  1 << (2 + 3)   -> " << (1 << (2 + 3)) << "   ✅ explicit\n";

    // ============================================================
    //  ⚠️ TRAP 3: cout AUR COMPARISON
    // ============================================================
    std::cout << "\n===== TRAP 3: cout << COMPARISON =====\n";
    const int a = 5, b = 3;
    // `<<` ki precedence `<` se ZYADA hai
    //   std::cout << a < b;
    //   -> (std::cout << a) < b
    //   -> ostream < int
    //   -> COMPILE ERROR
    std::cout << "  std::cout << a < b;     -> COMPILE ERROR\n";
    std::cout << "     parse: (std::cout << a) < b   -> ostream < int  ❌\n";
    std::cout << "  std::cout << (a < b);   -> " << (a < b) << "  ✅\n";

    // ============================================================
    //  ⚠️ TRAP 4: LOGICAL OPERATORS MIX
    // ============================================================
    std::cout << "\n===== TRAP 4: && aur || MIX =====\n";
    // `&&` ki precedence `||` se ZYADA hai (jaise `*` `+` se)
    const bool p = true, q = false, r = false;
    std::cout << "  p=true, q=false, r=false\n";
    std::cout << "  p || q && r      -> " << (p || q && r)
              << "   (parse: p || (q && r))\n";
    std::cout << "  (p || q) && r    -> " << ((p || q) && r)
              << "  <- BILKUL ALAG RESULT!\n";
    std::cout << "  p || (q && r)    -> " << (p || (q && r)) << "   ✅ explicit\n";
    std::cout << "\n  Technically pehla sahi hai, par padhne mein confusing.\n";
    std::cout << "  -Wparentheses warning deta hai.\n";

    // ============================================================
    //  ⚠️ TRAP 5: UNARY OPERATORS (right-associative)
    // ============================================================
    std::cout << "\n===== TRAP 5: UNARY OPERATORS =====\n";
    const int x = 5, y = 5;
    std::cout << "  x=5, y=5\n";
    std::cout << "  !x == y      -> " << (!x == y) << "\n";
    std::cout << "     parse: (!x) == y  ->  false == 5  ->  0 == 5  ->  false\n";
    std::cout << "  !(x == y)    -> " << !(x == y) << "  <- shayad yeh chahte the\n";

    // Pointer arithmetic
    std::cout << "\n  POINTERS:\n";
    int arr[] = {10, 20, 30};
    int* ptr = arr;
    std::cout << "    int arr[] = {10,20,30};  int* p = arr;\n";
    std::cout << "    *p++    -> " << *ptr++ << "   (value mili, PHIR p badha)\n";
    std::cout << "    *p ab   -> " << *ptr << "\n";
    ptr = arr;
    std::cout << "    *++p    -> " << *++ptr << "   (p PEHLE badha, phir value)\n";
    ptr = arr;
    std::cout << "    (*p)++  -> " << (*ptr)++ << "   (VALUE badhi, pointer nahi)\n";
    std::cout << "    arr[0] ab = " << arr[0] << "  <- array modify ho gaya!\n";

    // ============================================================
    //  ASSOCIATIVITY
    // ============================================================
    std::cout << "\n===== ASSOCIATIVITY =====\n";
    std::cout << "  LEFT-to-right (zyada tar):\n";
    std::cout << "    10 - 5 - 2      -> " << (10 - 5 - 2)
              << "    ((10-5)-2)\n";
    std::cout << "    100 / 10 / 2    -> " << (100 / 10 / 2)
              << "    ((100/10)/2)\n";
    std::cout << "    (cout << a << b -> (((cout << a) << b))  <- chaining isi se)\n";

    std::cout << "\n  RIGHT-to-left (unary, ternary, assignment):\n";
    int i, j, k;
    i = j = k = 7;
    std::cout << "    i = j = k = 7   -> i=" << i << " j=" << j << " k=" << k
              << "  (i = (j = (k = 7)))\n";
    std::cout << "    a ? b : c ? d : e   ->  a ? b : (c ? d : e)\n";

    // ============================================================
    //  PRACTICAL RULES
    // ============================================================
    std::cout << "\n===== 5 PRACTICAL RULES =====\n";
    std::cout << "  1. Bitwise (& | ^) use karo   -> BRACKETS lagao\n";
    std::cout << "  2. cout mein comparison       -> BRACKETS lagao\n";
    std::cout << "  3. && aur || mix karo         -> BRACKETS lagao\n";
    std::cout << "  4. Shift + arithmetic         -> BRACKETS lagao\n";
    std::cout << "  5. Confusion ho to            -> BRACKETS lagao\n";
    std::cout << "\n  Brackets FREE hain -- compiler unke liye extra code nahi banata.\n";
    std::cout << "  Agar aapko 2 second bhi sochna pada, brackets laga do.\n";

    std::cout << "\n  Detect karo:  g++ -Wall -Wextra -Wparentheses\n";

    return 0;
}
