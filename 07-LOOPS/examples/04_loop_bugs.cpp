// 04_loop_bugs.cpp
// ============================================================
// 7 CLASSIC LOOP BUGS -- har ek bounded hai (program HANG nahi hoga)
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 04_loop_bugs.cpp -o lbugs && ./lbugs
// ============================================================
// Har bug ke saath ek SAFETY CAP hai (`if (++guard > LIMIT) break;`) taaki
// demo terminate ho. Real code mein woh cap NAHI hota -> program hang / crash.
// Notes/answers sabse neeche.
// ============================================================

#include <iostream>
#include <vector>

int main() {
    std::cout << std::boolalpha;
    constexpr int CAP = 1000;   // safety cap -- infinite loops ko rokne ke liye

    // ============================================================
    //  BUG 1:  OFF-BY-ONE  (<=  ke bajaye  <)
    // ============================================================
    std::cout << "===== BUG 1: off-by-one (i <= n) =====\n";
    const std::vector<int> a = {10, 20, 30, 40, 50};   // size 5, valid index 0..4
    const int n = static_cast<int>(a.size());

    std::cout << "  [buggy]  i = 0 .. n  (<=): ";
    for (int i = 0; i <= n; ++i) {                     // ⚠️ i == 5 pe a[5] -> OOB
        if (i < n) std::cout << a[static_cast<std::size_t>(i)] << " ";
        else       std::cout << "[OOB read at i=" << i << "] ";
    }
    std::cout << "\n";

    std::cout << "  [fixed]  i = 0 .. n-1 (<) : ";
    for (int i = 0; i < n; ++i) {
        std::cout << a[static_cast<std::size_t>(i)] << " ";
    }
    std::cout << "\n";

    // ============================================================
    //  BUG 2:  INCREMENT bhool gaye  -> infinite
    // ============================================================
    std::cout << "\n===== BUG 2: increment missing =====\n";
    int i = 0, guard = 0;
    while (i < 5) {
        // ⚠️ ++i yahan hona chahiye tha
        if (++guard > CAP) { std::cout << "  [buggy]  SAFETY CAP hit -- i abhi bhi "
                                       << i << ", loop kabhi khatam nahi hota\n"; break; }
    }

    i = 0;
    while (i < 5) { ++i; }                             // ✅
    std::cout << "  [fixed]  loop khatam, i = " << i << "\n";

    // ============================================================
    //  BUG 3:  UNSIGNED reverse loop  (i >= 0 hamesha true)
    // ============================================================
    std::cout << "\n===== BUG 3: unsigned reverse loop =====\n";
    const std::vector<int> data = {1, 2, 3};
    guard = 0;
    std::cout << "  [buggy]  for (size_t i = size-1; i >= 0; --i): ";
    for (std::size_t idx = data.size() - 1; idx >= 0; --idx) {   // ⚠️ i >= 0 hamesha true
        if (idx < data.size()) std::cout << data[idx] << " ";
        else                   std::cout << "[wrap -> idx=" << idx << ", OOB] ";
        if (++guard >= 5) { std::cout << "... `idx >= 0` unsigned pe HAMESHA true\n"; break; }
    }

    std::cout << "  [fixed]  for (size_t i = size; i-- > 0; ): ";
    for (std::size_t idx = data.size(); idx-- > 0; ) {
        std::cout << data[idx] << " ";
    }
    std::cout << "\n";

    // ============================================================
    //  BUG 4:  FLOAT loop counter  (f != 1.0 kabhi nahi milta)
    // ============================================================
    std::cout << "\n===== BUG 4: float loop counter =====\n";
    guard = 0;
    std::cout << "  [buggy]  for (double f = 0; f != 1.0; f += 0.1):\n    ";
    std::cout.precision(17);
    for (double f = 0.0; f != 1.0; f += 0.1) {
        std::cout << f << "  ";
        if (++guard >= 12) { std::cout << "\n    ... 10th step pe f = 0.999...989 (1.0 nahi), "
                                          "phir 1.0 ko skip kar gaya\n"; break; }
    }
    std::cout.precision(6);

    std::cout << "  [fixed]  integer counter, phir *0.1:\n    ";
    for (int step = 0; step < 10; ++step) {
        std::cout << (step * 0.1) << " ";
    }
    std::cout << "\n";

    // ============================================================
    //  BUG 5:  BODY ke andar loop counter modify
    // ============================================================
    std::cout << "\n===== BUG 5: counter modified in body =====\n";
    const std::vector<int> xs = {0, 1, 2, 3, 4, 5, 6, 7};
    std::cout << "  [buggy]  body mein ++idx extra -> elements skip:\n    ";
    for (std::size_t idx = 0; idx < xs.size(); ++idx) {
        std::cout << xs[idx] << " ";
        ++idx;                                         // ⚠️ ab loop 0,2,4,6 dega
    }
    std::cout << "  <- 1,3,5,7 miss\n";

    std::cout << "  [fixed]  step chahiye to loop increment mein:\n    ";
    for (std::size_t idx = 0; idx < xs.size(); idx += 2) {
        std::cout << xs[idx] << " ";
    }
    std::cout << "\n";

    // ============================================================
    //  BUG 6:  GALAT DIRECTION  (++ jahan -- chahiye)
    // ============================================================
    std::cout << "\n===== BUG 6: wrong update direction =====\n";
    guard = 0;
    std::cout << "  [buggy]  for (int c = 5; c > 0; ++c): ";
    for (int c = 5; c > 0; ++c) {                      // ⚠️ c badhta hai -> kabhi <= 0 nahi (overflow tak)
        std::cout << c << " ";
        if (++guard >= 8) { std::cout << "... c ghatta nahi, badhta hai -> never ends\n"; break; }
    }
    std::cout << "  [fixed]  for (int c = 5; c > 0; --c): ";
    for (int c = 5; c > 0; --c) std::cout << c << " ";
    std::cout << "\n";

    // ============================================================
    //  BUG 7:  while + continue  -> increment skip -> infinite
    // ============================================================
    std::cout << "\n===== BUG 7: continue skips increment =====\n";
    i = 0; guard = 0;
    std::cout << "  [buggy]  while: continue ++i se PEHLE hai\n";
    while (i < 6) {
        if (i == 3) {
            // continue -> seedha condition pe wapas, ++i skip -> i hamesha 3
            if (++guard > CAP) { std::cout << "    SAFETY CAP -- i stuck at " << i << "\n"; break; }
            continue;
        }
        std::cout << "    processing " << i << "\n";
        ++i;
    }

    i = 0;
    std::cout << "  [fixed]  for-loop: increment continue se affected nahi\n    ";
    for (int idx = 0; idx < 6; ++idx) {                // for ka increment continue ke baad bhi chalta hai
        if (idx == 3) continue;
        std::cout << idx << " ";
    }
    std::cout << "\n";

    // ============================================================
    //  MORAL
    // ============================================================
    std::cout <<
        "\n-----------------------------------------------\n"
        "* Half-open ranges [0, n) socho -> `i < n`, `<=` nahi.\n"
        "* Reverse iterate: `for (i = n; i-- > 0;)` ya range-for.\n"
        "* Float ko loop counter mat banao -- integer counter, phir scale.\n"
        "* Counter sirf loop-header mein badlo, body mein nahi.\n"
        "* while + continue: `++i` ko continue se PEHLE rakho (ya for use karo).\n"
        "* -Wall -Wextra: -Wtype-limits, -Wsign-compare in mein se kuch pakadta hai.\n";

    return 0;
}

