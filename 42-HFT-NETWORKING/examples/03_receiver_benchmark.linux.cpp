// 03_receiver_benchmark.linux.cpp
// ============================================================
// Three receive strategies, SAME one-way multicast path, latency
// compared (07-busy-poll-sockets.md):
//   A) blocking recv()                       -- baseline, interrupt-driven
//   B) app-level busy-poll (MSG_DONTWAIT spin) -- 30/10's mechanism,
//      burns ~100% of one core CONTINUOUSLY, works on ANY interface
//   C) blocking recv() + SO_BUSY_POLL hint    -- KERNEL busy-polls for a
//      bounded budget before falling back to interrupt; app code stays
//      a normal blocking call. Only helps if the NIC driver implements
//      `ndo_busy_poll` -- expected ~= A on loopback (02's finding).
// ============================================================
//  LINUX-ONLY.
//  Linux / WSL pe:
//      g++ -std=c++20 -O2 -Wall -Wextra -pthread 03_receiver_benchmark.linux.cpp -o rxbench
// ============================================================

#define _GNU_SOURCE 1
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cerrno>
#include <atomic>
#include <algorithm>
#include <chrono>
#include <thread>
#include <vector>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/socket.h>

#ifndef SO_BUSY_POLL
#define SO_BUSY_POLL 46
#endif

struct Msg { std::uint64_t seq; std::uint64_t t_send_ns; };

static std::uint64_t now_ns() {
    return static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
}

static int join_mcast_socket(const char* group, std::uint16_t port, const char* ifname, bool nonblock) {
    int fd = ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC | (nonblock ? SOCK_NONBLOCK : 0), 0);
    int one = 1; ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons(port); a.sin_addr.s_addr = htonl(INADDR_ANY);
    ::bind(fd, reinterpret_cast<sockaddr*>(&a), sizeof a);
    ip_mreqn mreq{};
    ::inet_pton(AF_INET, group, &mreq.imr_multiaddr);
    mreq.imr_address.s_addr = htonl(INADDR_ANY);
    mreq.imr_ifindex = static_cast<int>(::if_nametoindex(ifname));
    ::setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof mreq);
    if (!nonblock) {   // blocking variants (A, C) -- safety net, never hang forever
        timeval tv{2, 0};
        ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    }
    return fd;
}

constexpr int  kN         = 50000;
constexpr int  kPaceUs    = 20;   // ~50K msg/s send rate

static void sender(const char* group, std::uint16_t port, const char* ifname, std::atomic<bool>& go) {
    int fd = ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    ip_mreqn txif{}; txif.imr_ifindex = static_cast<int>(::if_nametoindex(ifname));
    ::setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &txif, sizeof txif);
    int ttl = 1; ::setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof ttl);
    int lb  = 1; ::setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &lb, sizeof lb);
    sockaddr_in to{}; to.sin_family = AF_INET; to.sin_port = htons(port);
    ::inet_pton(AF_INET, group, &to.sin_addr);

    while (!go.load(std::memory_order_acquire)) std::this_thread::yield();
    for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(kN); ++i) {
        Msg m{i, now_ns()};
        ::sendto(fd, &m, sizeof m, 0, reinterpret_cast<sockaddr*>(&to), sizeof to);
        std::this_thread::sleep_for(std::chrono::microseconds(kPaceUs));
    }
    ::close(fd);
}

static void report(const char* name, std::vector<double>& ns) {
    if (ns.empty()) { std::printf("  %-30s : (no samples -- check multicast setup)\n", name); return; }
    std::sort(ns.begin(), ns.end());
    auto pc = [&](double p) {
        std::size_t i = static_cast<std::size_t>(p / 100.0 * static_cast<double>(ns.size()));
        return ns[std::min(i, ns.size() - 1)];
    };
    std::printf("  %-30s : p50 %8.0f  p99 %10.0f  p99.9 %11.0f  max %12.0f  ns  (n=%zu)\n",
                name, pc(50), pc(99), pc(99.9), ns.back(), ns.size());
}

