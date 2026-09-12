// 03_relaxed_ordering.cpp
// ============================================================
// memory_order_relaxed: atomicity GUARANTEE, ordering NAHI.
//   ✅ perfect for: standalone counters / stats jinka koi doosra
//      data ke saath ordering relation nahi.
//   ❌ galat for: publish/subscribe (ek flag jo doosre data ki
//      visibility control kare) — uske liye acquire/release (file 08).
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 03_relaxed_ordering.cpp -o rlx -pthread && ./rlx
// ============================================================

#include <cstdio>
#include <atomic>
#include <thread>
#include <vector>

int main() {
    // --------------------------------------------------------
    //  1. relaxed counter — CORRECT use. Har fetch_add atomic hai;
    //     final total exact. Koi ordering guarantee ki zaroorat nahi
    //     kyunki hum sirf ginti kar rahe hain.
    // --------------------------------------------------------
    {
        std::atomic<long> hits{0};
        std::atomic<long> misses{0};
        constexpr int kThreads = 8;
        constexpr long kIters = 1'000'000;

        std::vector<std::thread> ts;
        for (int t = 0; t < kThreads; ++t)
            ts.emplace_back([&, t] {
                for (long k = 0; k < kIters; ++k) {
                    if ((k + t) % 3 == 0) hits.fetch_add(1, std::memory_order_relaxed);
                    else                  misses.fetch_add(1, std::memory_order_relaxed);
                }
            });
        for (auto& t : ts) t.join();

        long total = hits.load() + misses.load();
        std::printf("1. relaxed counters: hits=%ld misses=%ld total=%ld  (expected %ld)  %s\n",
                    hits.load(), misses.load(), total,
                    static_cast<long>(kThreads) * kIters,
                    total == static_cast<long>(kThreads) * kIters ? "OK" : "WRONG");
        std::puts("   ^ counts exact — relaxed guarantees ATOMICITY of each fetch_add.");
    }

    // --------------------------------------------------------
    //  2. relaxed ke saath publish/subscribe — ⚠️ BROKEN (in theory)
    //     Producer: data likho, phir ready=1 (relaxed).
    //     Consumer: ready==1 dekho, phir data padho.
    //     relaxed -> compiler/CPU `data` write aur `ready` write ko
    //     REORDER kar sakta -> consumer ready=1 dekhe par data purana.
    //     x86 (TSO) pe yeh usually "kaam kar jaata" (store order preserved)
    //     — par woh LUCK hai, guarantee nahi. ARM pe toota dikhega.
    // --------------------------------------------------------
    {
        struct Payload { int a, b, c; };
        Payload      data{};
        std::atomic<int> ready{0};

        long mismatches = 0;
        constexpr int kRounds = 5000;          // thread create/join per round -> keep modest

        for (int r = 0; r < kRounds; ++r) {
            data = Payload{};
            ready.store(0, std::memory_order_relaxed);

            std::thread producer([&] {
                data.a = r; data.b = r + 1; data.c = r + 2;         // (1) write payload
                ready.store(1, std::memory_order_relaxed);           // (2) publish (relaxed!)
            });
            std::thread consumer([&] {
                while (ready.load(std::memory_order_relaxed) == 0) { /* spin */ }
                // ready==1 dikha. Kya data guaranteed visible? relaxed -> NAHI.
                if (data.a != r || data.b != r + 1 || data.c != r + 2)
                    ++mismatches;
            });
            producer.join();
            consumer.join();
        }
        std::printf("2. relaxed publish/subscribe: %ld mismatches in %d rounds\n",
                    mismatches, kRounds);
        std::puts("   ^ x86 pe aksar 0 (store order preserved) — par yeh UNDEFINED-adjacent.");
        std::puts("     ARM/weak arch pe non-zero. CORRECT fix: release store + acquire load");
        std::puts("     (file 08, examples/04) — tab data ki visibility GUARANTEED hoti.");
    }

    std::puts("\nSaar:");
    std::puts(" - relaxed = 'yeh op atomic hai' — aur bas. Koi happens-before, koi");
    std::puts("   'is ke pehle/baad ki writes visible' guarantee NAHI.");
    std::puts(" - Use relaxed for: hit/miss counters, retry counts, event tallies,");
    std::puts("   sampling — kuch bhi jahan sirf final count matter karti hai.");
    std::puts(" - Kabhi relaxed se do threads ke beech DATA publish mat karo. Uske liye");
    std::puts("   release/acquire (file 08). x86 par 'kaam karna' proof nahi hai.");
    return 0;
}
