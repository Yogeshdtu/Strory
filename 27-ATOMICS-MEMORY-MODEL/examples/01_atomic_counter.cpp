// 01_atomic_counter.cpp
// ============================================================
// Non-atomic counter (data race, lost updates) vs std::atomic<long>
// (correct). Plus: is_lock_free, is_always_lock_free, aur available
// operations ka overview.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 01_atomic_counter.cpp -o ac && ./ac
//   (-O2 taaki timing meaningful ho; folder check -O0 pe bhi compile OK)
// ============================================================

#include <cstdio>
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>

static constexpr int  kThreads = 8;
static constexpr long kIters   = 2'000'000;

using Clock = std::chrono::steady_clock;
static double ms_since(Clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

int main() {
    const long expected = static_cast<long>(kThreads) * kIters;
    std::printf("%d threads x %ld = expected %ld\n\n", kThreads, kIters, expected);

    // --------------------------------------------------------
    //  is_lock_free — kya yeh type hardware atomic instructions se
    //  chalta hai (true), ya andar chhupa mutex hai (false)?
    // --------------------------------------------------------
    std::puts("lock-free-ness:");
    std::printf("  atomic<int>::is_always_lock_free   = %d\n", std::atomic<int>::is_always_lock_free);
    std::printf("  atomic<long>::is_always_lock_free  = %d\n", std::atomic<long>::is_always_lock_free);
    std::printf("  atomic<void*>::is_always_lock_free = %d\n", std::atomic<void*>::is_always_lock_free);
    struct Big { long a, b, c; };
    std::printf("  atomic<Big>(24B)::is_always_lock_free = %d  (bada -> internal lock)\n",
                std::atomic<Big>::is_always_lock_free);
    std::atomic<long> probe{0};
    std::printf("  this atomic<long>.is_lock_free()   = %d\n\n", probe.is_lock_free());

    // --------------------------------------------------------
    //  1. NON-ATOMIC — data race, lost updates
    //  NOTE: `volatile` here ONLY to stop -O2 from coalescing the loop
    //  into `counter += kIters;` (which would hide the race). `volatile`
    //  is NOT atomic and does NOT fix anything — each `++` is still a
    //  racing load-add-store. It's still UB; we just make the bug visible.
    // --------------------------------------------------------
    {
        volatile long counter = 0;             // RACE (volatile != atomic)
        auto t0 = Clock::now();
        std::vector<std::thread> ts;
        for (int i = 0; i < kThreads; ++i)
            ts.emplace_back([&counter] {
                for (long k = 0; k < kIters; ++k)
                    counter = counter + 1;     // ⚠️ load-add-store, no sync -> lost updates
            });
        for (auto& t : ts) t.join();
        long final = counter;
        std::printf("non-atomic : %ld  %s   %7.1f ms\n",
                    final, final == expected ? "OK" : "WRONG (lost updates)", ms_since(t0));
    }

    // --------------------------------------------------------
    //  2. std::atomic<long> with ++ (== fetch_add, seq_cst by default)
    // --------------------------------------------------------
    {
        std::atomic<long> counter{0};
        auto t0 = Clock::now();
        std::vector<std::thread> ts;
        for (int i = 0; i < kThreads; ++i)
            ts.emplace_back([&counter] {
                for (long k = 0; k < kIters; ++k) ++counter;   // atomic RMW (seq_cst)
            });
        for (auto& t : ts) t.join();
        std::printf("atomic ++  : %ld  %s   %7.1f ms   (seq_cst)\n",
                    counter.load(), counter.load() == expected ? "OK" : "WRONG", ms_since(t0));
    }

    // --------------------------------------------------------
    //  3. std::atomic<long> with fetch_add(1, relaxed) — sabse sasta
    //     (sirf atomicity chahiye, ordering nahi — file 07)
    // --------------------------------------------------------
    {
        std::atomic<long> counter{0};
        auto t0 = Clock::now();
        std::vector<std::thread> ts;
        for (int i = 0; i < kThreads; ++i)
            ts.emplace_back([&counter] {
                for (long k = 0; k < kIters; ++k)
                    counter.fetch_add(1, std::memory_order_relaxed);
            });
        for (auto& t : ts) t.join();
        std::printf("fetch_add  : %ld  %s   %7.1f ms   (relaxed)\n",
                    counter.load(), counter.load() == expected ? "OK" : "WRONG", ms_since(t0));
    }

    std::puts("\nSaar:");
    std::puts(" - `long counter; ++counter` from 2+ threads = data race = UB (lost updates).");
    std::puts(" - `std::atomic<long>` ka ++ / fetch_add ATOMIC hai: load-modify-store");
    std::puts("   ek indivisible hardware op (x86: `lock xadd`). Koi update kho nahi sakta.");
    std::puts(" - `is_always_lock_free` true = hardware instruction, no hidden mutex.");
    std::puts(" - `relaxed` fetch_add == seq_cst fetch_add for a lone counter (no other");
    std::puts("   memory to order); relaxed thoda sasta on weak archs, ~same on x86.");
    return 0;
}
