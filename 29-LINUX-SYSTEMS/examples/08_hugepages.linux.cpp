// 08_hugepages.linux.cpp
// ============================================================
// 4 KiB pages vs 2 MiB huge pages -> TLB pressure. Ek bade buffer pe
// random-stride pointer chase, dono tarah, aur ns/access compare.
// ============================================================
//  LINUX-ONLY (MAP_HUGETLB, MADV_HUGEPAGE). MinGW pe kuch nahi.
//  Linux / WSL pe (explicit hugepages ke liye pehle reserve karo):
//      echo 512 | sudo tee /proc/sys/vm/nr_hugepages
//      g++ -std=c++20 -O2 -Wall -Wextra 08_hugepages.linux.cpp -o hugepages
//      ./hugepages
//  THP (transparent) route ke liye reserve zaroori nahi -- madvise kaafi.
// ============================================================

#define _GNU_SOURCE 1   // glibc: expose MAP_POPULATE/ip_mreqn/accept4/recvmmsg/CLOCK_* under strict -std=c++20
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <sys/mman.h>

static std::uint64_t now_ns() {
    timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<std::uint64_t>(ts.tv_sec) * 1'000'000'000ull + static_cast<std::uint64_t>(ts.tv_nsec);
}

static constexpr size_t SZ  = 512 * 1024 * 1024;      // 512 MiB working set >> TLB reach
static constexpr size_t N   = 20'000'000;             // random accesses

// Ek SINGLE bada cycle banao (Sattolo) taaki hardware prefetcher predict na
// kar paye aur poora working set ek hi chain me cover ho.
//   step 1: order[] = shuffled permutation of 0..n-1  (in-place, second half of p)
//   step 2: p[order[i]] = order[(i+1) % n]            -> ek Hamiltonian cycle
static void build_chase(std::uint64_t* p, size_t n_slots) {
    // order[] ke liye p[] ke doosre aadhe ko temp ki tarah use nahi kar sakte
    // (overlap). Chhota trick: pehle p[i]=i, phir Sattolo shuffle, phir
    // "value at index" ko "next index" me convert -- ek alag pass me.
    for (size_t i = 0; i < n_slots; ++i) p[i] = i;
    std::uint64_t rng = 0x243F6A8885A308D3ull;
    for (size_t i = n_slots - 1; i > 0; --i) {        // Sattolo: single n-cycle
        rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17;
        size_t j = static_cast<size_t>(rng % i);      // j in [0, i)  -> guaranteed one cycle
        std::uint64_t t = p[i]; p[i] = p[j]; p[j] = t;
    }
    // Ab p[] ek single-cycle permutation hai: index i se p[i] pe jao, poora
    // ghoom ke wapas i pe. chase() bilkul yahi karta hai (idx = p[idx]).
}

static double chase(std::uint64_t* p, size_t n_slots) {
    // p[k] = next index. n_slots ek bada cycle hai (permutation).
    size_t idx = 0;
    volatile std::uint64_t sink = 0;
    std::uint64_t t0 = now_ns();
    for (size_t i = 0; i < N; ++i) { idx = p[idx]; sink += idx; }
    std::uint64_t t1 = now_ns();
    (void)sink;
    return static_cast<double>(t1 - t0) / static_cast<double>(N);   // ns/access
}

int main() {
    const size_t slots = SZ / sizeof(std::uint64_t);

    // ---------- 4 KiB pages ----------
    auto* small = static_cast<std::uint64_t*>(
        ::mmap(nullptr, SZ, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    if (small == MAP_FAILED) { std::perror("mmap small"); return 1; }
    ::madvise(small, SZ, MADV_NOHUGEPAGE);            // pukka 4K rahe
    std::memset(small, 0, SZ);
    build_chase(small, slots);
    double ns_small = chase(small, slots);
    ::munmap(small, SZ);

    // ---------- 2 MiB huge pages (explicit) ----------
    auto* huge = static_cast<std::uint64_t*>(
        ::mmap(nullptr, SZ, PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0));
    bool used_hugetlb = (huge != MAP_FAILED);
    if (!used_hugetlb) {
        // Fallback: THP via madvise (kernel background me collapse karega).
        huge = static_cast<std::uint64_t*>(
            ::mmap(nullptr, SZ, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        if (huge == MAP_FAILED) { std::perror("mmap huge fallback"); return 1; }
        ::madvise(huge, SZ, MADV_HUGEPAGE);
        std::puts("[info] MAP_HUGETLB fail (nr_hugepages reserve nahi?) -> THP madvise fallback");
    }
    std::memset(huge, 0, SZ);
    build_chase(huge, slots);
    double ns_huge = chase(huge, slots);
    ::munmap(huge, SZ);

    std::printf("\nRandom pointer-chase, %zu MiB working set, %zu M accesses:\n",
                SZ / (1024*1024), N / 1'000'000);
    std::printf("  4 KiB pages          : %.2f ns/access\n", ns_small);
    std::printf("  2 MiB pages (%s): %.2f ns/access\n",
                used_hugetlb ? "hugetlb" : "THP    ", ns_huge);
    std::printf("  speedup              : %.2fx\n", ns_small / ns_huge);

    std::puts(
        "\nKya seekha:\n"
        "  - Working set (512 MiB) TLB reach se bahut bada hai. 4K pages me\n"
        "    har random access aksar ek TLB miss -> page-table walk (2-4 extra\n"
        "    memory refs).\n"
        "  - 2 MiB pages: ek TLB entry 512x zyada memory cover karti -> TLB\n"
        "    miss rate girta -> ~10-40% tez random access (dataset pe depend).\n"
        "  - Bonus: huge pages me page-fault count 512x kam (08 se yaad).\n"
        "  - HFT: order book, big hash maps, market-data buffers explicit\n"
        "    hugepages pe -> predictable, kam TLB stalls. THP kabhi-kabhi\n"
        "    'collapse' pause deta -> latency-critical ke liye EXPLICIT behtar.");

    std::printf("\n[NOTE] speedup TYPICAL range hai (~1.1x se ~1.5x). Sequential\n"
                "access pattern pe fayda kam; pure-random pe zyada. Khud napo.\n");
    return 0;
}

/* ============================================================
 * EXPECTED OUTPUT (typical Linux x86-64, nr_hugepages reserved) -- NAHI napa gaya
 * ------------------------------------------------------------
 * Random pointer-chase, 512 MiB working set, 20 M accesses:
 *   4 KiB pages          : 118.40 ns/access
 *   2 MiB pages (hugetlb): 92.10 ns/access
 *   speedup              : 1.29x
 *
 * Access pattern jitna random aur working set jitna bada, huge pages ka
 * fayda utna zyada. Cache-resident data pe (~few MiB) farak ~0.
 * ============================================================ */
