// 08_latency_recorder.cpp
// ============================================================
// Production mein latency kaise measure karo BINA khud latency add kiye.
//
// Idea: har event pe ek 64-bit value (ns ya ticks) ko ek LOG-LINEAR
// histogram bucket mein daal do. record() = ek bit-scan + ek increment
// (~1-3 ns, no branch loop, no alloc, no lock). Percentiles baad mein,
// off the hot path, buckets se nikaalo. Yeh HdrHistogram ka chhota version.
//
// Kyun log-linear buckets: latency 100 ns se 100 ms tak faili hoti (6
// orders of magnitude). Pure-linear buckets -> ya to itne saare ki memory
// bhar jaaye, ya itne mote ki p99 useless. Log-linear: har octave ko SUB
// barabar hisson mein baanto -> har bucket ~ek fixed PERCENT chauda ->
// bounded relative error (~1/SUB), thoda sa memory.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 08_latency_recorder.cpp -o r && ./r
// ============================================================

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <vector>
#include <x86intrin.h>

namespace ch = std::chrono;

template <class T>
static inline void keep(T& v) { asm volatile("" : "+r,m"(v) : : "memory"); }
static inline std::uint64_t tsc() { _mm_lfence(); auto t = __rdtsc(); _mm_lfence(); return t; }

// ============================================================
//  LatencyHistogram -- fixed size, O(1) record, percentile query at end.
//
//  Layout:
//   - values 0 .. SUB-1      : linear, one value per bucket   (SUB buckets)
//   - each octave [2^m, 2^(m+1))  for m = SUB_BITS .. 63 :
//         split into SUB equal sub-buckets of width 2^(m-SUB_BITS)
//  Relative error <= 1/SUB. SUB=64 -> ~1.5%.
// ============================================================
class LatencyHistogram {
public:
    static constexpr int          SUB_BITS = 6;
    static constexpr std::uint64_t SUB      = std::uint64_t{1} << SUB_BITS;  // 64
    static constexpr int          OCTAVES  = 64 - SUB_BITS;                 // 58 -> full u64 range
    static constexpr std::size_t  NBUCKETS = static_cast<std::size_t>(SUB) * (1 + OCTAVES);

    void record(std::uint64_t v) noexcept {
        ++counts_[index_of(v)];
        ++total_;
        max_ = v > max_ ? v : max_;
        min_ = v < min_ ? v : min_;
    }

    std::uint64_t percentile(double p) const noexcept {
        if (total_ == 0) return 0;
        const std::uint64_t target =
            static_cast<std::uint64_t>(std::ceil(p / 100.0 * static_cast<double>(total_)));
        std::uint64_t seen = 0;
        for (std::size_t i = 0; i < NBUCKETS; ++i) {
            seen += counts_[i];
            if (seen >= target) return upper_edge(i);       // conservative: report bucket top
        }
        return max_;
    }

    double mean() const noexcept {
        if (!total_) return 0.0;
        double s = 0.0;
        for (std::size_t i = 0; i < NBUCKETS; ++i)
            if (counts_[i])
                s += static_cast<double>(counts_[i])
                   * 0.5 * (static_cast<double>(lower_edge(i)) + static_cast<double>(upper_edge(i)));
        return s / static_cast<double>(total_);
    }

    std::uint64_t count() const noexcept { return total_; }
    std::uint64_t min()   const noexcept { return total_ ? min_ : 0; }
    std::uint64_t max()   const noexcept { return max_; }
    std::uint64_t bucket_count(std::size_t i) const noexcept { return counts_[i]; }

    // ---- bucket geometry (public so the demo can print edges) --------
    static std::size_t index_of(std::uint64_t v) noexcept {
        if (v < SUB) return static_cast<std::size_t>(v);
        const int msb = 63 - __builtin_clzll(v);            // >= SUB_BITS
        const int octave = msb - SUB_BITS;                  // 0-based
        const std::uint64_t sub = (v >> (msb - SUB_BITS)) & (SUB - 1);
        return static_cast<std::size_t>(SUB) + static_cast<std::size_t>(octave) * static_cast<std::size_t>(SUB)
             + static_cast<std::size_t>(sub);
    }
    static std::uint64_t lower_edge(std::size_t i) noexcept {
        if (i < SUB) return i;
        const std::size_t j = i - static_cast<std::size_t>(SUB);
        const int octave = static_cast<int>(j / SUB);
        const std::uint64_t sub = j % SUB;
        const int msb = octave + SUB_BITS;
        const std::uint64_t width = std::uint64_t{1} << octave;
        return (std::uint64_t{1} << msb) + sub * width;
    }
    static std::uint64_t upper_edge(std::size_t i) noexcept {
        if (i < SUB) return i + 1;
        const std::size_t j = i - static_cast<std::size_t>(SUB);
        const int octave = static_cast<int>(j / SUB);
        const std::uint64_t sub = j % SUB;
        const int msb = octave + SUB_BITS;
        const std::uint64_t width = std::uint64_t{1} << octave;
        return (std::uint64_t{1} << msb) + (sub + 1) * width;
    }

private:
    std::array<std::uint64_t, NBUCKETS> counts_{};
    std::uint64_t total_ = 0;
    std::uint64_t max_ = 0;
    std::uint64_t min_ = UINT64_MAX;
};

