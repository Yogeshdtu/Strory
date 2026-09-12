// 06_core_pinning.cpp
// ============================================================
// Thread-to-core pinning -- Windows-native API (this dev box is
// Windows/MinGW). The Linux equivalent (`sched_setaffinity` /
// `pthread_setaffinity_np`, `isolcpus`/`nohz_full`) is already built +
// verified in 29-LINUX-SYSTEMS/examples/06_cpu_affinity.linux.cpp and
// 29's production tuning checklist -- this file demonstrates the SAME
// CONCEPT (bind a thread to one logical core, stop the OS from moving
// it) with the API this box actually has, and MEASURES the effect: how
// much scheduling jitter does an unpinned thread suffer vs a pinned one?
// (08-core-pinning-strategy.md)
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 06_core_pinning.cpp -o pinning && ./pinning
// ============================================================

#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <thread>
#include <vector>
#include <x86intrin.h>

static inline std::uint64_t tsc() { _mm_lfence(); auto t = __rdtsc(); _mm_lfence(); return t; }

constexpr std::uint64_t kIters = 20'000'000;

struct JitterResult {
    std::vector<std::uint64_t> deltas;   // ticks between consecutive loop iterations
};

// Busy-spins for kIters iterations, recording the tick-delta between
// consecutive iterations. A thread that never gets preempted/migrated
// shows a tight, near-constant delta; a preemption shows up as one huge
// delta (the OS ran something else on this core/thread for a while).
static JitterResult measure_jitter() {
    JitterResult r;
    r.deltas.reserve(kIters);
    volatile std::uint64_t sink = 0;
    std::uint64_t prev = tsc();
    for (std::uint64_t i = 0; i < kIters; ++i) {
        sink = sink + 1;
        const std::uint64_t now = tsc();
        r.deltas.push_back(now - prev);
        prev = now;
    }
    return r;
}

static void report(const char* name, JitterResult& r) {
    std::sort(r.deltas.begin(), r.deltas.end());
    auto pc = [&](double p) {
        std::size_t i = static_cast<std::size_t>(p / 100.0 * static_cast<double>(r.deltas.size()));
        return r.deltas[std::min(i, r.deltas.size() - 1)];
    };
    // "hiccup" = a delta wildly bigger than the typical per-iteration
    // cost (that typical cost IS the p50 -- so compare against it).
    const std::uint64_t typical = pc(50);
    const std::uint64_t hiccup_threshold = typical * 200 + 2000;   // generous margin
    std::uint64_t hiccups = 0;
    for (auto d : r.deltas) if (d > hiccup_threshold) ++hiccups;

    std::printf("  %-10s : p50 %4llu  p99 %8llu  p99.9 %10llu  max %12llu ticks  |  "
                "hiccups(>%llu ticks) = %llu / %zu\n",
                name,
                static_cast<unsigned long long>(pc(50)),
                static_cast<unsigned long long>(pc(99)),
                static_cast<unsigned long long>(pc(99.9)),
                static_cast<unsigned long long>(r.deltas.back()),
                static_cast<unsigned long long>(hiccup_threshold),
                static_cast<unsigned long long>(hiccups), r.deltas.size());
}

int main() {
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    std::printf("Logical processors on this box: %lu\n", si.dwNumberOfProcessors);
    std::printf("Measuring %llu tight-loop iterations, tick-delta between consecutive\n"
                "iterations -- a large delta = this thread got preempted/moved.\n\n",
                static_cast<unsigned long long>(kIters));

    // ---- UNPINNED: OS free to schedule/move this thread anywhere ----
    JitterResult unpinned;
    {
        std::thread t([&] { unpinned = measure_jitter(); });
        t.join();
    }
    report("unpinned", unpinned);

    // ---- PINNED: locked to one specific logical core ----
    JitterResult pinned;
    {
        std::thread t([&] {
            const DWORD_PTR mask = static_cast<DWORD_PTR>(1) << (si.dwNumberOfProcessors - 1);
            const DWORD_PTR prev_mask = SetThreadAffinityMask(GetCurrentThread(), mask);
            if (prev_mask == 0) {
                std::printf("  (SetThreadAffinityMask FAILED, err=%lu -- measuring unpinned instead)\n",
                            GetLastError());
            }
            pinned = measure_jitter();
        });
        t.join();
    }
    report("pinned", pinned);

    std::puts("\nKya hua:");
    std::puts(" - Yeh REGULAR desktop box hai (koi isolcpus/nohz_full jaisi OS-level core");
    std::puts("   isolation nahi) -- SetThreadAffinityMask sirf 'yeh thread SIRF is core pe");
    std::puts("   chal sakta' guarantee karta, 'is core pe SIRF yeh thread chalega' NAHI.");
    std::puts("   Baaki system (background processes, interrupts) us core ko abhi bhi share");
    std::puts("   karte -- isliye 'pinned' yahan 'unpinned' se dramatically behtar NAHI hoga");
    std::puts("   jitna production HFT box (isolated core, 29's checklist) pe hota.");
    std::puts(" - Real fayda tab dikhta jab core GENUINELY isolated ho (isolcpus/nohz_full");
    std::puts("   Linux pe, 29-LINUX-SYSTEMS mein poora cover) -- yahan hum sirf AFFINITY");
    std::puts("   (kaunsa core allowed hai) demonstrate kar rahe, ISOLATION (koi aur us pe");
    std::puts("   na chale) nahi -- woh OS/kernel-config ka kaam hai, ek thread call ka nahi.");
    return 0;
}
