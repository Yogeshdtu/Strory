// 03_branch_prediction.cpp
// ============================================================
// The classic demo. Array pe loop jo har element pe ek data-dependent
// branch leta hai (`if (v[i] >= threshold)`). SORTED array -> branch pattern
// predictable (NNNN...YYYY) -> predictor ~100%. RANDOM -> ~50% -> har
// misprediction pe pipeline flush (~15-20 cycles).
//
// ⚠️ CLAUDE.md Rule 2: default `-O2` pe GCC is `if` ko **branchless `cmov`**
// bana deta -> koi misprediction possible nahi -> RANDOM==SORTED (no effect).
// Yeh KHUD ek lesson hai (compiler ne example 04 ka kaam kar diya). Yahan
// hum `#pragma GCC optimize("no-if-conversion...")` se if-conversion band
// karke ASLI branch rakhte hain, taaki misprediction cost dikhe.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 03_branch_prediction.cpp -o bpred && ./bpred
// ============================================================

#pragma GCC optimize("no-if-conversion", "no-if-conversion2", "no-tree-loop-if-convert")

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <chrono>
#include <random>
#include <vector>

using Clock = std::chrono::steady_clock;
static volatile std::int64_t g_sink;

// `carry` thread kiya jata (result har rep pe alag) -> compiler call ko
// loop se hoist / CSE nahi kar sakta. carry branch ko affect nahi karta.
static std::int64_t __attribute__((noinline))
sum_ge(const std::vector<int>& v, int thr, std::int64_t carry) {
    std::int64_t s = carry;
    for (int x : v) {
        if (x >= thr)               // <- data-dependent branch (if-conversion disabled)
            s += x;
    }
    return s;
}

int main() {
    constexpr std::size_t N = 1u << 15;     // 32768
    constexpr int         REPS = 20000;

    std::vector<int> data(N);
    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> dist(0, 255);
    for (auto& x : data) x = dist(rng);

    std::vector<int> sorted_data = data;
    std::sort(sorted_data.begin(), sorted_data.end());

    auto bench = [&](const std::vector<int>& v) {
        std::int64_t acc = 0;
        auto t0 = Clock::now();
        for (int r = 0; r < REPS; ++r) acc = sum_ge(v, 128, acc);   // carry threaded
        auto t1 = Clock::now();
        g_sink = acc;
        return std::chrono::duration<double>(t1 - t0).count() * 1e9
             / (static_cast<double>(N) * REPS);
    };

    (void)bench(data); (void)bench(sorted_data);        // warm-up

    const double ns_rand = bench(data);
    const double ns_sort = bench(sorted_data);

    std::printf("branch: `if (x >= 128) s += x`   over %zu ints, %d reps\n", N, REPS);
    std::printf("(if-conversion disabled so a real conditional jump is emitted)\n\n");
    std::printf("  RANDOM order : %6.3f ns/element   (~50%% mispredict)\n", ns_rand);
    std::printf("  SORTED order : %6.3f ns/element   (~perfectly predicted)\n", ns_sort);
    std::printf("\n  slowdown from the unpredictable branch: %.2fx\n", ns_rand / ns_sort);

    std::puts("\nKya hua:");
    std::puts(" - CPU har branch ka outcome PREDICT karke aage ki instructions");
    std::puts("   speculatively chalata hai. Sahi -> free. Galat -> speculated kaam");
    std::puts("   discard + pipeline refill (~15-20 cycles idle).");
    std::puts(" - SORTED: outcome pehle sab 'false' phir sab 'true' -- predictable");
    std::puts("   pattern, predictor ~100% sahi -> mispredict ~0.");
    std::puts(" - RANDOM: coin flip -- predictor kuch nahi kar sakta, ~50% galat.");
    std::puts("   ~16000 mispredicts per rep x ~15-20 cyc = woh extra time.");
    std::puts(" - **Default -O2 pe (bina yeh pragma) yeh farak GAYAB ho jata** -- GCC");
    std::puts("   `if` ko `cmovge` bana deta, no branch = no misprediction. Verify:");
    std::puts("   `./build.ps1 asm 31-CPU-ARCHITECTURE/examples/03_branch_prediction.cpp`");
    std::puts("   -> pragma ke saath `jl/jge` dikhega, bina ke `cmov`.");
    std::puts(" - HFT sabak: hot-path data-dependent branches jinka outcome random hai");
    std::puts("   (order matched? price crossed?) -> branchless banao (example 04), ya");
    std::puts("   data ko partition/sort karke branch predictable banao, ya");
    std::puts("   `[[likely]]`/`[[unlikely]]` jab ek side ~hamesha lagti hai.");
    return 0;
}
