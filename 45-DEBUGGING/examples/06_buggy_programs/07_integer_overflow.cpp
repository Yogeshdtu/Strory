// 07_integer_overflow.cpp  --  ismein EK bug hai.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -g -O0 07_integer_overflow.cpp -o t && ./t
// Expected: "REJECT: over cap"   Actual: "ACCEPT" (aur phir buffer OOB).
// ============================================================
#include <cstdio>
#include <vector>

static constexpr int kCap = 2'000'000'000;   // ~2e9, int ke max (2.147e9) ke neeche

// existing exposure + naya order cap ke andar hai? Agar haan to book karo.
static bool try_book(std::vector<long>& book, int existing, int add) {
    if (existing + add > kCap) {              // <-- dekho yahan
        std::printf("REJECT: over cap (%d + %d)\n", existing, add);
        return false;
    }
    std::printf("ACCEPT: %d + %d\n", existing, add);
    book.push_back(static_cast<long>(existing) + add);
    return true;
}

int main() {
    std::vector<long> book;
    // dono bade, sum > kCap -- reject hona chahiye
    try_book(book, 1'500'000'000, 1'400'000'000);   // sum = 2.9e9 > 2e9
    std::printf("book size = %zu (expected 0)\n", book.size());
    return 0;
}

// ============================================================
// BUG:     `existing + add` -- dono `int`. 1.5e9 + 1.4e9 = 2.9e9, jo `int`
//          ke max (~2.147e9) se bada. Signed integer overflow = UNDEFINED
//          BEHAVIOUR. Practice mein wrap hoke ~ -1.4e9 ho jaata -> `> kCap`
//          FALSE -> order galti se ACCEPT.
// SYMPTOM: Bade numbers pe check "kaam nahi karta". Chhote numbers pe
//          theek. `-O2` pe optimizer "signed overflow ho hi nahi sakta"
//          maan ke check aur bhi aggressively kaat sakta.
// TOOL:    `-fsanitize=undefined` -> "signed integer overflow: 1500000000
//          + 1400000000 cannot be represented in type 'int'" -- exact line.
//          `-ftrapv` (overflow pe abort). `-Wstrict-overflow=2`.
// FIX:     Overflow-safe comparison -- ghatao, jodo mat:
//            if (add > kCap - existing) { ... reject ... }   // dono side int-safe
//          ya wider type mein promote karo:
//            if (static_cast<long long>(existing) + add > kCap) ...
//          (05-OPERATORS/01, 23-ERROR-HANDLING/13, 43-HFT-OPTIMIZATION
//          fixed-point discussion).
// ============================================================
