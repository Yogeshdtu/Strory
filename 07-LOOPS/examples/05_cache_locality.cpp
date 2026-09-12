// 05_cache_locality.cpp
// ============================================================
// CACHE LOCALITY -- row-major vs column-major traversal (measured)
// ============================================================
//   BENCHMARK -> -O2 ZAROORI. -O0 pe numbers bekaar.
//   g++ -std=c++20 -O2 05_cache_locality.cpp -o cl && ./cl
// ============================================================
// Ek hi 2D data, ek hi total. Sirf traversal ka ORDER alag.
//   - row-major    : memory ke sequence mein chalo -> cache-friendly
//   - column-major : N ints ki chhalang har step -> har access ~cache miss
//
// Farq aksar 5-10x. Kyun? Cache line 64 bytes = 16 ints. Ek miss 16
// consecutive ints le aati hai. Sequential access -> 1 miss / 16 elements.
// Strided access -> 1 miss / 1 element  (+ TLB misses).
// ============================================================

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>

int main() {
    // N x N ints, row-major: element (i, j) -> index i*N + j
    constexpr std::size_t N = 4096;                 // 4096*4096*4 B = 64 MiB (>> L3 cache)
    std::vector<std::int32_t> m(N * N);

    for (std::size_t i = 0; i < N; ++i)
        for (std::size_t j = 0; j < N; ++j)
            m[i * N + j] = static_cast<std::int32_t>((i + j) & 0xF);

    auto ms = [](auto a, auto b) {
        return std::chrono::duration<double, std::milli>(b - a).count();
    };

    constexpr int REPS = 5;
    double tRow = 0.0, tCol = 0.0;
    long long sRow = 0, sCol = 0;

    for (int r = 0; r < REPS; ++r) {
        // -------- ROW-MAJOR: inner loop j -> consecutive addresses --------
        {
            long long sum = 0;
            auto t0 = std::chrono::steady_clock::now();
            for (std::size_t i = 0; i < N; ++i)
                for (std::size_t j = 0; j < N; ++j)
                    sum += m[i * N + j];
            auto t1 = std::chrono::steady_clock::now();
            tRow += ms(t0, t1);
            sRow = sum;
        }

        // -------- COLUMN-MAJOR: inner loop i -> stride N*4 bytes --------
        {
            long long sum = 0;
            auto t0 = std::chrono::steady_clock::now();
            for (std::size_t j = 0; j < N; ++j)
                for (std::size_t i = 0; i < N; ++i)
                    sum += m[i * N + j];
            auto t1 = std::chrono::steady_clock::now();
            tCol += ms(t0, t1);
            sCol = sum;
        }
    }

    std::cout << std::fixed << std::setprecision(1);
    std::cout << "Matrix: " << N << " x " << N << " int32  ("
              << (N * N * sizeof(std::int32_t) / (1024 * 1024)) << " MiB)\n";
    std::cout << "Avg over " << REPS << " reps:\n\n";
    std::cout << "  row-major   (inner j, sequential)      : " << std::setw(8) << (tRow / REPS) << " ms\n";
    std::cout << "  column-major (inner i, stride " << N * sizeof(std::int32_t) << " B) : "
              << std::setw(8) << (tCol / REPS) << " ms\n\n";

    if (tRow > 0.0)
        std::cout << std::setprecision(2)
                  << "  column-major / row-major = " << (tCol / tRow) << "x slower\n";

    std::cout << "  (sums match? " << (sRow == sCol ? "haan" : "NAHI") << ")\n";

    std::cout <<
        "\n"
        "  * Data values same, total same -- sirf ACCESS ORDER badla.\n"
        "  * Row-major: har cache line (16 ints) poori use hoti hai ->\n"
        "    1 cache miss har 16 elements pe. Hardware prefetcher bhi khush.\n"
        "  * Column-major: har line se sirf 1 int use, phir woh line evict ->\n"
        "    ~16x zyada memory traffic + har row alag TLB page.\n"
        "  * Fix: loops ko memory order mein chalao (inner loop = last index).\n"
        "    Agar columns chahiye HI, to data ko column-major store karo\n"
        "    (ek baar transpose), phir har scan fast. Folder 32 (cache) mein poora.\n"
        "\n"
        "  HFT: yahi soch AoS-vs-SoA, struct packing, aur order-book layout\n"
        "  decisions chalati hai -- 'jo saath process hota hai, woh saath rakho'.\n";

    return 0;
}
