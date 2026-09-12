// 02_stride_access.cpp
// ============================================================
// Stride ka asar do angle se:
//
//  PART 1 (sequential stride): fixed 64 MiB buffer. stride=1 pe har element
//    chhua jaata; bade stride pe kam elements par har access naya line.
//    "useful bandwidth" (kaam ke bytes / sec) girta -- tum 64 B line fetch
//    karte ho par sirf 4 B use.
//
//  PART 2 (sequential vs RANDOM line order, poora line padh ke): prefetcher
//    ki value. Sequential -> HW prefetcher aage ki lines pehle laa deta ->
//    streaming bandwidth. Random -> prefetcher andha -> har line alag miss.
//
// Ties: lesson 05 (spatial locality), lesson 06 (prefetching), lesson 13
// (bandwidth vs latency), lesson 04 (MLP -- kai misses ek saath in-flight).
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 02_stride_access.cpp -o stride && ./stride
// ============================================================

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <numeric>
#include <random>
#include <vector>

using Clock = std::chrono::steady_clock;

template <class T>
static inline void keep(T& v) { asm volatile("" : "+r,m"(v) : : "memory"); }

int main() {
    constexpr std::size_t BYTES = 64u << 20;
    constexpr std::size_t NI    = BYTES / sizeof(std::int32_t);   // 16 M ints
    constexpr int         REPS  = 8;
    std::vector<std::int32_t> a(NI, 1);

    std::puts("=== PART 1: sequential, varying stride (HW prefetcher ACTIVE) ===");
    std::puts(" stride(elem)  stride(B)   ns/access   useful GB/s (4B per access)");
    std::puts(" -----------   ---------   ---------   --------------------------");
    for (std::size_t s = 1; s <= 64; s <<= 1) {
        std::int64_t acc = 0;
        for (std::size_t i = 0; i < NI; i += s) acc += a[i];       // warm
        keep(acc);
        const std::size_t n_acc = (NI + s - 1) / s;
        auto t0 = Clock::now();
        for (int r = 0; r < REPS; ++r)
            for (std::size_t i = 0; i < NI; i += s) acc += a[i];
        auto t1 = Clock::now();
        keep(acc);
        const double sec = std::chrono::duration<double>(t1 - t0).count();
        const double ns  = sec * 1e9 / (static_cast<double>(n_acc) * REPS);
        const double gbs = static_cast<double>(n_acc) * REPS * sizeof(std::int32_t) / sec / 1e9;
        std::printf(" %9zu     %7zu     %8.3f     %10.2f\n",
                    s, s * sizeof(std::int32_t), ns, gbs);
    }
    std::puts("  -> stride badhte hi useful GB/s girta: 64 B line fetch, 4 B use.");

    // ---- PART 2: whole-line reads, sequential vs random line order ----
    constexpr std::size_t LINES = BYTES / 64;                      // 1 Mi lines
    constexpr std::size_t PER   = 16;                              // 16 ints = 64 B = full line
    std::vector<std::uint32_t> seq(LINES), rnd(LINES);
    std::iota(seq.begin(), seq.end(), 0u);
    rnd = seq;
    std::mt19937 rng(777);
    std::shuffle(rnd.begin(), rnd.end(), rng);

    auto bench = [&](const std::vector<std::uint32_t>& ord) {
        std::int64_t acc = 0;
        for (std::uint32_t ln : ord) {                             // warm
            const std::size_t base = static_cast<std::size_t>(ln) * PER;
            for (std::size_t k = 0; k < PER; ++k) acc += a[base + k];
        }
        keep(acc);
        auto t0 = Clock::now();
        for (int r = 0; r < 4; ++r)
            for (std::uint32_t ln : ord) {
                const std::size_t base = static_cast<std::size_t>(ln) * PER;
                for (std::size_t k = 0; k < PER; ++k) acc += a[base + k];
            }
        auto t1 = Clock::now();
        keep(acc);
        const double sec  = std::chrono::duration<double>(t1 - t0).count();
        const double nsln = sec * 1e9 / (static_cast<double>(LINES) * 4);
        const double gbs  = static_cast<double>(LINES) * 4 * 64 / sec / 1e9;
        return std::pair<double, double>{nsln, gbs};
    };

    std::puts("\n=== PART 2: read the WHOLE 64 B line, 1 Mi lines, 4 passes ===");
    const auto [ns_seq, gb_seq] = bench(seq);
    const auto [ns_rnd, gb_rnd] = bench(rnd);
    std::printf("  sequential line order : %6.2f ns/line   %6.2f GB/s\n", ns_seq, gb_seq);
    std::printf("  random     line order : %6.2f ns/line   %6.2f GB/s\n", ns_rnd, gb_rnd);
    std::printf("  random / sequential   : %.1fx slower\n", ns_rnd / ns_seq);

    std::puts("\nKya hua:");
    std::puts(" - PART 1: stride 1..16 elem tak useful GB/s ~6.5 -> ~1.0 gir jaata.");
    std::puts("   Har access ek 64 B line laata; stride 16 (=64 B) pe us line ka");
    std::puts("   sirf 4 B use hota -> 16x bandwidth waste. Prefetcher ns/access ko");
    std::puts("   utna nahi badhne deta, par 'kaam ke bytes' fir bhi kam.");
    std::puts(" - PART 2: dono cases mein poora line padha (100% use), sirf ORDER");
    std::puts("   ka farak. Sequential -> stream/next-line prefetcher aage ki lines");
    std::puts("   time se laa deta -> DRAM streaming bandwidth (~10+ GB/s ek core).");
    std::puts("   miss. (Phir bhi ~25-35 ns/line, ~80 ns true DRAM latency se kam --");
    std::puts("   kyunki line addresses ek doosre pe depend nahi karte, CPU ~10");
    std::puts("   misses ek saath in-flight rakhta = MLP, lesson 04.)");
    std::puts(" - Sabak: linear access = prefetcher ka dost. Contiguous rakho,");
    std::puts("   ya prefetch hint do (example 06) jab pattern random ho.");
    return 0;
}
