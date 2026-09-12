// 03_jitter_measure.cpp
// ============================================================
// JITTER = same kaam, alag-alag time. Yeh HFT ka asli dushman: average
// tez hai par kabhi-kabhi 50x slow -> woh spike order miss kara deta.
//
// Yeh example ek FIXED chhota kaam (128-element hash) crore baar chalata,
// har iteration ka time record karta, aur jitter dikhata:
//   - min (jitter-free floor)         <- yeh "asli" cost hai
//   - median, p99, p99.9, max
//   - max / min  ratio                <- jitter factor
//   - kitne samples > 2x min          <- "spike count"
// Phir do phases compare: CLEAN (kuch aur nahi ho raha) vs NOISY (beech
// mein malloc/free + a yield) -> noisy phase mein tail phat-ti hai.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 03_jitter_measure.cpp -o j && ./j
//   (behtar: pinned core pe -- folder 29/06. Yeh box pe pin nahi, isliye
//    jitter zyada dikhega -- wahi point hai.)
// ============================================================

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <vector>
#include <x86intrin.h>

namespace ch = std::chrono;

template <class T>
static inline void keep(T& v) { asm volatile("" : "+r,m"(v) : : "memory"); }
static inline std::uint64_t tsc() { _mm_lfence(); auto t = __rdtsc(); _mm_lfence(); return t; }

static double pct(const std::vector<double>& s, double p) {
    if (s.empty()) return 0.0;
    auto r = static_cast<std::size_t>(std::ceil(p / 100.0 * static_cast<double>(s.size())));
    if (r == 0) r = 1;
    if (r > s.size()) r = s.size();
    return s[r - 1];
}

// the fixed unit of work -- deterministic, ~scores of ns
static std::uint64_t unit(const std::uint32_t* a) {
    std::uint64_t h = 1469598103934665603ULL;
    for (int i = 0; i < 128; ++i) { h ^= a[static_cast<std::size_t>(i)]; h *= 1099511628211ULL; h ^= h >> 23; }
    return h;
}

static void report(const char* tag, std::vector<double> ns) {
    std::sort(ns.begin(), ns.end());
    const double mn = ns.front();
    const double md = pct(ns, 50);
    const double p99 = pct(ns, 99);
    const double p999 = pct(ns, 99.9);
    const double mx = ns.back();

    std::size_t spikes = 0;
    for (double v : ns) if (v > 2.0 * mn) ++spikes;

    std::printf("  %-7s  min %6.1f  med %6.1f  p99 %7.1f  p99.9 %8.1f  max %9.1f  |  max/min %6.1fx  spikes(>2x) %zu/%zu\n",
                tag, mn, md, p99, p999, mx, mx / mn, spikes, ns.size());
}

int main() {
    constexpr int ITERS = 300000;
    alignas(64) std::uint32_t buf[128];
    for (int i = 0; i < 128; ++i)
        buf[static_cast<std::size_t>(i)] = static_cast<std::uint32_t>(i) * 2654435761u;

    // ---- calibrate ticks->ns ----------------------------------------
    double tpns;
    {
        auto c0 = ch::steady_clock::now();
        std::uint64_t r0 = tsc();
        volatile std::uint64_t spin = 0;
        while (ch::duration_cast<ch::milliseconds>(ch::steady_clock::now() - c0).count() < 120)
            spin = spin + 1;
        std::uint64_t r1 = tsc();
        auto c1 = ch::steady_clock::now();
        tpns = static_cast<double>(r1 - r0)
             / static_cast<double>(ch::duration_cast<ch::nanoseconds>(c1 - c0).count());
    }
    std::printf("ticks_per_ns = %.4f\n\n", tpns);

    std::uint64_t sink = 0;

    // ---- PHASE A: clean -- just the unit, back to back --------------
    std::vector<double> clean;
    clean.reserve(ITERS);
    for (int i = 0; i < ITERS; ++i) {
        std::uint64_t t0 = tsc();
        std::uint64_t h = unit(buf);
        std::uint64_t t1 = tsc();
        sink ^= h;
        clean.push_back(static_cast<double>(t1 - t0) / tpns);
    }

    // ---- PHASE B: noisy -- every 200 iters do a malloc/free + yield -
    //      (simulates: an unrelated thread, a logging alloc, a syscall on
    //       the hot path -- the classic "why is my p99 bad" causes)
    std::vector<double> noisy;
    noisy.reserve(ITERS);
    for (int i = 0; i < ITERS; ++i) {
        if ((i % 200) == 0) {
            volatile auto* p = new std::uint8_t[4096];
            p[0] = static_cast<std::uint8_t>(i);
            delete[] p;
            std::this_thread::yield();               // give the scheduler a chance to steal the core
        }
        std::uint64_t t0 = tsc();
        std::uint64_t h = unit(buf);
        std::uint64_t t1 = tsc();
        sink ^= h;
        noisy.push_back(static_cast<double>(t1 - t0) / tpns);
    }
    keep(sink);

    std::puts("=== Jitter (per-iteration time of an IDENTICAL 128-hash unit) ===");
    report("CLEAN", clean);
    report("NOISY", noisy);

    // ---- where are the noisy spikes? show the 10 worst gaps ---------
    std::vector<double> worst = noisy;
    std::sort(worst.begin(), worst.end(), std::greater<>{});
    std::printf("\n  NOISY 10 worst samples (ns): ");
    for (int i = 0; i < 10; ++i) std::printf("%.0f ", worst[static_cast<std::size_t>(i)]);
    std::puts("");

    std::puts("\nKya seekha:");
    std::puts(" - 'min' hi jitter-free asli cost hai. Baaki sab min + interference.");
    std::puts(" - CLEAN phase mein bhi tail hoti (timer interrupt ~every 1-10ms,");
    std::puts("   frequency transitions, is box pe koi pinning nahi).");
    std::puts(" - NOISY phase: malloc/free + yield = page faults + scheduler +");
    std::puts("   cache/TLB pollution -> p99.9 aur max kai guna badh jaate.");
    std::puts(" - HFT fix: pin the core (isolcpus/taskset), no malloc on hot path");
    std::puts("   (pre-allocate + pools -- folder 14), busy-poll (no yield/syscall),");
    std::puts("   nohz_full + IRQ affinity. Lesson 06 mein poori list.");
    std::puts(" - Report karo: min + p99.9 + max. 'mean' jitter chhupata hai.");
    return 0;
}
