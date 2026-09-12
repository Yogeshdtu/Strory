// 01_tcp_echo_server.linux.cpp
// ============================================================
// Basic TCP echo server: socket -> setsockopt(SO_REUSEADDR) -> bind ->
// listen -> accept loop -> recv/send (short-read/short-write handled).
// Ek client at a time (blocking). epoll wala multi-client -> example 07.
// ============================================================
//  LINUX-ONLY (<sys/socket.h>, <netinet/in.h>, <arpa/inet.h>).
//  Linux / WSL pe:
//      g++ -std=c++20 -O2 -Wall -Wextra 01_tcp_echo_server.linux.cpp -o echo_srv
//      ./echo_srv 9099
//      # doosre terminal me:  nc 127.0.0.1 9099   ya  example 02
// ============================================================

#define _GNU_SOURCE 1   // glibc: expose MAP_POPULATE/ip_mreqn/accept4/recvmmsg/CLOCK_* under strict -std=c++20
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

// Poora buffer bhejo -- send() short-write kar sakta.
static bool send_all(int fd, const char* p, size_t n) {
    size_t off = 0;
    while (off < n) {
        ssize_t k = ::send(fd, p + off, n - off, MSG_NOSIGNAL);   // MSG_NOSIGNAL: no SIGPIPE
        if (k < 0) { if (errno == EINTR) continue; return false; }
        off += static_cast<size_t>(k);
    }
    return true;
}

int main(int argc, char** argv) {
    const uint16_t port = static_cast<uint16_t>(argc > 1 ? std::atoi(argv[1]) : 9099);

    int lfd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (lfd < 0) { perror("socket"); return 1; }

    int one = 1;
    ::setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);  // TIME_WAIT ke bawajood rebind

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);        // sab interfaces
    addr.sin_port = htons(port);                     // host->network byte order

    if (::bind(lfd, reinterpret_cast<sockaddr*>(&addr), sizeof addr) < 0) { perror("bind"); return 1; }
    if (::listen(lfd, 128) < 0) { perror("listen"); return 1; }     // backlog 128

    std::printf("echo server listening on 0.0.0.0:%u  (Ctrl-C to stop)\n", port);

    for (;;) {
        sockaddr_in cli{};
        socklen_t clen = sizeof cli;
        int cfd = ::accept4(lfd, reinterpret_cast<sockaddr*>(&cli), &clen, SOCK_CLOEXEC);
        if (cfd < 0) { if (errno == EINTR) continue; perror("accept"); break; }

        char ip[INET_ADDRSTRLEN];
        ::inet_ntop(AF_INET, &cli.sin_addr, ip, sizeof ip);
        std::printf("  + %s:%u connected (fd %d)\n", ip, ntohs(cli.sin_port), cfd);

        int nd = 1;
        ::setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, &nd, sizeof nd);  // Nagle off (example 03)

        char buf[65536];
        for (;;) {
            ssize_t n = ::recv(cfd, buf, sizeof buf, 0);
            if (n == 0) { std::printf("  - fd %d closed by peer\n", cfd); break; }   // orderly FIN
            if (n < 0)  { if (errno == EINTR) continue; perror("recv"); break; }
            if (!send_all(cfd, buf, static_cast<size_t>(n))) { perror("send"); break; }
        }
        ::close(cfd);
    }
    ::close(lfd);
    return 0;
}

/* ============================================================
 * EXPECTED (typical Linux) -- aapke box pe NAHI napa gaya
 * ------------------------------------------------------------
 * echo server listening on 0.0.0.0:9099  (Ctrl-C to stop)
 *   + 127.0.0.1:53124 connected (fd 4)
 *   - fd 4 closed by peer
 *
 * `nc 127.0.0.1 9099` se jo type karoge woh echo hoke wapas aayega.
 * Loopback RTT for a small echo: ~10-30 us (full kernel TCP stack both ways);
 * compare UDS ~2-5 us (29/09), shared-mem ~ns.
 * ============================================================ */
