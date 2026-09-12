// 05_aos_vs_soa.cpp
// ============================================================
// AoS  = Array of Structs : vector<Particle>
// SoA  = Struct of Arrays : alag vector<float> x, y, z, ...
//
// Test 1  scan few fields (x,y,z) over ALL particles, in order.
//         SoA jeetta: x[] dense -> 100% line use + vectorize.
//
// Test 2  update x,y,z from vx,vy,vz over ALL particles, in order.
//         SoA phir jeetta -- 6/8 fields chhune ke baad bhi -- kyunki
//         `x[i] += vx[i]*dt` cleanly vectorize hota; AoS ka 32-B stride
//         vectorizer ko rok deta. (Gap Test 1 jitna hi -- shrink NAHI hua.)
//
// Test 3  RANDOM particle order, sabhi 8 fields padho.
//         Ab AoS jeetta: ek particle = ek 64-B line. SoA mein wahi particle
//         8 alag arrays mein 8 alag random offsets pe = 8 cache lines.
//
// => "SoA hamesha better" GALAT. Access pattern decide karta.
// Ties: lesson 09 (AoS vs SoA deep), lesson 10 (DOD).
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 05_aos_vs_soa.cpp -o aos && ./aos
// ============================================================

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <numeric>
#include <random>
#include <vector>

using Clock = std::chrono::steady_clock;

template <class T>
static inline void keep(T& v) { asm volatile("" : "+r,m"(v) : : "memory"); }

constexpr std::size_t N = 4'000'000;    // ~ 122 MiB AoS -> DRAM-bound
constexpr int         REPS = 20;

struct Particle {
    float x, y, z;
    float vx, vy, vz;
    float mass;
    std::uint32_t id;
};   // 32 bytes -> exactly 2 per 64-B cache line

struct SoA {
    std::vector<float> x, y, z, vx, vy, vz, mass;
    std::vector<std::uint32_t> id;
    explicit SoA(std::size_t n)
        : x(n, 1.f), y(n, 1.f), z(n, 1.f),
          vx(n, .5f), vy(n, .5f), vz(n, .5f), mass(n, 2.f), id(n, 0u) {}
};

