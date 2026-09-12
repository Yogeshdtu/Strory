// 02_pipeline_stall.cpp
// ============================================================
// Modern CPU out-of-order hai -- woh independent instructions ko parallel
// chala sakta (ILP). Par ek LONG DEPENDENCY CHAIN usse rok deta:
// har op ko pichhle ka result chahiye -> koi parallelism nahi.
//
// Same total work, 3 tarah:
//   (A) ek serial chain     -> latency-bound (worst)
//   (B) 4 independent chains -> OoO engine 4 ko overlap karta (~4x)
//   (C) 8 independent chains -> aur ILP (ports saturate hone tak)
// keep() barriers taaki compiler chains ko merge/DCE na kare (Rule 2).
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 02_pipeline_stall.cpp -o pstall && ./pstall
// ============================================================

#include <cstdint>
#include <cstdio>
#include <chrono>

using Clock = std::chrono::steady_clock;
static constexpr std::uint64_t N = 400'000'000;
static volatile std::uint64_t g_sink;

template <class T>
static inline void keep(T& v) { asm volatile("" : "+r"(v) : : ); }

// ek "step": multiply-add. imul ki latency ~3 cyc -> serial chain dikhega.
static inline std::uint64_t step(std::uint64_t x, std::uint64_t k) {
    return x * 6364136223846793005ull + k + 1;
}

static double serial_1() {
    std::uint64_t a = 1;
    auto t0 = Clock::now();
    for (std::uint64_t i = 0; i < N; ++i) { a = step(a, i); keep(a); }
    auto t1 = Clock::now();
    g_sink = a;
    return std::chrono::duration<double>(t1 - t0).count() * 1e9 / static_cast<double>(N);
}

static double parallel_4() {
    std::uint64_t a = 1, b = 2, c = 3, d = 4;
    auto t0 = Clock::now();
    for (std::uint64_t i = 0; i < N / 4; ++i) {
        a = step(a, i); b = step(b, i); c = step(c, i); d = step(d, i);
        keep(a); keep(b); keep(c); keep(d);
    }
    auto t1 = Clock::now();
    g_sink = a ^ b ^ c ^ d;
    return std::chrono::duration<double>(t1 - t0).count() * 1e9 / static_cast<double>(N);
}

static double parallel_8() {
    // 8 SCALARS (array nahi -- warna keep() har `v[j]` ko memory se load/store
    // karwa deta aur ILP demo hi toot jata).
    std::uint64_t a=1,b=2,c=3,d=4,e=5,f=6,g=7,h=8;
    auto t0 = Clock::now();
    for (std::uint64_t i = 0; i < N / 8; ++i) {
        a=step(a,i); b=step(b,i); c=step(c,i); d=step(d,i);
        e=step(e,i); f=step(f,i); g=step(g,i); h=step(h,i);
        keep(a);keep(b);keep(c);keep(d);keep(e);keep(f);keep(g);keep(h);
    }
    auto t1 = Clock::now();
    g_sink = a^b^c^d^e^f^g^h;
    return std::chrono::duration<double>(t1 - t0).count() * 1e9 / static_cast<double>(N);
}

int main() {
    std::printf("Same %llu multiply-add ops, different dependency structure:\n", (unsigned long long)N);
    std::printf("(is box ~2 GHz; cycles ~ ns * GHz)\n\n");

    const double s1 = serial_1();
    const double p4 = parallel_4();
    const double p8 = parallel_8();

    std::printf("  (A) 1 serial chain      : %6.3f ns/op   (1.00x baseline)\n", s1);
    std::printf("  (B) 4 parallel chains   : %6.3f ns/op   (%.2fx vs A)\n", p4, s1 / p4);
    std::printf("  (C) 8 parallel chains   : %6.3f ns/op   (%.2fx vs A)\n", p8, s1 / p8);

    std::puts("\nKya hua:");
    std::puts(" - (A): har `a = step(a,i)` ko pichhla `a` chahiye. `imul` latency ~3 cyc");
    std::puts("   -> chain ~3 cyc/op, chahe 4 ALU ports free hon. Reorder buffer bhara,");
    std::puts("   par execute kuch nahi ho raha -- STALL.");
    std::puts(" - (B): 4 alag accumulators = 4 independent chains. OoO engine unki ops ko");
    std::puts("   interleave karke multiplier port ko har cycle busy rakhta -> ~3-4x.");
    std::puts(" - (C): 8 chains -> aur ILP, par ab multiplier port (1 mul/cycle start) ya");
    std::puts("   decode/retire bandwidth limit ban jata -> B se thoda hi behtar ya same.");
    std::puts(" - HFT sabak: ek lambi serial reduction (sum, hash, checksum) ko multiple");
    std::puts("   partial accumulators mein todo, phir end mein combine. Compiler yeh FP");
    std::puts("   reduction ke liye -O2 pe NAHI karta (associativity) -- haath se, ya");
    std::puts("   `-ffast-math` (risky). Integer reduction compiler khud unroll karta.");
    std::puts(" - Yeh wahi idea hai jo SIMD ke lanes dete hain (example 05) -- multiple");
    std::puts("   independent accumulators, hardware ya software.");
    return 0;
}
