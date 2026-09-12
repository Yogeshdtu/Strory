// 11_page_fault_warmup.cpp
// ============================================================
// Fresh memory ka pehla touch = a PAGE FAULT (kernel ek physical page
// map karta). ~1-3 us minor fault, ya ms agar disk-backed / swapped.
// Hot path pe yeh ek tail spike hai. Fix = WARM-UP: har page ko go-live
// se pehle chhoo lo, aur (Linux) lock kar do.
//
//   Yeh example COLD vs WARM first-write ki latency DISTRIBUTION dikhata:
//     COLD : ek bada fresh buffer, har 4 KB pe pehla write (fault har page)
//     WARM : same buffer, pehle ek full pass (pre-fault), phir dobara measure
//
//   Portable (MinGW/Windows + Linux). OS-specific hardening comments mein:
//     Linux : mmap(MAP_POPULATE) | madvise(MADV_WILLNEED) | mlockall(MCL_CURRENT|MCL_FUTURE)
//     Windows: VirtualLock() + SetProcessWorkingSetSize()
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 11_page_fault_warmup.cpp -o w && ./w
// ============================================================

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <x86intrin.h>

namespace ch = std::chrono;
template <class T> static inline void keep(T& v) { asm volatile("" : "+r,m"(v) : : "memory"); }
static inline std::uint64_t tsc() { _mm_lfence(); auto t = __rdtsc(); _mm_lfence(); return t; }
static double g_tpns = 1.0;

static void report(const char* tag, std::vector<double>& ns) {
    std::sort(ns.begin(), ns.end());
    auto pc = [&](double p) {
        std::size_t i = static_cast<std::size_t>(p / 100.0 * static_cast<double>(ns.size()));
        return ns[std::min(i, ns.size() - 1)];
    };
    double sum = 0; for (double v : ns) sum += v;
    std::printf("  %-16s mean %7.1f   p50 %6.1f   p99 %8.1f   p99.9 %9.1f   max %10.1f  ns\n",
                tag, sum / static_cast<double>(ns.size()), pc(50), pc(99), pc(99.9), ns.back());
}

int main() {
    {
        auto c0 = ch::steady_clock::now();
        std::uint64_t r0 = tsc();
        volatile std::uint64_t s = 0;
        while (ch::duration_cast<ch::milliseconds>(ch::steady_clock::now() - c0).count() < 120) s = s + 1;
        std::uint64_t r1 = tsc();
        auto c1 = ch::steady_clock::now();
        g_tpns = static_cast<double>(r1 - r0)
               / static_cast<double>(ch::duration_cast<ch::nanoseconds>(c1 - c0).count());
    }
    std::printf("ticks_per_ns = %.4f\n\n", g_tpns);

    constexpr std::size_t PAGES = 20000;                      // ~78 MB at 4 KB pages
    constexpr std::size_t PAGE = 4096;
    constexpr std::size_t BYTES = PAGES * PAGE;

    // ---- COLD: fresh allocation, first write to each page ------
    {
        // request from the OS fresh (calloc avoids libc reusing a hot arena)
        auto* buf = static_cast<std::uint8_t*>(std::malloc(BYTES));
        if (!buf) { std::puts("malloc failed"); return 1; }

        std::vector<double> lat;
        lat.reserve(PAGES);
        std::uint64_t acc = 0;
        for (std::size_t p = 0; p < PAGES; ++p) {
            std::uint8_t* addr = buf + p * PAGE;
            std::uint64_t t0 = tsc();
            *addr = static_cast<std::uint8_t>(p);             // <-- first touch: may fault
            std::uint64_t t1 = tsc();
            acc += *addr;
            lat.push_back(static_cast<double>(t1 - t0) / g_tpns);
        }
        keep(acc);
        report("COLD first-write", lat);

        // ---- WARM: buffer now fully resident, write again ------
        std::vector<double> lat2;
        lat2.reserve(PAGES);
        acc = 0;
        for (std::size_t p = 0; p < PAGES; ++p) {
            std::uint8_t* addr = buf + p * PAGE;
            std::uint64_t t0 = tsc();
            *addr = static_cast<std::uint8_t>(p + 1);
            std::uint64_t t1 = tsc();
            acc += *addr;
            lat2.push_back(static_cast<double>(t1 - t0) / g_tpns);
        }
        keep(acc);
        report("WARM re-write", lat2);
        std::free(buf);
    }

    std::puts("\n--- OS hardening for a real hot path (do this at startup) ---");
    std::puts("  Linux  : void* p = mmap(0, n, PROT_READ|PROT_WRITE,");
    std::puts("           MAP_PRIVATE|MAP_ANONYMOUS|MAP_POPULATE, -1, 0);  // pre-faults");
    std::puts("           mlockall(MCL_CURRENT | MCL_FUTURE);              // no swap, pre-fault future");
    std::puts("           + touch every page once, + madvise(MADV_WILLNEED / MADV_HUGEPAGE)");
    std::puts("  Windows: VirtualLock(p, n);  SetProcessWorkingSetSize(h, lo, hi);");
    std::puts("           + a full write pass. Large-page: MEM_LARGE_PAGES + SeLockMemoryPrivilege.");

    std::puts("\nKya seekha:");
    std::puts(" - COLD pass ki mean/p99/max WARM se kai guna badi — har naye page pe");
    std::puts("   ek minor fault (kernel page zero karta + PTE bharta).");
    std::puts(" - WARM pass ~flat — pages resident, TLB garam.");
    std::puts(" - Isliye: sab memory startup pe allocate + touch + (Linux) mlock karo.");
    std::puts("   Steady state mein ek bhi naya page fault nahi hona chahiye.");
    std::puts(" - Warm-up sirf memory nahi — code paths, caches, branch predictor bhi");
    std::puts("   (folder 35/09 cold-start; folder 36 lesson 20 cache warming).");
    return 0;
}
