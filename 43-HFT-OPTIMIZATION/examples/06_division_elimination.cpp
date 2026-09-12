// 06_division_elimination.cpp
// ============================================================
// Integer DIVISION x86 pe sabse dheemi common ALU op hai: ~20-30 cycles,
// aur pipeline NAHI hoti (throughput ~= latency). Alternatives:
//
//   A. a / b        -- b RUNTIME, har baar alag -> asli `div` instruction
//   B. a / K        -- K COMPILE-TIME const -> compiler magic-multiply+shift
//   C. a >> s       -- divisor power-of-two -> ek shift
//   D. a * recip    -- b RUNTIME par LOOP-INVARIANT -> ek baar 1/b nikaalo,
//                      phir multiply (reciprocal multiply). Yahan integer
//                      version: (a * R) >> 32, R = ceil(2^32 / b).
//
// >>> MEASUREMENT DISCIPLINE (CLAUDE.md ka warning) <<<
// Har loop body me SIRF wahi ek operation alag ho. Koi chhupi hui `%` ya
// doosri `/` nahi. Accumulator + keep() se dead-code elimination roko.
// Divisor arrays aise banaye ki compiler unhe const na maan le.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 06_division_elimination.cpp -o dv && ./dv
//   asm:  g++ -std=c++20 -O2 -S -masm=intel 06_division_elimination.cpp -o - | c++filt
//         (dekho: A me `div`, B me `mul`+`shr`, C me sirf `shr`)
// ============================================================

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace ch = std::chrono;
using Clock = ch::steady_clock;
template <class T> static inline void keep(T& v) { asm volatile("" : "+r,m"(v) : : "memory"); }

constexpr std::size_t N = 1u << 24;   // 16M

// A: true variable/variable division
[[gnu::noinline]]
static std::uint64_t div_var(const std::uint32_t* a, const std::uint32_t* b) {
    std::uint64_t s = 0;
    for (std::size_t i = 0; i < N; ++i) s += a[i] / b[i];
    return s;
}
// B: divide by a compile-time constant (1000) -> magic multiply
[[gnu::noinline]]
static std::uint64_t div_const(const std::uint32_t* a) {
    std::uint64_t s = 0;
    for (std::size_t i = 0; i < N; ++i) s += a[i] / 1000u;
    return s;
}
// C: power-of-two divisor -> shift
[[gnu::noinline]]
static std::uint64_t div_pow2(const std::uint32_t* a) {
    std::uint64_t s = 0;
    for (std::size_t i = 0; i < N; ++i) s += a[i] >> 10;   // / 1024
    return s;
}
// D: runtime but loop-invariant divisor -> precomputed integer reciprocal
//    q = (a * R) >> 32,  R = floor(2^32 / b) + 1  ("round-up" reciprocal).
//    EXACT for all a < 2^16 and b <= 1024 (error term a*1/2^32 < 2^-16 <
//    1/b, so it can never push a/b across an integer). Numerator ko 16-bit
//    rakhna is trick ki PRECONDITION hai -- warna fixup step chahiye.
[[gnu::noinline]]
static std::uint64_t div_recip(const std::uint32_t* a, std::uint32_t b) {
    const std::uint64_t R = ((static_cast<std::uint64_t>(1) << 32) / b) + 1;
    std::uint64_t s = 0;
    for (std::size_t i = 0; i < N; ++i)
        s += (static_cast<std::uint64_t>(a[i]) * R) >> 32;
    return s;
}

template <class F>
static double bench(F f, int reps = 25) {
    double best = 1e300;
    for (int r = 0; r < reps; ++r) {
        auto t0 = Clock::now();
        auto v = f();
        auto t1 = Clock::now();
        keep(v);
        best = std::min(best, static_cast<double>(ch::duration_cast<ch::nanoseconds>(t1 - t0).count()));
    }
    return best / static_cast<double>(N);
}

int main() {
    std::vector<std::uint32_t> a(N), b(N);
    // deterministic fill; b in [1000,1000] would let compiler cheat if it saw it,
    // so keep b genuinely varied and read from memory.
    std::uint64_t x = 0x1234567;
    for (std::size_t i = 0; i < N; ++i) {
        x = x * 6364136223846793005ULL + 1442695040888963407ULL;
        a[i] = static_cast<std::uint32_t>((x >> 33) & 0xFFFFu);        // 0..65535 (see D)
        b[i] = 1u + static_cast<std::uint32_t>((x >> 20) & 0x3FFu);    // 1..1024
    }
    // verify the reciprocal trick agrees with real division for b = 1000
    {
        std::uint64_t ref = 0, rec = 0;
        const std::uint64_t R = (static_cast<std::uint64_t>(1) << 32) / 1000u + 1;
        for (std::size_t i = 0; i < 100000; ++i) {
            ref += a[i] / 1000u;
            rec += (static_cast<std::uint64_t>(a[i]) * R) >> 32;
        }
        std::printf("reciprocal check (b=1000, 100k vals): real=%llu recip=%llu  %s\n\n",
                    static_cast<unsigned long long>(ref), static_cast<unsigned long long>(rec),
                    (ref == rec) ? "MATCH" : "MISMATCH (range too big for this R)");
    }

    const double A = bench([&] { return div_var  (a.data(), b.data()); });
    const double B = bench([&] { return div_const(a.data()); });
    const double C = bench([&] { return div_pow2 (a.data()); });
    const double D = bench([&] { return div_recip(a.data(), 1000u); });

    std::printf("A  a[i] / b[i]      (var/var  -> `div`)        : %6.3f ns/elem\n", A);
    std::printf("B  a[i] / 1000      (const    -> magic mul)     : %6.3f ns/elem   (%.1fx vs A)\n", B, A / B);
    std::printf("C  a[i] >> 10       (pow2     -> shift)         : %6.3f ns/elem   (%.1fx vs A)\n", C, A / C);
    std::printf("D  (a[i]*R) >> 32   (invariant-> recip mul)     : %6.3f ns/elem   (%.1fx vs A)\n", D, A / D);

    std::puts("\nKya seekha (measured, is box -- ratios quote karo, absolutes nahi):");
    std::puts(" - `div` sabse dheemi. Agar divisor COMPILE-TIME hai -> compiler khud");
    std::puts("   magic-multiply karta, tumhe kuch nahi karna (B). `constexpr` divisor");
    std::puts("   rakho jahan ho sake.");
    std::puts(" - Divisor power-of-two -> shift (C), sabse tez. Ring buffer size,");
    std::puts("   hash bucket count -> power-of-two rakho (& mask, >> shift).");
    std::puts(" - Divisor runtime PAR loop-invariant (e.g. SMA window, tick size) ->");
    std::puts("   ek baar reciprocal nikaalo, phir multiply (D). HFT: per-symbol");
    std::puts("   tick-size ka reciprocal symbol-setup pe cache karo.");
    std::puts(" - Jo NA karo: har technique blindly. `a/b` jahan b sach me har baar");
    std::puts("   alag hai (D nahi lagega) -- `div` hi rehne do, ya algorithm badlo.");
    return 0;
}
