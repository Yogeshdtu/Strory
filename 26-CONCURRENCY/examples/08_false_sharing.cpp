// 08_false_sharing.cpp
// ============================================================
// FALSE SHARING: N threads har ek apna counter increment karte hain.
// Agar counters ek array mein PACK hain (adjacent -> same 64-byte cache
// line), to har increment doosre core ki cache copy invalidate karta ->
// cache-line ping-pong -> slow. alignas(64) se har counter apni line pe
// -> fast. Same logic, sirf layout ka farq.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra -pthread 08_false_sharing.cpp -o fs && ./fs
//   (-O2 ZAROORI — warna dono equally slow)
// ============================================================

#include <cstdio>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <cstddef>

static constexpr int  kThreads = 8;
static constexpr long kIters   = 50'000'000;
static constexpr std::size_t kLine = 64;      // cache line (std::hardware_destructive_interference_size)

using Clock = std::chrono::steady_clock;
static double ms_since(Clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

// ---- PACKED: 8 counters back-to-back -> all within 1-2 cache lines ----
struct Packed {
    std::atomic<long> c[kThreads];             // adjacent -> share cache line(s)
};

// ---- PADDED: each counter on its own cache line ----
struct alignas(kLine) PaddedCounter {
    std::atomic<long> value{0};
    char pad[kLine - sizeof(std::atomic<long>)];   // fill the rest of the line
};

template <class Fn>
static double run(Fn per_thread) {
    auto t0 = Clock::now();
    std::vector<std::thread> ts;
    ts.reserve(kThreads);
    for (int i = 0; i < kThreads; ++i)
        ts.emplace_back(per_thread, i);
    for (auto& t : ts) t.join();
    return ms_since(t0);
}

int main() {
    std::printf("%d threads x %ld increments each, cache line = %zu bytes\n\n",
                kThreads, kIters, kLine);
    std::printf("sizeof(Packed)        = %zu  (all %d counters -> ~1 line)\n",
                sizeof(Packed), kThreads);
    std::printf("sizeof(PaddedCounter) = %zu  (1 counter per line)\n\n",
                sizeof(PaddedCounter));

    // --------------------------------------------------------
    //  PACKED — false sharing
    // --------------------------------------------------------
    Packed packed;
    for (auto& a : packed.c) a.store(0);
    double t_packed = run([&packed](int i) {
        auto& c = packed.c[static_cast<std::size_t>(i)];
        for (long k = 0; k < kIters; ++k)
            c.fetch_add(1, std::memory_order_relaxed);
    });

    // --------------------------------------------------------
    //  PADDED — no false sharing
    // --------------------------------------------------------
    std::vector<PaddedCounter> padded(kThreads);
    double t_padded = run([&padded](int i) {
        auto& c = padded[static_cast<std::size_t>(i)].value;
        for (long k = 0; k < kIters; ++k)
            c.fetch_add(1, std::memory_order_relaxed);
    });

    // verify both correct
    long sp = 0, sd = 0;
    for (auto& a : packed.c) sp += a.load();
    for (auto& x : padded)   sd += x.value.load();
    const long expected = static_cast<long>(kThreads) * kIters;

    std::printf("PACKED  (false sharing): %8.1f ms   [sum=%ld %s]\n",
                t_packed, sp, sp == expected ? "OK" : "WRONG");
    std::printf("PADDED  (alignas 64)   : %8.1f ms   [sum=%ld %s]\n",
                t_padded, sd, sd == expected ? "OK" : "WRONG");
    std::printf("\nspeedup from padding: %.2fx\n", t_packed / t_padded);

    std::puts("\nKya hua:");
    std::puts(" - PACKED: counters ek cache line pe. Core 2 ka fetch_add core 1 ki");
    std::puts("   line-copy ko INVALIDATE karta (MESI). Line har baar cores ke beech");
    std::puts("   ping-pong -> ~100+ cycle latency per op. 'False' kyunki logically");
    std::puts("   threads ek doosre ka data chhu bhi nahi rahe — sirf same line.");
    std::puts(" - PADDED: har counter apni 64-byte line pe -> koi invalidation ->");
    std::puts("   har core apni line locally rakhta -> near-local speed.");
    std::puts(" - Fix: hot per-thread mutable state ko alignas(64) karo. Ya better:");
    std::puts("   thread-local accumulate + ek final combine (example 03 wala (c)).");
    return 0;
}
