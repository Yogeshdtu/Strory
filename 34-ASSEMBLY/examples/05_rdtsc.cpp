// 05_rdtsc.cpp
// ============================================================
// `rdtsc` / `rdtscp` = Read Time-Stamp Counter. Ek 64-bit cycle counter jo
// EDX:EAX mein aata (high 32 : low 32). Fine-grained timing ka basis.
//
//   ./build.ps1 asm 34-ASSEMBLY/examples/05_rdtsc.cpp
//
// Assembly mein:
//   __rdtsc()   -> `rdtsc` ; `shl rdx,32` ; `or rax,rdx`   (combine halves)
//   __rdtscp()  -> `rdtscp` ; ... ; writes aux (core id) to [mem]
//   _mm_lfence() -> `lfence`  (serialize: pehle ke saare loads retire hon)
//
// ⚠️ rdtsc OUT-OF-ORDER execute ho sakta -- CPU use aage/peeche move kar
// sakta. Precise measurement ke liye `lfence` (ya `rdtscp` + `lfence`) se
// fence karo. `cpuid` bhi serialize karta par bahut mehnga.
//
// TSC "reference cycles" ginta (invariant-TSC pe fixed rate), core ke ACTUAL
// clock cycles nahi -- turbo/throttle pe ye alag hote (folder 31 lesson 13).
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 05_rdtsc.cpp -o rdt && ./rdt
// ============================================================

#include <x86intrin.h>
#include <chrono>
#include <cstdint>
#include <cstdio>

template <class T>
static inline void keep(T& v) { asm volatile("" : "+r,m"(v) : : "memory"); }

static inline std::uint64_t tsc_plain()  { return __rdtsc(); }
static inline std::uint64_t tsc_lfenced() {
    _mm_lfence(); std::uint64_t t = __rdtsc(); _mm_lfence(); return t;
}
static inline std::uint64_t tsc_p() {
    unsigned aux; std::uint64_t t = __rdtscp(&aux); _mm_lfence(); return t;
}

// calibrate: TSC ticks per nanosecond, from a ~150 ms wall interval
static double tsc_per_ns() {
    using Clk = std::chrono::steady_clock;
    auto w0 = Clk::now(); std::uint64_t c0 = tsc_lfenced();
    while (std::chrono::duration<double>(Clk::now() - w0).count() < 0.15) { }
    auto w1 = Clk::now(); std::uint64_t c1 = tsc_lfenced();
    double ns = std::chrono::duration<double, std::nano>(w1 - w0).count();
    return static_cast<double>(c1 - c0) / ns;
}

// measure the self-cost of each rdtsc flavour (back-to-back reads, min of many)
template <class F>
static std::uint64_t self_cost(F read) {
    std::uint64_t best = ~0ull;
    for (int i = 0; i < 200000; ++i) {
        std::uint64_t a = read();
        std::uint64_t b = read();
        std::uint64_t d = b - a;
        if (d < best) best = d;
    }
    return best;
}

int main() {
    const double tpn = tsc_per_ns();
    std::printf("calibration : %.3f TSC ticks/ns  (~%.2f GHz effective TSC rate)\n\n", tpn, tpn);

    std::uint64_t cp = self_cost([]{ return tsc_plain(); });
    std::uint64_t cl = self_cost([]{ return tsc_lfenced(); });
    std::uint64_t cq = self_cost([]{ return tsc_p(); });

    std::printf("self-cost (min delta between two back-to-back reads):\n");
    std::printf("  __rdtsc (plain)        : %llu ticks  (~%.1f ns)\n", (unsigned long long)cp, static_cast<double>(cp) / tpn);
    std::printf("  lfence;rdtsc;lfence    : %llu ticks  (~%.1f ns)\n", (unsigned long long)cl, static_cast<double>(cl) / tpn);
    std::printf("  rdtscp + lfence        : %llu ticks  (~%.1f ns)\n", (unsigned long long)cq, static_cast<double>(cq) / tpn);

    // time a known loop (dependent add, barrier'd so -O2 doesn't fold it)
    constexpr std::uint64_t ITERS = 100'000'000;
    std::uint64_t x = 1;
    std::uint64_t t0 = tsc_lfenced();
    for (std::uint64_t i = 0; i < ITERS; ++i) { x = x * 2862933555777941757ull + 3037000493ull; keep(x); }
    std::uint64_t t1 = tsc_lfenced();
    keep(x);
    double cyc_per_iter = static_cast<double>(t1 - t0) / ITERS;
    std::printf("\n100M dependent-LCG loop : %.2f TSC ticks/iter  (~%.2f ns/iter)\n",
                cyc_per_iter, cyc_per_iter / tpn);

    std::puts("\nKya seekha:");
    std::puts(" - plain __rdtsc sabse sasta par OUT-OF-ORDER -- chhoti region ki");
    std::puts("   timing galat de sakta (CPU ne rdtsc ko region ke bahar move kiya).");
    std::puts(" - lfence se pehle/baad wrap karke serialize karo -> reliable, ~2-4x");
    std::puts("   mehnga. rdtscp built-in load-fence deta (par store nahi) -> + lfence.");
    std::puts(" - Bade loops ke liye plain rdtsc theek (self-cost amortize).");
    std::puts(" - TSC = reference ticks, NOT core cycles. Invariant-TSC pe rate fixed,");
    std::puts("   par woh base-clock hai; turbo/throttle pe actual cycles alag");
    std::puts("   (folder 31 lesson 13). ns ke liye calibrate karo (upar).");
    std::puts(" - Assembly dekho: `./build.ps1 asm 34-ASSEMBLY/examples/05_rdtsc.cpp`");
    return 0;
}
