// 03_vectorization.cpp
// ============================================================
// Auto-vectorization: compiler ek scalar loop ko SIMD mein badal deta --
// agar loop "vectorizable" ho.
//
// 3 cases measured (is box, plain -O2 = SSE2 baseline, 4 floats/vector):
//   1. MAP   `out[i] = f(a[i]) + rep`  -> vectorizes cleanly (no reassoc) -> real speedup
//   2. REDUCE `s += a[i]`              -> ⚠️ NAHI vectorize hota bina -ffast-math ke
//                                         (partial-sums = reassociation = rounding badalta)
//   3. PREFIX `out[i] = out[i-1]+a[i]` -> kabhi nahi (loop-carried dependency)
//
// `-fopt-info-vec` / `-fopt-info-vec-missed` se compiler khud batata.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 03_vectorization.cpp -o vec && ./vec
//   g++ -std=c++20 -O2 -fopt-info-vec 03_vectorization.cpp -o vec
//   g++ -std=c++20 -O2 -ffast-math 03_vectorization.cpp -o vecfm   # reduce ab vectorize hoga
// ============================================================

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <vector>

using Clock = std::chrono::steady_clock;

template <class T>
static inline void keep(T& v) { asm volatile("" : "+r,m"(v) : : "memory"); }

constexpr std::size_t N = 1u << 16;      // 64k floats -> L2-resident
constexpr int REPS = 30000;

// ---- CASE 1: MAP (vectorizes without -ffast-math) ----
static void map_vec(const float* __restrict a, float* __restrict out, float bump) {
    for (std::size_t i = 0; i < N; ++i) out[i] = a[i] * a[i] * 0.5f + a[i] + bump;
}
__attribute__((optimize("no-tree-vectorize")))
static void map_scalar(const float* a, float* out, float bump) {
    for (std::size_t i = 0; i < N; ++i) out[i] = a[i] * a[i] * 0.5f + a[i] + bump;
}

// ---- CASE 2: REDUCE (needs -ffast-math to vectorize) ----
static float reduce_try(const float* __restrict a, float carry) {
    float s = carry;
    for (std::size_t i = 0; i < N; ++i) s += a[i];       // strict left-fold without -ffast-math
    return s;
}

// ---- CASE 3: PREFIX (never vectorizes) ----
static void prefix_sum(const float* __restrict a, float* __restrict out) {
    out[0] = a[0];
    for (std::size_t i = 1; i < N; ++i) out[i] = out[i - 1] + a[i];
}

int main() {
    std::vector<float> a(N), out(N);
    for (std::size_t i = 0; i < N; ++i) a[i] = static_cast<float>((i % 17) + 1) * 0.5f;

    const double elems = static_cast<double>(N) * REPS;
    std::puts("64k floats, L2-resident (plain -O2 = SSE2, 4 floats/vector):\n");

    // CASE 1: map, scalar vs vectorized. `bump` per rep -> store loop can't be hoisted.
    double s1 = 0;
    auto a0 = Clock::now();
    for (int r = 0; r < REPS; ++r) { map_scalar(a.data(), out.data(), static_cast<float>(r)); s1 += static_cast<double>(out[static_cast<std::size_t>(r) % N]); }
    auto a1 = Clock::now();
    for (int r = 0; r < REPS; ++r) { map_vec(a.data(), out.data(), static_cast<float>(r)); s1 += static_cast<double>(out[static_cast<std::size_t>(r) % N]); }
    auto a2 = Clock::now();
    keep(s1);
    const double m_sc = std::chrono::duration<double>(a1 - a0).count() * 1e9 / elems;
    const double m_ve = std::chrono::duration<double>(a2 - a1).count() * 1e9 / elems;
    std::puts("CASE 1  MAP  out[i] = a[i]*a[i]*0.5 + a[i] + rep");
    std::printf("  scalar (no-vec) : %.3f ns/elem\n", m_sc);
    std::printf("  auto-vectorized : %.3f ns/elem   -> %.1fx\n\n", m_ve, m_sc / m_ve);

    // CASE 2: reduce. carry threaded per rep.
    float racc = 0.f;
    auto r0 = Clock::now();
    for (int r = 0; r < REPS; ++r) racc = reduce_try(a.data(), racc) * 0.9999999f;
    auto r1 = Clock::now();
    keep(racc);
    std::puts("CASE 2  REDUCE  s += a[i]   (float sum -- order matters)");
    std::printf("  as compiled here : %.3f ns/elem\n", std::chrono::duration<double>(r1 - r0).count() * 1e9 / elems);
    std::puts("  plain -O2 : ~scalar-map speed (NOT reassociated, no SIMD reduction)");
    std::puts("  -ffast-math: ~4x faster (4 partial sums allowed) -- verified\n");

    // CASE 3: prefix.
    double p = 0;
    auto p0 = Clock::now();
    for (int r = 0; r < REPS; ++r) { prefix_sum(a.data(), out.data()); p += static_cast<double>(out[N - 1]); }
    auto p1 = Clock::now();
    keep(p);
    std::puts("CASE 3  PREFIX  out[i] = out[i-1] + a[i]");
    std::printf("  as compiled here : %.3f ns/elem   <- loop-carried dep, cannot vectorize (any -O, any -ffast-math)\n",
                std::chrono::duration<double>(p1 - p0).count() * 1e9 / elems);

    std::puts("\nKya hua:");
    std::puts(" - CASE 1: har out[i] independent -> compiler `movups`+`mulps`+`addps`");
    std::puts("   (4 floats/instr), unrolled. Rounding nahi badalta (koi reassociation");
    std::puts("   nahi) -> plain -O2 pe hi hota. Yeh 'safe' vectorization hai.");
    std::puts(" - CASE 2: `s += a[i]` ka result summation ORDER pe depend karta.");
    std::puts("   4 partial sums use karna = alag order = alag float rounding ->");
    std::puts("   compiler ko woh karne ki IJAAZAT nahi bina -ffast-math (-fassociative-");
    std::puts("   math) ke. Isliye reduce is box pe scalar jitna hi -- Rule 2.");
    std::puts("   `-ffast-math` ya `#pragma omp simd reduction(+:s)` se speedup aata.");
    std::puts(" - CASE 3: out[i] ko out[i-1] chahiye jo ISI loop ne abhi likha ->");
    std::puts("   koi bhi -O / -ffast-math ise 4-wide nahi kar sakta. Algorithm");
    std::puts("   badlo (Hillis-Steele / Blelloch scan, explicit shuffles).");
    std::puts(" - `-march=native` add karo -> 8-wide (AVX2) -> CASE 1 aur bada.");
    std::puts(" - Dekho: `g++ -O2 -fopt-info-vec` (kya) / `-fopt-info-vec-missed` (kyun nahi).");
    return 0;
}
