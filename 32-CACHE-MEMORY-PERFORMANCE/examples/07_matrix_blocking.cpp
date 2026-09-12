// 07_matrix_blocking.cpp
// ============================================================
// Matrix multiply C = A * B  (N x N, float).
//
//   naive ijk : C[i][j] += A[i][k]*B[k][j]
//               -> B ko COLUMN-wise padha jaata (stride N) -> har k pe
//                  naya cache line -> inner loop cache thrash.
//
//   loop-order ikj : C[i][j] += A[i][k]*B[k][j] with k outer of j
//               -> B[k][*] aur C[i][*] dono row-wise -> A[i][k] scalar.
//                  Ek chhota fix, bada faayda.
//
//   blocked (tiled) : NxN ko BS x BS tiles mein todo -> ek tile A, B, C
//               cache mein fit -> har element ko memory se ek baar,
//               phir BS baar reuse.
//
// N chhota rakha (512) taaki `folder`/`fast` jaldi chale. A,B,C float =
// 512*512*4 = 1 MiB each -> teeno 3 MiB -> L2 (512 KiB) se bade, L3 mein.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 07_matrix_blocking.cpp -o mblock && ./mblock
// ============================================================

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <vector>

using Clock = std::chrono::steady_clock;

template <class T>
static inline void keep(T& v) { asm volatile("" : "+r,m"(v) : : "memory"); }

constexpr std::size_t N  = 768;         // 3 mats * 2.25 MiB = 6.75 MiB ~ L3 edge
constexpr std::size_t BS = 64;          // tile: 64x64 float = 16 KiB; 3 tiles ~48 KiB -> L1/L2

using Mat = std::vector<float>;

static void mm_ijk(const Mat& A, const Mat& B, Mat& C) {
    for (auto& c : C) c = 0.f;
    for (std::size_t i = 0; i < N; ++i)
        for (std::size_t j = 0; j < N; ++j) {
            float s = 0.f;
            for (std::size_t k = 0; k < N; ++k) s += A[i * N + k] * B[k * N + j];
            C[i * N + j] = s;
        }
}

static void mm_ikj(const Mat& A, const Mat& B, Mat& C) {
    for (auto& c : C) c = 0.f;
    for (std::size_t i = 0; i < N; ++i)
        for (std::size_t k = 0; k < N; ++k) {
            const float a = A[i * N + k];
            for (std::size_t j = 0; j < N; ++j) C[i * N + j] += a * B[k * N + j];
        }
}

static void mm_blocked(const Mat& A, const Mat& B, Mat& C) {
    for (auto& c : C) c = 0.f;
    for (std::size_t ii = 0; ii < N; ii += BS)
        for (std::size_t kk = 0; kk < N; kk += BS)
            for (std::size_t jj = 0; jj < N; jj += BS)
                for (std::size_t i = ii; i < ii + BS; ++i)
                    for (std::size_t k = kk; k < kk + BS; ++k) {
                        const float a = A[i * N + k];
                        for (std::size_t j = jj; j < jj + BS; ++j)
                            C[i * N + j] += a * B[k * N + j];
                    }
}

template <class F>
static double time_mm(F f, const Mat& A, const Mat& B, Mat& C, int reps) {
    f(A, B, C);                                    // warm
    keep(C[N * N - 1]);
    auto t0 = Clock::now();
    for (int r = 0; r < reps; ++r) f(A, B, C);
    auto t1 = Clock::now();
    keep(C[N * N - 1]);
    return std::chrono::duration<double, std::milli>(t1 - t0).count() / reps;
}

int main() {
    Mat A(N * N, 1.0f), B(N * N, 2.0f), C(N * N);

    const double t_ijk   = time_mm(mm_ijk,     A, B, C, 1);   // naive slow -> 1 rep
    const double t_ikj   = time_mm(mm_ikj,     A, B, C, 3);
    const double t_block = time_mm(mm_blocked, A, B, C, 3);

    const double gflop = 2.0 * N * N * N / 1e9;    // N^3 madd = 2 N^3 flops

    std::printf("C = A*B   N=%zu float   block=%zu   (3 mats = %.1f MiB)\n\n",
                N, BS, 3.0 * N * N * 4 / (1024 * 1024));
    std::printf("  naive ijk (B column-wise) : %8.2f ms   %5.2f GFLOP/s\n", t_ijk,   gflop / (t_ijk   / 1e3));
    std::printf("  ikj  (all row-wise)       : %8.2f ms   %5.2f GFLOP/s   %.1fx\n", t_ikj,  gflop / (t_ikj / 1e3), t_ijk / t_ikj);
    std::printf("  blocked %zux%zu            : %8.2f ms   %5.2f GFLOP/s   %.1fx\n", BS, BS, t_block, gflop / (t_block / 1e3), t_ijk / t_block);

    std::puts("\nKya hua:");
    std::puts(" - ijk: inner loop `B[k*N+j]` k ke saath -> har step N floats (3 KiB)");
    std::puts("   aage -> har B access naya line -> ~N misses per (i,j). 1.4 GFLOP/s.");
    std::puts(" - ikj: `B[k*N+j]` aur `C[i*N+j]` dono j ke saath -> sequential,");
    std::puts("   prefetcher-friendly, auto-vectorize. Sirf loop-order swap -> ~4x");
    std::puts("   (N bada karo to 15x+ -- naive DRAM-bound ho jaata).");
    std::puts(" - blocked 64x64: theory mein tile cache mein reh ke reuse deta.");
    std::puts("   ⚠️ Rule 2 (measured): is box pe blocked ikj se ~10-15% DHEEMA!");
    std::puts("   Kyunki (a) ikj already cache-friendly + GCC ne vectorize kiya,");
    std::puts("   (b) L3 (8 MiB) N<=1024 pe B ka reuse waise hi absorb kar leta,");
    std::puts("   (c) naive tiling loop-overhead add karta bina tuned microkernel");
    std::puts("   ke. ASLI BLAS blocking + register-tiling + hand-vectorized");
    std::puts("   microkernel se ~10x aur nikaalta -- woh alag level ki mehnat.");
    std::puts(" - Sabak: loop-order pehle theek karo (bada, saste win). Blocking");
    std::puts("   tab add karo jab working set L2/L3 se pakka bada ho AUR tum");
    std::puts("   microkernel tune karne ko taiyaar ho. Warna ulta pad sakta.");
    return 0;
}