int main() {
    std::vector<Particle> aos(N);
    for (std::size_t i = 0; i < N; ++i)
        aos[i] = {1.f, 1.f, 1.f, .5f, .5f, .5f, 2.f, static_cast<std::uint32_t>(i)};
    SoA soa(N);

    std::printf("N=%zu particles   AoS struct=%zu B (%.0f MiB)   REPS=%d\n\n",
                N, sizeof(Particle),
                static_cast<double>(N) * sizeof(Particle) / (1024 * 1024), REPS);

    // ---------- Test 1: sum x+y+z, sequential ----------
    // accumulator `float` -- yeh sirf DCE-defeat checksum hai, precision bemani.
    auto t0 = Clock::now();
    float s1 = 0;
    for (int r = 0; r < REPS; ++r) {
        float s = 0;
        for (std::size_t i = 0; i < N; ++i) s += aos[i].x + aos[i].y + aos[i].z;
        s1 += s;
    }
    auto t1 = Clock::now();
    float s2 = 0;
    for (int r = 0; r < REPS; ++r) {
        float s = 0;
        for (std::size_t i = 0; i < N; ++i) s += soa.x[i] + soa.y[i] + soa.z[i];
        s2 += s;
    }
    auto t2 = Clock::now();
    keep(s1); keep(s2);
    const double a1 = std::chrono::duration<double, std::milli>(t1 - t0).count() / REPS;
    const double b1 = std::chrono::duration<double, std::milli>(t2 - t1).count() / REPS;

    // ---------- Test 2: pos += vel*dt, sequential ----------
    constexpr float dt = 0.01f;
    auto t3 = Clock::now();
    for (int r = 0; r < REPS; ++r)
        for (std::size_t i = 0; i < N; ++i) {
            aos[i].x += aos[i].vx * dt; aos[i].y += aos[i].vy * dt; aos[i].z += aos[i].vz * dt;
        }
    auto t4 = Clock::now();
    for (int r = 0; r < REPS; ++r)
        for (std::size_t i = 0; i < N; ++i) {
            soa.x[i] += soa.vx[i] * dt; soa.y[i] += soa.vy[i] * dt; soa.z[i] += soa.vz[i] * dt;
        }
    auto t5 = Clock::now();
    keep(aos[N - 1].x); keep(soa.x[N - 1]);
    const double a2 = std::chrono::duration<double, std::milli>(t4 - t3).count() / REPS;
    const double b2 = std::chrono::duration<double, std::milli>(t5 - t4).count() / REPS;

    // ---------- Test 3: RANDOM order, touch all 8 fields ----------
    std::vector<std::uint32_t> ord(N);
    std::iota(ord.begin(), ord.end(), 0u);
    std::mt19937 rng(99);
    std::shuffle(ord.begin(), ord.end(), rng);
    constexpr int R3 = 6;

    auto t6 = Clock::now();
    float c1 = 0;
    for (int r = 0; r < R3; ++r) {
        float s = 0;
        for (std::uint32_t k : ord) {
            const Particle& p = aos[k];
            s += p.x + p.y + p.z + p.vx + p.vy + p.vz + p.mass + static_cast<float>(p.id & 1u);
        }
        c1 += s;
    }
    auto t7 = Clock::now();
    float c2 = 0;
    for (int r = 0; r < R3; ++r) {
        float s = 0;
        for (std::uint32_t k : ord) {
            s += soa.x[k] + soa.y[k] + soa.z[k] + soa.vx[k] + soa.vy[k] + soa.vz[k]
               + soa.mass[k] + static_cast<float>(soa.id[k] & 1u);
        }
        c2 += s;
    }
    auto t8 = Clock::now();
    keep(c1); keep(c2);
    const double a3 = std::chrono::duration<double, std::milli>(t7 - t6).count() / R3;
    const double b3 = std::chrono::duration<double, std::milli>(t8 - t7).count() / R3;

    std::puts("Test 1  scan x,y,z sequential  (3/8 fields):");
    std::printf("  AoS %7.2f ms   SoA %7.2f ms   -> SoA %.1fx\n\n", a1, b1, a1 / b1);
    std::puts("Test 2  x,y,z += v*dt sequential  (6/8 fields):");
    std::printf("  AoS %7.2f ms   SoA %7.2f ms   -> SoA %.1fx  (gap ~same as T1: SoA vectorizes, AoS doesn't)\n\n", a2, b2, a2 / b2);
    std::puts("Test 3  RANDOM particle order, all 8 fields:");
    std::printf("  AoS %7.2f ms   SoA %7.2f ms   -> AoS %.1fx  (1 line vs 8 lines per particle)\n", a3, b3, b3 / a3);

    std::puts("\nKya hua:");
    std::puts(" - T1: AoS har particle ka 32-B struct line mein laata, sirf 12 B");
    std::puts("   (x,y,z) use. SoA x[] contiguous -> 100% line + 8-wide SIMD.");
    std::puts(" - T2: 6/8 fields chahiye phir bhi SoA ~utna hi aage -- kyunki");
    std::puts("   `x[i]+=vx[i]*dt` perfect vector loop; AoS ka 32-B stride");
    std::puts("   scatter/gather vectorizer ko rok deta. Gap SHRINK nahi hua.");
    std::puts(" - T3: random order + saare fields -> AoS jeetta. Ek particle =");
    std::puts("   ek 64-B line = 1 miss. SoA: same particle 8 arrays mein, 8");
    std::puts("   random offsets -> 8 alag lines -> 8 misses/particle.");
    std::puts(" - Rule: scan-few-fields-sequential => SoA. Random-access-whole-");
    std::puts("   object => AoS. Beech mein => AoSoA (chhote tiles of SoA).");
    return 0;
}
