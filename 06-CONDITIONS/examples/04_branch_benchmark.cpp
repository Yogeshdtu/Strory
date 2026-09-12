// 04_branch_benchmark.cpp
// ============================================================
// BRANCH PREDICTION -- sorted vs unsorted array (classic demo)
// ============================================================
//   BENCHMARK hai -> -O2 ZAROORI. -O0 pe yeh bekaar hai.
//   g++ -std=c++20 -O2 04_branch_benchmark.cpp -o bb && ./bb
// ============================================================
// Sawaal: bilkul SAME code, SAME data values -- sirf order alag.
//         Ek run doosre se 3-6x tez. Kyun?
//
// Jawaab: CPU har `if` ka result GUESS karti hai (branch predictor).
//         - sorted data  -> pattern predictable -> guess sahi -> ~0 cost
//         - random data  -> 50/50 -> guess aksar galat -> misprediction
//           penalty (~15-20 cycles har baar pipeline flush)
//
//         Branchless code mein koi guess nahi -> data order se farq nahi.
// ============================================================

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

// Compiler ko is value ko "use hua" maanne pe majboor karta hai --
// taaki woh loop ko optimize/vectorize karke hamara experiment na bigaade.
#if defined(__GNUC__) || defined(__clang__)
static inline void keepScalar(long long& x) { asm volatile("" : "+r"(x) : : ); }
#else
static volatile long long g_sink;
static inline void keepScalar(long long& x) { g_sink = x; }
#endif

// ------------------------------------------------------------
//  Version A: BRANCH ke saath.
//  taken-path mein barrier -> compiler isse cmov/branchless nahi bana
//  sakta, aur loop vectorize nahi hota. Ek asli conditional branch bachti hai.
// ------------------------------------------------------------
long long sumIfBig(const int* a, std::size_t n) {
    long long s = 0;
    for (std::size_t i = 0; i < n; ++i) {
        if (a[i] >= 128) {
            s += a[i];
            keepScalar(s);
        }
    }
    return s;
}

// ------------------------------------------------------------
//  Version B: BRANCHLESS. Koi `if` nahi.
//  (a[i] >= 128) -> 0 ya 1 -> multiply. CPU ko kuch guess nahi karna.
// ------------------------------------------------------------
long long sumMaskBig(const int* a, std::size_t n) {
    long long s = 0;
    for (std::size_t i = 0; i < n; ++i) {
        s += static_cast<long long>(a[i] >= 128) * a[i];
        keepScalar(s);
    }
    return s;
}

int main() {
    constexpr std::size_t N    = 32'768;   // array size
    constexpr int         REPS = 4'000;    // itni baar poora array process karo

    std::vector<int> data(N);

    // Values 0..255 -- to `>= 128` ~50% chance (unsorted par worst case guess)
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 255);
    for (int& v : data) v = dist(rng);

    auto bench = [&](auto fn) {
        long long checksum = 0;
        const auto t0 = std::chrono::steady_clock::now();
        for (int r = 0; r < REPS; ++r) {
            checksum += fn(data.data(), data.size());
        }
        const auto t1 = std::chrono::steady_clock::now();
        const double ms =
            std::chrono::duration<double, std::milli>(t1 - t0).count();
        return std::pair<double, long long>{ms, checksum};
    };

    std::cout << std::fixed << std::setprecision(1);
    std::cout << "N = " << N << ", REPS = " << REPS
              << "  (" << (static_cast<double>(N) * REPS / 1e6)
              << " M branch evaluations per row)\n\n";

    // ---- 1. UNSORTED + branch ----
    const auto [msUnsorted, ckU] = bench(sumIfBig);

    // ---- 2. UNSORTED + branchless ----
    const auto [msBranchless, ckBL] = bench(sumMaskBig);

    // ---- 3. SORTED + branch ----
    std::sort(data.begin(), data.end());
    const auto [msSorted, ckS] = bench(sumIfBig);

    std::cout << "  if-branch,  UNSORTED data : " << std::setw(8) << msUnsorted
              << " ms\n";
    std::cout << "  if-branch,  SORTED   data : " << std::setw(8) << msSorted
              << " ms   <- same code, same values, sirf order alag\n";
    std::cout << "  branchless, UNSORTED data : " << std::setw(8) << msBranchless
              << " ms   <- order se farq nahi padta\n\n";

    if (msSorted > 0.0)
        std::cout << "  sorted / unsorted speedup : "
                  << std::setprecision(2) << (msUnsorted / msSorted) << "x\n"
                  << std::setprecision(1);

    std::cout << "  (checksums match? "
              << ((ckU == ckS && ckU == ckBL) ? "haan" : "NAHI") << ")\n";

    // ============================================================
    //  SAMAJHNE WALI BAATEIN
    // ============================================================
    std::cout <<
        "\n"
        "  * UNSORTED: `a[i] >= 128` ~50/50 hai. Branch predictor har baar\n"
        "    lagbhag coin-toss haar-ta hai -> pipeline flush -> slow.\n"
        "  * SORTED: pehle saare chhote, phir saare bade. Branch ek hi baar\n"
        "    'palat-ta' hai. Predictor ~100% sahi -> branch lagbhag free.\n"
        "  * BRANCHLESS: koi prediction nahi. Constant time. Par har element\n"
        "    ka kaam karta hai (jab predictable branch ho to yeh SLOWER ho sakta).\n"
        "\n"
        "  Rule: pehle MEASURE karo. Predictable branch = free. Unpredictable\n"
        "  branch = branchless try karo. -O0 pe yeh poora experiment jhootha hai.\n"
        "\n"
        "  HFT: market data parsing / order matching ke hot loops mein data-\n"
        "  dependent branches latency ki tail (p99) ko phaila dete hain. Isi\n"
        "  wajah se lock-free ring buffers, branchless decoders, aur sorted/\n"
        "  bucketed layouts use hote hain. Folder 31 (branch prediction) aur\n"
        "  36 (low-latency) mein poora.\n";

    return 0;
}
