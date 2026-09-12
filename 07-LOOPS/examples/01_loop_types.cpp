// 01_loop_types.cpp
// ============================================================
// while / do-while / for -- teenon loop forms, aur unki equivalence
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 01_loop_types.cpp -o loops && ./loops
// ============================================================
// Yeh dikhata hai:
//   1. while    -- pehle check, phir chalao (0+ baar)
//   2. do-while -- pehle chalao, phir check (1+ baar), aur `;`
//   3. for      -- init/condition/increment ek jagah, loop var ka scope
//   4. for  <->  while equivalence
//   5. infinite loop forms + break se nikalna
//   6. loop counter ka scope
// ============================================================

#include <iostream>

int main() {
    // ============================================================
    //  1. while -- CHECK-then-RUN. Condition pehle test hoti hai.
    // ============================================================
    std::cout << "===== 1. while =====\n";
    int n = 5;
    std::cout << "  countdown: ";
    while (n > 0) {          // pehle `n > 0` check
        std::cout << n << " ";
        --n;                // ⚠️ yeh bhoole to INFINITE loop
    }
    std::cout << "\n  loop ke baad n = " << n << "\n";

    // Agar condition shuru mein hi false -> body ZERO baar chalti hai
    int m = 0;
    while (m > 0) {
        std::cout << "  yeh kabhi print nahi hoga\n";
        --m;
    }
    std::cout << "  (m > 0 false tha -> while body 0 baar chali)\n";

    // ============================================================
    //  2. do-while -- RUN-then-CHECK. Body kam se kam EK baar.
    // ============================================================
    std::cout << "\n===== 2. do-while =====\n";
    int k = 0;
    do {
        std::cout << "  do-while body chala (k = " << k << ")\n";
        ++k;
    } while (k < 0);        // ⚠️ `}` ke baad `while(...)` aur SEMICOLON zaroori
    std::cout << "  condition (k < 0) false thi, phir bhi body 1 baar chali\n";

    // Classic use: input validation / menu -- "pehle poocho, phir decide"
    // (yahan simulate kar rahe hain)
    int attempts = 0;
    int value;
    do {
        value = (attempts == 2) ? 42 : -1;   // 3rd try pe "valid" milta hai
        ++attempts;
        std::cout << "  try " << attempts << ": value = " << value << "\n";
    } while (value < 0);
    std::cout << "  valid value " << value << " " << attempts << " tries mein mila\n";

    // ============================================================
    //  3. for -- init ; condition ; increment  -- teenon ek line pe
    // ============================================================
    std::cout << "\n===== 3. for =====\n";
    std::cout << "  ";
    for (int i = 1; i <= 5; ++i) {   // i ka scope SIRF is loop mein
        std::cout << i << " ";
    }
    std::cout << "\n";
    // std::cout << i;   // ❌ ERROR -- i yahan exist nahi karta

    // for ke teeno parts optional hain
    std::cout << "  ";
    int j = 10;
    for (; j > 5;) {                 // init aur increment khaali
        std::cout << j << " ";
        --j;
    }
    std::cout << "\n";

    // ============================================================
    //  4. for  <->  while  -- yeh SAME cheez hai
    // ============================================================
    std::cout << "\n===== 4. for <-> while equivalence =====\n";
    //   for (INIT; COND; INCR) { BODY }
    // exactly barabar hai:
    //   { INIT; while (COND) { BODY; INCR; } }
    std::cout << "  for  : ";
    for (int i = 0; i < 5; ++i) std::cout << i << " ";
    std::cout << "\n  while: ";
    {
        int i = 0;
        while (i < 5) {
            std::cout << i << " ";
            ++i;
        }
    }
    std::cout << "\n";

    // ============================================================
    //  5. INFINITE loop forms -- aur break se nikalna
    // ============================================================
    std::cout << "\n===== 5. infinite loop + break =====\n";
    // for (;;)  aur  while (true)  -- dono "hamesha chalo"
    int count = 0;
    for (;;) {                        // "forever"
        ++count;
        if (count >= 3) break;       // <- nikalne ka raasta
    }
    std::cout << "  for(;;) + break: count = " << count << "\n";

    count = 0;
    while (true) {
        ++count;
        if (count >= 3) break;
    }
    std::cout << "  while(true) + break: count = " << count << "\n";
    std::cout << "  (event loops, servers, REPLs aise likhe jaate hain)\n";

    // ============================================================
    //  KAB KAUNSA
    // ============================================================
    std::cout <<
        "\n"
        "  for      -> jab iterations ki ginti/range pata ho (0..n, container)\n"
        "  while    -> jab tak koi condition sach ho (ginti pata nahi)\n"
        "  do-while -> jab body kam se kam EK baar chalni ho (menu, retry, input)\n";

    return 0;
}
