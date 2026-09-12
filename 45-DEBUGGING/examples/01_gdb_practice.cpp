// 01_gdb_practice.cpp
// ============================================================
// GDB practice program -- ismein koi bug NAHI hai. Iska kaam
// hai ki tum ispe gdb ke commands try karo: break, run, step,
// next, print, backtrace, watch, frame, finish.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -g -O0 01_gdb_practice.cpp -o gdbp
//   gdb -q ./gdbp
// ============================================================
// Lesson 02 aur 03 is file ke against REAL gdb transcripts
// dikhaate hain -- tum wahi commands khud chalao aur milaao.
// ============================================================

#include <cstdio>
#include <string>
#include <vector>

// ------------------------------------------------------------
//  Ek chhoti struct -- gdb mein `print` iska pretty form dikhata
// ------------------------------------------------------------
struct Account {
    std::string name;
    long        balance_cents;   // paisa hamesha integer cents mein (03-VARIABLES / 43/09)
    int         trades;
};

// ------------------------------------------------------------
//  Recursion -- gdb mein `backtrace` yahan gehra stack dikhayega
// ------------------------------------------------------------
static long factorial(int n) {
    if (n <= 1) {
        return 1;                       // <- yahan breakpoint lagao, `bt` maaro
    }
    long rest = factorial(n - 1);
    return static_cast<long>(n) * rest;
}

// ------------------------------------------------------------
//  Ek loop with an accumulator -- `watch total` yahan try karo
// ------------------------------------------------------------
static long sum_balances(const std::vector<Account>& accts) {
    long total = 0;
    for (std::size_t i = 0; i < accts.size(); ++i) {
        total += accts[i].balance_cents;   // har iteration pe `total` badalta
    }
    return total;
}

// ------------------------------------------------------------
//  Do-level call -- `step` andar jaata, `next` upar se guzarta,
//  `finish` current function poora karke wapas
// ------------------------------------------------------------
static long apply_fee(long balance_cents, long fee_cents) {
    long after = balance_cents - fee_cents;
    return after;
}

static long charge_all(std::vector<Account>& accts, long fee_cents) {
    long collected = 0;
    for (auto& a : accts) {
        long before = a.balance_cents;
        a.balance_cents = apply_fee(a.balance_cents, fee_cents);
        collected += (before - a.balance_cents);
        a.trades += 1;
    }
    return collected;
}

int main() {
    std::vector<Account> accts = {
        {"alice", 500000, 0},
        {"bob",   125000, 0},
        {"carol",  75000, 0},
    };

    long total_before = sum_balances(accts);            // 700000
    long fees = charge_all(accts, 2500);                // 3 * 2500 = 7500
    long total_after = sum_balances(accts);             // 692500
    long f5 = factorial(5);                             // 120

    std::printf("total_before = %ld cents\n", total_before);
    std::printf("fees_collected = %ld cents\n", fees);
    std::printf("total_after  = %ld cents\n", total_after);
    std::printf("factorial(5) = %ld\n", f5);

    // sanity (koi assert-fail nahi hona chahiye)
    if (total_before - fees != total_after) {
        std::printf("BUG: accounting mismatch\n");
        return 1;
    }
    return 0;
}
