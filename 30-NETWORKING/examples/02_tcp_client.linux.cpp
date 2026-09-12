// 02_tcp_client.linux.cpp
// ============================================================
// TCP client: getaddrinfo -> socket -> connect -> send request ->
// recv full reply. Round-trip latency measure (steady_clock).
// Server = example 01.
// ============================================================
//  LINUX-ONLY (POSIX sockets, netdb.h).
//  Linux / WSL pe:
//      g++ -std=c++20 -O2 -Wall -Wextra 02_tcp_client.linux.cpp -o tcp_cli
//      ./echo_srv 9099 &          # example 01
//      ./tcp_cli 127.0.0.1 9099
// ============================================================

#define _GNU_SOURCE 1   // glibc: expose MAP_POPULATE/ip_mreqn/accept4/recvmmsg/CLOCK_* under strict -std=c++20
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <algorithm>
#include <vector>
#include <unistd.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

static uint64_t now_ns() {
    return static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
}

static bool send_all(int fd, const char* p, size_t n) {
    size_t off = 0;
    while (off < n) {
        ssize_t k = ::send(fd, p + off, n - off, MSG_NOSIGNAL);
        if (k < 0) { if (errno == EINTR) continue; return false; }
        off += static_cast<size_t>(k);
    }
    return true;
}

int main(int argc, char** argv) {
    const char* host = argc > 1 ? argv[1] : "127.0.0.1";
    const char* port = argc > 2 ? argv[2] : "9099";
    const int   iters = argc > 3 ? std::atoi(argv[3]) : 20000;

    // getaddrinfo: hostname/service -> sockaddr(s). IPv4+IPv6 dono handle.
    addrinfo hints{};
    hints.ai_family   = AF_UNSPEC;         // v4 ya v6
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    if (int e = ::getaddrinfo(host, port, &hints, &res); e != 0) {
        std::fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(e));
        return 1;
    }

    int fd = -1;
    for (addrinfo* ai = res; ai; ai = ai->ai_next) {
        fd = ::socket(ai->ai_family, ai->ai_socktype | SOCK_CLOEXEC, ai->ai_protocol);
        if (fd < 0) continue;
        if (::connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) break;   // success
        ::close(fd); fd = -1;
    }
    ::freeaddrinfo(res);
    if (fd < 0) { perror("connect"); return 1; }

    int one = 1;
    ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);      // latency: Nagle off

    std::printf("connected to %s:%s  -- %d request/reply round-trips\n", host, port, iters);

    const char req[] = "PING....................................";   // 40 bytes
    char reply[128];
    std::vector<uint64_t> lat;
    lat.reserve(static_cast<size_t>(iters));

    for (int i = 0; i < iters; ++i) {
        uint64_t t0 = now_ns();
        if (!send_all(fd, req, sizeof req - 1)) { perror("send"); break; }
        size_t got = 0;
        while (got < sizeof req - 1) {                 // echo = same length
            ssize_t n = ::recv(fd, reply + got, sizeof reply - got, 0);
            if (n <= 0) { perror("recv"); ::close(fd); return 1; }
            got += static_cast<size_t>(n);
        }
        lat.push_back(now_ns() - t0);
    }
    ::close(fd);

    std::sort(lat.begin(), lat.end());
    auto q = [&](double p){ return lat[static_cast<size_t>(p * (lat.size() - 1))]; };
    std::printf("  round-trip ns:  p50=%llu  p90=%llu  p99=%llu  p99.9=%llu  max=%llu\n",
                (unsigned long long)q(0.50), (unsigned long long)q(0.90),
                (unsigned long long)q(0.99), (unsigned long long)q(0.999),
                (unsigned long long)lat.back());

    std::puts(
        "\nKya seekha:\n"
        "  - getaddrinfo portable naming: hostname/IP + service/port -> sockaddr,\n"
        "    v4/v6 agnostic. Hardcoded sockaddr_in se behtar.\n"
        "  - Loopback TCP round-trip ~10-30 us p50: send() syscall + TCP stack +\n"
        "    loopback 'transmit' + rx softirq + recv() syscall + wakeup, dono taraf.\n"
        "  - p99.9/max scheduler jitter se (threads unpinned). Real venue link pe\n"
        "    RTT ~microseconds-to-ms wire distance pe depend.\n"
        "  - TCP_NODELAY zaroori: bina uske small request/reply Nagle+delayed-ACK\n"
        "    se ~40 ms tak spike kar sakte (example 03).");

    std::printf("\n[NOTE] Numbers TYPICAL loopback hain, aapke box pe NAHI nape gaye.\n");
    return 0;
}

/* ============================================================
 * EXPECTED (typical Linux, loopback) -- NAHI napa gaya
 * ------------------------------------------------------------
 * connected to 127.0.0.1:9099  -- 20000 request/reply round-trips
 *   round-trip ns:  p50=18000  p90=24000  p99=52000  p99.9=180000  max=900000
 *
 * p50 ~15-30 us loopback. p99.9 tail = OS scheduler (pin threads to tighten).
 * ============================================================ */
