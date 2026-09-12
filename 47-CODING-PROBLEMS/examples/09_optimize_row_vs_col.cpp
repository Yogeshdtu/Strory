// 09_optimize_row_vs_col.cpp
// ============================================================
// Folder 47 file 09 #1: the same sum, two loop orders. Row-major traversal
// hits stride-1 (8 longs per cache line); column-major wastes 7/8 of every
// line it touches. Correctness asserted; timing printed for you to read.
// ============================================================
//   BENCHMARK (real numbers): ./build.ps1 fast 09_optimize_row_vs_col.cpp
//   -O0 pe benchmark BEKAAR hai — folder/checkall sirf compile check karte.
// ============================================================

#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

static std::int64_t sum_row_major(const std::vector<std::int64_t>& m, std::size_t n) {
    std::int64_t s = 0;
    for (std::size_t r = 0; r < n; ++r)
        for (std::size_t c = 0; c < n; ++c)
            s += m[r * n + c];                  // stride 1 — cache-friendly
    return s;
}

static std::int64_t sum_col_major(const std::vector<std::int64_t>& m, std::size_t n) {
    std::int64_t s = 0;
    for (std::size_t c = 0; c < n; ++c)
        for (std::size_t r = 0; r < n; ++r)
            s += m[r * n + c];                  // stride n — new cache line every access
    return s;
}

template <class F>
static double time_ms(F&& f) {
    const auto t0 = std::chrono::steady_clock::now();
    f();
    const auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

int main() {
    constexpr std::size_t n = 4096;             // 4096*4096*8 B = 128 MiB >> LLC
    std::vector<std::int64_t> m(n * n);
    for (std::size_t i = 0; i < m.size(); ++i)
        m[i] = static_cast<std::int64_t>(i & 63);

    // correctness: both orders must produce the identical sum
    volatile std::int64_t rm = 0, cm = 0;
    double t_row = 0.0, t_col = 0.0;
    for (int rep = 0; rep < 3; ++rep) {
        t_row = time_ms([&] { rm = sum_row_major(m, n); });
        t_col = time_ms([&] { cm = sum_col_major(m, n); });
    }
    assert(rm == cm);

    const double ratio = t_row > 0.0 ? t_col / t_row : 0.0;
    std::printf("row-major : %8.2f ms\n", t_row);
    std::printf("col-major : %8.2f ms\n", t_col);
    std::printf("col / row : %6.2fx  (measured ~45-70x here at -O2, n=%zu)\n", ratio, n);

    std::puts("09_optimize_row_vs_col: ALL PASS (see timing above)");
    return 0;
}

// ============================================================
// MEASURED on this repo's box (AMD Ryzen 7 4700U, GCC 15, -O2, unpinned):
//   row-major : ~7 - 11 ms
//   col-major : ~480 - 495 ms
//   ratio     : ~45 - 70x   (run-to-run varies; row-major time is the noisy one)
//
// RULE 2 — the number is BIGGER than the "~7x" you might expect. Why:
//   folder 32 file 02's ~7x was ns/line for a pure pointer-chase. Here it
//   stacks THREE effects:
//     1. cache lines: col-major uses 1 of 8 int64 per 64-B line (~8x traffic)
//     2. TLB: col stride = 4096*8 = 32 KiB -> a different page every access ->
//        the 4K-page TLB thrashes (folder 32 file 11)
//     3. row-major ALSO auto-vectorizes (contiguous) — col-major can't
//   Don't round it down to the textbook figure — measure and report what you see.
//
// TALKING POINTS
//   - The fix is a ONE-LINE loop reorder. No SIMD, no intrinsics. Always look
//     at the access pattern before reaching for anything fancy.
//   - Blocking/tiling helps when BOTH indices must be traversed (matmul) —
//     folder 32 file 07.
// ============================================================
