// 01_off_by_one.cpp  --  ismein EK bug hai. Dhoondo, tool chuno, fix karo.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -g -O0 01_off_by_one.cpp -o t && ./t
// Expected: "sum of 5 scores = 350"   Actual: kabhi 350+garbage, kabhi crash.
// ============================================================
#include <cstdio>
#include <vector>

static long sum_scores(const std::vector<int>& scores) {
    long total = 0;
    for (std::size_t i = 0; i <= scores.size(); ++i) {   // <-- dekho yahan
        total += scores[i];
    }
    return total;
}

int main() {
    std::vector<int> scores{100, 80, 40, 90, 40};   // sum = 350
    std::printf("sum of %zu scores = %ld\n", scores.size(), sum_scores(scores));
    return 0;
}

// ============================================================
// BUG:     Loop condition `i <= scores.size()`. Valid indices 0..size-1
//          hote hain; `i == size` pe `scores[size]` ek element AAGE padhta
//          hai -> out-of-bounds read (UB).
// SYMPTOM: Non-deterministic -- kabhi 350 + kachra, kabhi (agar us jagah
//          unmapped page ho) segfault. "Kabhi kaam karta" = classic UB tell.
// TOOL:    ASan (`-fsanitize=address`) -> "heap-buffer-overflow READ ...
//          0 bytes to the right of ..." exact line pe. Ya gdb:
//          `break sum_scores` -> `watch i` -> jab `i == 5` -> `print
//          scores.size()` (5) -> index 5 invalid. Ya -D_GLIBCXX_ASSERTIONS
//          (operator[] bhi check karega).
// FIX:     `for (std::size_t i = 0; i < scores.size(); ++i)`   (< not <=)
//          Ya range-for: `for (int s : scores) total += s;` -- index hi nahi,
//          off-by-one impossible (07-LOOPS/04, 09-ARRAYS/03).
// ============================================================
