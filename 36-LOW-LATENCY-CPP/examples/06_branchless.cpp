// 06_branchless.cpp
// ============================================================
// Branch vs branchless — MEASURED, dono directions.
// (Folder 31/04 ne branch prediction cost dikhaya; yahan hot-path lens +
// the honest trade-off: branchless HAMESHA jeet nahi.)
//
//   Test 1: sum of elements > threshold, on UNPREDICTABLE (random) data
//           branchy  `if (x > t) s += x;`   vs
//           mask     `s += x & -(x > t);`   vs
//           cmov     `s += (x > t) ? x : 0;`  (compiler -> cmov at -O2)
//
//   Test 2: same, on PREDICTABLE (sorted) data — branchless ka nuksaan
//
//   Test 3: small `switch` (5-way) as a jump/if-chain vs a lookup TABLE
//
// Trade-off:
//  - branchless: no misprediction, but ALWAYS does the work (both sides)
//    + longer dependency chain -> loses when the branch was predictable
//  - `-O2` often if-converts a simple `if` to `cmov` on its own — check asm
//  - lookup table: 1 load (maybe a cache miss) vs a predicted branch
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 06_branchless.cpp -o b && ./b
//   asm:  g++ -std=c++20 -O2 -S -masm=intel 06_branchless.cpp -o - | c++filt | less
// ============================================================

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

namespace ch = std::chrono;
using Clock = ch::steady_clock;
template <class T> static inline void keep(T& v) { asm volatile("" : "+r,m"(v) : : "memory"); }

[[gnu::noinline]] static std::int64_t sum_branchy(const std::int32_t* a, std::size_t n, std::int32_t t) {
    std::int64_t s = 0;
    for (std::size_t i = 0; i < n; ++i) if (a[i] > t) s += a[i];
    return s;
}
[[gnu::noinline]] static std::int64_t sum_mask(const std::int32_t* a, std::size_t n, std::int32_t t) {
    std::int64_t s = 0;
    for (std::size_t i = 0; i < n; ++i) {
        std::int32_t m = -static_cast<std::int32_t>(a[i] > t);   // 0 or -1
        s += a[i] & m;
    }
    return s;
}
[[gnu::noinline]] static std::int64_t sum_ternary(const std::int32_t* a, std::size_t n, std::int32_t t) {
    std::int64_t s = 0;
    for (std::size_t i = 0; i < n; ++i) s += (a[i] > t) ? a[i] : 0;   // -> cmov at -O2
    return s;
}

// ---- Test 3: 5-way classify --------------------------------------
enum Cls { A = 0, B, C, D, E };
[[gnu::noinline]] static std::int64_t classify_switch(const std::uint8_t* c, std::size_t n) {
    std::int64_t s = 0;
    for (std::size_t i = 0; i < n; ++i) {
        switch (c[i]) {
            case A: s += 1; break;
            case B: s += 3; break;
            case C: s += 7; break;
            case D: s += 15; break;
            default: s += 31; break;
        }
    }
    return s;
}
[[gnu::noinline]] static std::int64_t classify_table(const std::uint8_t* c, std::size_t n) {
    static constexpr std::int64_t TAB[5] = {1, 3, 7, 15, 31};
    std::int64_t s = 0;
    for (std::size_t i = 0; i < n; ++i) s += TAB[c[i]];
    return s;
}

template <class F>
static double bench(F f, int reps) {
    double best = 1e300;
    for (int r = 0; r < reps; ++r) {
        auto t0 = Clock::now();
        auto v = f();
        auto t1 = Clock::now();
        keep(v);
        best = std::min(best, static_cast<double>(ch::duration_cast<ch::nanoseconds>(t1 - t0).count()));
    }
    return best;
}

int main() {
    constexpr std::size_t N = 1u << 22;                  // 4M
    std::mt19937 rng(9);

    std::vector<std::int32_t> rnd(N), srt(N);
    for (auto& x : rnd) x = static_cast<std::int32_t>(rng() % 1000) - 500;
    srt = rnd;
    std::sort(srt.begin(), srt.end());
    std::vector<std::uint8_t> cls(N);
    for (auto& x : cls) x = static_cast<std::uint8_t>(rng() % 5);

    const std::int32_t T = 0;

    std::puts("=== Test 1: sum(x>0) on RANDOM data (unpredictable branch) ===");
    std::printf("  branchy   : %6.3f ns/elem\n", bench([&]{ return sum_branchy (rnd.data(), N, T); }, 20) / N);
    std::printf("  mask      : %6.3f ns/elem\n", bench([&]{ return sum_mask    (rnd.data(), N, T); }, 20) / N);
    std::printf("  ternary   : %6.3f ns/elem\n", bench([&]{ return sum_ternary (rnd.data(), N, T); }, 20) / N);

    std::puts("\n=== Test 2: same, on SORTED data (predictable branch) ===");
    std::printf("  branchy   : %6.3f ns/elem\n", bench([&]{ return sum_branchy (srt.data(), N, T); }, 20) / N);
    std::printf("  mask      : %6.3f ns/elem\n", bench([&]{ return sum_mask    (srt.data(), N, T); }, 20) / N);
    std::printf("  ternary   : %6.3f ns/elem\n", bench([&]{ return sum_ternary (srt.data(), N, T); }, 20) / N);

    std::puts("\n=== Test 3: 5-way classify — switch vs lookup table ===");
    std::printf("  switch    : %6.3f ns/elem\n", bench([&]{ return classify_switch(cls.data(), N); }, 20) / N);
    std::printf("  table     : %6.3f ns/elem\n", bench([&]{ return classify_table (cls.data(), N); }, 20) / N);

    std::puts("\nKya seekha (measured, is box):");
    std::puts(" - RANDOM: mask/ternary ~0.26 vs branchy ~0.32 ns/elem. mask version");
    std::puts("   SIMD ho gaya (~2 elem/cyc); mispredict bhi bachta.");
    std::puts(" - SORTED: teenon ~same as their RANDOM number -> matlab `-O2` ne");
    std::puts("   'branchy' ko BHI if-convert kar diya (cmov) — koi asli branch bacha");
    std::puts("   hi nahi, isliye sorted vs unsorted mein farak nahi (Rule 2, 33/08).");
    std::puts("   `./build.ps1 asm` se confirm karo: `cmovg` hai ya `jg`.");
    std::puts(" - switch vs table: **~12x** (3.9 vs 0.34)! Random `c[i]` pe switch ka");
    std::puts("   jump-table = ek UNPREDICTABLE indirect jump -> har element mispredict.");
    std::puts("   `TAB[c[i]]` = ek predicted load + vectorizes. Data-driven dispatch pe");
    std::puts("   array-lookup >> switch.");
    std::puts(" - Rule: branchless tabhi jab branch GENUINELY unpredictable ho. Warna");
    std::puts("   `[[likely]]` + straight-line hot path (33/08). Measure, guess mat.");
    return 0;
}
