// 08_rdtsc_timing.cpp
// ============================================================
// RDTSC = Read Time-Stamp Counter -- CPU ka cycle counter. Sabse fine,
// sabse sasta timestamp. Par:
//   - cycles deta, ns nahi -> calibrate karo
//   - out-of-order execute ho sakta -> lfence/rdtscp se serialize
//   - "invariant TSC" (example 07) chahiye warna frequency ke saath badalta
//
// Yahan: calibrate cycles/ns, phir teen RDTSC variants ki apni cost naapo,
// phir ek known-size loop ko cycle-count karo.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 08_rdtsc_timing.cpp -o rdtsc && ./rdtsc
// ============================================================

#include <cstdint>
#include <cstdio>
#include <chrono>
#include <x86intrin.h>

using Clock = std::chrono::steady_clock;
static volatile std::uint64_t g_sink;

static inline std::uint64_t rdtsc_plain() { return __rdtsc(); }

static inline std::uint64_t rdtsc_lfence() {
    _mm_lfence();                 // pehle ki saari loads retire
    std::uint64_t t = __rdtsc();
    _mm_lfence();
    return t;
}

static inline std::uint64_t rdtscp_ordered() {
    unsigned aux;
    std::uint64_t t = __rdtscp(&aux);   // rdtscp: waits for older instrs to retire
    _mm_lfence();                        // aur baad ki instrs ko rok
    return t;
}

// cycles/ns calibration: steady_clock ke against ~200ms busy-wait
static double calibrate_cycles_per_ns() {
    _mm_lfence();
    std::uint64_t c0 = __rdtsc();
    auto w0 = Clock::now();
    // ~200 ms busy loop
    while (std::chrono::duration<double>(Clock::now() - w0).count() < 0.2) {
        for (int k = 0; k < 1000; ++k) g_sink = g_sink + 1;
    }
    _mm_lfence();
    std::uint64_t c1 = __rdtsc();
    auto w1 = Clock::now();
    double ns = std::chrono::duration<double, std::nano>(w1 - w0).count();
    return static_cast<double>(c1 - c0) / ns;
}

template <class Fn>
static double self_cost_cycles(Fn read, std::uint64_t iters) {
    std::uint64_t acc = 0;
    std::uint64_t s = rdtscp_ordered();
    for (std::uint64_t i = 0; i < iters; ++i) acc += read();
    std::uint64_t e = rdtscp_ordered();
    g_sink = acc;
    return static_cast<double>(e - s) / static_cast<double>(iters);
}

int main() {
    const double cpn = calibrate_cycles_per_ns();
    std::printf("calibrated: %.3f cycles/ns  (~%.2f GHz effective)\n\n", cpn, cpn);

    const std::uint64_t IT = 5'000'000;
    const double c_plain  = self_cost_cycles(rdtsc_plain,     IT);
    const double c_lfence = self_cost_cycles(rdtsc_lfence,    IT);
    const double c_rdtscp = self_cost_cycles(rdtscp_ordered,  IT);

    std::printf("self-cost of ONE timestamp read:\n");
    std::printf("  __rdtsc (plain)          : %6.2f cycles   (~%.2f ns)  -- may reorder\n",
                c_plain,  c_plain  / cpn);
    std::printf("  lfence;rdtsc;lfence      : %6.2f cycles   (~%.2f ns)  -- serialized\n",
                c_lfence, c_lfence / cpn);
    std::printf("  rdtscp;lfence            : %6.2f cycles   (~%.2f ns)  -- serialized\n",
                c_rdtscp, c_rdtscp / cpn);

    // measure a known loop: 100M dependent adds -> ~1 cycle each expected.
    // asm barrier taaki `-O2` isse closed-form / DCE na kare (Rule 2).
    {
        std::uint64_t x = 1;
        const std::uint64_t LOOP = 100'000'000;
        std::uint64_t s = rdtscp_ordered();
        for (std::uint64_t i = 0; i < LOOP; ++i) { x = x + i + 1; asm volatile("" : "+r"(x)); }
        std::uint64_t e = rdtscp_ordered();
        g_sink = x;
        double cyc = static_cast<double>(e - s) / static_cast<double>(LOOP);
        std::printf("\n  100M-iter `x = x + i + 1` (barrier'd) : %.2f cycles/iter  (~%.2f ns)\n",
                    cyc, cyc / cpn);
    }

    std::puts("\nKya seekha:");
    std::puts(" - `__rdtsc()` sasta (~6-25 cyc) par out-of-order slip kar sakta -- ek");
    std::puts("   micro-region time karne pe woh region ke bahar ki instrs bhi count/miss");
    std::puts("   ho sakti. Coarse measurement (loop of millions) ke liye theek.");
    std::puts(" - `lfence;rdtsc;lfence` ya `rdtscp;lfence`: RDTSC ko fixed point pe pin");
    std::puts("   karta -> chhoti regions (~100 cyc) bhi accurately. Cost thoda zyada.");
    std::puts(" - Sabak: 1000 iterations ke loop ko time karo, phir /1000 -- per-iter");
    std::puts("   noise average ho jata aur RDTSC self-cost amortize. Ek single op ko");
    std::puts("   directly time karna = mostly measuring RDTSC + fences.");
    std::puts(" - cycles -> ns: startup pe ek baar calibrate (yahan jaisa), phir cache");
    std::puts("   kar lo. Invariant-TSC (example 07) hone pe cycles/ns constant rehta");
    std::puts("   chahe CPU frequency scale kare (TSC nominal rate pe chalta).");
    std::puts(" - HFT: latency timestamps ke liye `rdtscp` (~20-30 cyc) ya");
    std::puts("   `clock_gettime(CLOCK_MONOTONIC)` (vDSO, folder 29 file 16). Pinned");
    std::puts("   thread pe rakho taaki TSC cross-core skew ka sawaal na aaye.");
    return 0;
}
