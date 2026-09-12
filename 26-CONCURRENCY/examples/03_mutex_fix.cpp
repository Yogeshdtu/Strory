// 03_mutex_fix.cpp
// ============================================================
// Example 02 ka race, teen tareeke se fix:
//   (a) std::mutex + std::lock_guard  (critical section)
//   (b) std::atomic<long> + fetch_add  (lock-free for a counter)
//   (c) per-thread local sum + ek final combine  (no sharing at all — fastest)
// Aur timing compare — atomic vs mutex vs local.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra -pthread 03_mutex_fix.cpp -o mf && ./mf
//   (-O2 taaki timing meaningful ho)
// ============================================================

#include <cstdio>
#include <thread>
#include <vector>
#include <mutex>
#include <atomic>
#include <chrono>

static constexpr int  kThreads = 8;
static constexpr long kIters   = 2'000'000;

using Clock = std::chrono::steady_clock;
static double ms_since(Clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

int main() {
    const long expected = static_cast<long>(kThreads) * kIters;
    std::printf("expected = %ld  (%d threads x %ld)\n\n", expected, kThreads, kIters);

    // --------------------------------------------------------
    //  (a) std::mutex + lock_guard
    // --------------------------------------------------------
    {
        long counter = 0;
        std::mutex m;
        auto t0 = Clock::now();
        std::vector<std::thread> ts;
        for (int i = 0; i < kThreads; ++i)
            ts.emplace_back([&] {
                for (long k = 0; k < kIters; ++k) {
                    std::lock_guard<std::mutex> lk(m);   // RAII: lock in ctor, unlock in dtor
                    ++counter;
                }
            });
        for (auto& t : ts) t.join();
        std::printf("(a) mutex   : counter = %ld  %s   %8.1f ms\n",
                    counter, counter == expected ? "OK" : "WRONG", ms_since(t0));
    }

    // --------------------------------------------------------
    //  (b) std::atomic<long>
    // --------------------------------------------------------
    {
        std::atomic<long> counter{0};
        auto t0 = Clock::now();
        std::vector<std::thread> ts;
        for (int i = 0; i < kThreads; ++i)
            ts.emplace_back([&] {
                for (long k = 0; k < kIters; ++k)
                    counter.fetch_add(1, std::memory_order_relaxed);   // atomic RMW
            });
        for (auto& t : ts) t.join();
        std::printf("(b) atomic  : counter = %ld  %s   %8.1f ms\n",
                    counter.load(), counter.load() == expected ? "OK" : "WRONG",
                    ms_since(t0));
    }

    // --------------------------------------------------------
    //  (c) per-thread local + combine (no shared writes in the hot loop)
    // --------------------------------------------------------
    {
        std::vector<long> partials(kThreads, 0);
        auto t0 = Clock::now();
        std::vector<std::thread> ts;
        for (int i = 0; i < kThreads; ++i)
            ts.emplace_back([&partials, i] {
                long local = 0;                       // thread ka apna — no sharing
                for (long k = 0; k < kIters; ++k) ++local;
                partials[static_cast<std::size_t>(i)] = local;   // ek write, at the end
            });
        for (auto& t : ts) t.join();
        long total = 0;
        for (long p : partials) total += p;
        std::printf("(c) local+combine: counter = %ld  %s   %8.1f ms\n",
                    total, total == expected ? "OK" : "WRONG", ms_since(t0));
    }

    std::puts("");
    std::puts("Nateeja (is machine pe):");
    std::puts(" - mutex: correct, par har ++ pe lock/unlock = contention -> sabse dheema");
    std::puts(" - atomic: correct, lock-free, par har fetch_add ek cache-line ping-pong");
    std::puts("   (saare threads ek hi line pe likh rahe)");
    std::puts(" - local+combine: sharing hi nahi -> ~serial speed, N-way parallel. BEST");
    std::puts("   jab kaam ko partition kiya ja sake. (map-reduce shape)");
    return 0;
}
