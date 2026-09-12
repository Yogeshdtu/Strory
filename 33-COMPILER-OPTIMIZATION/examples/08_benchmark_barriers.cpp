// 08_benchmark_barriers.cpp
// ============================================================
// Benchmark likhne ka #1 rule: compiler ko woh kaam DELETE karne se roko
// jo tum naap rahe ho. `-O2` dead code / loop-invariant / closed-form
// solve karke tumhara loop 0 ns bana deta.
//
// Yahan 4 tareeke, badhte-strong order mein:
//   1. kuch nahi        -> loop GAYAB (0 ns) -- galat
//   2. volatile sink    -> kaam hota, par har iter store (extra cost)
//   3. DoNotOptimize(x)  -> value ko "escape" maano, 0 instructions emit
//   4. ClobberMemory()   -> "saari memory badal gayi" -- stores ko roke
//
// Yeh folder 31/32 ke `keep()` helper ka poora explanation hai.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 08_benchmark_barriers.cpp -o bar && ./bar
// ============================================================

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

using Clock = std::chrono::steady_clock;

// ---- Google-Benchmark style helpers (portable, GCC/Clang) ----------------
template <class T>
static inline void DoNotOptimize(T const& value) {
    // value ko ek input constraint deta -> compiler maanta ki koi ise padh
    // sakta hai -> use compute karna zaroori. Emits ZERO instructions.
    asm volatile("" : : "r,m"(value) : "memory");
}
template <class T>
static inline void DoNotOptimize(T& value) {
    asm volatile("" : "+r,m"(value) : : "memory");
}
static inline void ClobberMemory() {
    // "poori memory badal di" -> pending stores ko flush karna padega
    asm volatile("" : : : "memory");
}

// kaam: N tak squares ka sum
static std::uint64_t sum_squares(std::uint64_t n) {
    std::uint64_t s = 0;
    for (std::uint64_t i = 1; i <= n; ++i) s += i * i;
    return s;
}

int main() {
    constexpr std::uint64_t N = 200'000'000;
    constexpr int REPS = 5;

    // ---- 1. NO barrier: result use nahi hota -> loop delete ----
    {
        auto t0 = Clock::now();
        for (int r = 0; r < REPS; ++r) {
            std::uint64_t s = sum_squares(N);
            (void)s;                       // <- compiler: "s kahin use nahi" -> sab hatao
        }
        auto t1 = Clock::now();
        std::printf("1. no barrier          : %8.2f ms   <- loop DELETE ho gaya\n",
                    std::chrono::duration<double, std::milli>(t1 - t0).count() / REPS);
    }

    // ---- 2. volatile sink ----
    {
        volatile std::uint64_t sink = 0;
        auto t0 = Clock::now();
        for (int r = 0; r < REPS; ++r) sink = sum_squares(N);
        auto t1 = Clock::now();
        std::printf("2. volatile sink       : %8.2f ms   (kaam hota; +1 volatile store/rep)\n",
                    std::chrono::duration<double, std::milli>(t1 - t0).count() / REPS);
        (void)sink;
    }

    // ---- 3. DoNotOptimize on the result ----
    {
        auto t0 = Clock::now();
        for (int r = 0; r < REPS; ++r) {
            std::uint64_t s = sum_squares(N);
            DoNotOptimize(s);              // <- s "escape" -> compute zaroori, 0 extra instr
        }
        auto t1 = Clock::now();
        std::printf("3. DoNotOptimize(result): %8.2f ms   <- SAHI: kaam hota, zero overhead\n",
                    std::chrono::duration<double, std::milli>(t1 - t0).count() / REPS);
    }

    // ---- 4. DoNotOptimize the INPUT too (defeat closed-form / const-fold) ----
    {
        std::uint64_t n = N;
        auto t0 = Clock::now();
        for (int r = 0; r < REPS; ++r) {
            DoNotOptimize(n);             // n ab "unknown" -> sum(1..n) ka formula use nahi kar sakta
            std::uint64_t s = sum_squares(n);
            DoNotOptimize(s);
        }
        auto t1 = Clock::now();
        std::printf("4. DoNotOptimize(in+out): %8.2f ms   (input bhi opaque -> no formula shortcut)\n",
                    std::chrono::duration<double, std::milli>(t1 - t0).count() / REPS);
    }

    // ---- vector fill: ClobberMemory zaroori (store ka side effect) ----
    {
        std::vector<int> v(1'000'000);
        auto t0 = Clock::now();
        for (int r = 0; r < REPS; ++r) {
            for (std::size_t i = 0; i < v.size(); ++i) v[i] = static_cast<int>(i * 3) + r;
            ClobberMemory();               // "v badal gaya" -> fill loop ko delete mat karo
        }
        auto t1 = Clock::now();
        DoNotOptimize(v.data());
        std::printf("\n5. vector fill + ClobberMemory: %6.2f ms/rep   (stores retained)\n",
                    std::chrono::duration<double, std::milli>(t1 - t0).count() / REPS);
    }

    std::puts("\nKya hua:");
    std::puts(" - Case 1: `s` ka result kahin use nahi -> `-O2` ne sum_squares ka");
    std::puts("   poora call + loop hata diya -> ~0 ms. Classic benchmark bug.");
    std::puts(" - Case 2: `volatile sink` compiler ko har rep pe ek store karne");
    std::puts("   ko majboor karta -> kaam hota hai, par ek extra volatile store");
    std::puts("   /rep (bade loops mein negligible, chhote micro-bench mein noise).");
    std::puts(" - Case 3: DoNotOptimize(s) sirf yeh kehta 'koi `s` padh sakta' ->");
    std::puts("   compute zaroori, par khud ZERO instructions emit karta. Best.");
    std::puts(" - Case 4: agar loop ka result input ka closed-form ho (sum 1..n =");
    std::puts("   n(n+1)(2n+1)/6), compiler woh formula laga ke loop hata sakta.");
    std::puts("   Input ko bhi DoNotOptimize karo -> woh shortcut band.");
    std::puts(" - Case 5: writes ka 'use' unki memory hai -> ClobberMemory() se");
    std::puts("   compiler maanta stores visible hone chahiye -> fill retained.");
    std::puts(" - Yeh helpers hi folder 31/32 ke `keep()` hain. Har micro-benchmark");
    std::puts("   mein: input opaque, output escape, -O2 mandatory.");
    return 0;
}
