// 05_simd_basics.cpp
// ============================================================
// SIMD = Single Instruction, Multiple Data. Ek `addps` 4 floats ek saath
// jodta (SSE, 128-bit), `vaddps` (AVX) 8, `vaddps` zmm (AVX-512) 16.
// Scalar loop vs SSE (4-wide) vs AVX2 (8-wide) -- same float array sum.
//
// AVX2 functions pe __attribute__((target("avx2"))) hai -> file plain
// -O2 se compile hoti (koi -mavx2 flag nahi chahiye), aur CPUID se runtime
// pe check karke hi call hoti (example 07 jaisa).
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 05_simd_basics.cpp -o simd && ./simd
// ============================================================

#include <cstdint>
#include <cstdio>
#include <chrono>
#include <vector>
#include <cstring>
#include <immintrin.h>
#if defined(__GNUC__)
#  include <cpuid.h>
#endif

using Clock = std::chrono::steady_clock;
static volatile float g_sink;

static bool cpu_has_avx2() {
#if defined(__GNUC__)
    unsigned a, b, c, d;
    if (!__get_cpuid_count(7, 0, &a, &b, &c, &d)) return false;
    return (b & bit_AVX2) != 0;
#else
    return false;
#endif
}

// ---- scalar ----
static float sum_scalar(const float* p, std::size_t n) {
    float s = 0.0f;
    for (std::size_t i = 0; i < n; ++i) s += p[i];
    return s;
}

// ---- SSE: 4 floats/iteration, 4 partial accumulators in one xmm ----
static float sum_sse(const float* p, std::size_t n) {
    __m128 acc = _mm_setzero_ps();
    std::size_t i = 0;
    for (; i + 4 <= n; i += 4)
        acc = _mm_add_ps(acc, _mm_loadu_ps(p + i));
    alignas(16) float tmp[4];
    _mm_store_ps(tmp, acc);
    float s = tmp[0] + tmp[1] + tmp[2] + tmp[3];
    for (; i < n; ++i) s += p[i];
    return s;
}

// ---- AVX2: 8 floats/iteration ----
__attribute__((target("avx2")))
static float sum_avx2(const float* p, std::size_t n) {
    __m256 acc = _mm256_setzero_ps();
    std::size_t i = 0;
    for (; i + 8 <= n; i += 8)
        acc = _mm256_add_ps(acc, _mm256_loadu_ps(p + i));
    alignas(32) float tmp[8];
    _mm256_store_ps(tmp, acc);
    float s = 0.0f;
    for (float v : tmp) s += v;
    for (; i < n; ++i) s += p[i];
    return s;
}

template <class Fn>
static double bench(Fn fn, const float* p, std::size_t n, int reps) {
    float acc = 0.0f;
    auto t0 = Clock::now();
    for (int r = 0; r < reps; ++r) acc += fn(p, n);
    auto t1 = Clock::now();
    g_sink = acc;
    return std::chrono::duration<double>(t1 - t0).count() * 1e9
         / (static_cast<double>(n) * reps);
}

int main() {
    constexpr std::size_t N = 1u << 16;      // 65536 floats = 256 KiB (L2-ish)
    constexpr int         REPS = 30000;

    std::vector<float> data(N);
    for (std::size_t i = 0; i < N; ++i)
        data[i] = static_cast<float>((i % 17) + 1) * 0.5f;

    const bool avx2 = cpu_has_avx2();
    std::printf("float sum over %zu elements (%zu KiB), %d reps.  AVX2 available: %s\n\n",
                N, N * sizeof(float) / 1024, REPS, avx2 ? "yes" : "no");

    const double s_sca = bench(sum_scalar, data.data(), N, REPS);
    const double s_sse = bench(sum_sse,    data.data(), N, REPS);

    std::printf("  scalar (1-wide)  : %6.3f ns/elem\n", s_sca);
    std::printf("  SSE    (4-wide)  : %6.3f ns/elem   (%.2fx)\n", s_sse, s_sca / s_sse);

    if (avx2) {
        const double s_avx = bench(sum_avx2, data.data(), N, REPS);
        std::printf("  AVX2   (8-wide)  : %6.3f ns/elem   (%.2fx)\n", s_avx, s_sca / s_avx);
    } else {
        std::puts("  AVX2   (8-wide)  : skipped (CPU reports no AVX2)");
    }

    std::puts("\nKya hua:");
    std::puts(" - Scalar `s += p[i]` ek FP-add dependency chain -> latency-bound (~3-4");
    std::puts("   cycles/add), aur ek element per add.");
    std::puts(" - SSE: `_mm_add_ps` 4 lanes ek saath, aur acc ek 4-wide vector -> 4");
    std::puts("   independent partial sums -> width 4x + ILP. Practically ~3-4x.");
    std::puts(" - AVX2: 8 lanes -> ~6-8x scalar se (agar memory bandwidth limit na kare;");
    std::puts("   yahan 256 KiB dataset L2 mein fit karta to compute-bound rehta).");
    std::puts(" - Ye reduction hai: har vector-add still ek partial-sum chain hai, isi");
    std::puts("   liye multiple accumulators (yahan lanes) ILP dete hain -- example 02");
    std::puts("   wala hi idea, hardware lanes ke saath.");
    std::puts(" - FP add commutative hai par NOT associative -> compiler `-O2` pe is");
    std::puts("   scalar loop ko khud vectorize NAHI karega (result bit-exact badal sakta).");
    std::puts("   `-ffast-math` ya `#pragma omp simd reduction` ya haath se intrinsics.");
    std::puts(" - `target(\"avx2\")` attribute: sirf us function ke liye AVX2 codegen on;");
    std::puts("   baaki file baseline SSE2. Isi liye CPUID guard ZAROORI -- bina AVX2 wale");
    std::puts("   CPU pe call = #UD (illegal instruction) crash.");
    return 0;
}
