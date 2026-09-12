// 08_tlb_hugepages.cpp
// ============================================================
// TLB (Translation Lookaside Buffer) = virtual->physical page translations
// ka chhota cache. Miss -> "page walk" (x86-64: 4-level page table, upto 4
// dependent memory accesses).
//
// Is box ke typical numbers: L1 dTLB ~64 entries (4 KiB pages -> 256 KiB
// reach), L2 STLB ~1536 entries (~6 MiB reach). Working set ka page-count
// jab in limits ko paar karta -> har access ke saath page walk.
//
// Yeh example TLB ka ASAR portably naapta (Windows/MinGW pe chalta):
//   Curve A: ek hi 4 KiB page ke andar pointer-chase (TLB hamesha hit) --
//            sirf cache latency dikhta.
//   Curve B: har step NAYA page (1 slot per page), N pages sweep -- jab N
//            TLB reach cross karta, per-step cost chadhta (page walks).
// B - A ka delta ~= TLB-miss ka tax.
//
// Huge pages (2 MiB) -> ek TLB entry 512x zyada memory cover -> yeh cliff
// ~gayab. Allocation OS-specific hai -- lesson 11 mein Linux (MAP_HUGETLB,
// madvise(MADV_HUGEPAGE), /proc/sys/vm/nr_hugepages) aur Windows large-page
// path. Yahan hum sirf 4 KiB pages pe asar dikhate hain.
//
// (uint32 buffers -- koi reinterpret_cast / -Wcast-align nahi.)
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 08_tlb_hugepages.cpp -o tlb && ./tlb
// ============================================================

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <utility>
#include <vector>

using Clock = std::chrono::steady_clock;

template <class T>
static inline void keep(T& v) { asm volatile("" : "+r,m"(v) : : "memory"); }

constexpr std::size_t U32_PER_PAGE = 4096 / 4;    // 1024
constexpr std::size_t U32_PER_LINE = 64 / 4;      // 16
constexpr std::uint64_t STEPS = 40'000'000ull;

// Sattolo -> guaranteed single n-cycle permutation (folder 29 se sabak).
static void make_cycle(std::vector<std::uint32_t>& perm, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) perm[i] = static_cast<std::uint32_t>(i);
    std::uint64_t rng = 0x9E3779B97F4A7C15ull;
    for (std::size_t i = n - 1; i > 0; --i) {
        rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17;
        const std::size_t j = static_cast<std::size_t>(rng % i);   // 0..i-1
        std::swap(perm[i], perm[j]);
    }
}

// chase: buf[cur] -> next element-offset. Returns ns/step.
static double chase(const std::vector<std::uint32_t>& buf) {
    std::uint32_t cur = 0;
    auto t0 = Clock::now();
    for (std::uint64_t s = 0; s < STEPS; ++s) cur = buf[cur];
    auto t1 = Clock::now();
    keep(cur);
    return std::chrono::duration<double>(t1 - t0).count() * 1e9 / static_cast<double>(STEPS);
}

int main() {
    std::printf("pointer-chase, STEPS=%llu   (4 KiB pages; L2 STLB reach ~6 MiB here)\n\n",
                static_cast<unsigned long long>(STEPS));
    std::printf("  pages  footprint    A: 1 page    B: N pages    B-A (TLB tax)\n");
    std::printf("  -----  ---------    ---------    ----------    ------------\n");

    // Curve A is constant (one page, 64 slots) -- compute once.
    std::vector<std::uint32_t> bufA(U32_PER_PAGE, 0);
    {
        constexpr std::size_t slotsA = 4096 / 64;         // 64
        std::vector<std::uint32_t> perm(slotsA);
        make_cycle(perm, slotsA);
        for (std::size_t i = 0; i < slotsA; ++i)
            bufA[i * U32_PER_LINE] = static_cast<std::uint32_t>(perm[i] * U32_PER_LINE);
    }
    const double nsA = chase(bufA);

    for (std::size_t pages = 16; pages <= 8192; pages <<= 1) {
        std::vector<std::uint32_t> bufB(pages * U32_PER_PAGE, 0);
        std::vector<std::uint32_t> perm(pages);
        make_cycle(perm, pages);
        for (std::size_t i = 0; i < pages; ++i)
            bufB[i * U32_PER_PAGE] = static_cast<std::uint32_t>(perm[i] * U32_PER_PAGE);

        const double nsB = chase(bufB);
        std::printf("  %5zu   %6zu KiB    %7.2f ns   %8.2f ns    %8.2f ns\n",
                    pages, (pages * 4096) >> 10, nsA, nsB, nsB - nsA);
    }

    std::puts("\nKya hua:");
    std::puts(" - Curve A: chase ek hi page ke andar -> TLB entry hamesha hit ->");
    std::puts("   flat ~1.2 ns (L1 latency). Yeh control hai.");
    std::puts(" - Curve B: har hop naya page. 16..64 pages: ~3.4 ns (chhota TLB");
    std::puts("   overhead). 128..1024 pages (0.5..4 MiB): 5 -> 15 ns. 2048+ pages");
    std::puts("   (8 MiB+): ~95 ns -- bada cliff.");
    std::puts(" - ⚠️ Honest: 4 KiB pages ke saath page-count aur memory-footprint");
    std::puts("   ek saath badhte -> yeh cliff TLB (L2 STLB reach ~6 MiB) AUR cache");
    std::puts("   (L3 8 MiB -> DRAM) dono ko ek saath cross karta. Curve B mein");
    std::puts("   dono effects hain. Sirf-TLB isolate karne ke liye same cache");
    std::puts("   lines ko alag page-mappings se chhuna padta -> mmap aliasing");
    std::puts("   (Linux-only).");
    std::puts(" - Point phir bhi khada: page-strided bada working set = dono taraf");
    std::puts("   se maar. 2 MiB huge pages: ek TLB entry = 512 * 4 KiB -> STLB");
    std::puts("   reach 512x -> TLB wala hissa ~gayab (cache wala rahega). Lesson");
    std::puts("   11: allocate kaise (Linux MAP_HUGETLB / THP; Windows large pages).");
    return 0;
}
