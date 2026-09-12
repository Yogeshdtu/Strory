// 03_condition_bugs.cpp
// ============================================================
// 7 CLASSIC CONDITION BUGS -- pehchano, samjho, fix karo
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 03_condition_bugs.cpp -o bugs && ./bugs
// ============================================================
// ⚠️ Yeh file COMPILE HO JAATI HAI aur CHALTI HAI. Har bug ek
//    galat OUTPUT deta hai (crash nahi). Isi wajah se yeh khatarnak hain.
//
// Compile karte waqt COMPILER WARNINGS zaroor padho -- woh in mein se
// kai bugs khud pakad leta hai. Answers/notes sabse neeche.
// ============================================================

#include <iostream>
#include <vector>
#include <cmath>

// `&` demo ke liye -- side effect track karta hai
int rhsCalls = 0;
bool expensiveValidate() {
    ++rhsCalls;
    return true;
}

int main() {
    std::cout << std::boolalpha;

    // ============================================================
    //  BUG 1:  =  vs  ==   (assignment condition ke andar)
    // ============================================================
    std::cout << "===== BUG 1:  = vs ==  =====\n";
    int status = 0;                       // 0 = success

    if (status = 1) {                     // ⚠️ ASSIGNMENT -- status ab 1, condition true
        std::cout << "  [buggy]  error handler chala (status ab " << status << ")\n";
    }

    status = 0;
    if (status == 0) {                    // ✅ COMPARISON
        std::cout << "  [fixed]  success handler chala\n";
    }
    // Compiler: "warning: suggest parentheses around assignment used as truth value"

    // ============================================================
    //  BUG 2:  &  vs  &&   (short-circuit gayab)
    // ============================================================
    std::cout << "\n===== BUG 2:  & vs &&  =====\n";
    const std::vector<int>* data = nullptr;

    rhsCalls = 0;
    // ⚠️ `&` bitwise hai -> DONO side hamesha evaluate. Yahan `data`
    //    null hai, isliye `!data->empty()` ko chalana CRASH hota --
    //    hum ne isko safe rakha hai (sirf expensiveValidate() count karte hain).
    if ((data != nullptr) & expensiveValidate()) {
        std::cout << "  [buggy]  process\n";
    }
    std::cout << "  [buggy]  data==null hone par bhi RHS " << rhsCalls
              << " baar chala  (real code mein yeh null-deref crash hota)\n";

    rhsCalls = 0;
    if ((data != nullptr) && expensiveValidate()) {   // ✅ short-circuit
        std::cout << "  [fixed]  process\n";
    }
    std::cout << "  [fixed]  RHS " << rhsCalls << " baar chala (LHS false -> skip)\n";

    // ============================================================
    //  BUG 3:  float  ==   (exact equality)
    // ============================================================
    std::cout << "\n===== BUG 3:  float ==  =====\n";
    const double sum = 0.1 + 0.2;

    if (sum == 0.3) {
        std::cout << "  [buggy]  0.1 + 0.2 == 0.3\n";
    } else {
        std::cout.precision(17);
        std::cout << "  [buggy]  0.1 + 0.2 != 0.3   (sum = " << sum
                  << ",  0.3 = " << 0.3 << ")\n";
        std::cout.precision(6);
    }

    if (std::fabs(sum - 0.3) < 1e-9) {                 // ✅ epsilon
        std::cout << "  [fixed]  fabs(sum - 0.3) < 1e-9  -> equal maana\n";
    }

    // ============================================================
    //  BUG 4:  MISSING BRACES  (sirf pehli line if ke andar)
    // ============================================================
    std::cout << "\n===== BUG 4:  missing braces  =====\n";
    const bool isAdmin = false;

    if (isAdmin)
        std::cout << "  [buggy]  grant admin panel\n";
        std::cout << "  [buggy]  grant DELETE rights   <- yeh if ke BAHAR hai!\n";

    if (isAdmin) {                                     // ✅ braces
        std::cout << "  [fixed]  grant admin panel\n";
        std::cout << "  [fixed]  grant DELETE rights\n";
    }

    // ============================================================
    //  BUG 5:  EXTRA  ;  AFTER if
    // ============================================================
    std::cout << "\n===== BUG 5:  stray semicolon  =====\n";
    const int lives = 0;

    if (lives <= 0);                                   // ⚠️ `;` -> empty body
    {
        std::cout << "  [buggy]  GAME OVER  (lives=" << lives
                  << " -- par yeh block hamesha chalta hai)\n";
    }
    // Compiler: "warning: suggest braces around empty body in an 'if' statement"

    if (lives <= 0) {                                  // ✅
        std::cout << "  [fixed]  GAME OVER\n";
    }

    // ============================================================
    //  BUG 6:  CHAINED COMPARISON   0 < x < 10
    // ============================================================
    std::cout << "\n===== BUG 6:  chained comparison  =====\n";
    const int value = 100;                             // range se BAHAR

    // Parse: (0 < value) < 10  ->  (true) < 10  ->  1 < 10  ->  true  (hamesha!)
    if (0 < value < 10) {
        std::cout << "  [buggy]  " << value << " is in (0, 10)   <- galat, 100 hai!\n";
    }

    if (0 < value && value < 10) {                     // ✅
        std::cout << "  [fixed]  in range\n";
    } else {
        std::cout << "  [fixed]  " << value << " NOT in (0, 10)\n";
    }

    // ============================================================
    //  BUG 7:  UNSIGNED underflow condition mein
    // ============================================================
    std::cout << "\n===== BUG 7:  unsigned underflow  =====\n";
    const std::vector<int> empty;

    // empty.size() == 0, aur uska type UNSIGNED hai (size_t).
    // `size() - 1` = 0 - 1 = wrap around -> 18446744073709551615.
    // `>= 0` unsigned ke liye HAMESHA true (compiler -Wtype-limits deta hai).
    if (empty.size() - 1 >= 0) {
        std::cout << "  [buggy]  'last index' = " << (empty.size() - 1)
                  << "  (>= 0 -> 'valid' samjha) -> back() access UB hota\n";
    }

    if (!empty.empty()) {                              // ✅ guard pehle
        std::cout << "  [fixed]  last index = " << (empty.size() - 1) << "\n";
    } else {
        std::cout << "  [fixed]  empty container -> koi last element nahi\n";
    }

    // ============================================================
    //  MORAL
    // ============================================================
    std::cout << "\n-----------------------------------------------\n";
    std::cout << "GCC ne -Wall -Wextra ke saath BUG 1, 4, 5, 6, 7 warn kiye\n"
                 "(-Wparentheses, -Wmisleading-indentation, -Wempty-body,\n"
                 " -Wtype-limits). BUG 2 aur 3 SILENT nikle -- unke liye\n"
                 "aadat, code review, aur tests chahiye.\n"
                 "Isliye: -Wall -Wextra HAMESHA. Aur CI mein -Werror.\n";

    return 0;
}

