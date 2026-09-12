// 07_page_faults.linux.cpp
// ============================================================
// Page fault ki cost: (A) lazy allocation -> pehli touch pe minor fault,
// (B) mlockall + pre-fault -> steady-state me zero faults. getrusage se
// fault count bhi print.
// ============================================================
//  LINUX-ONLY (mlockall, MCL_*, madvise, getrusage ru_minflt). MinGW pe nahi.
//  Linux / WSL pe:
//      g++ -std=c++20 -O2 -Wall -Wextra 07_page_faults.linux.cpp -o page_faults
//      ./page_faults
//      /usr/bin/time -v ./page_faults   # "page faults" line dekho
// ============================================================

#define _GNU_SOURCE 1   // glibc: expose MAP_POPULATE/ip_mreqn/accept4/recvmmsg/CLOCK_* under strict -std=c++20
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <sys/mman.h>
#include <sys/resource.h>

static std::uint64_t now_ns() {
    timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<std::uint64_t>(ts.tv_sec) * 1'000'000'000ull + static_cast<std::uint64_t>(ts.tv_nsec);
}
static long minflt() {
    rusage ru{}; getrusage(RUSAGE_SELF, &ru); return ru.ru_minflt;
}

static constexpr size_t SZ    = 128 * 1024 * 1024;   // 128 MiB
static constexpr size_t PAGES = SZ / 4096;

// Ek buffer ko sequentially chhuo, per-page latency naapo. `first`=true means
// abhi pages mapped nahi -> har touch = minor fault. `first`=false -> warm.
static void touch_pass(const char* tag, volatile unsigned char* p, bool measure_tail) {
    long f0 = minflt();
    std::uint64_t worst = 0, sum = 0;
    for (size_t i = 0; i < SZ; i += 4096) {
        std::uint64_t t0 = now_ns();
        p[i] = static_cast<unsigned char>(i);
        std::uint64_t d = now_ns() - t0;
        sum += d;
        if (d > worst) worst = d;
    }
    long faults = minflt() - f0;
    std::printf("  %-22s : %zu pages, %.1f ms, avg %.0f ns/page, worst %llu ns, minor-faults %ld\n",
                tag, PAGES, static_cast<double>(sum) / 1e6,
                static_cast<double>(sum) / static_cast<double>(PAGES),
                static_cast<unsigned long long>(worst), faults);
    if (measure_tail && worst > 0) { /* nothing extra */ }
}

int main() {
    std::printf("RES page size = 4096, buffer = %zu MiB (%zu pages)\n\n", SZ / (1024*1024), PAGES);

    // ---------- A. LAZY: koi mlock nahi ----------
    auto* a = static_cast<unsigned char*>(
        ::mmap(nullptr, SZ, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    if (a == MAP_FAILED) { std::perror("mmap A"); return 1; }
    std::puts("A) Lazy mapping (default):");
    touch_pass("cold (faulting)", a, true);
    touch_pass("warm (mapped)", a, false);
    ::munmap(a, SZ);

    // ---------- B. mlockall + pre-fault ----------
    // MCL_CURRENT|MCL_FUTURE: ab tak ki aur aage ki saari memory RAM me pinned,
    // kabhi swap/reclaim nahi hogi.
    if (::mlockall(MCL_CURRENT | MCL_FUTURE) != 0)
        std::perror("  mlockall (root/ulimit -l chahiye ho sakta)");

    auto* b = static_cast<unsigned char*>(
        ::mmap(nullptr, SZ, PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0));   // POPULATE: abhi fault
    if (b == MAP_FAILED) { std::perror("mmap B"); return 1; }
    std::memset(b, 0, SZ);                       // aur pukka karo -- har page dirty+resident

    std::puts("\nB) mlockall + MAP_POPULATE + memset (pre-faulted, pinned):");
    touch_pass("first real touch", b, true);    // ab yeh warm jaisa hona chahiye
    touch_pass("second touch", b, false);
    ::munmap(b, SZ);
    ::munlockall();

    std::puts(
        "\nKya seekha:\n"
        "  - Cold pass me PAGES minor-faults hote hain; avg ~200-600 ns/page,\n"
        "    par 'worst' ek page pe kaafi zyada -- yehi woh spike hai jo trading\n"
        "    hours me nahi chahiye.\n"
        "  - Warm pass: 0 faults, avg ~5-15 ns/page (sirf store).\n"
        "  - B me pre-fault + mlockall ke baad 'first real touch' bhi warm jaisa:\n"
        "    faults ~0, worst chhota. Memory RAM me locked -> reclaim/swap se\n"
        "    surprise major fault (~ms) bhi impossible.\n"
        "  - HFT warm-up: startup pe saari arenas/pools allocate karo, mlockall\n"
        "    karo, har page ko chhuo. Steady state me page fault = 0.");

    std::printf("\n[NOTE] Numbers TYPICAL hain (kernel, THP, RAM speed pe nirbhar).\n"
                "Ratio cold/warm ~ 20-60x hamesha bada rehta.\n");
    return 0;
}

/* ============================================================
 * EXPECTED OUTPUT (typical Linux x86-64) -- aapke box pe NAHI napa gaya
 * ------------------------------------------------------------
 * RES page size = 4096, buffer = 128 MiB (32768 pages)
 *
 * A) Lazy mapping (default):
 *   cold (faulting)        : 32768 pages, 11.8 ms, avg 360 ns/page, worst 48000 ns, minor-faults 32768
 *   warm (mapped)          : 32768 pages, 0.3 ms, avg 9 ns/page, worst 900 ns, minor-faults 0
 *
 * B) mlockall + MAP_POPULATE + memset (pre-faulted, pinned):
 *   first real touch       : 32768 pages, 0.3 ms, avg 9 ns/page, worst 1100 ns, minor-faults 0
 *   second touch           : 32768 pages, 0.3 ms, avg 9 ns/page, worst 800 ns, minor-faults 0
 *
 * "worst" cold pe ~10-100 us (TLB shootdown / zeroing / THP collapse). Wahi
 * spike pre-faulting se steady state se hat jata hai.
 * ============================================================ */
