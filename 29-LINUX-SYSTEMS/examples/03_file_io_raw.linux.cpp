// 03_file_io_raw.linux.cpp
// ============================================================
// Raw fd I/O (open/read/write/close) vs C stdio (fread) vs C++ iostream.
// Aur: har write() ek syscall hai -> chhoti writes = latency killer.
// ============================================================
//  LINUX-ONLY (fcntl/unistd raw fd flags, O_DIRECT optional). Portable-ish
//  hai par yeh folder Linux hai; MinGW pe kuch flags nahi milte.
//  Linux / WSL pe:
//      g++ -std=c++20 -O2 -Wall -Wextra 03_file_io_raw.linux.cpp -o file_io_raw
//      ./file_io_raw
//      strace -c ./file_io_raw      # write() syscalls ka count dekho
// ============================================================

#define _GNU_SOURCE 1   // glibc: expose MAP_POPULATE/ip_mreqn/accept4/recvmmsg/CLOCK_* under strict -std=c++20
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <unistd.h>
#include <fstream>

static std::uint64_t now_ns() {
    timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<std::uint64_t>(ts.tv_sec) * 1'000'000'000ull + static_cast<std::uint64_t>(ts.tv_nsec);
}

static constexpr int    kLines  = 200'000;
static constexpr char   kPath[] = "/tmp/cppm_io_demo.txt";

// A) Har line ek alag write() syscall -> kLines syscalls.
static double write_unbuffered() {
    int fd = ::open(kPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    char buf[64];
    const std::uint64_t t0 = now_ns();
    for (int i = 0; i < kLines; ++i) {
        int n = std::snprintf(buf, sizeof buf, "line %d value %d\n", i, i * 7);
        ::write(fd, buf, static_cast<size_t>(n));      // <-- ek syscall PER LINE
    }
    ::fsync(fd);
    const std::uint64_t t1 = now_ns();
    ::close(fd);
    return static_cast<double>(t1 - t0) / 1e6;         // ms
}

// B) Userspace me ek bade buffer me jama karo, phir ~gine-chune write() calls.
static double write_userbuffered() {
    int fd = ::open(kPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    static char big[1 << 20];                          // 1 MiB staging buffer
    size_t used = 0;
    char line[64];
    const std::uint64_t t0 = now_ns();
    for (int i = 0; i < kLines; ++i) {
        int n = std::snprintf(line, sizeof line, "line %d value %d\n", i, i * 7);
        if (used + static_cast<size_t>(n) > sizeof big) {
            ::write(fd, big, used);                    // buffer bhara -> ek flush
            used = 0;
        }
        std::memcpy(big + used, line, static_cast<size_t>(n));
        used += static_cast<size_t>(n);
    }
    if (used) ::write(fd, big, used);
    ::fsync(fd);
    const std::uint64_t t1 = now_ns();
    ::close(fd);
    return static_cast<double>(t1 - t0) / 1e6;
}

// C) C++ ofstream -- iostream ka apna buffer hai (stdio jaisa), par har << pe
//    formatting + locale + sentry ka overhead.
static double write_ofstream() {
    std::ofstream f(kPath, std::ios::trunc);
    const std::uint64_t t0 = now_ns();
    for (int i = 0; i < kLines; ++i)
        f << "line " << i << " value " << (i * 7) << '\n';
    f.flush();
    const std::uint64_t t1 = now_ns();
    return static_cast<double>(t1 - t0) / 1e6;
}

int main() {
    std::printf("%d lines, teen tareeke se likho:\n\n", kLines);
    std::printf("  A) raw write() per line (unbuffered) : %8.2f ms\n", write_unbuffered());
    std::printf("  B) userspace-buffered raw write()    : %8.2f ms\n", write_userbuffered());
    std::printf("  C) std::ofstream (iostream buffer)   : %8.2f ms\n", write_ofstream());

    ::unlink(kPath);

    std::puts(
        "\nKya seekha:\n"
        "  - A me kLines syscalls hoti hain (strace -c se dekho). Har write()\n"
        "    ~300-800 ns trap + kernel copy. Isi liye A sabse slow.\n"
        "  - B me ~ (total_bytes / 1 MiB) syscalls -- yaani mutthi bhar. 10-50x tez.\n"
        "  - C bhi buffered hai (~4-8 KiB stdio buffer) par per-<< formatting\n"
        "    overhead se B se dheema.\n"
        "  - HFT logging isi wajah se: userspace ring buffer me bytes daalo,\n"
        "    ek alag thread bade chunks me write() kare. Hot thread kabhi\n"
        "    write() syscall na maare.");

    std::printf("\n[NOTE] Absolute ms values disk (SSD/NVMe/tmpfs), page cache aur\n"
                "kernel version pe depend karte. Ratio (A >> B) stable rehta.\n");
    return 0;
}

/* ============================================================
 * EXPECTED OUTPUT (typical Linux x86-64, tmpfs /tmp) -- aapke box pe NAHI napa gaya
 * ------------------------------------------------------------
 * 200000 lines, teen tareeke se likho:
 *
 *   A) raw write() per line (unbuffered) :   118.40 ms
 *   B) userspace-buffered raw write()    :     3.10 ms
 *   C) std::ofstream (iostream buffer)   :    12.70 ms
 *
 * strace -c ./file_io_raw  -> "write" calls: A ~200003, B ~10, C ~30-50.
 * Ratio A/B ~ 20-40x. Disk-backed (ext4, fsync) pe sab bade ho jayenge par
 * A ka relative dard aur badh jayega.
 * ============================================================ */
