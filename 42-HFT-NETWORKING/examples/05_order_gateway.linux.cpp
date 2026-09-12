// 05_order_gateway.linux.cpp
// ============================================================
// A minimal TCP "order gateway": TCP_NODELAY, keepalive tuning, and
// combining a fixed header + variable body into ONE write (`writev`) so
// it goes out as ONE packet regardless of Nagle state -- two separate
// small `send()` calls is the classic mistake that RE-INTRODUCES the
// Nagle-vs-delayed-ACK stall 30/03 already measured, even with
// TCP_NODELAY set on the SECOND send (the first send's ACK is still
// pending). (11-tcp-for-order-gateways.md)
// ============================================================
//  LINUX-ONLY (POSIX sockets, TCP_NODELAY, writev).
//  Linux / WSL pe:
//      g++ -std=c++20 -O2 -Wall -Wextra -pthread 05_order_gateway.linux.cpp -o gateway
// ============================================================

#define _GNU_SOURCE 1
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <unistd.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

struct OrderHeader { std::uint32_t seq; std::uint16_t symbol_len; std::uint16_t qty; };

static std::uint64_t now_ns() {
    return static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
}

// Server: accept ONE connection, tune it, echo back every message
// (ack-style, like a gateway confirming receipt) as fast as possible.
static void gateway_server(int listen_fd, std::atomic<bool>& stop) {
    sockaddr_in peer{}; socklen_t pl = sizeof peer;
    int cfd = ::accept4(listen_fd, reinterpret_cast<sockaddr*>(&peer), &pl, SOCK_CLOEXEC);
    if (cfd < 0) return;

    int one = 1;
    ::setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);        // no Nagle buffering
    int ka = 1; ::setsockopt(cfd, SOL_SOCKET, SO_KEEPALIVE, &ka, sizeof ka);
    int idle = 10, intvl = 3, cnt = 3;                                     // aggressive-ish for a gateway
    ::setsockopt(cfd, IPPROTO_TCP, TCP_KEEPIDLE,  &idle,  sizeof idle);
    ::setsockopt(cfd, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof intvl);
    ::setsockopt(cfd, IPPROTO_TCP, TCP_KEEPCNT,   &cnt,   sizeof cnt);
#ifdef TCP_USER_TIMEOUT
    unsigned user_timeout_ms = 5000;   // fail a stalled connection in seconds, not minutes
    ::setsockopt(cfd, IPPROTO_TCP, TCP_USER_TIMEOUT, &user_timeout_ms, sizeof user_timeout_ms);
#endif

    char buf[256];
    while (!stop.load(std::memory_order_relaxed)) {
        const ssize_t n = ::recv(cfd, buf, sizeof buf, 0);
        if (n <= 0) break;
        ::send(cfd, buf, static_cast<std::size_t>(n), 0);   // ack echo
    }
    ::close(cfd);
}

