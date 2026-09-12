// 02_uninitialized.cpp  --  ismein EK bug hai.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -g -O0 02_uninitialized.cpp -o t && ./t
//   g++ -std=c++20 -Wall -Wextra -O2 02_uninitialized.cpp -o t   # <- yahan warning milegi
// Expected: "max = 91"   Actual: kabhi 91, kabhi ek bada garbage number.
// ============================================================
#include <cstdio>
#include <vector>

static int find_max(const std::vector<int>& v) {
    int running_max;                 // <-- dekho yahan
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (v[i] > running_max) {
            running_max = v[i];
        }
    }
    return running_max;
}

int main() {
    std::vector<int> v{42, 17, 91, 8, 63};
    std::printf("max = %d\n", find_max(v));
    return 0;
}

// ============================================================
// BUG:     `int running_max;` initialize nahi hua. Pehli iteration mein
//          `v[0] > running_max` ek GARBAGE value se compare hota. Agar
//          garbage bahut bada hua, koi bhi `v[i]` use beat nahi karega,
//          aur function garbage return karega.
// SYMPTOM: Non-deterministic. `-O0` pe stack slot pe jo pada tha wahi;
//          alag build/run/machine pe alag. Kabhi "sahi" (garbage chhota
//          nikal aaya).
// TOOL:    `-Wmaybe-uninitialized` -- `-O1`/`-O2` ke saath GCC warn karta
//          (`-O0` pe yeh analysis nahi chalta). Clang MSan
//          (`-fsanitize=memory`) runtime pe exact use pakadta. valgrind
//          memcheck: "Conditional jump ... depends on uninitialised
//          value(s)".
// FIX:     Sensible seed do:
//            int running_max = v.empty() ? INT_MIN : v[0];
//          ya `#include <algorithm>` -> `return *std::max_element(v.begin(),
//          v.end());` (empty pe pehle check). Rule: HAR local ko declare
//          karte hi initialize karo (03-VARIABLES/03, 22-MODERN-CPP).
// ============================================================
