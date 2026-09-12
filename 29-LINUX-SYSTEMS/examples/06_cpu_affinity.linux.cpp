// 06_cpu_affinity.linux.cpp
// ============================================================
// Thread ko ek core pe PIN karo (sched_setaffinity / pthread_setaffinity_np)
// aur dikhao ki pinning se jitter (max - min iteration time) kam hota hai.
// ============================================================
//  LINUX-ONLY (sched.h CPU_SET, sched_setaffinity). MinGW pe SetThreadAffinityMask
//  hota hai par API alag; yeh folder Linux hai.
//  Linux / WSL pe:
//      g++ -std=c++20 -O2 -Wall -Wextra -pthread 06_cpu_affinity.linux.cpp -o affinity
//      ./affinity
//      # behtar: core 3 ko isolate karke -> boot: isolcpus=3 nohz_full=3
//      taskset -c 3 ./affinity
// ============================================================

#define _GNU_SOURCE
#include <sched.h>
#include <pthread.h>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <thread>
#include <vector>
#include <algorithm>

static std::uint64_t now_ns() {
    timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<std::uint64_t>(ts.tv_sec) * 1'000'000'000ull + static_cast<std::uint64_t>(ts.tv_nsec);
}

static bool pin_to(int cpu) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    return pthread_setaffinity_np(pthread_self(), sizeof(set), &set) == 0;
}

// Ek fixed chunk of work -- har baar barabar hona chahiye. Jitna jitter,
// utna OS ne beech me kuch aur schedule kiya / migrate kiya / cache thanda hua.
static std::uint64_t fixed_work(std::uint64_t seed) {
    std::uint64_t x = seed;
    for (int i = 0; i < 20'000; ++i) x = x * 6364136223846793005ull + 1442695040888963407ull;
    return x;
}

static void measure(const char* label, int pin_cpu) {
    if (pin_cpu >= 0) {
        if (!pin_to(pin_cpu)) { std::printf("  [%s] pin fail (core %d available?)\n", label, pin_cpu); return; }
    }
    constexpr int kRuns = 4000;
    std::vector<std::uint64_t> dt(kRuns);
    std::uint64_t sink = 1;
    for (int i = 0; i < kRuns; ++i) {
        std::uint64_t t0 = now_ns();
        sink ^= fixed_work(sink + static_cast<std::uint64_t>(i));
        dt[static_cast<size_t>(i)] = now_ns() - t0;
    }
    std::sort(dt.begin(), dt.end());
    auto ns = [&](double q) { return dt[static_cast<size_t>(q * (kRuns - 1))]; };
    std::printf("  [%-16s] min=%llu  p50=%llu  p99=%llu  p999=%llu  max=%llu ns   (sink=%llu)\n",
                label,
                (unsigned long long)dt.front(), (unsigned long long)ns(0.50),
                (unsigned long long)ns(0.99), (unsigned long long)ns(0.999),
                (unsigned long long)dt.back(), (unsigned long long)sink);
}

int main() {
    unsigned ncpu = std::thread::hardware_concurrency();
    std::printf("cores available: %u\n\n", ncpu);

    std::puts("Same fixed work, teen tarah:");
    measure("unpinned", -1);
    if (ncpu >= 2) measure("pinned core 1", 1);
    if (ncpu >= 4) measure("pinned core 3", 3);

    // Current affinity mask print karo.
    cpu_set_t cur; CPU_ZERO(&cur);
    if (sched_getaffinity(0, sizeof(cur), &cur) == 0) {
        std::printf("\nmain thread ab in cores pe chal sakta: ");
        for (int c = 0; c < CPU_SETSIZE; ++c) if (CPU_ISSET(c, &cur)) std::printf("%d ", c);
        std::printf("\n");
    }

    std::puts(
        "\nKya seekha:\n"
        "  - Pinning se min/p50 aksar thoda hi badalta -- asli faayda TAIL me:\n"
        "    p99/p999/max girte hain kyunki thread migrate nahi hota (L1/L2\n"
        "    cache warm rehti) aur scheduler use idhar-udhar nahi phenkta.\n"
        "  - Sabse bada faayda tab jab woh core OS aur IRQ se ISOLATED ho\n"
        "    (isolcpus=, nohz_full=, irq affinity dusre core pe). Bina isolation\n"
        "    ke pinning ~30-60% jitter kam karti; isolation ke saath ~10x.\n"
        "  - HFT: har critical thread (feed, strategy, gateway) apne dedicated,\n"
        "    isolated core pe. hyperthread sibling bhi khali chhodo.");

    std::printf("\n[NOTE] Ye numbers machine, kernel, aur -- sabse zyada -- core\n"
                "isolation setup pe depend karte. Shared laptop pe tail bada hi rahega.\n");
    return 0;
}

/* ============================================================
 * EXPECTED OUTPUT (typical Linux x86-64, NON-isolated box) -- NAHI napa gaya
 * ------------------------------------------------------------
 * cores available: 8
 *
 * Same fixed work, teen tarah:
 *   [unpinned        ] min=8100  p50=8300  p99=15200  p999=41000  max=210000 ns
 *   [pinned core 1    ] min=8050  p50=8150  p99=9600   p999=12800  max=64000 ns
 *   [pinned core 3    ] min=8040  p50=8120  p99=9200   p999=11500  max=39000 ns
 *
 * min/p50 ~same; p99..max pinned me kaafi tight. isolcpus=3 nohz_full=3 wale
 * core pe max ~9000-10000 ns tak gir sakta (jitter ~10x kam).
 * ============================================================ */