int main() {
    const char* group  = "239.1.2.6";
    const std::uint16_t port = 9402;
    const char* ifname = "lo";

    // ---- A) blocking recv, no hints ----
    {
        int fd = join_mcast_socket(group, port, ifname, /*nonblock=*/false);
        std::atomic<bool> go{false};
        std::thread tx(sender, group, port, ifname, std::ref(go));
        go.store(true, std::memory_order_release);
        std::vector<double> lat; lat.reserve(kN);
        Msg m;
        for (int i = 0; i < kN; ++i) {
            const ssize_t n = ::recv(fd, &m, sizeof m, 0);
            const auto trecv = now_ns();
            if (n == static_cast<ssize_t>(sizeof m)) lat.push_back(static_cast<double>(trecv - m.t_send_ns));
        }
        tx.join(); ::close(fd);
        report("A) blocking recv", lat);
    }

    // ---- B) app-level busy-poll (non-blocking spin) ----
    {
        int fd = join_mcast_socket(group, port, ifname, /*nonblock=*/true);
        std::atomic<bool> go{false};
        std::thread tx(sender, group, port, ifname, std::ref(go));
        go.store(true, std::memory_order_release);
        std::vector<double> lat; lat.reserve(kN);
        Msg m;
        for (int i = 0; i < kN; ++i) {
            ssize_t n;
            for (;;) { n = ::recv(fd, &m, sizeof m, 0); if (n >= 0 || errno != EAGAIN) break; }
            const auto trecv = now_ns();
            if (n == static_cast<ssize_t>(sizeof m)) lat.push_back(static_cast<double>(trecv - m.t_send_ns));
        }
        tx.join(); ::close(fd);
        report("B) app busy-poll (spin)", lat);
    }

    // ---- C) blocking recv + SO_BUSY_POLL hint ----
    {
        int fd = join_mcast_socket(group, port, ifname, /*nonblock=*/false);
        int budget_us = 50;
        ::setsockopt(fd, SOL_SOCKET, SO_BUSY_POLL, &budget_us, sizeof budget_us);
        std::atomic<bool> go{false};
        std::thread tx(sender, group, port, ifname, std::ref(go));
        go.store(true, std::memory_order_release);
        std::vector<double> lat; lat.reserve(kN);
        Msg m;
        for (int i = 0; i < kN; ++i) {
            const ssize_t n = ::recv(fd, &m, sizeof m, 0);
            const auto trecv = now_ns();
            if (n == static_cast<ssize_t>(sizeof m)) lat.push_back(static_cast<double>(trecv - m.t_send_ns));
        }
        tx.join(); ::close(fd);
        report("C) blocking + SO_BUSY_POLL", lat);
    }

    std::puts(
        "\nKya hua:\n"
        "  - B (app-level spin) A se BEHTAR hona chahiye (30/10 ka SAME finding,\n"
        "    ab one-way multicast pe) -- koi interrupt/wakeup round-trip nahi.\n"
        "  - C, is machine (loopback) pe, A ke KAAFI KAREEB rehna chahiye -- SO_BUSY_POLL\n"
        "    ka fayda driver-dependent hai (02 se), aur `lo` woh support nahi karta.\n"
        "  - Real NIC hardware pe: C, B ke KAREEB aa jaata (kernel khud driver poll\n"
        "    karta), PAR app ka apna CPU sirf recv() ke andar THODI der (budget)\n"
        "    ke liye busy hota -- B jaisa HAMESHA 100%% spin nahi.");
    std::printf("\n[NOTE] Windows/MinGW dev box pe likha gaya. Numbers is machine pe NAHI\n"
                "nape gaye -- Linux/WSL (ideally real NIC, loopback nahi) pe verify karo.\n");
    return 0;
}

/* ============================================================
 * EXPECTED (typical Linux, if=lo) -- NAHI napa gaya
 * ------------------------------------------------------------
 * A) blocking recv                : p50    9000  p99    28000  p99.9    120000  max   1500000  ns
 * B) app busy-poll (spin)         : p50    3500  p99     9000  p99.9     22000  max    180000  ns
 * C) blocking + SO_BUSY_POLL      : p50    8800  p99    27000  p99.9    115000  max   1400000  ns
 *
 * C ~= A on loopback (no driver busy-poll support). On real NIC hardware
 * with a busy-poll-capable driver, C would move MUCH closer to B's
 * numbers, without the app spinning at 100% the whole time.
 * ============================================================ */
