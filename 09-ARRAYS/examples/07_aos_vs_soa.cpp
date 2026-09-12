// 07_aos_vs_soa.cpp
// ============================================================
// Array-of-Structs vs Struct-of-Arrays -- data layout aur cache (measured)
// ============================================================
//   BENCHMARK -> -O2 ZAROORI.
//   g++ -std=c++20 -O2 07_aos_vs_soa.cpp -o aos && ./aos
// ============================================================
// Ek "entity" mein 6 fields. 10 lakh entities.
//
//   AoS : struct Entity { x, y, z, vx, vy, vz };  vector<Entity>
//         -> memory: [x0 y0 z0 vx0 vy0 vz0][x1 y1 z1 ...] ...
//         -> sirf `x` chahiye? har cache line se 1/6 kaam ka data.
//
//   SoA : struct { vector<int> x, y, z, vx, vy, vz; }
//         -> memory: [x0 x1 x2 ...] [y0 y1 y2 ...] ...
//         -> sirf `x` chahiye? har cache line 16 x-values (pure use).
//
// Field-subset access pe SoA jeet-ta hai. Poora entity chahiye to AoS.
// ============================================================

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>

constexpr std::size_t N = 4'000'000;
constexpr int REPS = 40;

struct Entity {                       // 6 * 4 = 24 bytes
    std::int32_t x, y, z, vx, vy, vz;
};

int main() {
    // ---- AoS ----
    std::vector<Entity> aos(N);
    for (std::size_t i = 0; i < N; ++i) {
        auto v = static_cast<std::int32_t>(i & 0xFF);
        aos[i] = {v, v, v, 1, 1, 1};
    }

    // ---- SoA ----
    struct SoA {
        std::vector<std::int32_t> x, y, z, vx, vy, vz;
    } soa;
    soa.x.resize(N);  soa.y.resize(N);  soa.z.resize(N);
    soa.vx.assign(N, 1);  soa.vy.assign(N, 1);  soa.vz.assign(N, 1);
    for (std::size_t i = 0; i < N; ++i)
        soa.x[i] = soa.y[i] = soa.z[i] = static_cast<std::int32_t>(i & 0xFF);

    auto ms = [](auto a, auto b) {
        return std::chrono::duration<double, std::milli>(b - a).count();
    };

    // ============================================================
    //  TASK A: sirf `x` field ko touch karo  (1 field of 6) -- RMW
    //  (reduction NAHI -- taaki widening-reduction vectorization ka
    //   confound na aaye; sirf memory access pattern isolate ho)
    // ============================================================
    auto t0 = std::chrono::steady_clock::now();
    for (int r = 0; r < REPS; ++r)
        for (Entity& e : aos) e.x = e.x * 3 + 1;        // stride 24 B -- har cache line ka 4/64 use
    auto t1 = std::chrono::steady_clock::now();

    for (int r = 0; r < REPS; ++r)
        for (std::int32_t& v : soa.x) v = v * 3 + 1;    // contiguous -- har cache line ka 64/64 use
    auto t2 = std::chrono::steady_clock::now();

    std::cout << std::fixed << std::setprecision(1);
    std::cout << "N = " << N << ", REPS = " << REPS << "\n\n";
    std::cout << "TASK A -- touch ONE field (x = x*3 + 1):\n";
    std::cout << "  AoS (vector<Entity>, stride 24 B) : " << std::setw(8) << ms(t0, t1) << " ms\n";
    std::cout << "  SoA (vector<int32>, contiguous)   : " << std::setw(8) << ms(t1, t2) << " ms\n";
    if (ms(t1, t2) > 0.0)
        std::cout << "  SoA " << std::setprecision(2) << (ms(t0, t1) / ms(t1, t2))
                  << "x faster  (5/6 of every AoS cache line was wasted)"
                  << std::setprecision(1) << "\n\n";

    // ============================================================
    //  TASK B: x += vx  (2 fields of 6) -- update
    // ============================================================
    auto t3 = std::chrono::steady_clock::now();
    for (int r = 0; r < REPS; ++r)
        for (Entity& e : aos) e.x += e.vx;
    auto t4 = std::chrono::steady_clock::now();
    for (int r = 0; r < REPS; ++r)
        for (std::size_t i = 0; i < N; ++i) soa.x[i] += soa.vx[i];
    auto t5 = std::chrono::steady_clock::now();

    std::cout << "TASK B -- x += vx (2 of 6 fields):\n";
    std::cout << "  AoS : " << std::setw(8) << ms(t3, t4) << " ms\n";
    std::cout << "  SoA : " << std::setw(8) << ms(t4, t5) << " ms\n";

    // correctness: dono layouts pe same transforms -> aos[k].x == soa.x[k]
    bool ok = true;
    for (std::size_t k = 0; k < N; k += N / 17 + 1)
        if (aos[k].x != soa.x[k]) ok = false;
    std::cout << "  (AoS vs SoA results " << (ok ? "match" : "DIFFER") << ")\n\n";

    std::cout <<
        "  * Field-subset access (analytics, SIMD): SoA jeet-ta hai -- har\n"
        "    byte jo cache mein aata hai woh kaam ka hota hai.\n"
        "  * Poora entity ek saath chahiye (e.g. 'move entity i'): AoS behtar\n"
        "    -- ek entity ke saare fields ek cache line mein.\n"
        "  * SoA + SIMD auto-vectorize aasan hota hai (contiguous same-type).\n"
        "  * HFT: order book = SoA-style price-level arrays; market-data decode\n"
        "    = fields ko alag arrays mein. Folders 11, 32, 36, 39.\n";

    return 0;
}