int main() {
    std::printf("histogram: %zu buckets, %.1f KB  (fixed, no alloc/lock on hot path)\n\n",
                LatencyHistogram::NBUCKETS, sizeof(LatencyHistogram) / 1024.0);

    // ---- calibrate ticks->ns --------------------------------------
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

    // ---- build a simulated latency stream -------------------------
    //  base ~ lognormal around 250 ns; ~0.5% spikes up to ~50 us
    std::mt19937_64 rng(2025);
    std::lognormal_distribution<double> base(std::log(250.0), 0.35);
    constexpr int EVENTS = 2'000'000;
    std::vector<std::uint64_t> stream(EVENTS);
    for (int i = 0; i < EVENTS; ++i) {
        double v = base(rng);
        if ((rng() % 1000) < 5) v *= 20.0 + static_cast<double>(rng() % 100);
        stream[static_cast<std::size_t>(i)] = static_cast<std::uint64_t>(v) + 1;
    }

    // ============================================================
    //  1. record() overhead vs the naive "push raw samples" approach
    // ============================================================
    LatencyHistogram hist;
    std::uint64_t t0 = tsc();
    for (int i = 0; i < EVENTS; ++i) hist.record(stream[static_cast<std::size_t>(i)]);
    std::uint64_t t1 = tsc();
    keep(hist);
    std::printf("hist.record()      : %.2f ns/event\n",
                static_cast<double>(t1 - t0) / tpns / EVENTS);

    std::vector<std::uint64_t> raw;
    raw.reserve(EVENTS);
    std::uint64_t t2 = tsc();
    for (int i = 0; i < EVENTS; ++i) raw.push_back(stream[static_cast<std::size_t>(i)]);
    std::uint64_t t3 = tsc();
    keep(raw);
    std::printf("vector.push_back() : %.2f ns/event   (+%zu MB RAM, + O(n log n) sort later,\n",
                static_cast<double>(t3 - t2) / tpns / EVENTS,
                (raw.size() * sizeof(std::uint64_t)) >> 20);
    std::puts("                                          + a realloc mid-stream can itself spike)");

    // ============================================================
    //  2. histogram percentiles vs exact (sorted raw)
    // ============================================================
    std::sort(raw.begin(), raw.end());
    auto exact = [&](double p) {
        auto r = static_cast<std::size_t>(std::ceil(p / 100.0 * static_cast<double>(raw.size())));
        if (r == 0) r = 1;
        return raw[r - 1];
    };

    std::puts("\n            hist(approx)   exact          rel.err");
    for (double p : {50.0, 90.0, 99.0, 99.9, 99.99}) {
        std::uint64_t h = hist.percentile(p);
        std::uint64_t e = exact(p);
        double err = e ? 100.0 * (static_cast<double>(h) - static_cast<double>(e)) / static_cast<double>(e) : 0.0;
        std::printf("  p%-6.2f  %9llu ns  %9llu ns   %+6.2f%%\n",
                    p, static_cast<unsigned long long>(h), static_cast<unsigned long long>(e), err);
    }
    std::printf("  mean     %9.0f ns   (!= p50 -- right skew)\n", hist.mean());
    std::printf("  min/max  %llu / %llu ns\n",
                static_cast<unsigned long long>(hist.min()),
                static_cast<unsigned long long>(hist.max()));

    // ============================================================
    //  3. log-scaled view: 32 display bins, equal RATIO width from
    //     min..max. Yahi tareeka hai latency plot karne ka -- linear
    //     x-axis pe bulk ek line ban jaata aur tail invisible.
    // ============================================================
    std::puts("\n=== distribution (log-spaced display bins) ===");
    constexpr int DB = 32;
    std::array<std::uint64_t, DB> disp{};
    const double lo = std::log(static_cast<double>(std::max<std::uint64_t>(hist.min(), 1)));
    const double hi = std::log(static_cast<double>(std::max<std::uint64_t>(hist.max(), 2)));
    for (std::size_t i = 0; i < LatencyHistogram::NBUCKETS; ++i) {
        std::uint64_t c = hist.bucket_count(i);
        if (!c) continue;
        double mid = 0.5 * (static_cast<double>(LatencyHistogram::lower_edge(i))
                          + static_cast<double>(LatencyHistogram::upper_edge(i)));
        int b = static_cast<int>((std::log(mid) - lo) / (hi - lo) * (DB - 1) + 0.5);
        b = std::clamp(b, 0, DB - 1);
        disp[static_cast<std::size_t>(b)] += c;
    }
    std::uint64_t dpeak = *std::max_element(disp.begin(), disp.end());
    for (int b = 0; b < DB; ++b) {
        double edge = std::exp(lo + (static_cast<double>(b) + 0.5) / DB * (hi - lo));
        int bar = dpeak ? static_cast<int>((disp[static_cast<std::size_t>(b)] * 56) / dpeak) : 0;
        std::printf("  ~%9.0f ns |%-56s %llu\n",
                    edge, std::string(static_cast<std::size_t>(bar), '#').c_str(),
                    static_cast<unsigned long long>(disp[static_cast<std::size_t>(b)]));
    }
    std::puts("  (do humps: bulk ~250 ns + ek alag spike cluster ~5-50 us --");
    std::puts("   linear x-axis pe yeh dusra hump bilkul dikhta hi nahi.)");

    std::puts("\nKya seekha:");
    std::puts(" - record() O(1), fixed memory, hot path pe alloc/lock/sort nahi.");
    std::puts(" - Log-linear buckets: p50..p99.99 sab ~1-2% error mein, ~30 KB mein.");
    std::puts(" - Raw vector exact hai par RAM khaata + baad mein sort + push_back");
    std::puts("   mid-stream realloc kar ke KHUD ek spike daal sakta.");
    std::puts(" - Multi-thread: per-thread histogram + periodic lock-free merge,");
    std::puts("   ya ek SPSC ring se ek aggregator thread ko bhejo (folder 28).");
    std::puts(" - Coordinated omission: agar load-generator system slow hone pe");
    std::puts("   request bhejna rok deta hai, tail under-count hoti. HdrHistogram ka");
    std::puts("   recordValueWithExpectedInterval() ise theek karta (lesson 05, 07).");
    return 0;
}
