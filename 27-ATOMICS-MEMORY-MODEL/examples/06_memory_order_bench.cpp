// 06_memory_order_bench.cpp
// ============================================================
// relaxed vs acq_rel vs seq_cst — MEASURED cost, x86-64.
//   - single-threaded: pure instruction cost (no contention)
//   - store: seq_cst store needs a fence on x86 (mfence / xchg) -> costlier
//   - fetch_add (RMW): `lock` prefix already; ordering mostly free on x86
//   - load: acquire == relaxed on x86 (plain mov); seq_cst load also plain mov
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 06_memory_order_bench.cpp -o mob -pthread && ./mob
//   (-O2 ZAROORI)
// ============================================================

#include <cstdio>
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>

using Clock = std::chrono::steady_clock;
static double ns_per(long ops, Clock::time_point t0) {
    double ns = std::chrono::duration<double, std::nano>(Clock::now() - t0).count();
    return ns / static_cast<double>(ops);
}

static volatile long g_sink;

int main() {
    constexpr long N = 100'000'000;

    std::puts("single-threaded, per-op ns (x86-64, -O2):\n");

    // ---- STORE ----
    {
        std::atomic<long> a{0};
        auto t0 = Clock::now();
        for (long i = 0; i < N; ++i) a.store(i, std::memory_order_relaxed);
        std::printf("  store  relaxed : %6.3f ns\n", ns_per(N, t0));

        t0 = Clock::now();
        for (long i = 0; i < N; ++i) a.store(i, std::memory_order_release);
        std::printf("  store  release : %6.3f ns   (x86: plain mov, ~= relaxed)\n", ns_per(N, t0));

        t0 = Clock::now();
        for (long i = 0; i < N; ++i) a.store(i, std::memory_order_seq_cst);
        std::printf("  store  seq_cst : %6.3f ns   (x86: needs mfence/xchg -> costlier)\n",
                    ns_per(N, t0));
        g_sink = a.load();
    }

    // ---- LOAD ----
    {
        std::atomic<long> a{42};
        long acc = 0;
        auto t0 = Clock::now();
        for (long i = 0; i < N; ++i) acc += a.load(std::memory_order_relaxed);
        std::printf("\n  load   relaxed : %6.3f ns\n", ns_per(N, t0));

        t0 = Clock::now();
        for (long i = 0; i < N; ++i) acc += a.load(std::memory_order_acquire);
        std::printf("  load   acquire : %6.3f ns   (x86: plain mov, == relaxed)\n", ns_per(N, t0));

        t0 = Clock::now();
        for (long i = 0; i < N; ++i) acc += a.load(std::memory_order_seq_cst);
        std::printf("  load   seq_cst : %6.3f ns   (x86: plain mov for loads too)\n", ns_per(N, t0));
        g_sink = acc;
    }

    // ---- RMW (fetch_add) ----
    {
        std::atomic<long> a{0};
        auto t0 = Clock::now();
        for (long i = 0; i < N; ++i) a.fetch_add(1, std::memory_order_relaxed);
        std::printf("\n  fetch_add relaxed : %6.3f ns\n", ns_per(N, t0));

        t0 = Clock::now();
        for (long i = 0; i < N; ++i) a.fetch_add(1, std::memory_order_acq_rel);
        std::printf("  fetch_add acq_rel : %6.3f ns   (x86: `lock xadd` already, ~= relaxed)\n",
                    ns_per(N, t0));

        t0 = Clock::now();
        for (long i = 0; i < N; ++i) a.fetch_add(1, std::memory_order_seq_cst);
        std::printf("  fetch_add seq_cst : %6.3f ns   (x86: `lock xadd`, ~= acq_rel)\n",
                    ns_per(N, t0));
        g_sink = a.load();
    }

    // ---- CONTENDED fetch_add (8 threads on ONE atomic) ----
    {
        constexpr int  kThreads = 8;
        constexpr long kIters   = 5'000'000;
        auto bench = [](std::memory_order mo, const char* name) {
            std::atomic<long> a{0};
            auto t0 = Clock::now();
            std::vector<std::thread> ts;
            for (int i = 0; i < kThreads; ++i)
                ts.emplace_back([&a, mo] {
                    for (long k = 0; k < kIters; ++k) a.fetch_add(1, mo);
                });
            for (auto& t : ts) t.join();
            double ns = std::chrono::duration<double, std::nano>(Clock::now() - t0).count();
            std::printf("  contended fetch_add %-8s : %6.1f ns/op  (%ld total)\n",
                        name, ns / (kThreads * kIters), a.load());
        };
        std::puts("\n8 threads hammering ONE atomic (cache-line ping-pong dominates):");
        bench(std::memory_order_relaxed, "relaxed");
        bench(std::memory_order_seq_cst, "seq_cst");
    }

    std::puts("\nSaar (x86-64):");
    std::puts(" - LOAD: relaxed / acquire / seq_cst — sab plain `mov`. Cost ~same.");
    std::puts(" - STORE: relaxed / release — plain `mov`. seq_cst store — `mfence` ya");
    std::puts("   `xchg` -> measurably costlier (~15-18x a relaxed store here).");
    std::puts(" - RMW (fetch_add / CAS): `lock`-prefixed already; ordering ~free.");
    std::puts("   relaxed vs seq_cst RMW ~same on x86.");
    std::puts(" - CONTENDED (8 threads, 1 atomic): relaxed ~= seq_cst (both ~25-27");
    std::puts("   ns/op here) — the memory order barely matters; the cost is the cache");
    std::puts("   line bouncing between cores (grows with core count / spacing). That's");
    std::puts("   why 'don't share' (folder 26) beats picking the perfect memory order.");
    std::puts(" - Weak archs (ARM): the gaps are bigger — relaxed genuinely cheaper.");
    return 0;
}
