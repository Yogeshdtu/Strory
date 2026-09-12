// 04_benchmark_mistakes.cpp
// ============================================================
// 6 galat benchmark, har ek ke saath uska FIX -- dono numbers side by
// side. Jab tak dono na dikhao, tab tak pata nahi chalta ki number jhooth
// tha.
//
//   1. Dead code elimination   -- result use nahi -> loop GAYAB
//   2. Constant folding        -- input literal -> compile-time pe solve
//   3. Loop-invariant hoisting  -- kaam loop se bahar chala gaya
//   4. Cold start / page faults -- pehla run cache-cold + faults ke saath
//   5. Timer overhead > op      -- ek-ek op time karne pe now() hi dominate
//   6. Ek measurement           -- ek sample = interrupt ka shikaar
//
// Sink ke liye ek `volatile` global -- yeh compiler har haal mein rakhta
// (folded constant ho ya nahi), aur timed region ke BAHAR ek hi store hai.
// Opaque input ke liye ek `volatile` global load -- compiler value fold
// nahi kar sakta.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 04_benchmark_mistakes.cpp -o m && ./m
//   ( -O2 ZAROORI -- -O0 pe yeh bugs dikhte hi nahi, aur -O0 number bekaar )
// ============================================================

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <numeric>
#include <vector>

namespace ch = std::chrono;
using Clock = ch::steady_clock;

// runtime sink + runtime source -- compiler inhe elide/fold nahi kar sakta
static volatile std::uint64_t g_sink = 0;
static volatile std::uint64_t g_src  = 0x9E3779B97F4A7C15ULL;

static double ns_per(std::size_t work, ch::steady_clock::duration d) {
    return static_cast<double>(ch::duration_cast<ch::nanoseconds>(d).count()) / static_cast<double>(work);
}

// a non-trivial pure function we'll (try to) benchmark
static std::uint64_t mix(std::uint64_t x) {
    x ^= x >> 33; x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}

