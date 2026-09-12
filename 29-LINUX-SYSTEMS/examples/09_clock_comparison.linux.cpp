// 09_clock_comparison.linux.cpp
// ============================================================
// Time kaise padho, aur har tarika kitna mehnga:
//   std::chrono::steady_clock  vs  clock_gettime(CLOCK_MONOTONIC*)  vs  rdtsc.
// Aur: CLOCK_MONOTONIC_COARSE ki cost + resolution trade-off.
// ============================================================
//  LINUX-ONLY (clock_gettime CLOCK_* macros, vDSO). rdtsc x86-64.
//  Linux / WSL pe:
//      g++ -std=c++20 -O2 -Wall -Wextra 09_clock_comparison.linux.cpp -o clocks
//      ./clocks
//      # vDSO confirm: ltrace/strace me clock_gettime dikhna NAHI chahiye
//      strace -c -e trace=clock_gettime ./clocks
// ============================================================

#define _GNU_SOURCE 1   // glibc: expose MAP_POPULATE/ip_mreqn/accept4/recvmmsg/CLOCK_* under strict -std=c++20
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <chrono>

static volatile std::uint64_t g_sink = 0;

static inline std::uint64_t rdtscp_now() {
    std::uint32_t lo, hi, aux;
    __asm__ __volatile__("rdtscp" : "=a"(lo), "=d"(hi), "=c"(aux));
    return (static_cast<std::uint64_t>(hi) << 32) | lo;
}

template <class F>
static void bench(const char* label, std::uint64_t iters, F&& read_time) {
    for (std::uint64_t i = 0; i < 50'000; ++i) g_sink += read_time();     // warm
    timespec a, b;
    clock_gettime(CLOCK_MONOTONIC, &a);
    for (std::uint64_t i = 0; i < iters; ++i) g_sink += read_time();
    clock_gettime(CLOCK_MONOTONIC, &b);
    double ns = static_cast<double>((b.tv_sec - a.tv_sec) * 1'000'000'000L + (b.tv_nsec - a.tv_nsec))
              / static_cast<double>(iters);
    std::printf("  %-38s %7.1f ns/read\n", label, ns);
}

int main() {
    const std::uint64_t N = 5'000'000;
    std::puts("Time-read microbench (kam = behtar):\n");

    bench("clock_gettime(CLOCK_MONOTONIC)", N, [] {
        timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
        return static_cast<std::uint64_t>(t.tv_nsec);
    });
    bench("clock_gettime(CLOCK_MONOTONIC_RAW)", N, [] {
        timespec t; clock_gettime(CLOCK_MONOTONIC_RAW, &t);   // NTP slew se free; kabhi vDSO nahi
        return static_cast<std::uint64_t>(t.tv_nsec);
    });
    bench("clock_gettime(CLOCK_MONOTONIC_COARSE)", N, [] {
        timespec t; clock_gettime(CLOCK_MONOTONIC_COARSE, &t); // ~1ms resolution, sabse sasta
        return static_cast<std::uint64_t>(t.tv_nsec);
    });
    bench("clock_gettime(CLOCK_REALTIME)", N, [] {
        timespec t; clock_gettime(CLOCK_REALTIME, &t);
        return static_cast<std::uint64_t>(t.tv_nsec);
    });
    bench("std::chrono::steady_clock::now()", N, [] {
        return static_cast<std::uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count());
    });
    bench("rdtscp (raw cycle counter)", N, [] { return rdtscp_now(); });

    // Resolution check: do lagatar reads ka minimum non-zero delta.
    auto min_delta = [](auto read) {
        std::uint64_t best = ~0ull;
        for (int i = 0; i < 200000; ++i) {
            std::uint64_t x = read(), y = read();
            if (y > x && y - x < best) best = y - x;
        }
        return best;
    };
    std::uint64_t d_mono = min_delta([] {
        timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
        return static_cast<std::uint64_t>(t.tv_sec) * 1'000'000'000ull + static_cast<std::uint64_t>(t.tv_nsec);
    });
    std::uint64_t d_coarse = min_delta([] {
        timespec t; clock_gettime(CLOCK_MONOTONIC_COARSE, &t);
        return static_cast<std::uint64_t>(t.tv_sec) * 1'000'000'000ull + static_cast<std::uint64_t>(t.tv_nsec);
    });
    std::printf("\n  min observable step  MONOTONIC ~ %llu ns, MONOTONIC_COARSE ~ %llu ns\n",
                (unsigned long long)d_mono, (unsigned long long)d_coarse);

    std::puts(
        "\nKya seekha:\n"
        "  - CLOCK_MONOTONIC / REALTIME / chrono::steady_clock sab vDSO se aate\n"
        "    (Linux x86-64) -> ~15-30 ns, koi kernel trap nahi. HFT hot path pe OK.\n"
        "  - CLOCK_MONOTONIC_RAW aksar vDSO se NAHI -> asli syscall (~250+ ns).\n"
        "    Isse hot path pe mat padho.\n"
        "  - CLOCK_MONOTONIC_COARSE sabse sasta (~5-8 ns) par ~1 ms granular --\n"
        "    'roughly abhi kya time hai' ke liye theek, latency naapne ke liye NAHI.\n"
        "  - rdtscp sabse tez (~8-15 ns) aur finest, par: raw cycles hain (ns\n"
        "    me convert karna padta), core migrate pe TSC skew ho sakta (aaj ke\n"
        "    CPUs pe invariant TSC se kam), aur pipeline serialize karta hai.\n"
        "  - Practical HFT: timestamp ke liye rdtscp + ek calibrated cycles->ns,\n"
        "    ya seedha clock_gettime(CLOCK_MONOTONIC) agar 20 ns budget me fit ho.");

    std::printf("\n[NOTE] Sab TYPICAL numbers hain. `clocksource` (tsc vs hpet vs kvm),\n"
                "kernel version, aur mitigations se badlenge. Khud napo.\n  (sink=%llu)\n",
                (unsigned long long)g_sink);
    return 0;
}

/* ============================================================
 * EXPECTED OUTPUT (typical Linux x86-64, clocksource=tsc) -- NAHI napa gaya
 * ------------------------------------------------------------
 * Time-read microbench (kam = behtar):
 *
 *   clock_gettime(CLOCK_MONOTONIC)             22.1 ns/read
 *   clock_gettime(CLOCK_MONOTONIC_RAW)        268.4 ns/read
 *   clock_gettime(CLOCK_MONOTONIC_COARSE)       6.9 ns/read
 *   clock_gettime(CLOCK_REALTIME)              21.8 ns/read
 *   std::chrono::steady_clock::now()           23.0 ns/read
 *   rdtscp (raw cycle counter)                 11.4 ns/read
 *
 *   min observable step  MONOTONIC ~ 20 ns, MONOTONIC_COARSE ~ 1000000 ns
 *
 * Agar clocksource=hpet ho to CLOCK_MONOTONIC ~500-1000 ns (bahut bura) --
 * `cat /sys/devices/system/clocksource/clocksource0/current_clocksource`.
 * ============================================================ */