// ============================================================
//                   N O T E S  /  A N S W E R S
// ============================================================
//
// BUG 1  [off-by-one]  `i <= n` array ke aakhri valid index (n-1) se ek aage
//   jaata hai -> a[n] out-of-bounds (UB: garbage read ya crash). Half-open
//   range [0, n) yaad rakho: `for (i = 0; i < n; ++i)`. Ya range-for.
//
// BUG 2  [increment missing]  `while` ki condition kabhi false nahi hoti kyunki
//   `i` badalta hi nahi -> infinite loop. `for` mein increment header mein hota
//   hai -> yeh galti kam hoti hai. Isliye known count pe `for` prefer karo.
//
// BUG 3  [unsigned reverse]  `size()` ka type unsigned (size_t). `for (size_t i =
//   n-1; i >= 0; --i)` -> `i >= 0` HAMESHA true (unsigned kabhi negative nahi).
//   Aur `i == 0` pe `--i` wrap hokar huge -> a[huge] OOB. -Wtype-limits warn.
//   Fixes: `for (size_t i = n; i-- > 0;)`, ya `std::ssize()` (signed), ya range-for.
//
// BUG 4  [float counter]  0.1 binary floating point mein exact nahi. Jodte-jodte
//   error jama hota hai -> `f` kabhi exactly 1.0 nahi. `!=` condition -> infinite
//   (ya UB range). Integer counter use karo: `for (int k = 0; k < 10; ++k) f = k*0.1;`
//
// BUG 5  [counter modified in body]  Loop header `++idx` karta hai, phir body
//   `++idx` aur -> 2 ka step -> aadhe elements skip. Step chahiye to LOOP INCREMENT
//   mein likho (`idx += 2`), body mein nahi.
//
// BUG 6  [wrong direction]  `c > 0` ko false karne ke liye `c` GHATNA chahiye, par
//   `++c` badha raha hai -> condition kabhi false nahi (int overflow tak, jo UB).
//
// BUG 7  [continue skips increment]  `while` mein `continue` seedha condition-check
//   pe jaata hai. Agar `++i` `continue` ke BAAD hai, to woh skip ho jaata hai ->
//   `i` stuck -> infinite. `for` mein increment clause `continue` ke baad bhi
//   chalta hai -> yeh bug `for` mein nahi hota.
//
// ============================================================
