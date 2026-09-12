// 12_hot_cold_split.cpp
// ============================================================
// I-CACHE locality: ek hot loop ke andar ek rarely-taken COLD path
// (error handling / logging / a slow fallback). Agar woh cold code hot
// path ke beech INLINE hai, to:
//   - hot loop ka machine code bada -> kam L1i / uop-cache mein fit
//   - cold code beech mein pada -> har hot iteration uske upar se "jump"
//   - branch predictor / prefetch ko extra kaam
//
// Fix: cold code ko OUT-OF-LINE karo:
//   [[unlikely]] on the branch  +  the cold body in a [[gnu::cold,noinline]]
//   function  -> compiler ise .text.unlikely mein daal deta, hot loop
//   straight-line + dense.
//
//   Yahan 3 versions, hot-loop time measured (cold path ~1/10000 hit):
//     A. cold body inline in the loop
//     B. cold body in a plain function (compiler MAY still inline)
//     C. cold body in [[gnu::cold]] [[gnu::noinline]] + [[unlikely]] branch
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 12_hot_cold_split.cpp -o hc && ./hc
//   asm:  g++ -std=c++20 -O2 -S -masm=intel 12_hot_cold_split.cpp -o - | c++filt
//         (dekho: version C mein cold body loop ke BAAD / alag section mein)
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

static volatile std::uint64_t g_cold_sink = 0;

// ---- the cold body, three ways -------------------------------
static inline std::uint64_t cold_inline(std::uint64_t h) {
    // pretend: format a log line, walk a fallback table, update stats — bulky
    for (int k = 0; k < 32; ++k) h = h * 6364136223846793005ULL + static_cast<std::uint64_t>(k);
    return h;
}
static std::uint64_t cold_plain(std::uint64_t h) {
    for (int k = 0; k < 32; ++k) h = h * 6364136223846793005ULL + static_cast<std::uint64_t>(k);
    return h;
}
[[gnu::cold]] [[gnu::noinline]] static std::uint64_t cold_outlined(std::uint64_t h) {
    for (int k = 0; k < 32; ++k) h = h * 6364136223846793005ULL + static_cast<std::uint64_t>(k);
    return h;
}

static inline std::uint64_t hot_work(std::uint64_t h, std::uint64_t x) {
    h ^= x; h *= 0x100000001b3ULL; h ^= h >> 29; return h;
}

[[gnu::noinline]] static std::uint64_t run_A(const std::uint64_t* a, const std::uint8_t* rare, std::size_t n) {
    std::uint64_t h = 0;
    for (std::size_t i = 0; i < n; ++i) {
        h = hot_work(h, a[i]);
        if (rare[i]) h ^= cold_inline(h);
    }
    return h;
}
[[gnu::noinline]] static std::uint64_t run_B(const std::uint64_t* a, const std::uint8_t* rare, std::size_t n) {
    std::uint64_t h = 0;
    for (std::size_t i = 0; i < n; ++i) {
        h = hot_work(h, a[i]);
        if (rare[i]) h ^= cold_plain(h);
    }
    return h;
}
[[gnu::noinline]] static std::uint64_t run_C(const std::uint64_t* a, const std::uint8_t* rare, std::size_t n) {
    std::uint64_t h = 0;
    for (std::size_t i = 0; i < n; ++i) {
        h = hot_work(h, a[i]);
        if (rare[i]) [[unlikely]] h ^= cold_outlined(h);
    }
    return h;
}

template <class F>
static double bench(std::size_t n, F f, int reps = 30) {
    double best = 1e300;
    for (int r = 0; r < reps; ++r) {
        auto t0 = Clock::now();
        auto v = f();
        auto t1 = Clock::now();
        keep(v);
        best = std::min(best, static_cast<double>(ch::duration_cast<ch::nanoseconds>(t1 - t0).count()));
    }
    return best / static_cast<double>(n);
}

int main() {
    constexpr std::size_t N = 1u << 23;                       // 8M
    std::mt19937 rng(5);
    std::vector<std::uint64_t> a(N);
    for (auto& x : a) x = rng();
    std::vector<std::uint8_t> rare(N, 0);
    for (std::size_t i = 0; i < N; ++i) rare[i] = (rng() % 10000 == 0) ? 1 : 0;   // ~1 in 10000

    std::size_t hits = 0;
    for (auto v : rare) hits += v;
    std::printf("cold-path hit rate: %zu / %zu  (~1 in %.0f)\n\n",
                hits, N, static_cast<double>(N) / static_cast<double>(hits ? hits : 1));

    std::printf("A cold INLINE in loop        : %6.3f ns/iter\n", bench(N, [&]{ return run_A(a.data(), rare.data(), N); }));
    std::printf("B cold in plain function     : %6.3f ns/iter\n", bench(N, [&]{ return run_B(a.data(), rare.data(), N); }));
    std::printf("C cold [[gnu::cold]]+unlikely: %6.3f ns/iter\n", bench(N, [&]{ return run_C(a.data(), rare.data(), N); }));
    g_cold_sink = run_C(a.data(), rare.data(), N);            // keep cold code reachable

    std::puts("\nKya seekha:");
    std::puts(" - Is micro-bench mein A/B/C ~BARABAR (~1.49 ns/iter) — Rule 2. Cold");
    std::puts("   body chhota hai aur poora hot loop L1i mein fit ho jaata, to");
    std::puts("   layout se farak nahi pada. Number chhupaya nahi — yahi honest result.");
    std::puts(" - Concept phir bhi sahi: cold body loop mein inline (A) -> hot loop ka");
    std::puts("   code bada + cold instructions beech mein. Plain fn (B) -> compiler");
    std::puts("   decide karta. [[gnu::cold]]+[[gnu::noinline]]+[[unlikely]] (C) ->");
    std::puts("   guaranteed .text.unlikely -> hot loop straight-line + dense.");
    std::puts(" - Asli faayda ek BADE hot path mein jahan I-cache/uop-cache already");
    std::puts("   tight hai (folder 35/11 `perf`: 'Frontend Bound' high). Wahan hot/cold");
    std::puts("   split + PGO (33/11) 10-30% de sakte. `./build.ps1 asm` se dekho:");
    std::puts("   version C mein cold_outlined loop ke code se DOOR hai.");
    return 0;
}
