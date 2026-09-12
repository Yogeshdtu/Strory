// 02_stack_vs_heap.cpp
// ============================================================
// Stack allocation vs heap allocation -- speed comparison (measured)
// ============================================================
//   BENCHMARK -- -O2 ZAROORI:
//     g++ -std=c++20 -O2 02_stack_vs_heap.cpp -o svh && ./svh
//     ya:  .\build.ps1 fast 14-MEMORY/examples/02_stack_vs_heap.cpp
// ============================================================
//   Stack "allocation" = bas ek register (rsp) ko move karna -> ~0 cost, koi syscall nahi.
//   Heap allocation    = allocator ka kaam (free list dhoondho, shayad lock, shayad
//                        OS se aur memory maango) -> stack se bahut mehnga.
// ============================================================

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <new>

using Clock = std::chrono::steady_clock;

static void sink(void* p) { asm volatile("" : : "r"(p) : "memory"); }

int main() {
    const long ITERS = 2'000'000;
    const std::size_t N = 64;                 // 64 ints = 256 bytes

    // -------- stack: ek local array, har iteration --------
    std::uint64_t checksum1 = 0;
    auto t0 = Clock::now();
    for (long i = 0; i < ITERS; ++i) {
        int buf[N];                           // "allocation" = rsp -= 256  (compile-time known)
        buf[0] = static_cast<int>(i);
        buf[N - 1] = 7;
        sink(buf);
        checksum1 += static_cast<std::uint64_t>(buf[0] + buf[N - 1]);
    }                                         // "deallocation" = rsp += 256
    auto t1 = Clock::now();

    // -------- heap: new[] + delete[], har iteration --------
    std::uint64_t checksum2 = 0;
    auto t2 = Clock::now();
    for (long i = 0; i < ITERS; ++i) {
        int* buf = new int[N];                // allocator ka kaam
        buf[0] = static_cast<int>(i);
        buf[N - 1] = 7;
        sink(buf);
        checksum2 += static_cast<std::uint64_t>(buf[0] + buf[N - 1]);
        delete[] buf;                         // wapas allocator ko
    }
    auto t3 = Clock::now();

    auto ns = [](Clock::time_point a, Clock::time_point b) {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(b - a).count();
    };
    const double si = static_cast<double>(ns(t0, t1)) / static_cast<double>(ITERS);
    const double hi = static_cast<double>(ns(t2, t3)) / static_cast<double>(ITERS);

    std::cout << "iterations        : " << ITERS << "   (block = " << N * sizeof(int) << " bytes)\n\n";
    std::cout << "stack  per iter   : " << si << " ns\n";
    std::cout << "heap   per iter   : " << hi << " ns   (new[] + delete[])\n\n";
    std::cout << "heap / stack ratio: " << (si > 0 ? hi / si : hi) << "x\n";
    std::cout << "checksums equal   : " << (checksum1 == checksum2 ? "yes" : "NO") << "\n";

    std::cout <<
        "\n"
        "  Stack: size compile-time pata hai -> 'allocate' = ek rsp adjust (aksar free).\n"
        "  Heap : har call allocator ke through -- yahi wajah hai ki hot code heap se\n"
        "         door rehta hai (pre-allocate / pool / arena -- file 08, 10).\n";
    return 0;
}
