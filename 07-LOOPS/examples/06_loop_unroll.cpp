// 06_loop_unroll.cpp
// ============================================================
// LOOP UNROLLING -- manual vs compiler (measured, aur nateeja SURPRISING hai)
// ============================================================
//   BENCHMARK -> optimization flags MATTER. Yeh SAB chalao aur compare:
//     g++ -std=c++20 -O2                06_loop_unroll.cpp -o lu   && ./lu
//     g++ -std=c++20 -O3                06_loop_unroll.cpp -o lu3  && ./lu3
//     g++ -std=c++20 -O2 -march=native  06_loop_unroll.cpp -o lun  && ./lun
// ============================================================
// Sawaal: naive sum-loop ko haath se 4x unroll karne se tez hota hai?
//
// Jawaab: DEPENDS ON FLAGS -- aur yahi asli sabak hai.
//   * -O2 akela           : manual unroll ~2x TEZ  (compiler ne reduction ka
//                           dependency-chain nahi toda)
//   * -O3  ya  -march=native: naive KHUD tez ho jaata hai; manual unroll ab
//                           BARABAR ya SLOWER (compiler ki vectorization se ladta hai)
//
// Manual unroll ka faayda (jahan hota hai) "kam iterations" se nahi -- balki
// 4 ALAG accumulators se aata hai, jo `s += a[i]` ki loop-carried dependency
// todte hain (har add pichle add ka wait nahi karta -> ILP).
//
// Yeh bhi "measure karo, assume mat karo" ka example hai.
// ============================================================

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <vector>

#if defined(__GNUC__) || defined(__clang__)
#define NOINLINE __attribute__((noinline))
#else
#define NOINLINE
#endif

// -------- naive: seedha loop --------
NOINLINE std::int64_t sumNaive(const std::int32_t* a, std::size_t n) {
    std::int64_t s = 0;
    for (std::size_t i = 0; i < n; ++i) s += a[i];
    return s;
}

// -------- manual 4x unroll: 4 accumulators (dependency chain todta hai) --------
NOINLINE std::int64_t sumUnroll4(const std::int32_t* a, std::size_t n) {
    std::int64_t s0 = 0, s1 = 0, s2 = 0, s3 = 0;
    std::size_t i = 0;
    const std::size_t limit = n - (n % 4);
    for (; i < limit; i += 4) {
        s0 += a[i + 0];
        s1 += a[i + 1];
        s2 += a[i + 2];
        s3 += a[i + 3];
    }
    for (; i < n; ++i) s0 += a[i];        // bacha hua tail
    return s0 + s1 + s2 + s3;
}

// -------- library: std::accumulate --------
NOINLINE std::int64_t sumStd(const std::int32_t* a, std::size_t n) {
    return std::accumulate(a, a + n, std::int64_t{0});
}

int main() {
    constexpr std::size_t N = 1u << 22;              // ~4.2M int32 = 16 MiB
    constexpr int REPS = 300;

    std::vector<std::int32_t> a(N);
    for (std::size_t i = 0; i < N; ++i)
        a[i] = static_cast<std::int32_t>(i & 0x3F);

    auto ms = [](auto x, auto y) {
        return std::chrono::duration<double, std::milli>(y - x).count();
    };

    auto bench = [&](auto fn) {
        std::int64_t chk = 0;
        auto t0 = std::chrono::steady_clock::now();
        for (int r = 0; r < REPS; ++r) chk += fn(a.data(), a.size());
        auto t1 = std::chrono::steady_clock::now();
        return std::pair<double, std::int64_t>{ms(t0, t1), chk};
    };

    auto [tN, cN] = bench(sumNaive);
    auto [tU, cU] = bench(sumUnroll4);
    auto [tS, cS] = bench(sumStd);

    std::cout << std::fixed << std::setprecision(1);
    std::cout << "N = " << N << " int32, REPS = " << REPS << "\n\n";
    std::cout << "  naive  for-loop      : " << std::setw(8) << tN << " ms\n";
    std::cout << "  manual 4x unroll     : " << std::setw(8) << tU << " ms\n";
    std::cout << "  std::accumulate      : " << std::setw(8) << tS << " ms\n\n";

    std::cout << std::setprecision(2);
    const double ratio = (tN > 0.0) ? (tU / tN) : 0.0;
    std::cout << "  unroll / naive = " << ratio << "x  ";
    if (ratio < 0.85)      std::cout << "-> manual unroll TEZ (compiler ne reduction chain nahi toda)\n";
    else if (ratio > 1.15) std::cout << "-> manual unroll SLOWER (compiler ki vectorization se ladai)\n";
    else                   std::cout << "-> lagbhag barabar (compiler ne khud kaam kar diya)\n";
    std::cout << "  (checksums match? " << ((cN == cU && cN == cS) ? "haan" : "NAHI") << ")\n";

    std::cout <<
        "\n"
        "  Yeh SAME binary alag flags pe alag kahani sunata hai (measured, GCC 15):\n"
        "    -O2               : naive ~1570 ms , unroll ~680 ms   (unroll 2.3x TEZ)\n"
        "    -O2 -funroll-loops: naive ~1060 ms , unroll ~740 ms\n"
        "    -O3               : naive  ~600 ms , unroll ~725 ms   (unroll ab SLOWER)\n"
        "    -O2 -march=native : naive  ~545 ms , unroll ~530 ms   (barabar)\n"
        "    -O3 -march=native : naive  ~550 ms , unroll ~660 ms   (unroll SLOWER)\n"
        "\n"
        "  * -O2 pe unroll ka faayda 4 accumulators se hai -- `s += a[i]` ki\n"
        "    loop-carried dependency tut-ti hai (add latency-bound se throughput-bound).\n"
        "  * -O3 / -march=native pe compiler KHUD yeh (aur SIMD) kar deta hai,\n"
        "    aur manual unroll usse constrain karke SLOW kar deta hai.\n"
        "  * std::accumulate hamesha naive jaisa -- alag codegen nahi.\n"
        "  * -O0 pe sab slow, comparison meaningless.\n"
        "\n"
        "  RULE: hand-unroll last resort hai. Pehle: (1) naive + readable likho,\n"
        "  (2) sahi flags do (-O2/-O3, -march=native), (3) profiler ne bola to\n"
        "  `-S`/godbolt dekho compiler ne kya kiya. Hand-unroll brittle hai --\n"
        "  agli compiler/CPU pe ulta pad sakta hai. Folder 33 (compiler opt), 34 (asm).\n";

    return 0;
}
