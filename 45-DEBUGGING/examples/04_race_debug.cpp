// 04_race_debug.cpp
// ============================================================
// ⚠️  DATA RACE. Do threads ek `long` counter ko bina synchronisation
//     ke ++ karte hain. Compile CLEAN. Runtime pe: total galat aata
//     hai (expected se kam), aur har run pe alag -- classic race
//     signature. Yeh lesson 06 (TSan) aur 08 (multithreaded debug)
//     ka lab hai.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -g -O2 -pthread 04_race_debug.cpp -o race
//   ./race                 # "total = 1873421  (expected 2000000)"  -- galat
//   for i in $(seq 1 20); do ./race; done   # har baar alag number
//
//   # Linux / Clang -- race ko naam se pakdo:
//   g++ -std=c++20 -g -O1 -pthread -fsanitize=thread 04_race_debug.cpp -o race_tsan
//   ./race_tsan            # ThreadSanitizer: data race report (neeche)
// ============================================================
// MinGW/Windows box pe libtsan nahi -- yeh file yahan compile+run
// hoti hai (race dikhta hai as a wrong total), par TSan report ke
// liye Linux/Clang chahiye. Expected TSan output + FIX neeche.
// ============================================================

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

// -- shared state --------------------------------------------
static long        g_counter_racy   = 0;    // BUG: plain long, no sync
static std::atomic<long> g_counter_ok{0};   // reference: correct

static constexpr int  kThreads = 4;
static constexpr long kPerThread = 500'000;

// har thread yeh chalata
static void worker(bool safe_mode) {
    for (long i = 0; i < kPerThread; ++i) {
        if (safe_mode) {
            // atomic RMW -- koi race nahi
            g_counter_ok.fetch_add(1, std::memory_order_relaxed);
        } else {
            // BUG: read-modify-write on a plain long, shared, unsynchronised.
            // do threads `load; +1; store` interleave karte -> updates kho
            // jaate. TSan ise "data race" bolega.
            g_counter_racy = g_counter_racy + 1;
        }
    }
}

int main(int argc, char** argv) {
    const bool safe = (argc > 1 && std::strcmp(argv[1], "--safe") == 0);

    std::vector<std::thread> ts;
    ts.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) ts.emplace_back(worker, safe);
    for (auto& th : ts) th.join();

    const long expected = kThreads * kPerThread;   // 2,000,000
    const long got = safe ? g_counter_ok.load() : g_counter_racy;

    std::printf("%s: total = %ld  (expected %ld)%s\n",
                safe ? "safe" : "racy", got, expected,
                got == expected ? "" : "   <-- LOST UPDATES");
    return got == expected ? 0 : 1;
}

// ============================================================
//                    T S a n   R E P O R T   (shape)
// ============================================================
//
// $ ./race_tsan
// ==================
// WARNING: ThreadSanitizer: data race (pid=12345)
//   Write of size 8 at 0x... by thread T2:
//     #0 worker(bool)         04_race_debug.cpp:45
//     #1 ...                  <thread>
//
//   Previous write of size 8 at 0x... by thread T1:
//     #0 worker(bool)         04_race_debug.cpp:45
//
//   Location is global 'g_counter_racy' of size 8 at 0x...
//
//   Thread T2 (running) created by main thread at:
//     #0 pthread_create
//     #1 main                 04_race_debug.cpp:57
// ==================
// ThreadSanitizer: reported 1 warnings
//
// PADHNA:
//   - "data race" = do accesses, same location, at least ek WRITE, no
//     happens-before edge between them (27-ATOMICS/01).
//   - TSan dono stack traces deta -- dono racing accesses ki exact line.
//   - Yeh UB hai. "Chalta dikhta hai" ka matlab kuch nahi -- optimizer
//     is loop ko `g_counter_racy += kPerThread` mein fold kar sakta,
//     ya tearing ho sakti (27-ATOMICS).
//
// FIX (koi ek):
//   1. std::atomic<long> counter;  counter.fetch_add(1, relaxed);
//        -- yahi is counter ke liye sahi + tez (26/13, 27/06). `--safe`
//           mode yehi karta hai.
//   2. std::mutex m;  { std::lock_guard lk(m); ++counter; }
//        -- zyada general (jab multiple fields ek saath update ho), par
//           per-increment lock mehnga (26/06).
//   3. Har thread apna local sum rakhe, end mein ek baar add kare
//        -- sharding: sabse tez jab possible (contention hi hata do).
//
// VERIFY: fix ke baad `for i in $(seq 1 50); do ./race || break; done`
//         -- 50/50 runs `2000000` aane chahiye. Ek bhi mismatch = fix
//         adhoora (01-debugging-mindset: reproduction ko 100% banao).
// ============================================================
