// 02_percentiles.cpp
// ============================================================
// "Average latency 210 ns" -- yeh number JHOOTH bolta hai. Yeh example
// ek workload ko 200k baar chalata hai, har run ki latency record karta,
// aur dikhata: mean vs median vs p90/p99/p99.9/p99.99/max, + ASCII histogram.
//
// Sabak: latency distribution RIGHT-SKEWED hoti (lambi tail). Mean tail se
// upar khinch jaata; median "typical" batata; p99/p99.9 batata "kitna
// bura ho sakta". HFT mein tail hi maayne rakhti.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 02_percentiles.cpp -o p && ./p
// ============================================================

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>
#include <x86intrin.h>

namespace ch = std::chrono;

template <class T>
static inline void keep(T& v) { asm volatile("" : "+r,m"(v) : : "memory"); }

static inline std::uint64_t tsc() { _mm_lfence(); auto t = __rdtsc(); _mm_lfence(); return t; }

// ---------- percentile: nearest-rank on a SORTED vector -------------------
static double pct(const std::vector<double>& sorted, double p) {
    if (sorted.empty()) return 0.0;
    // nearest-rank: ceil(p/100 * N), 1-indexed
    std::size_t rank = static_cast<std::size_t>(std::ceil(p / 100.0 * static_cast<double>(sorted.size())));
    if (rank == 0) rank = 1;
    if (rank > sorted.size()) rank = sorted.size();
    return sorted[rank - 1];
}

int main() {
    constexpr int SAMPLES = 200000;

    // ---- the work: hash a small buffer, with a small random size so the
    //      "fast" path has a natural spread (real code does). MOSTLY fast,
    //      but ~1% of runs we also touch a big cold buffer (simulates a
    //      cache-miss storm / slow code path / page fault) -> a tail.
    constexpr std::size_t HOT = 1024;                // 4 KB -- L1
    constexpr std::size_t COLD = 1u << 20;           // 4 MB -- misses
    std::vector<std::uint32_t> hot(HOT), cold(COLD);
    std::mt19937 rng(12345);
    for (auto& x : hot)  x = rng();
    for (auto& x : cold) x = rng();

    std::vector<double> lat;
    lat.reserve(SAMPLES);

    std::uint64_t sink = 0;
    for (int s = 0; s < SAMPLES; ++s) {
        const bool slow = (rng() % 100) == 0;        // ~1%

        const std::size_t hw = 400 + (rng() % 600);  // 400..999 hashed -> natural spread
        std::uint64_t t0 = tsc();
        std::uint64_t h = 14695981039346656037ULL;
        for (std::size_t i = 0; i < hw; ++i) { h ^= hot[i]; h *= 1099511628211ULL; }
        if (slow) {
            // random-stride walk over the cold buffer -> a burst of misses
            std::size_t idx = h & (COLD - 1);
            for (int k = 0; k < 64; ++k) {
                h ^= cold[idx];
                idx = (idx + 9973) & (COLD - 1);
            }
        }
        std::uint64_t t1 = tsc();
        sink ^= h;

        lat.push_back(static_cast<double>(t1 - t0));
    }
    keep(sink);

    // ---- convert ticks -> ns via a quick calibration -------------------
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
    for (auto& v : lat) v /= tpns;                   // ns now

    // ---- statistics --------------------------------------------------
    std::vector<double> sorted = lat;
    std::sort(sorted.begin(), sorted.end());

    double sum = 0.0;
    for (double v : lat) sum += v;
    const double mean = sum / static_cast<double>(lat.size());

    double var = 0.0;
    for (double v : lat) var += (v - mean) * (v - mean);
    var /= static_cast<double>(lat.size());
    const double sd = std::sqrt(var);

    const double median = pct(sorted, 50);
    const double mn = sorted.front();
    const double mx = sorted.back();

    std::printf("=== %d samples, latency in ns ===\n\n", SAMPLES);
    std::printf("  min      : %8.1f\n", mn);
    std::printf("  mean     : %8.1f   <- tail isko upar khinchti\n", mean);
    std::printf("  median   : %8.1f   <- 'typical' run\n", median);
    std::printf("  stddev   : %8.1f   (right-skew -> sd bhi bada, useless akela)\n", sd);
    std::printf("  p90      : %8.1f\n", pct(sorted, 90));
    std::printf("  p99      : %8.1f\n", pct(sorted, 99));
    std::printf("  p99.9    : %8.1f\n", pct(sorted, 99.9));
    std::printf("  p99.99   : %8.1f\n", pct(sorted, 99.99));
    std::printf("  max      : %8.1f   <- worst observed\n", mx);

    // tail amplification -- yeh number risk batata, "average" nahi
    std::printf("\n  p99   / median  = %5.1fx     <- 1-in-100 request\n",   pct(sorted, 99)   / median);
    std::printf("  p99.9 / median  = %5.1fx     <- 1-in-1000\n",           pct(sorted, 99.9) / median);
    std::printf("  p99.99/ median  = %5.1fx     <- 1-in-10000\n",          pct(sorted, 99.99)/ median);
    std::printf("  max   / median  = %5.1fx     <- worst dekha\n",         mx / median);
    std::printf("  Note: yahan mean (%.0f) ~ median (%.0f) -- bulk lagbhag symmetric,\n", mean, median);
    std::puts("        par tail (p99.99, max) median se kai guna upar hai aur run-to-run");
    std::puts("        badalta rehta. 'Mean theek' != 'tail theek'. Latency mein hamesha");
    std::puts("        percentiles report karo, average nahi.");

    // ---- ASCII histogram: 40 linear buckets from min..p99.5 ----------
    std::puts("\n=== Histogram (linear buckets, clipped at p99.5) ===");
    const double lo = mn;
    const double hi = pct(sorted, 99.5);
    constexpr int B = 40;
    std::vector<int> bucket(B, 0);
    int overflow = 0;
    for (double v : lat) {
        if (v > hi) { ++overflow; continue; }
        int b = static_cast<int>((v - lo) / (hi - lo) * (B - 1));
        if (b < 0) b = 0;
        if (b >= B) b = B - 1;
        ++bucket[static_cast<std::size_t>(b)];
    }
    int peak = *std::max_element(bucket.begin(), bucket.end());
    for (int i = 0; i < B; ++i) {
        double centre = lo + (static_cast<double>(i) + 0.5) / B * (hi - lo);
        int bar = peak ? bucket[static_cast<std::size_t>(i)] * 50 / peak : 0;
        std::printf("  %7.0f ns |%s\n", centre, std::string(static_cast<std::size_t>(bar), '#').c_str());
    }
    std::printf("  > %.0f ns   : %d samples (%.2f%%)  <- the tail the histogram cut off\n",
                hi, overflow, 100.0 * overflow / SAMPLES);

    std::puts("\nKya seekha:");
    std::puts(" - Distribution right-skewed: ek chhota hot peak + lambi tail.");
    std::puts(" - mean > median (tail ne mean ko upar khincha). 'Average' report");
    std::puts("   karna = tail chhupana.");
    std::puts(" - p99/p99.9 woh number hai jispe SLA / risk decide hota. HFT mein");
    std::puts("   'p99.9 tick-to-trade' likha jaata, mean nahi.");
    std::puts(" - Agla example: yeh tail aati kahan se (jitter) -- 03_jitter_measure.");
    return 0;
}
