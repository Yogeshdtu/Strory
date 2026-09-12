// 03_matrix_traversal.cpp
// ============================================================
// Row-major storage (`m[i*N + j]`) mein:
//   - i outer, j inner  -> memory ko SEQUENTIAL padha jaata (cache-friendly)
//   - j outer, i inner  -> har step N*4 bytes aage (column) -> har element
//     ~naya cache line + TLB pressure -> cache-hostile
//
// Folder 01 lesson 11 mein "7x" dava kiya tha -- yahan ASLI factor napte.
//
// ⚠️ CLAUDE.md Rule 2: GCC `-ftree-loop-interchange` (sirf `-O3`+ pe ON) col-major
// loop ko khud row-major bana sakta -> farak gayab. Is repo ke `fast`/`folder`
// `-O2` use karte -> interchange OFF -> ASLI access-pattern ka farak dikhta.
// `-O3 -march=native` pe test karo to dono ~barabar mil sakte (= compiler ne
// fix kar diya) -- woh khud ek finding hai. Verify: `build.ps1 asm ...`.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 03_matrix_traversal.cpp -o mtrav && ./mtrav
// ============================================================

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

using Clock = std::chrono::steady_clock;

template <class T>
static inline void keep(T& v) { asm volatile("" : "+r,m"(v) : : "memory"); }

// N x N ints. N=4096 -> 64 MiB -> L3 (8 MiB) se bahut bada.
constexpr std::size_t N = 4096;

static std::int64_t sum_row_major(const std::vector<std::int32_t>& m) {
    std::int64_t s = 0;
    for (std::size_t i = 0; i < N; ++i)
        for (std::size_t j = 0; j < N; ++j)
            s += m[i * N + j];            // stride 1 element -> sequential
    return s;
}

static std::int64_t sum_col_major(const std::vector<std::int32_t>& m) {
    std::int64_t s = 0;
    for (std::size_t j = 0; j < N; ++j)
        for (std::size_t i = 0; i < N; ++i)
            s += m[i * N + j];            // stride N elements -> new line each step
    return s;
}

int main() {
    std::vector<std::int32_t> m(N * N, 1);

    (void)sum_row_major(m); (void)sum_col_major(m);   // warm (both touch all pages)

    auto t0 = Clock::now();
    std::int64_t r = sum_row_major(m);
    auto t1 = Clock::now();
    std::int64_t c = sum_col_major(m);
    auto t2 = Clock::now();
    keep(r); keep(c);

    const double row_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    const double col_ms = std::chrono::duration<double, std::milli>(t2 - t1).count();
    const double elems  = static_cast<double>(N) * N;

    std::printf("matrix %zu x %zu ints  (%.0f MiB)\n\n", N, N, elems * 4 / (1024 * 1024));
    std::printf("  row-major  (i,j):  %8.2f ms   %6.3f ns/element\n", row_ms, row_ms * 1e6 / elems);
    std::printf("  col-major  (j,i):  %8.2f ms   %6.3f ns/element\n", col_ms, col_ms * 1e6 / elems);
    std::printf("\n  col-major / row-major = %.1fx slower\n", col_ms / row_ms);

    std::puts("\nKya hua:");
    std::puts(" - Data row-major hai: m[i][0], m[i][1], ... memory mein lagataar.");
    std::puts(" - row-major loop: har cache line (16 ints) ke 16 accesses -> ~1");
    std::puts("   miss / 16 elements, aur prefetcher aage ki lines kheenchta.");
    std::puts(" - col-major loop: har step N=4096 ints = 16 KiB aage -> har element");
    std::puts("   naya line (miss) AUR naya page har 4 rows (TLB miss). Prefetcher");
    std::puts("   yeh stride bhi kabhi pakadta par 64 MiB working set DRAM-bound.");
    std::puts(" - Fix: loop order match karo storage order se; ya pehle transpose");
    std::puts("   karo (ek baar), phir sequential; ya blocking (example 07).");
    std::puts(" - ⚠️ Rule 2 (measured): is code ko `-O3 -march=native` se compile");
    std::puts("   karo to GCC ka -ftree-loop-interchange col-major nest ko KHUD");
    std::puts("   swap kar deta -> dono ~barabar (~0.25 ns/elem, ratio ~1.0).");
    std::puts("   Yaani sochne se pehle: bad loop order compiler bhi kabhi pakad");
    std::puts("   leta -- par sirf simple, provably-safe nests mein. Bharosa mat");
    std::puts("   karo; access pattern khud theek likho.");
    return 0;
}
