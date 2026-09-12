// 05_branch_hints.cpp
// ============================================================
// `[[likely]]` / `[[unlikely]]` (C++20) aur `__builtin_expect(cond, val)`
// (GCC/Clang) compiler ko batate ki kaunsi branch aksar lagti hai.
//
// Yeh **predictor** ko nahi -- HARDWARE predictor already runtime pe seekh
// leta hai. Yeh **code layout** ko affect karta:
//   - hot path straight-line (no taken branch), cold path aage jump
//   - cold path ko function ke end mein / alag section mein -> hot `.text`
//     compact -> better I-cache utilization
//   - compiler cold path pe kam optimize/inline karta (space bachao)
//
// Isliye measured speedup aksar CHHOTA hota jab tak I-cache pressure na
// ho. Asli faayda: big hot loop mein rarely-taken error/slow-path ko
// bahar rakhna.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 05_branch_hints.cpp -o bh && ./bh
//   ./build.ps1 asm 33-COMPILER-OPTIMIZATION/examples/05_branch_hints.cpp
//     (unlikely block loop ke baad, `jmp` se skip; likely path fall-through)
// ============================================================

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

using Clock = std::chrono::steady_clock;

template <class T>
static inline void keep(T& v) { asm volatile("" : "+r,m"(v) : : "memory"); }

[[gnu::noinline]] static std::uint64_t slow_path(std::uint64_t x) {
    // pretend-expensive rare handler
    std::uint64_t h = x;
    for (int k = 0; k < 8; ++k) h = h * 6364136223846793005ULL + 1;
    return h;
}

// hot loop: ~1 in 1000 elements triggers the rare path
static std::uint64_t process_plain(const std::vector<std::uint8_t>& v) {
    std::uint64_t acc = 0;
    for (std::uint8_t b : v) {
        if (b == 0) acc += slow_path(acc);          // rare
        else        acc += b;
    }
    return acc;
}
static std::uint64_t process_hinted(const std::vector<std::uint8_t>& v) {
    std::uint64_t acc = 0;
    for (std::uint8_t b : v) {
        if (b == 0) [[unlikely]] acc += slow_path(acc);
        else        [[likely]]   acc += b;
    }
    return acc;
}
static std::uint64_t process_builtin(const std::vector<std::uint8_t>& v) {
    std::uint64_t acc = 0;
    for (std::uint8_t b : v) {
        if (__builtin_expect(b == 0, 0)) acc += slow_path(acc);
        else                             acc += b;
    }
    return acc;
}

template <class F>
static double bench(F f, const std::vector<std::uint8_t>& v, const char* name) {
    constexpr int REPS = 4000;
    std::uint64_t acc = 0;
    acc += f(v);
    keep(acc);
    auto t0 = Clock::now();
    for (int r = 0; r < REPS; ++r) acc += f(v);
    auto t1 = Clock::now();
    keep(acc);
    const double ns = std::chrono::duration<double>(t1 - t0).count() * 1e9
                    / (static_cast<double>(v.size()) * REPS);
    std::printf("  %-16s : %.4f ns/elem\n", name, ns);
    return ns;
}

int main() {
    std::vector<std::uint8_t> v(1u << 16);
    std::uint64_t rng = 12345;
    for (auto& b : v) {
        rng = rng * 6364136223846793005ULL + 1;
        std::uint8_t x = static_cast<std::uint8_t>((rng >> 33) % 1000);
        b = (x == 0) ? 0 : static_cast<std::uint8_t>((x % 200) + 1);   // ~1/1000 zeros
    }

    std::puts("hot loop, rare (~1/1000) slow path:\n");
    const double p = bench(process_plain,   v, "no hint");
    const double h = bench(process_hinted,  v, "[[likely/unlikely]]");
    const double b = bench(process_builtin, v, "__builtin_expect");
    std::printf("\n  no-hint / hinted   = %.2fx\n", p / h);
    std::printf("  no-hint / builtin  = %.2fx   (both small -- HW predictor already nails a 1/1000 branch)\n", p / b);

    std::puts("\nKya hua:");
    std::puts(" - Yeh branch ka outcome ~99.9% same hai -> HARDWARE predictor use");
    std::puts("   ~perfectly predict kar leta bina kisi hint ke. Isliye ns farak");
    std::puts("   is micro-bench mein chhota (~noise se ~1.1x).");
    std::puts(" - Hint ka asli kaam CODE LAYOUT hai (assembly dekho): hinted version");
    std::puts("   mein `slow_path` call loop-body se BAHAR nikal jaata (cold section),");
    std::puts("   hot path straight-line chalta -> better I-cache density in a BIG");
    std::puts("   function with many such rare checks.");
    std::puts(" - Galat hint (`[[likely]]` on the rare side) ULTA -> hot path pe");
    std::puts("   extra jump + cold code beech mein. Sirf tab lagao jab PAKKA ho.");
    std::puts(" - Behtar: **PGO** (lesson 11) -- compiler asli run se seekhta");
    std::puts("   kaunsi branch/kitni baar, tumhe guess nahi karna padta.");
    std::puts(" - `std::unreachable()` / `__builtin_unreachable()` = strongest hint");
    std::puts("   ('yeh case ho hi nahi sakta') -- galat hua to UB.");
    return 0;
}
