// 01_optimization_levels.cpp
// ============================================================
// Ek hi code, alag -O levels. Yeh file khud batati hai kis level pe
// compile hui (predefined macros se), aur ek workload time karti hai.
//
// Isse 5 tarah compile karke chalao:
//   for L in O0 O1 O2 O3 Os; do g++ -std=c++20 -$L 01_optimization_levels.cpp -o o_$L && ./o_$L; done
//
// (`./build.ps1 fast` sirf -O2 chalata -- baaki manually.)
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 01_optimization_levels.cpp -o o2 && ./o2
// ============================================================

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

using Clock = std::chrono::steady_clock;

template <class T>
static inline void keep(T& v) { asm volatile("" : "+r,m"(v) : : "memory"); }

// GCC/Clang optimization level ko string mein batao (predefined macros).
static const char* opt_level() {
#if defined(__OPTIMIZE_SIZE__)
    return "-Os (size)";
#elif defined(__OPTIMIZE__)
    // __OPTIMIZE__ any -O1/2/3/fast pe defined; exact level ka macro nahi hota,
    // par -O3-only passes ka pata __GNUC__ se nahi chalta -> hint print.
    return "-O1 / -O2 / -O3 (optimized; __OPTIMIZE__ set)";
#else
    return "-O0 (no optimization)";
#endif
}

// workload: naive matrix-ish reduction with a small inner function
static inline std::int64_t mix(std::int64_t x) {
    return (x * 2654435761LL) ^ (x >> 13);
}

int main() {
    std::printf("compiled with: %s\n", opt_level());
    std::printf("(GCC %d.%d.%d)\n", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);

    constexpr std::size_t N = 4096;
    std::vector<std::int32_t> a(N * N);
    for (std::size_t i = 0; i < a.size(); ++i)
        a[i] = static_cast<std::int32_t>((i * 7 + 3) & 0xFF);

    constexpr int REPS = 3;
    std::int64_t acc = 0;
    auto t0 = Clock::now();
    for (int r = 0; r < REPS; ++r) {
        std::int64_t s = 0;
        for (std::size_t i = 0; i < N; ++i)
            for (std::size_t j = 0; j < N; ++j)
                s += mix(a[i * N + j]) + static_cast<std::int64_t>(j);   // load + call(or inline) + add
        acc += s;
    }
    auto t1 = Clock::now();
    keep(acc);

    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count() / REPS;
    std::printf("workload: %.2f ms/pass   (16.7M inner iters)\n", ms);
    std::printf("checksum: %lld\n\n", static_cast<long long>(acc));

    std::puts("Compare across levels (run the for-loop in the header):");
    std::puts(" -O0 : har statement stack pe round-trips, `mix` ek real `call`,");
    std::puts("       loop counters memory mein. Baseline -- 5-20x dheema.");
    std::puts(" -O1 : registers use, `mix` inline, dead code hata, basic CSE.");
    std::puts(" -O2 : + inlining heuristics, loop opts, if-conversion, SLP,");
    std::puts("       instruction scheduling. Yeh production default.");
    std::puts(" -O3 : + aggressive auto-vectorization, loop interchange/unroll-and-jam,");
    std::puts("       function cloning. Kabhi tez, kabhi code-bloat se ULTA (measure).");
    std::puts(" -Os : -O2 minus jo cheezein code size badhati (unroll, some inline).");
    std::puts("       I-cache-bound code pe kabhi -O2 se tez.");
    std::puts(" -Ofast / -ffast-math : -O3 + IEEE float rules todo (lesson 13) --");
    std::puts("       reproducibility jaati; sirf jaanke.");
    return 0;
}
