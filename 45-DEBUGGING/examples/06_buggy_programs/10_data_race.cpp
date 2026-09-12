// 10_data_race.cpp  --  ismein EK bug hai.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -g -O0 -pthread 10_data_race.cpp -o t && ./t
//   for i in $(seq 1 40); do ./t; done          # ~15-20% runs INCONSISTENT
// Expected: "processed 40000 ... counts consistent"  (har baar)
// Actual:   kabhi-kabhi "processed < 40000" ya "accepted+rejected != processed",
//           har run alag -- race ka fingerprint.
//
// NOTE: `-O2` pe yeh race aksar CHHUP jaata -- compiler `g_stats.processed
//       += 1` ko loop se register mein hoist karke ek hi store kar deta
//       ("register promotion"), to har thread ka contribution lagbhag
//       atomic ho jaata. "Chalta hai -O2 pe" == "sahi hai" NAHI -- UB
//       abhi bhi UB.
// ============================================================
#include <cstdio>
#include <thread>
#include <vector>

struct Stats {
    long processed = 0;
    long accepted  = 0;
    long rejected  = 0;
};

static Stats g_stats;                          // <-- shared, no sync

// order id even -> accept, odd -> reject. (dummy rule)
static void worker(int start, int count) {
    for (int i = 0; i < count; ++i) {
        int id = start + i;
        g_stats.processed += 1;               // <-- teen non-atomic RMW
        if (id % 2 == 0) g_stats.accepted += 1;
        else             g_stats.rejected += 1;
    }
}

int main() {
    const int kThreads = 4;
    const int kEach = 10000;
    std::vector<std::thread> ts;
    for (int t = 0; t < kThreads; ++t) ts.emplace_back(worker, t * kEach, kEach);
    for (auto& th : ts) th.join();

    const long expect = static_cast<long>(kThreads) * kEach;   // 40000
    std::printf("processed %ld (expect %ld), accepted %ld + rejected %ld = %ld\n",
                g_stats.processed, expect,
                g_stats.accepted, g_stats.rejected,
                g_stats.accepted + g_stats.rejected);
    const bool ok = (g_stats.processed == expect) &&
                    (g_stats.accepted + g_stats.rejected == g_stats.processed);
    std::printf("%s\n", ok ? "counts consistent" : "  <-- INCONSISTENT (race)");
    return ok ? 0 : 1;
}

// ============================================================
// BUG:     `g_stats` ke fields ko 4 threads bina kisi mutex/atomic ke
//          `+= 1` karte. `x += 1` = load, add, store -- do threads
//          interleave karte to updates kho jaate. Teen alag fields, teen
//          alag races. Data race = UNDEFINED BEHAVIOUR (27-ATOMICS/01).
// SYMPTOM: `processed` aksar 40000 se kam. Kabhi `accepted + rejected !=
//          processed` (kyunki teen counters independently corrupt hote).
//          Har run alag -- non-determinism = race ka fingerprint. Kam
//          threads / kam iterations pe kabhi "chal jaata" (aur chhup
//          jaata).
// TOOL:    ThreadSanitizer (`-fsanitize=thread`) -> "data race ... Write
//          ... Previous write ... Location is global 'g_stats'" dono
//          stack traces ke saath. valgrind `--tool=helgrind`. Ya reasoning:
//          shared mutable state + no synchronization + non-deterministic
//          output.
// FIX (koi ek):
//   1. std::atomic<long> processed{0}, accepted{0}, rejected{0};
//        -> `.fetch_add(1, std::memory_order_relaxed)` (26/13, 27/06).
//   2. Har field ke liye ya poore struct ke liye ek std::mutex, har
//      update `std::lock_guard` ke andar (26/06).
//   3. Har thread apna local `Stats` bhare, end mein ek baar (lock ya
//      single-thread merge) jodo -- contention hi khatam (best when
//      possible; 41-HFT-CONCURRENCY sharding).
// VERIFY:  fix ke baad `for i in $(seq 1 100); do ./t || break; done` ->
//          100/100 "counts consistent". Ek bhi fail = fix adhoora.
// ============================================================
