// 06_autovectorization.cpp
// ============================================================
// Compiler khud SIMD laga sakta hai (`-O2`+ pe `-ftree-vectorize`), PAR
// sirf jab loop "vectorizable" ho: countable trip count, koi loop-carried
// dependency nahi, no aliasing, simple control flow.
//
// SAME map function do baar: ek default (vectorized), ek
// `optimize("no-tree-vectorize")` (scalar) -> seedhi A/B comparison.
// Phir: prefix-sum (dependency -> can't) aur no-restrict (aliasing -> can't).
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 06_autovectorization.cpp -o autovec && ./autovec
//   kya vectorize hua:
//     g++ -std=c++20 -O2 -fopt-info-vec-optimized 06_autovectorization.cpp -c -o nul
// ============================================================

#include <cstdint>
#include <cstdio>
#include <chrono>
#include <vector>

using Clock = std::chrono::steady_clock;
static volatile std::int64_t g_sink;

static constexpr std::size_t N = 4096;         // 16 KiB ints -> L1-resident
static constexpr int         REPS = 300000;

// per-element: 6 int ops -> SIMD 4-wide se ~3-4x kam instructions
static inline int work(int a) {
    return ((a * 3 + 1) ^ (a >> 1)) + (a & 0x5555) - (a << 2);
}

// A-vec: default -> auto-vectorized (SSE2, 4 ints/instr)
static void map_vec(int* __restrict out, const int* __restrict in, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) out[i] = work(in[i]);
}

// A-scalar: SAME code, vectorizer explicitly off
__attribute__((optimize("no-tree-vectorize")))
static void map_scalar(int* __restrict out, const int* __restrict in, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) out[i] = work(in[i]);
}

// C: loop-carried dependency (prefix sum) -- out[i] needs out[i-1]
static void scan_dep(int* __restrict out, const int* __restrict in, std::size_t n) {
    int acc = 0;
    for (std::size_t i = 0; i < n; ++i) { acc += in[i]; out[i] = acc; }
}

// D: pointers NOT __restrict -> compiler must assume out/in may overlap
static void map_norestrict(int* out, const int* in, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) out[i] = work(in[i]);
}

template <class Fn>
static double bench(Fn fn, int* out, const int* in) {
    auto t0 = Clock::now();
    for (int r = 0; r < REPS; ++r) fn(out, in, N);
    auto t1 = Clock::now();
    std::int64_t s = 0; for (std::size_t i = 0; i < N; ++i) s += out[i];
    g_sink = s;
    return std::chrono::duration<double>(t1 - t0).count() * 1e9
         / (static_cast<double>(N) * REPS);
}

int main() {
    std::vector<int> in(N), out(N);
    for (std::size_t i = 0; i < N; ++i) in[i] = static_cast<int>((i * 2654435761u) & 0xFFFFu);

    const double vec = bench(map_vec,        out.data(), in.data());
    const double sca = bench(map_scalar,     out.data(), in.data());
    const double dep = bench(scan_dep,       out.data(), in.data());
    const double nor = bench(map_norestrict, out.data(), in.data());

    std::printf("per-element time (ns), %zu ints x %d reps, ~6 int ops/elem:\n\n", N, REPS);
    std::printf("  map  auto-vectorized  (restrict)     : %6.3f ns\n", vec);
    std::printf("  map  scalar (SAME code, vec OFF)      : %6.3f ns   -> vectorized %.2fx faster\n",
                sca, sca / vec);
    std::printf("  prefix-sum (loop-carried dependency)  : %6.3f ns   -> NOT vectorizable\n", dep);
    std::printf("  map  no __restrict (possible alias)   : %6.3f ns   -> %.2fx vs vectorized\n",
                nor, nor / vec);

    std::puts("\nKya seekha:");
    std::puts(" - map_vec vs map_scalar: SAME source. vec version `-O2` pe SSE2 se");
    std::puts("   vectorize hui (`movdqu` + `pmulld`/`paddd`/`pxor`, 4 ints/instr) ->");
    std::puts("   ~2-4x. `-fopt-info-vec-optimized` se confirm.");
    std::puts(" - prefix-sum nahi hoti: `acc += in[i]` loop-carried dependency hai");
    std::puts("   (out[i] ko out[i-1] chahiye). Vectorized prefix-sum possible hai par");
    std::puts("   compiler by default nahi karta -- special (log-depth) algorithm.");
    std::puts(" - no __restrict: compiler yeh nahi maan sakta out aur in disjoint hain");
    std::puts("   (`map(x, x+1, n)` legal hai) -> ya runtime alias-check + 2 versions,");
    std::puts("   ya scalar fallback -> vectorized se dheema.");
    std::puts(" - HFT: hot maps/transforms vectorizable rakho -- `__restrict` (ya alag");
    std::puts("   buffers), fixed countable loops, no early break, no data dependency,");
    std::puts("   SoA layout (folder 32). `-march=native` -> wider vectors (AVX2/512).");
    std::puts("   `-fopt-info-vec-missed` batata kyun nahi hua.");
    return 0;
}
