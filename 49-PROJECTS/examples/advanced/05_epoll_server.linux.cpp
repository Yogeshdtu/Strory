// 05_epoll_server.linux.cpp  --  49-PROJECTS advanced P5   (LINUX ONLY)
// ============================================================
// Single-threaded, non-blocking TCP echo server on epoll. Handles many
// clients, partial reads/writes, per-fd output buffering, EAGAIN back-off,
// EPOLLET (edge-triggered), and graceful shutdown via a self-pipe on SIGINT.
// NO thread per connection. This is the OS event-loop core a feed handler /
// order gateway sits on (folder 42).
//
// *.linux.cpp -> checkall / build.ps1 folder SKIP this. Verify on Linux/WSL:
//   g++ -std=c++20 -O2 -Wall -Wextra 05_epoll_server.linux.cpp -o srv && ./srv 9099 &
//   printf 'hello\nworld\n' | nc 127.0.0.1 9099
// ============================================================

#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>

namespace {

int g_shutdown_pipe[2] = {-1, -1};

void on_sigint(int) {
    const char b = 1;
    ssize_t n = ::write(g_shutdown_pipe[1], &b, 1);   // async-signal-safe
    (void)n;
}

bool set_nonblock(int fd) {
    int fl = ::fcntl(fd, F_GETFL, 0);
    return fl != -1 && ::fcntl(fd, F_SETFL, fl | O_NONBLOCK) != -1;
}

struct Conn {
    std::string outbuf;      // pending bytes we couldn't write yet
    bool        want_write = false;
};

void update_epoll(int ep, int fd, bool want_write) {
    epoll_event ev{};
    ev.events  = EPOLLIN | EPOLLRDHUP | EPOLLET | (want_write ? EPOLLOUT : 0u);
    ev.data.fd = fd;
    ::epoll_ctl(ep, EPOLL_CTL_MOD, fd, &ev);
}

void close_conn(int ep, int fd, std::unordered_map<int, Conn>& conns) {
    ::epoll_ctl(ep, EPOLL_CTL_DEL, fd, nullptr);
    ::close(fd);
    conns.erase(fd);
}

// Drain the socket (EPOLLET: must read until EAGAIN), echo into outbuf, flush.
void handle_readable(int ep, int fd, std::unordered_map<int, Conn>& conns) {
    Conn& c = conns[fd];
    char buf[16384];
    for (;;) {
        ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n > 0) {
            c.outbuf.append(buf, static_cast<size_t>(n));
        } else if (n == 0) {
            close_conn(ep, fd, conns);            // peer closed
            return;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;                                 // drained
        } else if (errno == EINTR) {
            continue;
        } else {
            close_conn(ep, fd, conns);
            return;
        }
    }
    // try to flush
    while (!c.outbuf.empty()) {
        ssize_t n = ::send(fd, c.outbuf.data(), c.outbuf.size(), MSG_NOSIGNAL);
        if (n > 0) {
            c.outbuf.erase(0, static_cast<size_t>(n));
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;                                 // kernel buffer full -> wait for EPOLLOUT
        } else if (errno == EINTR) {
            continue;
        } else {
            close_conn(ep, fd, conns);
            return;
        }
    }
    const bool need_out = !c.outbuf.empty();
    if (need_out != c.want_write) { c.want_write = need_out; update_epoll(ep, fd, need_out); }
}

void handle_writable(int ep, int fd, std::unordered_map<int, Conn>& conns) {
    handle_readable(ep, fd, conns);                 // same flush logic; harmless extra recv
}

} // namespace

int main(int argc, char** argv) {
    const int port = argc > 1 ? std::atoi(argv[1]) : 9099;

    if (::pipe(g_shutdown_pipe) != 0) { std::perror("pipe"); return 1; }
    set_nonblock(g_shutdown_pipe[0]);
    std::signal(SIGINT, on_sigint);
    std::signal(SIGPIPE, SIG_IGN);

    int lst = ::socket(AF_INET, SOCK_STREAM, 0);
    if (lst < 0) { std::perror("socket"); return 1; }
    int one = 1;
    ::setsockopt(lst, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (::bind(lst, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) { std::perror("bind"); return 1; }
    if (::listen(lst, 512) != 0) { std::perror("listen"); return 1; }
    set_nonblock(lst);

    int ep = ::epoll_create1(0);
    if (ep < 0) { std::perror("epoll_create1"); return 1; }

    auto add = [&](int fd, uint32_t events) {
        epoll_event ev{};
        ev.events = events;
        ev.data.fd = fd;
        ::epoll_ctl(ep, EPOLL_CTL_ADD, fd, &ev);
    };
    add(lst, EPOLLIN);
    add(g_shutdown_pipe[0], EPOLLIN);

    std::unordered_map<int, Conn> conns;
    std::printf("echo server on :%d  (Ctrl-C to stop)\n", port);

    epoll_event events[256];
    bool running = true;
    while (running) {
        int n = ::epoll_wait(ep, events, 256, -1);
        if (n < 0) { if (errno == EINTR) continue; std::perror("epoll_wait"); break; }
        for (int i = 0; i < n; ++i) {
            const int fd = events[i].data.fd;
            const uint32_t ev = events[i].events;

            if (fd == g_shutdown_pipe[0]) { running = false; break; }

            if (fd == lst) {
                for (;;) {                          // accept4 loop (thundering herd)
                    int cfd = ::accept4(lst, nullptr, nullptr, SOCK_NONBLOCK);
                    if (cfd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        if (errno == EINTR) continue;
                        std::perror("accept4");
                        break;
                    }
                    int nd = 1;
                    ::setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, &nd, sizeof(nd));
                    add(cfd, EPOLLIN | EPOLLRDHUP | EPOLLET);
                    conns.emplace(cfd, Conn{});
                }
                continue;
            }

            if (ev & (EPOLLHUP | EPOLLERR)) { close_conn(ep, fd, conns); continue; }
            if (ev & EPOLLIN)               handle_readable(ep, fd, conns);
            if ((ev & EPOLLOUT) && conns.count(fd)) handle_writable(ep, fd, conns);
            if ((ev & EPOLLRDHUP) && conns.count(fd) && conns[fd].outbuf.empty())
                close_conn(ep, fd, conns);
        }
    }

    for (auto& [fd, c] : conns) { (void)c; ::close(fd); }
    ::close(lst);
    ::close(ep);
    ::close(g_shutdown_pipe[0]);
    ::close(g_shutdown_pipe[1]);
    std::puts("server stopped");
    return 0;
}

// ============================================================
// TALKING POINTS
//   - EPOLLET (edge-triggered): you get ONE notification per readiness edge,
//     so you MUST loop recv()/accept4() until EAGAIN or you lose data.
//   - Partial writes: send() can accept fewer bytes than offered. Keep the
//     remainder in Conn::outbuf and register EPOLLOUT; deregister it once the
//     buffer drains (don't leave EPOLLOUT on -> busy-loop).
//   - SIGINT via a self-pipe: the handler only does an async-signal-safe
//     write(); the event loop treats the pipe fd like any other -> clean stop.
//   - SIGPIPE ignored + MSG_NOSIGNAL: a write to a closed peer returns EPIPE
//     instead of killing the process.
//   - One thread, one epoll, N connections. Multi-core = SO_REUSEPORT + one
//     epoll per thread (folder 42). io_uring is the modern successor.
// ============================================================