// ============================================================
//                      N O T E S  /  A N S W E R S
// ============================================================
//
// BUG 1  [=  vs  ==]
//   `if (status = 1)` assignment hai: status ko 1 set karta hai, phir
//   1 ko condition (true) maanta hai. `==` chahiye tha.
//   Compiler warning: -Wparentheses.
//   Bachao: bool ko seedha likho `if (ok)`, warnings ON, ya CI mein -Werror.
//
// BUG 2  [&  vs  &&]
//   `&` bitwise AND -- short-circuit NAHI karta. Dono operands evaluate hote
//   hain. `if (p != null & p->f())` -> p null hone par bhi p->f() chalega -> crash.
//   `&&` chahiye. (`|` vs `||` bhi wahi kahani.)
//
// BUG 3  [float ==]
//   0.1, 0.2, 0.3 binary floating point mein exact nahi bante. Unka jod
//   0.3 se ~1e-17 door hota hai. `==` false deta hai.
//   Fix: |a - b| <= epsilon  (relative + absolute). Folder 05 file 03 dekho.
//
// BUG 4  [missing braces]
//   Bina { } ke, `if` sirf AGLI EK statement control karta hai. Doosri line
//   hamesha chalti hai -- yahan "DELETE rights" har user ko mil gaye.
//   Fix: hamesha braces.
//
// BUG 5  [stray ;]
//   `if (cond);` -> `;` khali statement hai, wahi if ka poora body ban gaya.
//   Neeche wala { } block ab if se juda hi nahi -- hamesha chalta hai.
//   Compiler warning: -Wempty-body.
//
// BUG 6  [chained comparison]
//   `0 < x < 10` = `(0 < x) < 10` = `(0 ya 1) < 10` = hamesha true.
//   C++ mein comparison chain nahi hoti (Python se alag).
//   Fix: `0 < x && x < 10`.
//
// BUG 7  [unsigned underflow]
//   `container.size()` ka type unsigned hai (size_t). Khali container pe
//   `size() - 1` = 0 - 1 wrap hokar bahut bada number ban jaata hai.
//   Aur `unsigned >= 0` HAMESHA true hai -> "valid index hai" wala check
//   jhootha nikal-ta hai -> back()/[] access = out-of-bounds UB.
//   Compiler warning: -Wtype-limits ("comparison ... always true").
//   Fix: pehle `!empty()` guard, ya signed `std::ssize()`, ya `size() >= 1`.
//
// ============================================================
