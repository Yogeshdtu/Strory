// 03_signed_unsigned.cpp  --  ismein EK bug hai.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -g -O0 03_signed_unsigned.cpp -o t && ./t
// Expected: gaps of {} = 0, gaps of {3,7,10} = 7.   Actual: {} pe crash / hang.
// ============================================================
#include <cstdio>
#include <vector>

// consecutive elements ke beech ke differences ka sum
static long sum_gaps(const std::vector<int>& v) {
    long s = 0;
    for (std::size_t i = 0; i < v.size() - 1; ++i) {   // <-- dekho yahan
        s += v[i + 1] - v[i];
    }
    return s;
}

int main() {
    std::vector<int> a{3, 7, 10};
    std::vector<int> empty;
    std::printf("gaps of {3,7,10} = %ld\n", sum_gaps(a));   // 7
    std::printf("gaps of {} = %ld\n", sum_gaps(empty));     // 0 -- yahan phatega
    return 0;
}

// ============================================================
// BUG:     `v.size()` ka type `std::size_t` hai -- UNSIGNED. Jab `v` khaali
//          hai, `v.size() - 1` = `0u - 1` = 18446744073709551615 (wrap),
//          crash nahi karta -- loop ~1.8e19 baar chalne lagta, `v[i+1]`
//          turant out-of-bounds -> segfault (ya "hang" jab tak crash na ho).
// SYMPTOM: Non-empty input pe theek, EMPTY input pe crash/hang. Boundary
//          bug -- "kya yeh input empty ho sakta hai?" hamesha poocho.
// TOOL:    ASan -> "heap-buffer-overflow" pehli hi iteration pe. gdb ->
//          `break sum_gaps if v.size() == 0` -> `print v.size() - 1`
//          (huge). `-Wstrict-overflow` / `-fsanitize=unsigned-integer-
//          overflow` (Clang) bhi ishaara deta. `-Wsign-conversion` yahan
//          chup hai kyunki dono side already unsigned.
// FIX:     Subtraction se pehle guard, ya subtraction hi mat karo:
//            for (std::size_t i = 0; i + 1 < v.size(); ++i)   // no underflow
//          ya `if (v.size() < 2) return 0;` pehle. Rule: unsigned se ghatao
//          to pehle `a >= b` verify karo (05-OPERATORS/03, 03-VARIABLES).
// ============================================================
