// 01_cache_line_size.cpp
// ============================================================
// Cache line size ko EMPIRICALLY detect karo. Ek bade array (> LLC) pe
// ek full pass, badhte stride ke saath. Har pass array ke saare bytes
// "cover" karta (working set constant), par accesses ki ginti = N/stride.
//
// time-per-access ko dekho:
//   stride < line size  -> ek 64B line ke andar kai accesses; pehla access
//                          miss, baaki hit -> per-access cost ~ linear in stride
//   stride >= line size  -> har access ek naya line -> per-access cost PLATEAU
// "Knee" jahan ramp flatten hota = cache line size (x86 pe 64 bytes).
//
// NOTE: HW stride-prefetcher sequential pattern ko detect karke miss latency
// ka bahut hissa chhupa deta -> ramp textbook se SOFT dikhta, par knee ~64B pe
// phir bhi. Prefetcher-defeated (random) version example 02 mein -- wahan cliff
// sharp hota.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 01_cache_line_size.cpp -o cls && ./cls
// ============================================================

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

using Clock = std::chrono::steady_clock;

// Google-Benchmark DoNotOptimize style barrier: 0 instructions, par compiler
// ko `acc` ko "escape" maan'na padta -> loop DCE nahi hota.
template <class T>
static inline void keep(T& v) { asm volatile("" : "+r,m"(v) : : "memory"); }

int main() {
    // 64 MiB: is box ke L3 (8 MiB per CCX) se 8x bada -> bade stride pe
    // har naya line DRAM se aata.
    constexpr std::size_t N = 64u << 20;            // bytes, power of two
    std::vector<std::uint8_t> buf(N, 1u);

    constexpr int REPS = 9;          // min-of-REPS lete hain (jitter-resistant)

    std::printf("buffer = %zu MiB   (full pass per stride; min of %d passes)\n", N >> 20, REPS);
    std::printf("\n stride(B)    accesses/pass       ns/access (min)\n");
    std::printf(  " ---------    -------------       ---------------\n");

    double prev = 0.0;
    std::size_t knee = 0;
    bool seen_ramp = false;          // koi ratio > 1.5 (steep part) dikha?

    for (std::size_t stride = 1; stride <= 512; stride <<= 1) {
        const std::size_t n_acc = N / stride;
        std::uint64_t acc = 0;

        // warm-up pass
        for (std::size_t i = 0; i < N; i += stride) acc += buf[i];
        keep(acc);

        // har pass alag time karo; MINIMUM lo -- OS jitter / freq dips sirf
        // time BADHATE hain, ghatate nahi, to min sabse saaf signal hai.
        double best = 1e300;
        for (int r = 0; r < REPS; ++r) {
            auto t0 = Clock::now();
            for (std::size_t i = 0; i < N; i += stride) acc += buf[i];
            auto t1 = Clock::now();
            keep(acc);
            const double ns1 = std::chrono::duration<double>(t1 - t0).count() * 1e9
                             / static_cast<double>(n_acc);
            if (ns1 < best) best = ns1;
        }
        const double ns = best;
        std::printf(" %8zu     %12zu        %8.3f\n", stride, n_acc, ns);

        // steep ramp (cost stride ke saath ~double) ke baad pehli baar jab
        // growth < 1.3x ho jaye -> pichla stride hi knee (= line size) tha.
        if (prev > 0.0) {
            const double ratio = ns / prev;
            if (ratio > 1.5) seen_ramp = true;
            if (seen_ramp && ratio < 1.3 && knee == 0) knee = stride >> 1;
        }
        prev = ns;
    }

    std::printf("\nAuto-detected knee ~= %zu B", knee ? knee : 64);
    std::printf("   [x86 cache line = 64 B; is box pe ramp 32->128 tak chadhta]\n");

    std::puts("\nKya hua:");
    std::puts(" - Har DRAM->cache transfer 64 bytes (ek 'cache line') ka hota,");
    std::puts("   chahe tum 1 byte maango ya 64.");
    std::puts(" - stride 1..8: ek line ke andar 64..8 accesses -> pehla miss,");
    std::puts("   baaki hit -> per-access cost chhota (aur prefetcher chhupa raha).");
    std::puts(" - stride 16..64: har line ke accesses kam -> miss ki cost kam");
    std::puts("   amortize -> per-access cost tezi se chadhta.");
    std::puts(" - ⚠️ CLAUDE.md Rule 2: is box pe ramp 64 pe cleanly flatten NAHI");
    std::puts("   hota -- 128 tak chadhta. Kyun: DRAM/next-line prefetcher stride");
    std::puts("   64 pe bhi thoda madad karta, 128+ pe adjacent-line prefetch");
    std::puts("   aadha waste, aur bade stride pe TLB coverage badalti. Sequential");
    std::puts("   scan ka knee prefetcher 'smear' kar deta.");
    std::puts(" - SHARP 64 B cliff dekhna hai to prefetcher ko haraao -> example 02");
    std::puts("   (random line order): wahan 64 B pe saaf jump.");
    return 0;
}
