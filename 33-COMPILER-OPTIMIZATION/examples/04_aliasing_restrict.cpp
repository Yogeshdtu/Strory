// 04_aliasing_restrict.cpp
// ============================================================
// ALIASING = do pointers same memory ko point kar sakte hain. Compiler ko
// yeh worst-case maan'na padta -> ek store ke baad har (possibly-aliased)
// load ko RELOAD karo -> na hoist, na CSE, na vectorize.
//
// Classic: `out[i] = in[i] * (*scale) + (*offset)`. Agar `out` `scale`/
// `offset` ko point kar sakta, to `*scale` aur `*offset` HAR ITERATION
// memory se dobara padhne padte (kya pata `out[i]=` ne unhe badal diya) ->
// 2 extra loads/iter + vectorization block.
//
// `T* __restrict p` = "p ke through likhi memory ko is function mein koi
// doosra pointer nahi chhuta" -> ab compiler `*scale`/`*offset` ek baar
// register mein le ke poora loop vectorize karta.
//
// NOTE: functions `[[gnu::noinline]]` hain -- jaan-boojh kar. Agar inline
// hone dete, to compiler caller (main) ka context dekh ke KHUD prove kar
// leta ki `out` (heap vector) `scale`/`offset` (main ke locals) se alias
// nahi karta -> may-alias version bhi vectorize -> koi farak nahi. Yeh
// khud ek sabak hai: inlining + LTO aliasing ko aksar hal kar dete hain
// (isliye header-only hot code + `-flto`). Standalone/cross-TU functions
// mein `__restrict` chahiye.
//
// Assembly / vec-report:
//   g++ -std=c++20 -O2 -fopt-info-vec 04_aliasing_restrict.cpp
//     -> sirf `affine_restrict` ka loop "vectorized" dikhega
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 04_aliasing_restrict.cpp -o alias && ./alias
// ============================================================

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <vector>

using Clock = std::chrono::steady_clock;

template <class T>
static inline void keep(T& v) { asm volatile("" : "+r,m"(v) : : "memory"); }

constexpr std::size_t N = 1u << 16;
constexpr int REPS = 30000;

// compiler MUST assume `out` may alias `scale` / `offset` / `in`
[[gnu::noinline]] static void affine_alias(float* out, const float* in, const float* scale, const float* offset) {
    for (std::size_t i = 0; i < N; ++i) out[i] = in[i] * (*scale) + (*offset);
}
// promise: no overlap -> *scale/*offset hoisted, loop vectorized
[[gnu::noinline]] static void affine_restrict(float* __restrict out, const float* __restrict in,
                            const float* __restrict scale, const float* __restrict offset) {
    for (std::size_t i = 0; i < N; ++i) out[i] = in[i] * (*scale) + (*offset);
}

// a genuine loop-carried dependency: __restrict helps codegen but CANNOT parallelize
static void blur3(float* x) {
    for (std::size_t i = 1; i + 1 < N; ++i) x[i] = (x[i - 1] + x[i] + x[i + 1]) * (1.f / 3);
}

int main() {
    std::vector<float> in(N), out(N), x(N);
    for (std::size_t i = 0; i < N; ++i) { in[i] = static_cast<float>(i % 13); x[i] = static_cast<float>(i % 5); }
    const float scale = 1.5f, offset = 0.25f;
    const double elems = static_cast<double>(N) * REPS;

    auto run = [&](auto fn, const char* name) {
        double acc = 0;
        auto t0 = Clock::now();
        for (int r = 0; r < REPS; ++r) {
            fn();
            acc += static_cast<double>(out[static_cast<std::size_t>(r) % N]);   // per-rep read -> no hoist
        }
        auto t1 = Clock::now();
        keep(acc);
        const double ns = std::chrono::duration<double>(t1 - t0).count() * 1e9 / elems;
        std::printf("  %-22s : %.3f ns/elem\n", name, ns);
        return ns;
    };

    std::puts("out[i] = in[i] * (*scale) + (*offset)   over 64k floats:\n");
    const double al = run([&]{ affine_alias(out.data(), in.data(), &scale, &offset); },    "may-alias (plain ptr)");
    const double re = run([&]{ affine_restrict(out.data(), in.data(), &scale, &offset); }, "__restrict");
    std::printf("\n  may-alias / __restrict = %.2fx\n", al / re);

    // blur (in-place, genuinely serial)
    double bacc = 0;
    auto t0 = Clock::now();
    for (int r = 0; r < REPS / 4; ++r) { blur3(x.data()); bacc += static_cast<double>(x[N / 2]); }
    auto t1 = Clock::now();
    keep(bacc);
    std::printf("\n  blur3 in-place (real serial dep): %.3f ns/elem\n",
                std::chrono::duration<double>(t1 - t0).count() * 1e9 / (static_cast<double>(N) * (REPS / 4)));

    std::puts("\nKya hua:");
    std::puts(" - may-alias: `out[i] = ...` ke baad compiler nahi jaanta `*scale`");
    std::puts("   badla ya nahi (out scale ko point kar sakta) -> `*scale` aur");
    std::puts("   `*offset` har iteration RELOAD -> 2 extra loads/iter + loop scalar");
    std::puts("   rehta (`mulss`/`addss`).");
    std::puts(" - __restrict: 'out alag memory hai' -> `*scale`/`*offset` ek baar");
    std::puts("   register mein -> loop 4-wide vectorize (`mulps`/`addps`) + unroll");
    std::puts("   -> is box pe measurably tez (SSE2 baseline; `-march=native` -> 8-wide).");
    std::puts(" - `std::vector`s alag hain, par ek noinline/cross-TU function ke");
    std::puts("   ANDAR se compiler ko yeh pata nahi -> `__restrict` chahiye. (Inline");
    std::puts("   hone dete to compiler khud prove kar leta -- isliye ye example");
    std::puts("   `[[gnu::noinline]]` use karta. LTO aksar aliasing hal kar deta.)");
    std::puts("   `g++ -O2 -fopt-info-vec` -> sirf restrict wala loop 'vectorized'.");
    std::puts(" - blur3: `x[i-1]` isi loop ki abhi-likhi value hai -- ASLI dependency.");
    std::puts("   `__restrict` isse 4-wide nahi kar sakta (correctness). Real fix:");
    std::puts("   double-buffer (out != in) ya explicit SIMD scan.");
    std::puts(" - C: `restrict` keyword. C++: `__restrict` / `__restrict__` (GCC/Clang/");
    std::puts("   MSVC `__restrict`) -- non-standard par universal.");
    return 0;
}
