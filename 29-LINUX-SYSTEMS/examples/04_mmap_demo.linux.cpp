// 04_mmap_demo.linux.cpp
// ============================================================
// mmap() teen roop: (1) file mapping -> read()/write() ke bina file I/O,
// (2) anonymous mapping -> malloc jaisa bada block, (3) MAP_POPULATE se
// pre-fault. Lazy page fault ki cost bhi dikhayi.
// ============================================================
//  LINUX-ONLY (<sys/mman.h>, MAP_ANONYMOUS, MAP_POPULATE). MinGW pe mmap nahi.
//  Linux / WSL pe:
//      g++ -std=c++20 -O2 -Wall -Wextra 04_mmap_demo.linux.cpp -o mmap_demo
//      ./mmap_demo
// ============================================================

#define _GNU_SOURCE 1   // glibc: expose MAP_POPULATE/ip_mreqn/accept4/recvmmsg/CLOCK_* under strict -std=c++20
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

static std::uint64_t now_ns() {
    timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<std::uint64_t>(ts.tv_sec) * 1'000'000'000ull + static_cast<std::uint64_t>(ts.tv_nsec);
}

int main() {
    // ---------- 1. FILE MAPPING ----------
    const char* path = "/tmp/cppm_mmap_demo.bin";
    const size_t FSZ = 4 * 1024 * 1024;                 // 4 MiB

    int fd = ::open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (::ftruncate(fd, static_cast<off_t>(FSZ)) != 0) { std::perror("ftruncate"); return 1; }

    auto* fmap = static_cast<unsigned char*>(
        ::mmap(nullptr, FSZ, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    if (fmap == MAP_FAILED) { std::perror("mmap file"); return 1; }

    // File ko aise likho jaise woh ek array ho -- koi write() syscall nahi.
    for (size_t i = 0; i < FSZ; i += 4096) fmap[i] = static_cast<unsigned char>(i / 4096);
    ::msync(fmap, FSZ, MS_SYNC);                        // dirty pages disk pe flush
    std::printf("1. File mapping: %zu bytes ko pointer se likha (no write() calls)\n", FSZ);
    std::printf("   fmap[0]=%u fmap[4096]=%u fmap[8192]=%u\n",
                fmap[0], fmap[4096], fmap[8192]);
    ::munmap(fmap, FSZ);
    ::close(fd);
    ::unlink(path);

    // ---------- 2. ANONYMOUS MAPPING + LAZY FAULT COST ----------
    const size_t ASZ = 256 * 1024 * 1024;              // 256 MiB
    auto* amap = static_cast<unsigned char*>(
        ::mmap(nullptr, ASZ, PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    if (amap == MAP_FAILED) { std::perror("mmap anon"); return 1; }
    std::printf("\n2. Anonymous mapping: %zu MiB reserve hua -- abhi 0 physical pages\n",
                ASZ / (1024 * 1024));

    // Pehli baar har page ko chhuo -> har touch pe ek MINOR page fault.
    const std::uint64_t t0 = now_ns();
    for (size_t i = 0; i < ASZ; i += 4096) amap[i] = 1;
    const std::uint64_t t1 = now_ns();
    const size_t pages = ASZ / 4096;
    std::printf("   pehli touch (%zu pages): %.1f ms total, ~%.0f ns/page fault\n",
                pages, static_cast<double>(t1 - t0) / 1e6,
                static_cast<double>(t1 - t0) / static_cast<double>(pages));

    // Doosri baar: pages ab mapped hain -> koi fault nahi, sirf memory store.
    const std::uint64_t t2 = now_ns();
    for (size_t i = 0; i < ASZ; i += 4096) amap[i] = 2;
    const std::uint64_t t3 = now_ns();
    std::printf("   doosri touch (no faults): %.1f ms total, ~%.0f ns/page\n",
                static_cast<double>(t3 - t2) / 1e6,
                static_cast<double>(t3 - t2) / static_cast<double>(pages));
    ::munmap(amap, ASZ);

    // ---------- 3. MAP_POPULATE: fault ab, latency baad me nahi ----------
    const std::uint64_t p0 = now_ns();
    auto* pmap = static_cast<unsigned char*>(
        ::mmap(nullptr, ASZ, PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0));
    const std::uint64_t p1 = now_ns();
    if (pmap == MAP_FAILED) { std::perror("mmap populate"); return 1; }
    std::printf("\n3. MAP_POPULATE: mmap() call khud %.1f ms li (saare pages abhi faulted)\n",
                static_cast<double>(p1 - p0) / 1e6);
    const std::uint64_t p2 = now_ns();
    for (size_t i = 0; i < ASZ; i += 4096) pmap[i] = 3;
    const std::uint64_t p3 = now_ns();
    std::printf("   ab pehli touch bhi fault-free: %.1f ms\n",
                static_cast<double>(p3 - p2) / 1e6);
    ::munmap(pmap, ASZ);

    std::puts(
        "\nKya seekha:\n"
        "  - File mapping: file = array. Random access, page cache shared,\n"
        "    koi read()/write() trap nahi. Bade read-only datasets ke liye best.\n"
        "  - Anonymous mapping lazy hai: address space milta, physical RAM tab\n"
        "    jab pehli baar page chhuo -> ~200-800 ns/page minor fault.\n"
        "  - MAP_POPULATE (ya mlock/pre-touch) fault ko startup pe le aata,\n"
        "    taaki trading hours me koi surprise fault na ho. HFT warm-up ka core.");

    std::printf("\n[NOTE] Numbers TYPICAL hain -- THP, kernel version, RAM speed se\n"
                "badlenge. Ratio (pehli touch >> doosri touch) hamesha bada.\n");
    return 0;
}

/* ============================================================
 * EXPECTED OUTPUT (typical Linux x86-64) -- aapke box pe NAHI napa gaya
 * ------------------------------------------------------------
 * 1. File mapping: 4194304 bytes ko pointer se likha (no write() calls)
 *    fmap[0]=0 fmap[4096]=1 fmap[8192]=2
 *
 * 2. Anonymous mapping: 256 MiB reserve hua -- abhi 0 physical pages
 *    pehli touch (65536 pages): 28.5 ms total, ~435 ns/page fault
 *    doosri touch (no faults): 0.9 ms total, ~14 ns/page
 *
 * 3. MAP_POPULATE: mmap() call khud 21.0 ms li (saare pages abhi faulted)
 *    ab pehli touch bhi fault-free: 0.9 ms
 *
 * pehli-touch / doosri-touch ratio ~ 25-35x. THP (2 MiB pages) on hone pe
 * per-"page" fault count 512x kam, to total fault time bhi bahut gir sakta.
 * ============================================================ */