int main() {
    constexpr std::uint16_t port = 9404;
    constexpr int kN = 20000;

    int lfd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    int one = 1; ::setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    sockaddr_in a{}; a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK); a.sin_port = htons(port);
    if (::bind(lfd, reinterpret_cast<sockaddr*>(&a), sizeof a) < 0) { std::perror("bind"); return 1; }
    ::listen(lfd, 1);

    std::atomic<bool> stop{false};
    std::thread srv(gateway_server, lfd, std::ref(stop));

    int cfd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    sockaddr_in to{}; to.sin_family = AF_INET; to.sin_port = htons(port);
    ::inet_pton(AF_INET, "127.0.0.1", &to.sin_addr);
    if (::connect(cfd, reinterpret_cast<sockaddr*>(&to), sizeof to) < 0) { std::perror("connect"); return 1; }
    ::setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);

    const char symbol[] = "RELIANCE";
    std::vector<double> lat_writev; lat_writev.reserve(kN);
    std::vector<double> lat_two_sends; lat_two_sends.reserve(kN);

    // ---- A) header + body in ONE writev() -- ONE packet ----
    for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(kN); ++i) {
        OrderHeader h{i, sizeof symbol, static_cast<std::uint16_t>(100 + (i % 50))};
        iovec iov[2] = {{&h, sizeof h}, {const_cast<char*>(symbol), sizeof symbol}};
        const auto t0 = now_ns();
        ::writev(cfd, iov, 2);
        char ack[256];
        ::recv(cfd, ack, sizeof ack, 0);
        lat_writev.push_back(static_cast<double>(now_ns() - t0));
    }

    // ---- B) header then body -- TWO separate send() calls ----
    for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(kN); ++i) {
        OrderHeader h{i, sizeof symbol, static_cast<std::uint16_t>(100 + (i % 50))};
        const auto t0 = now_ns();
        ::send(cfd, &h, sizeof h, 0);
        ::send(cfd, symbol, sizeof symbol, 0);
        char ack[256]; std::size_t got = 0;
        while (got < sizeof h + sizeof symbol) {
            const ssize_t n = ::recv(cfd, ack + got, sizeof ack - got, 0);
            if (n <= 0) break;
            got += static_cast<std::size_t>(n);
        }
        lat_two_sends.push_back(static_cast<double>(now_ns() - t0));
    }

    stop.store(true, std::memory_order_relaxed);
    ::shutdown(cfd, SHUT_RDWR);
    ::close(cfd);
    srv.join();
    ::close(lfd);

    auto report = [](const char* name, std::vector<double>& v) {
        if (v.empty()) { std::printf("  %-28s : (no samples)\n", name); return; }
        std::sort(v.begin(), v.end());
        auto pc = [&](double p) {
            std::size_t i = static_cast<std::size_t>(p / 100.0 * static_cast<double>(v.size()));
            return v[std::min(i, v.size() - 1)];
        };
        std::printf("  %-28s : p50 %7.0f  p99 %9.0f  p99.9 %10.0f  ns\n",
                    name, pc(50), pc(99), pc(99.9));
    };
    report("A) writev (1 packet)", lat_writev);
    report("B) 2x send (2 packets)", lat_two_sends);

    std::puts(
        "\nKya seekha:\n"
        "  - TCP_NODELAY server AUR client dono taraf set kiya -- Nagle DONO\n"
        "    directions mein disable, warna ek taraf ka buffering doosri taraf\n"
        "    ke round-trip ko bhi delay kar sakta.\n"
        "  - A (writev, ek packet): header+body EK saath jaate -- kernel unhe\n"
        "    ek hi TCP segment mein daal sakta (agar chhote hain), ek hi ACK cycle.\n"
        "  - B (do send calls): TCP_NODELAY sender ko WAIT karne se rokta (Nagle\n"
        "    off), isliye yahan 40ms-scale stall NAHI hota -- woh interaction\n"
        "    SPECIFICALLY tab hoti jab sender khud Nagle-limited ho (30/03, jahan\n"
        "    TCP_NODELAY OFF tha). Yahan B ka overhead chhota hai: EXTRA syscall\n"
        "    + potentially 2 packets/2 ACKs bandwidth pe (real, non-loopback\n"
        "    network pe zyaada matter karta) -- writev ki jeet 'crash-save' nahi,\n"
        "    'thoda consistently behtar + ek DISCIPLINE' hai.\n"
        "  - Keepalive (TCP_KEEPIDLE/INTVL/CNT) + TCP_USER_TIMEOUT: ek stalled\n"
        "    gateway connection ko SECONDS mein fail karna, minutes mein nahi\n"
        "    (37/13's fail-closed risk principle -- ek dead connection pe orders\n"
        "    bhejte rehna khatarnak hai).");
    std::printf("\n[NOTE] Windows/MinGW dev box pe likha gaya, Linux/WSL pe verify karo.\n"
                "Numbers is machine pe NAHI nape gaye.\n");
    return 0;
}

/* ============================================================
 * EXPECTED (typical Linux, loopback, both sides TCP_NODELAY) -- NAHI napa gaya
 * ------------------------------------------------------------
 * A) writev (1 packet)         : p50      15  p99       45  p99.9       120  ns
 * B) 2x send (2 packets)       : p50      28  p99       95  p99.9       310  ns
 *
 * B roughly 1.5-2x worse even WITH TCP_NODELAY on loopback (extra
 * syscall + occasional extra packet/ACK cycle) -- NOT a 40ms-scale stall,
 * because TCP_NODELAY on the SENDER is exactly what prevents that specific
 * interaction (30/03's ~40ms number happens when the SENDER has Nagle ON,
 * not the receiver's delayed-ACK alone). Over a real (non-loopback)
 * network, B's extra packet costs real bandwidth+ACK round-trips that
 * loopback hides almost entirely -- the gap would widen, but from "1.5-2x"
 * territory, not into 30/03's 40ms territory (that needs Nagle actually
 * enabled somewhere in the path, which this example deliberately avoids).
 * ============================================================ */
