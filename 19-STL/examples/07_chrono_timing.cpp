// 07_chrono_timing.cpp
// ============================================================
// <chrono> -- code timing karne ka SAHI tareeka
// ============================================================
//   g++ -std=c++20 -O2 07_chrono_timing.cpp -o ct && ./ct
//   (BENCHMARK -- -O2 zaroori; -O0 pe numbers bekaar)
// ============================================================
//   RULES:
//   1. steady_clock use karo (monotonic), system_clock NAHI (NTP jump kar sakti hai)
//   2. -O2 pe compile karo, aur measured value ko "use" karo (warna optimizer loop hata deta)
//   3. warm-up run karo (caches, branch predictor, CPU freq)
//   4. multiple iterations -> per-op = total / N  (single op clock resolution se chhota hota)
//   5. clock resolution jaan lo (yahan ~100 ns) -- usse chhoti cheez batch karo
// ============================================================

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using Clock = std::chrono::steady_clock;

static volatile double g_sink = 0;

int main() {
    // ---- clock resolution ----
    {
        long long mn = 1'000'000'000;
        for (int i = 0; i < 100000; ++i) {
            auto a = Clock::now();
            auto b = Clock::now();
            long long d = std::chrono::duration_cast<std::chrono::nanoseconds>(b - a).count();
            if (d > 0 && d < mn) mn = d;
        }
        std::printf("steady_clock smallest nonzero delta : ~%lld ns  (this box's resolution)\n\n", mn);
    }

    const long N = 5'000'000;
    std::vector<double> data(static_cast<std::size_t>(N));
    for (long i = 0; i < N; ++i) data[static_cast<std::size_t>(i)] = static_cast<double>(i % 1000) + 0.5;

    // ---- WRONG WAY: no warm-up, no sink, single run ----
    {
        auto t0 = Clock::now();
        double s = 0;
        for (double x : data) s += std::sqrt(x);
        auto t1 = Clock::now();
        // if we don't use `s`, -O2 may delete the whole loop -> "0 ns"
        g_sink = s;                                     // force the loop to happen
        double ns = static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
        std::printf("sum of sqrt (1 run, no warm-up) : %.1f ns total, %.2f ns/elem\n", ns, ns / static_cast<double>(N));
    }

    // ---- BETTER WAY: warm-up + multiple reps + report min ----
    {
        auto bench = [&] {
            auto t0 = Clock::now();
            double s = 0;
            for (double x : data) s += std::sqrt(x);
            auto t1 = Clock::now();
            g_sink = s;
            return std::chrono::duration<double, std::nano>(t1 - t0).count();
        };

        bench();                                        // WARM-UP (not timed)
        double best = 1e18;
        for (int rep = 0; rep < 10; ++rep) best = std::min(best, bench());
        std::printf("sum of sqrt (10 reps, min)      : %.1f ns total, %.3f ns/elem\n",
                    best, best / static_cast<double>(N));
    }

    // ---- duration arithmetic ----
    {
        using namespace std::chrono_literals;
        auto a = 1500ms;
        auto b = 2s;
        std::printf("\nduration math: 1500ms + 2s = %lld ms\n",
                    std::chrono::duration_cast<std::chrono::milliseconds>(a + b).count());
        std::printf("               as seconds (double) = %.3f s\n",
                    std::chrono::duration<double>(a + b).count());
    }

    // ---- for wall-clock timestamps (NOT for measuring intervals) ----
    {
        auto now = std::chrono::system_clock::now();
        auto secs = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        std::printf("\nsystem_clock (wall time) epoch seconds: %lld\n", static_cast<long long>(secs));
        std::printf("  ^ use system_clock for TIMESTAMPS; steady_clock for INTERVALS.\n");
    }

    std::printf("(sink %g)\n", g_sink);
    std::printf(
        "\n"
        "  1. steady_clock for intervals, system_clock for timestamps.\n"
        "  2. -O2 + use the result (sink) or the loop vanishes.\n"
        "  3. warm-up run, then multiple reps -- report min (least noise), not mean.\n"
        "  4. per-op = total / N; single op is below clock resolution.\n"
        "  (HFT: rdtsc for sub-100ns -- folder 14 example 06, folder 35.)\n");
    return 0;
}
