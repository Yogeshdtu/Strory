// 01_timing_methods.cpp
// ============================================================
// "Kitne baje hai" poochne ke 5 tareeke — resolution, self-cost, aur
// ek fixed workload pe reading. Kaunsa kab use karo.
//
//   chrono::system_clock        -- wall clock (NTP se jump kar sakta) -- NEVER for intervals
//   chrono::steady_clock        -- monotonic; ~ clock_gettime(MONOTONIC) wrapper -- default
//   chrono::high_resolution_clock-- usually steady_clock ka alias -- naam pe mat jao
//   clock() / CLOCK_PROCESS...   -- CPU time (~ms granularity) -- coarse, sleep count nahi hota
//   __rdtsc()                    -- CPU tick counter, sub-ns, no syscall -- fencing/calibration chahiye
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 01_timing_methods.cpp -o t && ./t
//   (folder 34 lesson 11 + example 05 = rdtsc ka deep dive.)
// ============================================================

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <vector>
#include <x86intrin.h>          // __rdtsc, _mm_lfence

namespace ch = std::chrono;

// ---- barrier: compiler ko result delete karne se roko (folder 33/14) -----
template <class T>
static inline void keep(T& v) { asm volatile("" : "+r,m"(v) : : "memory"); }

// ---- fenced rdtsc (folder 34 lesson 11 ka recipe) -----------------------
static inline std::uint64_t tsc_now() {
    _mm_lfence();
    std::uint64_t t = __rdtsc();
    _mm_lfence();
    return t;
}

// ---- the workload we time: integer hash over a buffer -------------------
static std::uint64_t work(const std::uint32_t* a, std::size_t n) {
    std::uint64_t h = 1469598103934665603ULL;         // FNV-ish
    for (std::size_t i = 0; i < n; ++i) {
        h ^= a[i];
        h *= 1099511628211ULL;
        h ^= h >> 27;
    }
    return h;
}

