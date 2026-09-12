// 03_const_ref_performance.cpp
// ============================================================
// Pass by VALUE (copy) vs pass by CONST REFERENCE -- measured cost
// ============================================================
//   BENCHMARK -- -O2 ZAROORI:
//     g++ -std=c++20 -O2 03_const_ref_performance.cpp -o crp && ./crp
//     ya:  .\build.ps1 fast 13-REFERENCES/examples/03_const_ref_performance.cpp
//   -O0 pe numbers bekaar (sab dheema, aur inlining/elision off).
// ============================================================
//   by value : har call pe poora object COPY (std::vector -> operator new + memcpy + free)
//   const&   : sirf ek address (8 bytes) pass hota hai -- zero copy
//
//   Dono functions ko __attribute__((noinline)) diya hai -- taaki compiler
//   copy ko "optimize away" na kare. Real code mein aisa tab hota hai jab
//   function doosri translation unit mein defined ho (compiler use dekh nahi
//   sakta) -- to yeh benchmark us situation ko model karta hai.
// ============================================================

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <vector>

using Clock = std::chrono::steady_clock;

// Compiler ko value "use" karwao -- warna loop optimize-away ho jaayega.
static inline void sink(std::uint64_t v) {
    asm volatile("" : : "r"(v) : "memory");
}

// by value -- caller ko har call se pehle ek poora naya vector banana padta hai
__attribute__((noinline))
std::uint64_t peekByValue(std::vector<int> v) {
    return static_cast<std::uint64_t>(v.front()) + static_cast<std::uint64_t>(v.back());
}

// by const ref -- sirf ek pointer, koi allocation nahi
__attribute__((noinline))
std::uint64_t peekByConstRef(const std::vector<int>& v) {
    return static_cast<std::uint64_t>(v.front()) + static_cast<std::uint64_t>(v.back());
}

int main() {
    const int  N     = 4096;      // vector size (ints)
    const long ITERS  = 300000;   // calls per test

    std::vector<int> data(static_cast<std::size_t>(N));
    std::iota(data.begin(), data.end(), 1);

    // ---------- by value ----------
    std::uint64_t acc1 = 0;
    auto t0 = Clock::now();
    for (long i = 0; i < ITERS; ++i) {
        asm volatile("" : : "r"(data.data()) : "memory");   // contents opaque -> copy hoke rahega
        acc1 += peekByValue(data);
    }
    auto t1 = Clock::now();
    sink(acc1);

    // ---------- by const ref ----------
    std::uint64_t acc2 = 0;
    auto t2 = Clock::now();
    for (long i = 0; i < ITERS; ++i) {
        asm volatile("" : : "r"(data.data()) : "memory");
        acc2 += peekByConstRef(data);
    }
    auto t3 = Clock::now();
    sink(acc2);

    auto ms = [](Clock::time_point a, Clock::time_point b) {
        return std::chrono::duration<double, std::milli>(b - a).count();
    };
    const double vms = ms(t0, t1);
    const double rms = ms(t2, t3);
    const double denom = static_cast<double>(ITERS);

    std::cout << "vector size    : " << N << " ints ("
              << static_cast<std::size_t>(N) * sizeof(int) << " bytes)\n";
    std::cout << "calls per test : " << ITERS << "\n\n";
    std::cout << "by value  (copy)  : " << vms << " ms   ("
              << (vms * 1e6 / denom) << " ns/call)\n";
    std::cout << "by const& (alias) : " << rms << " ms   ("
              << (rms * 1e6 / denom) << " ns/call)\n\n";
    std::cout << "speedup (value / ref) : " << (vms / rms) << "x\n";
    std::cout << "checksums match       : " << (acc1 == acc2 ? "yes" : "NO") << "\n";

    std::cout <<
        "\n"
        "  Har by-value call = 1 operator new(" << (static_cast<std::size_t>(N) * sizeof(int))
        << ") + memcpy + free.\n"
        "  Function body trivial hai (2 elements padho) -- to poora farq copy ka hai.\n"
        "  Isliye bade objects HAMESHA 'const T&' se pass karo (jab tak copy zaroori na ho).\n";
    return 0;
}
