// 07_false_sharing_fix.cpp
// ============================================================
// False sharing in a lock-free control block. An SPSC ring has a
// producer-owned index (head) and a consumer-owned index (tail).
// If they sit in the SAME 64-byte cache line, every head update on
// one core INVALIDATES the other core's copy of the line it needs
// for tail — even though the two threads never touch each other's
// variable. Pure coherence tax, zero logical sharing.
//
// Fix: put each hot field on its own cache line (alignas(64) /
// std::hardware_destructive_interference_size).
//
// This isolates the effect: each thread only fetch_add's ITS OWN
// atomic (an RFO every op), never reads the other. Any slowdown in
// the "adjacent" layout is 100% false sharing.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 07_false_sharing_fix.cpp -o fsfix && ./fsfix
//   (-O2 REQUIRED — at -O0 the loop overhead hides it)
// ============================================================

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <new>
#include <thread>
#include <chrono>

using Clock = std::chrono::steady_clock;

#ifdef __cpp_lib_hardware_interference_size
static constexpr std::size_t kLine = std::hardware_destructive_interference_size;
#else
static constexpr std::size_t kLine = 64;
#endif

constexpr std::uint64_t kIters = 200'000'000;

// ---- ADJACENT: head and tail almost certainly share one line ----
struct Adjacent {
    std::atomic<std::uint64_t> head{0};
    std::atomic<std::uint64_t> tail{0};
};

// ---- PADDED: head and tail forced onto separate lines ----
struct Padded {
    alignas(kLine) std::atomic<std::uint64_t> head{0};
    alignas(kLine) std::atomic<std::uint64_t> tail{0};
    char pad_[kLine];
};

template <class Ring>
static double run(Ring& r) {
    std::atomic<bool> go{false};
    auto worker = [&](std::atomic<std::uint64_t>* idx) {
        while (!go.load(std::memory_order_acquire)) {}
        for (std::uint64_t i = 0; i < kIters; ++i)
            idx->fetch_add(1, std::memory_order_relaxed);
    };
    std::thread producer(worker, &r.head);
    std::thread consumer(worker, &r.tail);
    auto t0 = Clock::now();
    go.store(true, std::memory_order_release);
    producer.join();
    consumer.join();
    return std::chrono::duration<double>(Clock::now() - t0).count();
}

int main() {
    std::printf("false sharing in an SPSC index pair — %llu fetch_add each, line = %zu B\n",
                (unsigned long long)kIters, kLine);
    std::printf("sizeof(Adjacent) = %zu   sizeof(Padded) = %zu\n\n",
                sizeof(Adjacent), sizeof(Padded));

    Adjacent adj;
    Padded   pad;

    const double t_adj = run(adj);
    const double t_pad = run(pad);

    const double di = static_cast<double>(kIters);
    std::printf("  ADJACENT (same line)   : %6.3f s   %5.2f ns/op\n",
                t_adj, t_adj * 1e9 / di);
    std::printf("  PADDED   (own lines)   : %6.3f s   %5.2f ns/op\n",
                t_pad, t_pad * 1e9 / di);
    std::printf("\n  speedup from padding: %.2fx\n", t_adj / t_pad);

    std::puts("\nKya hua:");
    std::puts(" - ADJACENT: head aur tail ek hi 64B line pe. Core A ka head.fetch_add");
    std::puts("   ek RFO (read-for-ownership) hai -> woh poori line ko exclusive maangta");
    std::puts("   -> Core B ki us line ki copy (jisme tail hai) INVALIDATE. Agli baar");
    std::puts("   Core B ko tail chahiye -> miss -> line wapas kheencho. Har op pe");
    std::puts("   ping-pong. Threads logically alag variables chhu rahe hain -> 'false'.");
    std::puts(" - PADDED: alignas(kLine) se head/tail alag lines pe. Har core apni line");
    std::puts("   locally exclusive rakhta -> koi cross-core invalidation -> near-local.");
    std::puts(" - Fix source: std::hardware_destructive_interference_size (yeh compile");
    std::puts("   ke -Winterference-size warning de sakta — value compiler versions ke");
    std::puts("   beech badal sakti; production mein 64 hardcode + static_assert bhi OK).");
    std::puts(" - SPSC ring mein: producer-block {head, cached_tail} ek line pe,");
    std::puts("   consumer-block {tail, cached_head} doosri line pe (example 02 V2).");
    std::puts(" - Agar yeh speedup is laptop pe chhota dikhe: 2 threads shayad ek hi");
    std::puts("   physical core ke SMT siblings pe hain (shared L1) -> miss sasta.");
    std::puts("   Zyada cores / alag socket / pinned threads pe gap bahut bada hota");
    std::puts("   (folder 26 example 08: 8 threads -> ~10x).");
    return 0;
}