int main() {
    constexpr int N = 20'000'000;
    const std::uint64_t seed = g_src;               // opaque (volatile load)

    // ============================================================
    //  1. DEAD CODE ELIMINATION  -- sirf sink ka farq
    // ============================================================
    std::puts("=== 1. Dead code elimination (only difference: the sink) ===");
    {
        // BUG: sum kahin use nahi -> -O2 poora loop hata deta
        auto t0 = Clock::now();
        std::uint64_t sum = 0;
        for (int i = 0; i < N; ++i) sum += mix(seed + static_cast<std::uint64_t>(i));
        auto t1 = Clock::now();
        // (sum ko yahan chhod diya -- "accidentally" use nahi kiya)
        std::printf("  BUG  (no sink)      : %6.3f ns/op   <- loop delete, jhooth\n",
                    ns_per(N, t1 - t0));
    }
    {
        auto t0 = Clock::now();
        std::uint64_t sum = 0;
        for (int i = 0; i < N; ++i) sum += mix(seed + static_cast<std::uint64_t>(i));
        auto t1 = Clock::now();
        g_sink = sum;                               // <- sink: sum ko compute karna majboori
        std::printf("  FIX  (g_sink = sum) : %6.3f ns/op\n", ns_per(N, t1 - t0));
    }

    // ============================================================
    //  2. CONSTANT FOLDING  -- input compile-time constant
    // ============================================================
    std::puts("\n=== 2. Constant folding (input is a literal) ===");
    {
        // BUG: mix(42) har baar wahi -> compiler ek baar solve karke poora
        //      loop ko `sum = N * constant` bana deta (phir woh bhi fold)
        auto t0 = Clock::now();
        std::uint64_t sum = 0;
        for (int i = 0; i < N; ++i) sum += mix(42);
        auto t1 = Clock::now();
        g_sink = sum;
        std::printf("  BUG  (mix(42))      : %6.3f ns/op   <- constant, loop folded away\n",
                    ns_per(N, t1 - t0));
    }
    {
        auto t0 = Clock::now();
        std::uint64_t sum = 0;
        for (int i = 0; i < N; ++i) sum += mix(seed + static_cast<std::uint64_t>(i));
        auto t1 = Clock::now();
        g_sink = sum;
        std::printf("  FIX  (opaque seed)  : %6.3f ns/op\n", ns_per(N, t1 - t0));
    }

    // ============================================================
    //  3. LOOP-INVARIANT HOISTING  -- kaam loop-invariant hai
    // ============================================================
    std::puts("\n=== 3. Loop-invariant code motion ===");
    {
        const std::uint64_t x = seed;               // runtime, par loop mein constant
        // BUG: mix(x) i pe depend nahi -> compiler use loop ke BAHAR ek baar
        //      compute karta, phir `sum += r` N baar (sirf add ki speed)
        auto t0 = Clock::now();
        std::uint64_t sum = 0;
        for (int i = 0; i < N; ++i) sum += mix(x);
        auto t1 = Clock::now();
        g_sink = sum;
        std::printf("  BUG  (mix(x), x fix): %6.3f ns/op   <- mix hoisted, tum `add` naap rahe\n",
                    ns_per(N, t1 - t0));
    }
    {
        std::vector<std::uint64_t> in(4096);
        std::iota(in.begin(), in.end(), seed);
        auto t0 = Clock::now();
        std::uint64_t sum = 0;
        for (int i = 0; i < N; ++i)
            sum += mix(in[static_cast<std::size_t>(i) & 4095]);   // input badalta -> hoist nahi
        auto t1 = Clock::now();
        g_sink = sum;
        std::printf("  FIX  (varying input): %6.3f ns/op\n", ns_per(N, t1 - t0));
    }

    // ============================================================
    //  4. COLD START  -- pehla run cache-cold + page faults
    // ============================================================
    std::puts("\n=== 4. Cold start / first-iteration page faults ===");
    {
        constexpr std::size_t BYTES = 64u << 20;    // 64 MB -- freshly allocated
        std::vector<std::uint8_t> big(BYTES);       // pages abhi commit nahi (first touch pe)
        auto run_once = [&](std::uint64_t salt) {
            auto t0 = Clock::now();
            std::uint64_t h = salt;
            for (std::size_t i = 0; i < BYTES; i += 4096) {
                big[i] = static_cast<std::uint8_t>(h);
                h += 1099511628211ULL;
            }
            auto t1 = Clock::now();
            g_sink = h;
            return static_cast<double>(ch::duration_cast<ch::nanoseconds>(t1 - t0).count());
        };
        double first = run_once(seed);
        double best = 1e18;
        for (int r = 0; r < 20; ++r) best = std::min(best, run_once(seed + static_cast<std::uint64_t>(r) + 1));
        std::printf("  run[0] (cold+faults): %9.0f ns\n", first);
        std::printf("  min of next 20      : %9.0f ns   (%.1fx faster -- warm-up karo, run[0] discard)\n",
                    best, first / best);
    }

    // ============================================================
    //  5. TIMER OVERHEAD  -- op timer se sasta hai
    // ============================================================
    std::puts("\n=== 5. Timer overhead swamps a cheap op ===");
    {
        // BUG: ek `mix` (~1-2 ns) ke aage-peeche now() (~20-30 ns) -> tum
        //      99% timer naap rahe
        std::uint64_t x = seed;
        constexpr int M = 200000;
        double acc = 0;
        for (int i = 0; i < M; ++i) {
            auto t0 = Clock::now();
            x = mix(x);
            auto t1 = Clock::now();
            acc += static_cast<double>(ch::duration_cast<ch::nanoseconds>(t1 - t0).count());
        }
        g_sink = x;
        std::printf("  BUG  (1 op / timestamp) : %6.2f ns/op   <- ~timer self-cost, not mix\n",
                    acc / M);
    }
    {
        // FIX: batch -- ek timestamp pair, andar bahut saare ops
        std::uint64_t x = seed;
        constexpr int BATCH = 20'000'000;
        auto t0 = Clock::now();
        for (int i = 0; i < BATCH; ++i) x = mix(x);
        auto t1 = Clock::now();
        g_sink = x;
        std::printf("  FIX  (batch of %d) : %6.2f ns/op   (chain -- latency; independent hota to kam)\n",
                    BATCH, ns_per(BATCH, t1 - t0));
    }

    // ============================================================
    //  6. ONE MEASUREMENT  -- ek sample = shor
    // ============================================================
    std::puts("\n=== 6. Single measurement vs min-of-N ===");
    {
        std::vector<std::uint64_t> in(1u << 16);
        std::iota(in.begin(), in.end(), seed);
        auto one_run = [&] {
            auto t0 = Clock::now();
            std::uint64_t h = 0;
            for (auto v : in) h += mix(v);
            auto t1 = Clock::now();
            g_sink = h;
            return static_cast<double>(ch::duration_cast<ch::nanoseconds>(t1 - t0).count())
                 / static_cast<double>(in.size());
        };
        double single = one_run();
        std::vector<double> runs;
        for (int r = 0; r < 50; ++r) runs.push_back(one_run());
        std::sort(runs.begin(), runs.end());
        double mn = runs.front(), md = runs[runs.size() / 2], mx = runs.back();
        std::printf("  one run             : %6.3f ns/op\n", single);
        std::printf("  min / median / max  : %6.3f / %6.3f / %6.3f  (spread %.0f%%)\n",
                    mn, md, mx, 100.0 * (mx - mn) / mn);
        std::puts("  -> report the MIN (least interference) + the distribution, never one run");
    }

    std::puts("\nNichod: har micro-benchmark mein -- (a) output escape (sink),");
    std::puts("(b) input opaque, (c) input i-pe-depend ya varying, (d) warm-up +");
    std::puts("run[0] discard, (e) batch cheap ops, (f) min-of-N + spread. -O2 always.");
    return 0;
}
