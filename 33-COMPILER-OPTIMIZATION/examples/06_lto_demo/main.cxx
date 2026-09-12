// main.cxx -- calls hot_transform (defined in mathx.cxx) once per array element.
// ============================================================
// Build TWO ways and compare:
//   NO  LTO : g++ -O2 -c main.cxx mathx.cxx ; g++ main.o mathx.o -o demo_nolto
//   WITH LTO: g++ -O2 -flto -c main.cxx mathx.cxx ; g++ -O2 -flto main.o mathx.o -o demo_lto
// (build.ps1 / build.sh yahi karte.)
//
// Loop THROUGHPUT-bound hai (har out[i] independent) -- isliye:
//   NO LTO  : har element pe ek real `call` (opaque; overhead + no fusion/vec)
//   WITH LTO: hot_transform main ke loop mein inline -> call gaya, aur ab
//             5-step hash ko unroll/schedule/(maybe SIMD) kar sakta.
// ============================================================
#include "mathx.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

using Clock = std::chrono::steady_clock;

template <class T>
static inline void keep(T& v) { asm volatile("" : "+r,m"(v) : : "memory"); }

int main() {
    constexpr std::size_t N = 1u << 20;      // 1M elements
    constexpr int REPS = 400;
    std::vector<std::uint32_t> in(N), out(N);
    for (std::size_t i = 0; i < N; ++i) in[i] = static_cast<std::uint32_t>(i * 2654435761u);

    // warm
    for (std::size_t i = 0; i < N; ++i) out[i] = hot_transform(in[i]);
    keep(out[0]);

    std::uint32_t bump = 0;
    auto t0 = Clock::now();
    for (int r = 0; r < REPS; ++r) {
        for (std::size_t i = 0; i < N; ++i) out[i] = hot_transform(in[i] ^ bump);
        bump = out[static_cast<std::size_t>(r) % N];                    // per-rep dep -> loop can't be hoisted
    }
    auto t1 = Clock::now();
    keep(bump); keep(out[N - 1]);

    const double ns = std::chrono::duration<double>(t1 - t0).count() * 1e9
                    / (static_cast<double>(N) * REPS);
#ifdef __LTO_BUILD__
    std::printf("WITH -flto : %.3f ns/elem   (bump=%u)\n", ns, bump);
#else
    std::printf("NO   -flto : %.3f ns/elem   (bump=%u)\n", ns, bump);
#endif
    std::puts("  (throughput loop: without LTO, one opaque `call` per element;");
    std::puts("   with LTO, hot_transform is inlined into main's loop -> the call");
    std::puts("   is gone and the 5-op hash can be unrolled / scheduled / SIMD-ed.)");
    return 0;
}
