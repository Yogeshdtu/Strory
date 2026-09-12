// 10_batching.cpp
// ============================================================
// Batching = ek fixed per-operation cost ko N items pe amortize karo.
// Classic throughput vs latency TRADE-OFF, measured as a curve.
//
//   "Operation" = ek function jismein ek fixed setup cost hai (yahan: ek
//   fenced timer read + a small non-inlinable prologue) + per-item work.
//   Per-item call: setup har item pe. Batch of B: setup ek baar / B items.
//
//   Do latency numbers jo batching pe ulte chalte:
//     - per-item processing latency  -> batching se GIRTI (amortized)
//     - first-item / head-of-line latency -> batching se BADHTI (item ko
//       batch bharne ka wait, phir poora batch process)
//
// HFT lens: batching throughput deta, par ek item ko "batch ready" hone tak
// rukna padta -> tail latency. Market-data decode 32-at-a-time theek; ek
// single urgent order ko batch karna galat.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 10_batching.cpp -o b && ./b
// ============================================================

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <x86intrin.h>

namespace ch = std::chrono;
using Clock = ch::steady_clock;
template <class T> static inline void keep(T& v) { asm volatile("" : "+r,m"(v) : : "memory"); }

// per-batch fixed cost: a non-inlinable call that does a little bookkeeping
[[gnu::noinline]] static std::uint64_t batch_setup(std::uint64_t seed) {
    // pretend: bounds check, acquire a buffer, read a config word, a fence
    _mm_lfence();
    return seed * 6364136223846793005ULL + 1442695040888963407ULL;
}
static inline std::uint64_t item_work(std::uint64_t h, std::uint64_t x) {
    h ^= x; h *= 0x100000001b3ULL; h ^= h >> 27; return h;
}

int main() {
    constexpr std::size_t N = 1u << 22;                       // 4M items
    std::vector<std::uint64_t> in(N);
    for (std::size_t i = 0; i < N; ++i) in[i] = i * 2654435761u + 1;

    std::printf("%-8s  %-14s  %-16s  %-16s\n",
                "batch B", "ns / item", "throughput M/s", "head-of-line ns");
    std::printf("%s\n", std::string(60, '-').c_str());

    for (std::size_t B : {std::size_t{1}, std::size_t{2}, std::size_t{4}, std::size_t{8},
                          std::size_t{16}, std::size_t{32}, std::size_t{64}, std::size_t{256},
                          std::size_t{1024}}) {
        double best = 1e300;
        for (int rep = 0; rep < 15; ++rep) {
            auto t0 = Clock::now();
            std::uint64_t h = 0;
            for (std::size_t base = 0; base < N; base += B) {
                h ^= batch_setup(h);                          // fixed cost, once per batch
                std::size_t end = std::min(base + B, N);
                for (std::size_t i = base; i < end; ++i) h = item_work(h, in[i]);
            }
            auto t1 = Clock::now();
            keep(h);
            best = std::min(best, static_cast<double>(ch::duration_cast<ch::nanoseconds>(t1 - t0).count()));
        }
        const double ns_item = best / static_cast<double>(N);
        const double mps = 1000.0 / ns_item;
        // head-of-line: an item arriving right after a batch closed waits for
        // ~B more items to arrive, then the whole batch is processed.
        // Model arrival at steady throughput -> wait ~= (B-1) * inter-arrival
        // + one batch of processing. Here inter-arrival ~= ns_item (saturated).
        const double hol = static_cast<double>(B - 1) * ns_item + static_cast<double>(B) * ns_item;
        std::printf("%-8zu  %-14.3f  %-16.1f  %-16.1f\n", B, ns_item, mps, hol);
    }

    std::puts("\nKya seekha:");
    std::puts(" - B badhaao -> ns/item girta (batch_setup B items pe baant-ta) ->");
    std::puts("   throughput badhta. Faayda B~16-64 tak, phir flat (setup ~amortized).");
    std::puts(" - PAR head-of-line latency B ke saath ~linearly badhti — ek item ko");
    std::puts("   batch bharne + process hone ka wait.");
    std::puts(" - Trade-off ka faisla: throughput-bound stage (feed decode, disk");
    std::puts("   flush, syscall) -> batch. Latency-critical single event (an order,");
    std::puts("   a risk trip) -> B=1, process turant.");
    std::puts(" - 'Opportunistic batching': jo items ABHI queue mein hain unhe ek");
    std::puts("   saath lo (B = current depth), par kabhi bharne ka wait mat karo.");
    return 0;
}
