// 02_inlining_demo.cpp
// ============================================================
// Inlining = function call ki jagah uska body paste karna. Faayde:
// call/ret overhead gaya, args register-shuffle gaya, aur -- sabse bada --
// caller ke context mein constant-propagation / CSE / vectorization ab
// function ke aar-paar kaam kar sakte.
//
// Teen versions:
//   force_noinline  -> real `call` har iteration
//   normal          -> compiler decide karta (chhota + hot -> inline)
//   force_inline    -> hamesha inline (__attribute__((always_inline)))
//
// Assembly dekho:  ./build.ps1 asm 33-COMPILER-OPTIMIZATION/examples/02_inlining_demo.cpp
//   noinline loop mein `call _Z...` dikhega; inline mein nahi.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 02_inlining_demo.cpp -o inl && ./inl
// ============================================================

#include <chrono>
#include <cstdint>
#include <cstdio>

using Clock = std::chrono::steady_clock;

template <class T>
static inline void keep(T& v) { asm volatile("" : "+r,m"(v) : : "memory"); }

// same tiny body, 3 linkage/inline flavours
__attribute__((noinline))
static std::uint32_t scale_noinline(std::uint32_t x, std::uint32_t k) {
    return x * k + (x >> 3);
}
static std::uint32_t scale_normal(std::uint32_t x, std::uint32_t k) {
    return x * k + (x >> 3);
}
__attribute__((always_inline))
static inline std::uint32_t scale_inline(std::uint32_t x, std::uint32_t k) {
    return x * k + (x >> 3);
}

template <class F>
static double bench(F f, const char* name) {
    constexpr std::uint64_t ITERS = 400'000'000;
    std::uint32_t acc = 1;
    // warm
    for (std::uint64_t i = 0; i < ITERS / 10; ++i) acc = f(acc, 3u) ^ static_cast<std::uint32_t>(i);
    keep(acc);
    auto t0 = Clock::now();
    for (std::uint64_t i = 0; i < ITERS; ++i)
        acc = f(acc, 3u) ^ static_cast<std::uint32_t>(i);   // acc carried -> can't hoist
    auto t1 = Clock::now();
    keep(acc);
    const double ns = std::chrono::duration<double>(t1 - t0).count() * 1e9 / static_cast<double>(ITERS);
    std::printf("  %-14s : %.3f ns/iter   (acc=%u)\n", name, ns, acc);
    return ns;
}

int main() {
    std::puts("tiny body `x*k + (x>>3)` in a 400M carried loop:\n");
    const double n = bench([](std::uint32_t x, std::uint32_t k){ return scale_noinline(x, k); }, "noinline");
    const double d = bench([](std::uint32_t x, std::uint32_t k){ return scale_normal(x, k); }, "normal");
    const double i = bench([](std::uint32_t x, std::uint32_t k){ return scale_inline(x, k); }, "always_inline");

    std::printf("\n  noinline / inline = %.2fx   (call+ret+arg-shuffle overhead per iter)\n", n / i);
    std::printf("  normal == inline ? %s (compiler already inlined the tiny hot body)\n",
                (d < n * 0.8) ? "haan" : "nahi");

    std::puts("\nKya hua:");
    std::puts(" - noinline: har iteration ek `call`, args ko `edi`/`esi` mein daalo,");
    std::puts("   `ret`, return value `eax` se lo. ~2-4 extra uops + a taken branch");
    std::puts("   pair + the compiler can't optimize across the call.");
    std::puts(" - inline: body loop mein paste -> `imul`/`shr`/`add`/`xor` seedha,");
    std::puts("   koi call nahi. Yahi normal (default) bhi karta -- chhota + hot.");
    std::puts(" - `inline` keyword ka asli kaam ODR hai (multiple definitions OK),");
    std::puts("   NOT 'inline karo'. `always_inline` woh force karta; `noinline` roke.");
    std::puts(" - Bada faayda measured overhead nahi -- yeh ki inline hone ke baad");
    std::puts("   constant-fold / CSE / vectorization function boundary ke paar");
    std::puts("   chalte hain (example 03/04). Isliye header-only hot code + LTO.");
    std::puts(" - ⚠️ over-inlining -> I-cache bloat -> bade functions pe ULTA.");
    std::puts("   Compiler heuristics (size, call-count, hotness) mostly theek.");
    return 0;
}
