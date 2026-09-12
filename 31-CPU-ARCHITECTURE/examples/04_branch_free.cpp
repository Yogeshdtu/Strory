// 04_branch_free.cpp
// ============================================================
// Example 03 dikhaya: unpredictable branch mehnga. Fix #1 = branchless.
// Ek branch ko arithmetic/bit-trick se replace karo taaki koi prediction
// ho hi na -- data ki predictability se latency independent ho jaye.
//
// Par branchless HAMESHA behtar nahi: agar branch PREDICTABLE hota, to
// branchy version free tha aur branchless extra ALU work karta. Dono cases.
//
// `#pragma GCC optimize("no-if-conversion")` taaki "branchy" wala VERSION
// sach mein branchy rahe (warna -O2 usse bhi cmov bana deta -> comparison
// meaningless -- example 03 ka Rule 2 point).
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 04_branch_free.cpp -o bfree && ./bfree
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

// `carry` threaded -> compiler call ko hoist/CSE nahi kar sakta (Rule 2).

// --- BRANCHY: real conditional jump (if-conversion disabled) ---
static std::int64_t __attribute__((noinline))
sum_branchy(const std::vector<int>& v, int thr, std::int64_t carry) {
    std::int64_t s = carry;
    for (int x : v) if (x >= thr) s += x;
    return s;
}

// --- BRANCHLESS: mask = (x >= thr) ? -1 : 0, phir (x & mask) add ---
static std::int64_t __attribute__((noinline))
sum_branchless(const std::vector<int>& v, int thr, std::int64_t carry) {
    std::int64_t s = carry;
    for (int x : v) {
        std::int64_t take = -static_cast<std::int64_t>(x >= thr);   // 0 or -1
        s += (static_cast<std::int64_t>(x) & take);
    }
    return s;
}

int main() {
    constexpr std::size_t N = 1u << 15;
    constexpr int         REPS = 20000;

    std::vector<int> rnd(N);
    std::mt19937 rng(999);
    std::uniform_int_distribution<int> dist(0, 255);
    for (auto& x : rnd) x = dist(rng);
    std::vector<int> srt = rnd;
    std::sort(srt.begin(), srt.end());

    auto bench = [&](auto fn, const std::vector<int>& v) {
        std::int64_t acc = 0;
        auto t0 = Clock::now();
        for (int r = 0; r < REPS; ++r) acc = fn(v, 128, acc);   // carry threaded
        auto t1 = Clock::now();
        g_sink = acc;
        return std::chrono::duration<double>(t1 - t0).count() * 1e9
             / (static_cast<double>(N) * REPS);
    };

    (void)bench(sum_branchy, rnd); (void)bench(sum_branchless, rnd);   // warm-up

    const double by_rnd = bench(sum_branchy,    rnd);
    const double bf_rnd = bench(sum_branchless, rnd);
    const double by_srt = bench(sum_branchy,    srt);
    const double bf_srt = bench(sum_branchless, srt);

    std::printf("            %14s   %14s\n", "branchy", "branchless");
    std::printf("  RANDOM :  %10.3f ns   %10.3f ns   -> branchless %.2fx faster (branch UNpredictable)\n",
                by_rnd, bf_rnd, by_rnd / bf_rnd);
    std::printf("  SORTED :  %10.3f ns   %10.3f ns   -> branchy %.2fx faster (branch predictable -> free)\n",
                by_srt, bf_srt, bf_srt / by_srt);

    std::puts("\nKya hua:");
    std::puts(" - RANDOM data: branchy ~50% mispredict (~15-20 cyc each). branchless mein");
    std::puts("   koi branch hi nahi -> koi mispredict -> constant, fast. Branchless jeeta.");
    std::puts(" - SORTED data: branchy branch ~100% predicted -> effectively free, aur");
    std::puts("   'false' case pe kuch karta hi nahi. branchless HAR element pe extra");
    std::puts("   AND+add. Yahan branchy ~1.2x aage (0.5 vs 0.6 ns) -- branchless ka");
    std::puts("   fixed cost predictable branch se zyada hai.");
    std::puts(" - Sabak: branchless tab chuno jab branch outcome UNPREDICTABLE ho. Agar");
    std::puts("   ek side ~hamesha lagti hai (error checks, rare paths) -> branchy +");
    std::puts("   `[[likely]]`/`[[unlikely]]` behtar (cold path ko I-cache se door bhi).");
    std::puts(" - Techniques: `x & -(cond)` masking, `cmov` (compiler se -- example 03");
    std::puts("   mein default -O2 ne khud kiya!), lookup table, `std::min/std::max`,");
    std::puts("   `abs` bit-trick, arithmetic (`sign = (a>b) - (a<b)`), SIMD predication");
    std::puts("   (hamesha branchless -- example 05).");
    std::puts(" - Verify with asm: `cmov*` = branchless; `j*` to a label = branch.");
    return 0;
}
