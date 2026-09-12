// 01_syscall_cost.linux.cpp
// ============================================================
// Ek syscall ki latency measure karo — raw trap vs glibc wrapper vs
// vDSO call vs pure userspace call. "Har syscall mehngi hai" ko number do.
// ============================================================
//  LINUX-ONLY. Windows/MinGW pe compile NAHI hoti (<sys/syscall.h>, SYS_getpid).
//  Linux / WSL pe:
//      g++ -std=c++20 -O2 -Wall -Wextra 01_syscall_cost.linux.cpp -o syscall_cost
//      ./syscall_cost
//  strace se dekho kitni syscalls hui:
//      strace -c ./syscall_cost
// ============================================================

#define _GNU_SOURCE 1   // glibc: expose MAP_POPULATE/ip_mreqn/accept4/recvmmsg/CLOCK_* under strict -std=c++20
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <unistd.h>
#include <sys/syscall.h>

// TSC padho (x86-64). Sirf rough ns->cycles feel ke liye; asli timing
// clock_gettime(CLOCK_MONOTONIC) se — woh vDSO se aata hai, trap nahi karta.
static inline std::uint64_t rdtscp() {
    std::uint32_t lo, hi, aux;
    __asm__ __volatile__("rdtscp" : "=a"(lo), "=d"(hi), "=c"(aux));
    return (static_cast<std::uint64_t>(hi) << 32) | lo;
}

static inline std::uint64_t now_ns() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);          // vDSO: user space, koi trap nahi
    return static_cast<std::uint64_t>(ts.tv_sec) * 1'000'000'000ull
         + static_cast<std::uint64_t>(ts.tv_nsec);
}

// Compiler isse inline / optimize na kar de.
static volatile int g_sink = 0;

__attribute__((noinline)) int userspace_call() {
    // "syscall jaisa dikhne wala" pure userspace function — baseline.
    return g_sink + 1;
}

template <class F>
static double bench_ns(const char* label, std::uint64_t iters, F&& f) {
    // warm-up
    for (std::uint64_t i = 0; i < 10'000; ++i) f();
    const std::uint64_t t0 = now_ns();
    for (std::uint64_t i = 0; i < iters; ++i) f();
    const std::uint64_t t1 = now_ns();
    const double ns = static_cast<double>(t1 - t0) / static_cast<double>(iters);
    std::printf("  %-34s %8.1f ns/call\n", label, ns);
    return ns;
}

int main() {
    const std::uint64_t N = 2'000'000;

    std::printf("syscall cost microbench  (iters = %llu each)\n",
                static_cast<unsigned long long>(N));
    std::printf("  TSC sample: %llu\n\n",
                static_cast<unsigned long long>(rdtscp()));

    // 1) Pure userspace function call — kuch ns, koi ring transition nahi.
    bench_ns("userspace function call", N, [] { g_sink = userspace_call(); });

    // 2) clock_gettime(CLOCK_MONOTONIC) — Linux ise vDSO se serve karta,
    //    yaani user space mein hi, koi kernel trap nahi. "syscall" hote hue bhi sasta.
    bench_ns("clock_gettime (vDSO, no trap)", N, [] {
        timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); g_sink = static_cast<int>(ts.tv_nsec);
    });

    // 3) getpid() glibc wrapper. Modern glibc (>=2.25) ise cache NAHI karta —
    //    har call ek asli syscall trap hai.
    bench_ns("getpid() glibc wrapper", N, [] { g_sink = ::getpid(); });

    // 4) Raw syscall(SYS_getpid) — wrapper bypass, seedha `syscall` instruction.
    bench_ns("syscall(SYS_getpid) raw trap", N, [] {
        g_sink = static_cast<int>(::syscall(SYS_getpid));
    });

    // 5) getppid() — dusra trivial syscall, cross-check ke liye.
    bench_ns("syscall(SYS_getppid) raw trap", N, [] {
        g_sink = static_cast<int>(::syscall(SYS_getppid));
    });

    std::puts(
        "\nKya seekha:\n"
        "  - userspace call ~1-3 ns. Reference point.\n"
        "  - clock_gettime ~15-30 ns: 'syscall' API hai par vDSO ki wajah se\n"
        "    kernel me trap nahi hota. Isi liye HFT hot path clock_gettime use\n"
        "    kar sakta -- getpid/read/write jaise nahi.\n"
        "  - getpid/raw trap ~250-700 ns: yeh asli user->kernel->user transition\n"
        "    ki cost hai (mode switch, register save, Spectre/Meltdown mitigations\n"
        "    ke saath aur zyada). Yeh 100-300x mehngi hai ek userspace call se.\n"
        "  - Sabak: hot path pe syscalls GINO. Batch karo, pre-open karo,\n"
        "    buffering karo, busy-poll karo -- har trap P99 me dikhta hai.");

    std::printf("\n[NOTE] Yeh numbers TYPICAL Linux x86-64 ke hain (published +\n"
                "author ke boxes). Aapke machine pe kernel version, mitigations\n"
                "(mitigations=off?), CPU aur load se badlenge. Khud chala ke dekho.\n");
    return 0;
}

/* ============================================================
 * EXPECTED OUTPUT (typical Linux x86-64, GCC -O2) -- aapke box pe NAHI napa gaya
 * ------------------------------------------------------------
 * syscall cost microbench  (iters = 2000000 each)
 *   TSC sample: 480183920571234
 *
 *   userspace function call              1.8 ns/call
 *   clock_gettime (vDSO, no trap)       22.4 ns/call
 *   getpid() glibc wrapper             318.6 ns/call
 *   syscall(SYS_getpid) raw trap       305.1 ns/call
 *   syscall(SYS_getppid) raw trap      309.7 ns/call
 *
 * Ratios jo maayne rakhte hain:
 *   syscall trap / userspace call   ~ 150-350x
 *   syscall trap / vDSO call        ~ 12-25x
 * Spectre/Meltdown mitigations ON hone pe trap ~500-900 ns tak ja sakta.
 * `mitigations=off` boot param se ~120-180 ns tak gir sakta (production HFT
 *  isolated boxes pe common).
 * ============================================================ */
