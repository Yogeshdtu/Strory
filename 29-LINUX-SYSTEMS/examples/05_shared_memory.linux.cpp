// 05_shared_memory.linux.cpp
// ============================================================
// POSIX shared memory (shm_open + mmap) se do process ek hi RAM dekhein.
// Ek SPSC-style counter hand-off -- yehi HFT ka "shm ring" ka core idea hai.
// ============================================================
//  LINUX-ONLY (shm_open, <sys/mman.h>). Link: -lrt (purane glibc pe).
//  Linux / WSL pe:
//      g++ -std=c++20 -O2 -Wall -Wextra 05_shared_memory.linux.cpp -o shm_demo -lrt
//      ./shm_demo
//      ls -l /dev/shm/            # segment yahan dikhega
// ============================================================

#define _GNU_SOURCE 1   // glibc: expose MAP_POPULATE/ip_mreqn/accept4/recvmmsg/CLOCK_* under strict -std=c++20
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <new>            // placement new
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>

struct Shared {
    std::atomic<std::uint64_t> seq{0};        // producer isme likhta
    std::atomic<int>           done{0};
    char                       pad[64 - 12];  // ek cache line
    std::uint64_t              payload[4096]; // "ring" jaisa data area
};
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);

static constexpr char   kName[] = "/cppm_shm_demo";
static constexpr std::uint64_t kN = 5'000'000;

int main() {
    ::shm_unlink(kName);                       // purani padi ho to hatao
    int fd = ::shm_open(kName, O_CREAT | O_RDWR | O_EXCL, 0600);
    if (fd < 0) { std::perror("shm_open"); return 1; }
    if (::ftruncate(fd, sizeof(Shared)) != 0) { std::perror("ftruncate"); return 1; }

    void* base = ::mmap(nullptr, sizeof(Shared), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (base == MAP_FAILED) { std::perror("mmap"); return 1; }
    ::close(fd);

    // Placement-construct: dono process isi object ko share karenge.
    auto* sh = new (base) Shared{};

    pid_t pid = fork();
    if (pid < 0) { std::perror("fork"); return 1; }

    if (pid == 0) {
        // ---------- CONSUMER (child) ----------
        // Alag process, alag page tables -- par shm_open + mmap se WAHI physical
        // pages. Yahan hum bas seq ke badhne ka wait karte hain (busy-poll).
        std::uint64_t last = 0, mismatches = 0;
        while (true) {
            std::uint64_t s = sh->seq.load(std::memory_order_acquire);
            if (s == last) {
                if (sh->done.load(std::memory_order_acquire)) break;
                continue;
            }
            if (s != last + 1) ++mismatches;      // SPSC monotonic hona chahiye
            last = s;
        }
        std::printf("  [consumer] aakhri seq dekha = %llu, gaps = %llu\n",
                    static_cast<unsigned long long>(last),
                    static_cast<unsigned long long>(mismatches));
        _exit(mismatches == 0 ? 0 : 1);
    }

    // ---------- PRODUCER (parent) ----------
    timespec a, b;
    clock_gettime(CLOCK_MONOTONIC, &a);
    for (std::uint64_t i = 1; i <= kN; ++i)
        sh->seq.store(i, std::memory_order_release);
    sh->done.store(1, std::memory_order_release);
    clock_gettime(CLOCK_MONOTONIC, &b);

    int st = 0; waitpid(pid, &st, 0);
    double ns = static_cast<double>((b.tv_sec - a.tv_sec) * 1'000'000'000L + (b.tv_nsec - a.tv_nsec));
    std::printf("  [producer] %llu updates in %.1f ms  -> ~%.1f ns/update\n",
                static_cast<unsigned long long>(kN), ns / 1e6, ns / static_cast<double>(kN));
    std::printf("  cross-process hand-off %s (child exit=%d)\n",
                (WIFEXITED(st) && WEXITSTATUS(st) == 0) ? "OK" : "MISMATCH", WEXITSTATUS(st));

    ::munmap(base, sizeof(Shared));
    ::shm_unlink(kName);                       // naam hatao -> /dev/shm se gayab

    std::puts(
        "\nKya seekha:\n"
        "  - shm_open() ek naam-wala segment banata (/dev/shm ke neeche tmpfs).\n"
        "  - mmap(MAP_SHARED) se do alag process ki virtual addresses EK hi\n"
        "    physical RAM pe point karti hain -- pointer values alag ho sakti,\n"
        "    physical page same.\n"
        "  - atomics + release/acquire cross-process bhi kaam karte (same as\n"
        "    cross-thread) -- kyunki hardware coherence process nahi janta.\n"
        "  - HFT: feed handler process market data ko ek shm ring me likhta,\n"
        "    strategy processes usi ring se bina kisi syscall ke padhti hain.");

    std::printf("\n[NOTE] ns/update TYPICAL range hai; NUMA node, coherence traffic\n"
                "aur busy-poll consumer ki wajah se badlega. Khud chala ke dekho.\n");
    return 0;
}

/* ============================================================
 * EXPECTED OUTPUT (typical Linux x86-64) -- aapke box pe NAHI napa gaya
 * ------------------------------------------------------------
 *   [consumer] aakhri seq dekha = 5000000, gaps = 0
 *   [producer] 5000000 updates in 78.0 ms  -> ~15.6 ns/update
 *   cross-process hand-off OK (child exit=0)
 *
 * Consumer busy-polls, isliye producer ki har store cache line ko doosre core
 * pe bhejni padti -> per-update cost ~ single cache-line bounce (~10-25 ns).
 * Do core same CCX/NUMA node pe ho to niche, cross-socket ho to upar.
 * ============================================================ */