int main() {
    // ============================================================
    //  1. RESOLUTION / GRANULARITY  -- back-to-back reads ka smallest
    //     non-zero delta. "clock ticks kitne fine?"
    // ============================================================
    std::puts("=== 1. Resolution (smallest observable non-zero step) ===");

    {
        // steady_clock
        std::int64_t min_step = INT64_MAX;
        for (int i = 0; i < 200000; ++i) {
            auto a = ch::steady_clock::now();
            auto b = ch::steady_clock::now();
            auto d = ch::duration_cast<ch::nanoseconds>(b - a).count();
            if (d > 0 && d < min_step) min_step = d;
        }
        std::printf("  steady_clock            : %lld ns\n", static_cast<long long>(min_step));
    }
    {
        std::int64_t min_step = INT64_MAX;
        for (int i = 0; i < 200000; ++i) {
            auto a = ch::high_resolution_clock::now();
            auto b = ch::high_resolution_clock::now();
            auto d = ch::duration_cast<ch::nanoseconds>(b - a).count();
            if (d > 0 && d < min_step) min_step = d;
        }
        std::printf("  high_resolution_clock   : %lld ns\n", static_cast<long long>(min_step));
    }
    {
        std::int64_t min_step = INT64_MAX;
        for (int i = 0; i < 200000; ++i) {
            auto a = ch::system_clock::now();
            auto b = ch::system_clock::now();
            auto d = ch::duration_cast<ch::nanoseconds>(b - a).count();
            if (d > 0 && d < min_step) min_step = d;
        }
        std::printf("  system_clock            : %lld ns   (wall clock -- interval ke liye MAT use karo)\n",
                    static_cast<long long>(min_step));
    }
    {
        // clock() -- CPU time, POSIX CLOCKS_PER_SEC usually 1e6 -> ~1 us tick,
        // but often updated only every scheduler tick (~1-10 ms)
        std::int64_t min_step = INT64_MAX;
        for (int i = 0; i < 200000; ++i) {
            std::clock_t a = std::clock();
            std::clock_t b = std::clock();
            auto d = static_cast<std::int64_t>(b - a);
            if (d > 0 && d < min_step) min_step = d;
        }
        double ns = static_cast<double>(min_step) * 1e9 / static_cast<double>(CLOCKS_PER_SEC);
        std::printf("  clock() (CPU time)      : %.0f ns  (CLOCKS_PER_SEC=%ld)\n",
                    ns, static_cast<long>(CLOCKS_PER_SEC));
    }
    {
        // rdtsc -- ticks, convert later
        std::uint64_t min_step = UINT64_MAX;
        for (int i = 0; i < 200000; ++i) {
            std::uint64_t a = __rdtsc();
            std::uint64_t b = __rdtsc();
            std::uint64_t d = b - a;
            if (d > 0 && d < min_step) min_step = d;
        }
        std::printf("  __rdtsc() (plain)       : %llu ticks\n",
                    static_cast<unsigned long long>(min_step));
    }

    // ============================================================
    //  2. CALIBRATE rdtsc -> ns   (folder 34 lesson 11)
    // ============================================================
    double ticks_per_ns = 0.0;
    {
        auto c0 = ch::steady_clock::now();
        std::uint64_t r0 = tsc_now();
        // busy ~150 ms
        volatile std::uint64_t spin = 0;
        while (ch::duration_cast<ch::milliseconds>(ch::steady_clock::now() - c0).count() < 150)
            spin = spin + 1;
        std::uint64_t r1 = tsc_now();
        auto c1 = ch::steady_clock::now();
        double dns = static_cast<double>(ch::duration_cast<ch::nanoseconds>(c1 - c0).count());
        ticks_per_ns = static_cast<double>(r1 - r0) / dns;
        std::printf("\n=== 2. rdtsc calibration ===\n  ticks_per_ns = %.4f  (~%.2f GHz TSC rate)\n",
                    ticks_per_ns, ticks_per_ns);
    }

    // ============================================================
    //  3. SELF-COST  -- ek reading lene mein kitna lagta
    // ============================================================
    std::puts("\n=== 3. Timer self-cost (ns per call, min of many) ===");
    constexpr int CAL = 500000;
    {
        auto t0 = ch::steady_clock::now();
        for (int i = 0; i < CAL; ++i) { auto x = ch::steady_clock::now(); keep(x); }
        auto t1 = ch::steady_clock::now();
        std::printf("  steady_clock::now()     : %6.1f ns\n",
                    static_cast<double>(ch::duration_cast<ch::nanoseconds>(t1 - t0).count()) / CAL);
    }
    {
        std::uint64_t a = tsc_now();
        for (int i = 0; i < CAL; ++i) { std::uint64_t x = __rdtsc(); keep(x); }
        std::uint64_t b = tsc_now();
        std::printf("  __rdtsc() (plain)       : %6.1f ns\n",
                    static_cast<double>(b - a) / ticks_per_ns / CAL);
    }
    {
        std::uint64_t a = tsc_now();
        for (int i = 0; i < CAL; ++i) { std::uint64_t x = tsc_now(); keep(x); }
        std::uint64_t b = tsc_now();
        std::printf("  lfence;rdtsc;lfence     : %6.1f ns\n",
                    static_cast<double>(b - a) / ticks_per_ns / CAL);
    }

    // ============================================================
    //  4. TIME A FIXED WORKLOAD with each -- min of N runs
    // ============================================================
    std::puts("\n=== 4. Same workload, different timers (min of 15 runs) ===");
    constexpr std::size_t N = 1u << 16;             // 64k u32 -> ~256 KB (L2)
    std::vector<std::uint32_t> buf(N);
    for (std::size_t i = 0; i < N; ++i)
        buf[i] = static_cast<std::uint32_t>(i * 2654435761u);

    auto bench = [&](const char* name, auto measure_one) {
        double best = 1e300;
        for (int r = 0; r < 15; ++r) best = std::min(best, measure_one());
        std::printf("  %-22s : %8.2f ns  (%.3f ns/elem)\n", name, best, best / N);
    };

    bench("steady_clock", [&] {
        auto t0 = ch::steady_clock::now();
        std::uint64_t h = work(buf.data(), N);
        auto t1 = ch::steady_clock::now();
        keep(h);
        return static_cast<double>(ch::duration_cast<ch::nanoseconds>(t1 - t0).count());
    });
    bench("rdtsc (fenced)+calib", [&] {
        std::uint64_t a = tsc_now();
        std::uint64_t h = work(buf.data(), N);
        std::uint64_t b = tsc_now();
        keep(h);
        return static_cast<double>(b - a) / ticks_per_ns;
    });

    // ============================================================
    //  5. Faisla
    // ============================================================
    std::puts("\n=== Kaunsa timer kab ===");
    std::puts("  interval >= ~1 us   -> chrono::steady_clock  (portable, ns-direct, no calibration)");
    std::puts("  interval <  ~100 ns -> fenced rdtsc + calibration + pin (folder 34/11)");
    std::puts("  wall-clock 'kab hua'-> system_clock  (par delta ke liye NAHI -- NTP jump)");
    std::puts("  CPU vs wall split   -> clock() / getrusage  (coarse, ~ms)");
    std::puts("  high_resolution_clock: aksar steady_clock ka alias -- explicitly steady_clock likho");
    return 0;
}
