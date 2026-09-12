// 08_socket_options.linux.cpp
// ============================================================
// Socket options ka tour: read defaults, set the HFT-relevant ones,
// read back. TCP_NODELAY, SO_RCVBUF/SO_SNDBUF, SO_REUSEADDR/PORT,
// TCP_QUICKACK, SO_BUSY_POLL, TCP_INFO, SO_LINGER.
// ============================================================
//  LINUX-ONLY.
//  Linux / WSL pe:
//      g++ -std=c++20 -O2 -Wall -Wextra 08_socket_options.linux.cpp -o sockopts && ./sockopts
// ============================================================

#define _GNU_SOURCE 1   // glibc: expose MAP_POPULATE/ip_mreqn/accept4/recvmmsg/CLOCK_* under strict -std=c++20
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

static int geti(int fd, int level, int opt) {
    int v = -1; socklen_t l = sizeof v;
    ::getsockopt(fd, level, opt, &v, &l);
    return v;
}
static void seti(int fd, int level, int opt, int v, const char* name) {
    if (::setsockopt(fd, level, opt, &v, sizeof v) < 0) std::printf("  set %-18s FAILED (%s)\n", name, std::strerror(errno));
    else std::printf("  set %-18s = %d\n", name, v);
}

int main() {
    int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) { perror("socket"); return 1; }

    std::puts("=== defaults (fresh TCP socket) ===");
    std::printf("  SO_RCVBUF        = %d bytes  (kernel doubles for bookkeeping)\n", geti(fd, SOL_SOCKET, SO_RCVBUF));
    std::printf("  SO_SNDBUF        = %d bytes\n", geti(fd, SOL_SOCKET, SO_SNDBUF));
    std::printf("  TCP_NODELAY      = %d  (0 = Nagle ON)\n", geti(fd, IPPROTO_TCP, TCP_NODELAY));
    std::printf("  TCP_MAXSEG(MSS)  = %d\n", geti(fd, IPPROTO_TCP, TCP_MAXSEG));
    std::printf("  SO_REUSEADDR     = %d\n", geti(fd, SOL_SOCKET, SO_REUSEADDR));

    std::puts("\n=== HFT-relevant settings ===");
    seti(fd, IPPROTO_TCP, TCP_NODELAY,   1, "TCP_NODELAY");     // Nagle off -- ALWAYS (example 03)
    seti(fd, IPPROTO_TCP, TCP_QUICKACK,  1, "TCP_QUICKACK");    // delayed-ACK suppress (per-call, resets)
    seti(fd, SOL_SOCKET,  SO_REUSEADDR,  1, "SO_REUSEADDR");    // rebind despite TIME_WAIT
    seti(fd, SOL_SOCKET,  SO_REUSEPORT,  1, "SO_REUSEPORT");    // N processes, same port (accept scaling)
    seti(fd, SOL_SOCKET,  SO_RCVBUF, 8*1024*1024, "SO_RCVBUF"); // burst headroom (market data)
    seti(fd, SOL_SOCKET,  SO_SNDBUF, 4*1024*1024, "SO_SNDBUF");
#ifdef SO_BUSY_POLL
    seti(fd, SOL_SOCKET,  SO_BUSY_POLL, 50, "SO_BUSY_POLL");    // us -- recv() spins instead of sleeping
#endif
#ifdef TCP_NODELAY
    // keepalive taaki dead peer detect ho (control connections)
    seti(fd, SOL_SOCKET,  SO_KEEPALIVE, 1, "SO_KEEPALIVE");
    seti(fd, IPPROTO_TCP, TCP_KEEPIDLE, 10, "TCP_KEEPIDLE");
    seti(fd, IPPROTO_TCP, TCP_KEEPINTVL, 3, "TCP_KEEPINTVL");
    seti(fd, IPPROTO_TCP, TCP_KEEPCNT,  3, "TCP_KEEPCNT");
#endif

    // SO_LINGER: close() pe kya -- {1, 0} = turant RST bhejo (no TIME_WAIT).
    // Careful: unsent data drop. Kuch HFT gateways fast-cycle reconnect ke liye use karte.
    linger lg{1, 0};
    if (::setsockopt(fd, SOL_SOCKET, SO_LINGER, &lg, sizeof lg) == 0)
        std::puts("  set SO_LINGER         = {on, 0}  (RST on close, no TIME_WAIT)");

    std::puts("\n=== read back ===");
    std::printf("  SO_RCVBUF        = %d\n", geti(fd, SOL_SOCKET, SO_RCVBUF));
    std::printf("  SO_SNDBUF        = %d\n", geti(fd, SOL_SOCKET, SO_SNDBUF));
    std::printf("  TCP_NODELAY      = %d\n", geti(fd, IPPROTO_TCP, TCP_NODELAY));

    ::close(fd);

    std::puts(
        "\nKya seekha:\n"
        "  - TCP_NODELAY: har socket pe, bina exception. #1 HFT socket option.\n"
        "  - SO_RCVBUF bada: market-data burst me kernel queue overflow se bacho.\n"
        "    (`net.core.rmem_max` bhi badhao warna cap lag jaata -- 29/06.)\n"
        "  - SO_REUSEPORT: kai worker processes ek port pe accept/recv -> kernel\n"
        "    load-balance karta. Scaling ke liye.\n"
        "  - SO_BUSY_POLL: recv() block hone ke bajaye ~N us spin -> wakeup latency\n"
        "    (~1-5 us syscall+sched) hot path se hatata. Power ki keemat pe.\n"
        "  - TCP_QUICKACK per-call reset hota -- har recv ke baad dobara set,\n"
        "    ya TCP_NODELAY pe bharosa. TCP_INFO se RTT/cwnd/retransmits inspect.\n"
        "  - SO_LINGER{1,0}: close pe RST, TIME_WAIT skip -- fast reconnect, par\n"
        "    unsent data loss. Soch samajh ke.");

    std::printf("\n[NOTE] default buffer sizes distro/sysctl pe depend; TYPICAL values dikhaye.\n");
    return 0;
}

/* ============================================================
 * EXPECTED (typical Linux) -- NAHI napa gaya
 * ------------------------------------------------------------
 * === defaults (fresh TCP socket) ===
 *   SO_RCVBUF        = 131072 bytes  (kernel doubles for bookkeeping)
 *   SO_SNDBUF        = 16384 bytes
 *   TCP_NODELAY      = 0  (0 = Nagle ON)
 *   TCP_MAXSEG(MSS)  = 536
 *   SO_REUSEADDR     = 0
 *
 * === HFT-relevant settings ===
 *   set TCP_NODELAY        = 1
 *   set TCP_QUICKACK       = 1
 *   ... (SO_RCVBUF readback ~ min(2*requested, rmem_max*2))
 *
 * SO_RCVBUF readback aksar 2x requested (kernel bookkeeping) ya rmem_max pe capped.
 * ============================================================ */
