// 02_race_condition.cpp
// ============================================================
// ⚠️ RACE CONDITION demo. N threads ek shared counter ko bina sync
// ke ++ karte hain. `counter++` = load, add, store — 3 steps. Do
// threads inhe interleave karen to updates KHO jaate hain.
//
// Final value < N*ITERS, aur har run pe alag (non-deterministic).
// Yeh DATA RACE hai = UNDEFINED BEHAVIOUR (bhale yahan "sirf" lost
// updates dikhte hon).
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -pthread 02_race_condition.cpp -o rc && ./rc
//   Linux pe pakdo:  g++ -std=c++20 -O1 -g -fsanitize=thread 02_race_condition.cpp -o rc && ./rc
//   (MinGW pe libtsan nahi — Linux/Clang chahiye TSan ke liye)
// ============================================================

#include <cstdio>
#include <thread>
#include <vector>

static constexpr int  kThreads = 8;
static constexpr long kIters   = 200000;

int main() {
    std::printf("expected final value = %d threads x %ld iters = %ld\n\n",
                kThreads, kIters, static_cast<long>(kThreads) * kIters);

    // 3 baar chalao — har baar alag (galat) answer
    for (int run = 1; run <= 3; ++run) {
        long counter = 0;                       // shared, NON-atomic

        std::vector<std::thread> ts;
        ts.reserve(kThreads);
        for (int i = 0; i < kThreads; ++i)
            ts.emplace_back([&counter] {
                for (long k = 0; k < kIters; ++k)
                    ++counter;                   // ⚠️ RACE: load-add-store, no sync
            });
        for (auto& t : ts) t.join();

        long expected = static_cast<long>(kThreads) * kIters;
        std::printf("run %d: counter = %ld  (lost %ld updates, %.1f%%)\n",
                    run, counter, expected - counter,
                    100.0 * static_cast<double>(expected - counter) /
                        static_cast<double>(expected));
    }

    std::puts("");
    std::puts("Kya hua:");
    std::puts(" - ++counter compiler ke liye: reg = counter; reg = reg + 1; counter = reg;");
    std::puts(" - Thread A load karta (counter=100), Thread B bhi load (100), dono +1,");
    std::puts("   dono store 101. Do increments the, ek hi count hua. LOST UPDATE.");
    std::puts(" - Har run pe interleaving alag -> answer non-deterministic.");
    std::puts(" - Yeh formally UNDEFINED BEHAVIOUR hai (data race). '-O2' pe compiler");
    std::puts("   aur bhi surprising kar sakta (loop hoist, etc.).");
    std::puts("");
    std::puts("Fix (example 03): std::mutex + lock_guard, ya std::atomic<long>.");
    return 0;
}
