// 07_epoll_server.linux.cpp
// ============================================================
// Poora epoll event loop: ek listening socket + N non-blocking client
// sockets, sab ek epoll instance me. Edge-triggered (EPOLLET) --
// har readable fd ko EAGAIN tak drain karo. Yeh HFT event loop ka skeleton.
// ============================================================
//  LINUX-ONLY (sys/epoll.h -- Windows pe epoll hai hi nahi; IOCP alag model).
//  Linux / WSL pe:
//      g++ -std=c++20 -O2 -Wall -Wextra 07_epoll_server.linux.cpp -o epoll_srv
//      ./epoll_srv 9400
//      # kai clients:  for i in $(seq 50); do (echo hi | nc 127.0.0.1 9400 &); done
// ============================================================

#define _GNU_SOURCE 1   // glibc: expose MAP_POPULATE/ip_mreqn/accept4/recvmmsg/CLOCK_* under strict -std=c++20
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/socket.h>

static int set_nonblock(int fd) {
    int fl = ::fcntl(fd, F_GETFL, 0);
    return ::fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

int main(int argc, char** argv) {
    const uint16_t port = static_cast<uint16_t>(argc > 1 ? std::atoi(argv[1]) : 9400);

    int lfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    int one = 1;
    ::setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    ::setsockopt(lfd, SOL_SOCKET, SO_REUSEPORT, &one, sizeof one);   // multi-process accept scaling

    sockaddr_in a{};
    a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_ANY); a.sin_port = htons(port);
    if (::bind(lfd, reinterpret_cast<sockaddr*>(&a), sizeof a) < 0) { perror("bind"); return 1; }
    if (::listen(lfd, 512) < 0) { perror("listen"); return 1; }

    int ep = ::epoll_create1(EPOLL_CLOEXEC);
    if (ep < 0) { perror("epoll_create1"); return 1; }

    epoll_event ev{};
    ev.events  = EPOLLIN;                  // listener: level-triggered accept theek
    ev.data.fd = lfd;
    ::epoll_ctl(ep, EPOLL_CTL_ADD, lfd, &ev);

    std::printf("epoll echo server on :%u  (EPOLLET clients)\n", port);

    constexpr int MAXEV = 256;
    epoll_event events[MAXEV];
    long conns = 0, total_bytes = 0;

    for (;;) {
        int nfd = ::epoll_wait(ep, events, MAXEV, -1);      // -1 = block; HFT: 0 + busy-poll
        if (nfd < 0) { if (errno == EINTR) continue; perror("epoll_wait"); break; }

        for (int i = 0; i < nfd; ++i) {
            int fd = events[i].data.fd;

            if (fd == lfd) {
                // saare pending connections accept karo (level-triggered, par loop anyway)
                for (;;) {
                    int cfd = ::accept4(lfd, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
                    if (cfd < 0) { if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                                   if (errno == EINTR) continue; perror("accept4"); break; }
                    int nd = 1; ::setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, &nd, sizeof nd);
                    epoll_event cev{};
                    cev.events  = EPOLLIN | EPOLLET;        // edge-triggered client
                    cev.data.fd = cfd;
                    ::epoll_ctl(ep, EPOLL_CTL_ADD, cfd, &cev);
                    ++conns;
                }
                continue;
            }

            if (events[i].events & (EPOLLHUP | EPOLLERR)) {
                ::epoll_ctl(ep, EPOLL_CTL_DEL, fd, nullptr);
                ::close(fd);
                continue;
            }

            // EPOLLET: fd ko EAGAIN tak drain karo, warna baaki data ka event dobara nahi aayega
            bool closed = false;
            for (;;) {
                char buf[16384];
                ssize_t n = ::recv(fd, buf, sizeof buf, 0);
                if (n > 0) {
                    total_bytes += n;
                    // echo back (short-write ko yahan simple rakha -- real me buffer+EPOLLOUT)
                    ssize_t off = 0;
                    while (off < n) {
                        ssize_t k = ::send(fd, buf + off, static_cast<size_t>(n - off), MSG_NOSIGNAL);
                        if (k < 0) { if (errno == EINTR) continue;
                                     if (errno == EAGAIN) break;   // TX buffer full -- real code: queue + EPOLLOUT
                                     closed = true; break; }
                        off += k;
                    }
                } else if (n == 0) {
                    closed = true; break;                  // peer FIN
                } else {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) break;   // drained -- normal
                    if (errno == EINTR) continue;
                    closed = true; break;
                }
            }
            if (closed) {
                ::epoll_ctl(ep, EPOLL_CTL_DEL, fd, nullptr);
                ::close(fd);
                std::printf("  fd %d closed. live conns approx %ld, total echoed %ld bytes\r",
                            fd, --conns, total_bytes);
                std::fflush(stdout);
            }
        }
    }
    ::close(ep); ::close(lfd);
    return 0;
}

/* ============================================================
 * EXPECTED (typical Linux) -- NAHI napa gaya
 * ------------------------------------------------------------
 * epoll echo server on :9400  (EPOLLET clients)
 *   fd 53 closed. live conns approx 0, total echoed 12840 bytes
 *
 * epoll_wait O(ready fds), select/poll O(all fds) -- 10k idle connections pe
 * epoll ~flat, poll ~linear scan har call. Yehi wajah epoll HFT/servers ka
 * default hai. HFT hot path: epoll_wait(timeout=0) + busy-poll, ya kernel bypass.
 * ============================================================ */
