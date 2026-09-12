// 04_false_sharing.cpp
// ============================================================
// FALSE SHARING: do threads alag-alag variables likhte hain, par woh
// variables EK hi 64-byte cache line pe hain. Coherence protocol (MESI)
// line ko unit maanta -> har write doosre core ki copy INVALIDATE karta ->
// line har baar cores ke beech "ping-pong" -> ~40-100+ ns per steal.
//
// Version A: counters ek array mein adjacent (sab ek/do lines pe).
// Version B: har counter alignas(64) -> apni line -> zero contention.
//
// Data race nahi hai (har thread apna index) -- bug PERFORMANCE ka hai.
// Isliye plain int chalega; hum `volatile` se store ko real rakhte hain.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra -pthread 04_false_sharing.cpp -o fshare && ./fshare
//   (MinGW pe -pthread optional; Linux pe zaroori)
// ============================================================

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <new>
#include <thread>
#include <vector>

using Clock = std::chrono::steady_clock;

#ifdef __cpp_lib_hardware_interference_size
constexpr std::size_t kLine = std::hardware_destructive_interference_size;
#else
constexpr std::size_t kLine = 64;
#endif

constexpr int         kThreads = 4;
constexpr std::uint64_t kIters  = 80'000'000ull;

// ---- Version A: packed counters, same line -------------------------------
struct Packed { std::uint64_t v; };                 // 8 bytes, 8 per line

// ---- Version B: each counter on its own line ----------------------------
struct alignas(kLine) Padded {
    std::uint64_t v;
    char pad[kLine - sizeof(std::uint64_t)];
};

template <class Arr>
static double run(Arr& counters) {
    for (auto& c : counters) c.v = 0;
    std::vector<std::thread> ts;
    auto t0 = Clock::now();
    for (int t = 0; t < kThreads; ++t) {
        ts.emplace_back([&counters, t] {
            // apne slot pe hi likhte -> koi data race nahi
            volatile std::uint64_t* p = &counters[static_cast<std::size_t>(t)].v;
            for (std::uint64_t i = 0; i < kIters; ++i) *p = *p + 1;
        });
    }
    for (auto& th : ts) th.join();
    auto t1 = Clock::now();
    return std::chrono::duration<double>(t1 - t0).count();
}

int main() {
    std::printf("threads=%d  iters/thread=%llu  hardware_destructive_interference_size=%zu\n\n",
                kThreads, static_cast<unsigned long long>(kIters), kLine);

    std::vector<Packed> a(kThreads);
    std::vector<Padded> b(kThreads);

    (void)run(a); (void)run(b);                       // warm

    const double sa = run(a);
    const double sb = run(b);

    std::printf("  A  packed  (all counters in ~1 line): %7.3f s   %.2f ns/inc\n",
                sa, sa * 1e9 / (static_cast<double>(kIters) * kThreads));
    std::printf("  B  padded  (each counter its own line): %7.3f s   %.2f ns/inc\n",
                sb, sb * 1e9 / (static_cast<double>(kIters) * kThreads));
    std::printf("\n  A / B = %.1fx  (false-sharing tax)\n", sa / sb);
    std::puts("  (is box pe A/B run-to-run ~6x se ~40x tak jhoolta -- neeche kyun)");

    std::puts("\nKya hua:");
    std::puts(" - A: char[0..31] ek line pe -> thread 0 ka write thread 1 ki cached");
    std::puts("   line-copy ko INVALID kar deta. Agli baar thread 1 ko line doosre");
    std::puts("   core se laani padti (RFO). 4 cores line ke liye ladte -> ping-pong.");
    std::puts(" - B: har counter apni 64 B line pe -> koi sharing nahi -> har core");
    std::puts("   apni line L1 mein rakh ke free chalta -> ~linear scaling.");
    std::puts(" - Tell-tale sign: thread badhane pe throughput BADHNE ke bajaye");
    std::puts("   GHATTA hai -> false sharing suspect karo. `perf c2c` confirm karta.");
    std::puts(" - A ka time run-to-run bahut jhoolta (~6x-40x) kyunki OS har baar");
    std::puts("   threads ko alag physical cores pe daalta -- same CCX (L3 share)");
    std::puts("   vs alag CCX pe line bounce ki cost alag. B stable rehta. Yaani");
    std::puts("   false sharing sirf slow nahi, JITTERY bhi banata (p99 kills).");
    std::puts(" - Fix: alignas(64) / padding; ya per-thread local accumulator,");
    std::puts("   end mein combine (yeh best -- line kabhi share hi nahi hoti).");
    return 0;
}
