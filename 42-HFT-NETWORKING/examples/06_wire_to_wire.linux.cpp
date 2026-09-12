// 06_wire_to_wire.linux.cpp
// ============================================================
// CAPSTONE: full wire-to-wire (tick-to-trade, 37/14) breakdown using
// REAL kernel timestamps at every hop -- combines 01's multicast
// receive, 04's TX+RX same-clock-domain timestamping technique, and a
// simulated "internal processing" stage, across THREE roles:
//
//   [Market data sender] --multicast--> [Strategy/gateway] --UDP--> [Exchange sim]
//         TX-ts (kernel)      RX-ts (kernel)   TX-ts (kernel)   RX-ts (kernel)
//         |<------ hop 1 ------>|<-- processing -->|<---- hop 2 ---->|
//         |<---------------------- total wire-to-wire --------------------->|
//
// The 3 roles here run SEQUENTIALLY in one thread (send, then recv, then
// send again, ...) -- a real system would run them as separate processes/
// threads; this demo keeps it single-threaded because loopback delivery
// is fast+reliable enough for a one-at-a-time paced send to work, and the
// KERNEL TIMESTAMPS (not wall-clock ordering) are what make each hop's
// number meaningful regardless of how many threads did the calling.
// (14-wire-to-wire-measurement.md, 16-building-md-receiver.md)
// ============================================================
//  LINUX-ONLY (SO_TIMESTAMPING TX+RX, multicast, MSG_ERRQUEUE).
//  Linux / WSL pe:
//      g++ -std=c++20 -O2 -Wall -Wextra -pthread 06_wire_to_wire.linux.cpp -o w2w
// ============================================================

#define _GNU_SOURCE 1
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <chrono>
#include <thread>
#include <vector>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/socket.h>
#include <linux/net_tstamp.h>
#include <linux/errqueue.h>

struct Msg { std::uint32_t seq; };

static std::uint64_t ts_to_ns(const timespec& t) {
    return static_cast<std::uint64_t>(t.tv_sec) * 1'000'000'000ull + static_cast<std::uint64_t>(t.tv_nsec);
}

static void enable_tx_ts(int fd) {
    int f = SOF_TIMESTAMPING_TX_SOFTWARE | SOF_TIMESTAMPING_SOFTWARE
          | SOF_TIMESTAMPING_TX_HARDWARE | SOF_TIMESTAMPING_RAW_HARDWARE;
    ::setsockopt(fd, SOL_SOCKET, SO_TIMESTAMPING, &f, sizeof f);
}
static void enable_rx_ts(int fd) {
    int f = SOF_TIMESTAMPING_RX_SOFTWARE | SOF_TIMESTAMPING_SOFTWARE
          | SOF_TIMESTAMPING_RX_HARDWARE | SOF_TIMESTAMPING_RAW_HARDWARE;
    ::setsockopt(fd, SOL_SOCKET, SO_TIMESTAMPING, &f, sizeof f);
}

static bool get_tx_ts(int fd, std::uint32_t expect_seq, std::uint64_t& out_ns) {
    char data[64]; char ctrl[256];
    for (int tries = 0; tries < 200; ++tries) {
        iovec iov{data, sizeof data};
        msghdr msg{}; msg.msg_iov = &iov; msg.msg_iovlen = 1;
        msg.msg_control = ctrl; msg.msg_controllen = sizeof ctrl;
        const ssize_t n = ::recvmsg(fd, &msg, MSG_ERRQUEUE);
        if (n < 0) { std::this_thread::sleep_for(std::chrono::microseconds(20)); continue; }
        if (static_cast<std::size_t>(n) < sizeof(Msg)) continue;
        Msg got{}; std::memcpy(&got, data, sizeof got);
        if (got.seq != expect_seq) continue;
        for (cmsghdr* c = CMSG_FIRSTHDR(&msg); c; c = CMSG_NXTHDR(&msg, c)) {
            if (c->cmsg_level == SOL_SOCKET && c->cmsg_type == SO_TIMESTAMPING) {
                auto* t = reinterpret_cast<scm_timestamping*>(CMSG_DATA(c));
                const std::uint64_t sw = ts_to_ns(t->ts[0]);
                const std::uint64_t hw = ts_to_ns(t->ts[2]);
                out_ns = hw ? hw : sw;
                return out_ns != 0;
            }
        }
    }
    return false;
}

static bool recv_with_rx_ts(int fd, Msg& out, std::uint64_t& rx_ns) {
    char data[64]; char ctrl[256];
    iovec iov{data, sizeof data};
    msghdr msg{}; msg.msg_iov = &iov; msg.msg_iovlen = 1;
    msg.msg_control = ctrl; msg.msg_controllen = sizeof ctrl;
    const ssize_t n = ::recvmsg(fd, &msg, 0);
    if (n < static_cast<ssize_t>(sizeof(Msg))) return false;
    std::memcpy(&out, data, sizeof out);
    rx_ns = 0;
    for (cmsghdr* c = CMSG_FIRSTHDR(&msg); c; c = CMSG_NXTHDR(&msg, c)) {
        if (c->cmsg_level == SOL_SOCKET && c->cmsg_type == SO_TIMESTAMPING) {
            auto* t = reinterpret_cast<scm_timestamping*>(CMSG_DATA(c));
            const std::uint64_t sw = ts_to_ns(t->ts[0]);
            const std::uint64_t hw = ts_to_ns(t->ts[2]);
            rx_ns = hw ? hw : sw;
        }
    }
    return rx_ns != 0;
}

int main() {
    constexpr int kN = 1000;
    const char* group = "239.1.2.7";
    constexpr std::uint16_t mcast_port = 9405, order_port = 9406;

    // ---- socket 1: market-data sender (multicast, TX timestamps) ----
    int mfd_tx = ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    enable_tx_ts(mfd_tx);
    ip_mreqn mif{}; mif.imr_ifindex = static_cast<int>(::if_nametoindex("lo"));
    ::setsockopt(mfd_tx, IPPROTO_IP, IP_MULTICAST_IF, &mif, sizeof mif);
    int lb = 1; ::setsockopt(mfd_tx, IPPROTO_IP, IP_MULTICAST_LOOP, &lb, sizeof lb);
    sockaddr_in mto{}; mto.sin_family = AF_INET; mto.sin_port = htons(mcast_port);
    ::inet_pton(AF_INET, group, &mto.sin_addr);

    // ---- socket 2: strategy/gateway's market-data receiver (RX timestamps) ----
    int mfd_rx = ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    int one = 1; ::setsockopt(mfd_rx, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    sockaddr_in mbind{}; mbind.sin_family = AF_INET; mbind.sin_port = htons(mcast_port); mbind.sin_addr.s_addr = htonl(INADDR_ANY);
    ::bind(mfd_rx, reinterpret_cast<sockaddr*>(&mbind), sizeof mbind);
    ip_mreqn mreq{}; ::inet_pton(AF_INET, group, &mreq.imr_multiaddr);
    mreq.imr_address.s_addr = htonl(INADDR_ANY); mreq.imr_ifindex = static_cast<int>(::if_nametoindex("lo"));
    ::setsockopt(mfd_rx, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof mreq);
    enable_rx_ts(mfd_rx);
    timeval rcv_tv{2, 0};   // safety net -- never hang forever on a dropped/lost packet
    ::setsockopt(mfd_rx, SOL_SOCKET, SO_RCVTIMEO, &rcv_tv, sizeof rcv_tv);

    // ---- socket 3: strategy/gateway's order-out sender (TX timestamps) ----
    int ofd_tx = ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    enable_tx_ts(ofd_tx);
    sockaddr_in oto{}; oto.sin_family = AF_INET; oto.sin_port = htons(order_port);
    ::inet_pton(AF_INET, "127.0.0.1", &oto.sin_addr);

    // ---- socket 4: exchange-simulator's order receiver (RX timestamps) ----
    int ofd_rx = ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    ::setsockopt(ofd_rx, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    sockaddr_in obind{}; obind.sin_family = AF_INET; obind.sin_addr.s_addr = htonl(INADDR_LOOPBACK); obind.sin_port = htons(order_port);
    ::bind(ofd_rx, reinterpret_cast<sockaddr*>(&obind), sizeof obind);
    enable_rx_ts(ofd_rx);
    ::setsockopt(ofd_rx, SOL_SOCKET, SO_RCVTIMEO, &rcv_tv, sizeof rcv_tv);   // same safety net

    std::vector<double> hop1_ns, proc_ns, hop2_ns, total_ns;
    hop1_ns.reserve(kN); proc_ns.reserve(kN); hop2_ns.reserve(kN); total_ns.reserve(kN);

    for (std::uint32_t seq = 0; seq < static_cast<std::uint32_t>(kN); ++seq) {
        // 1. "Market data" leaves the wire (multicast send)
        Msg tick{seq};
        ::sendto(mfd_tx, &tick, sizeof tick, 0, reinterpret_cast<sockaddr*>(&mto), sizeof mto);

        // 2. Strategy/gateway receives it
        Msg rx_tick{}; std::uint64_t md_rx_ns = 0;
        if (!recv_with_rx_ts(mfd_rx, rx_tick, md_rx_ns)) continue;

        std::uint64_t md_tx_ns = 0;
        if (!get_tx_ts(mfd_tx, seq, md_tx_ns)) continue;

        // 3. "Processing" (trivial here -- real strategy logic goes here)
        volatile std::uint32_t dummy = rx_tick.seq * 3u + 1u;
        (void)dummy;

        // 4. Order goes out
        Msg order{seq};
        ::sendto(ofd_tx, &order, sizeof order, 0, reinterpret_cast<sockaddr*>(&oto), sizeof oto);

        Msg rx_order{}; std::uint64_t ord_rx_ns = 0;
        if (!recv_with_rx_ts(ofd_rx, rx_order, ord_rx_ns)) continue;

        std::uint64_t ord_tx_ns = 0;
        if (!get_tx_ts(ofd_tx, seq, ord_tx_ns)) continue;

        if (md_rx_ns >= md_tx_ns && ord_tx_ns >= md_rx_ns && ord_rx_ns >= ord_tx_ns) {
            hop1_ns.push_back(static_cast<double>(md_rx_ns - md_tx_ns));
            proc_ns.push_back(static_cast<double>(ord_tx_ns - md_rx_ns));
            hop2_ns.push_back(static_cast<double>(ord_rx_ns - ord_tx_ns));
            total_ns.push_back(static_cast<double>(ord_rx_ns - md_tx_ns));
        }
        std::this_thread::sleep_for(std::chrono::microseconds(300));
    }
    ::close(mfd_tx); ::close(mfd_rx); ::close(ofd_tx); ::close(ofd_rx);

    auto report = [](const char* name, std::vector<double> v) {
        if (v.empty()) { std::printf("  %-28s : (no samples)\n", name); return; }
        std::sort(v.begin(), v.end());
        auto pc = [&](double p) {
            std::size_t i = static_cast<std::size_t>(p / 100.0 * static_cast<double>(v.size()));
            return v[std::min(i, v.size() - 1)];
        };
        std::printf("  %-28s : p50 %8.0f   p99 %10.0f   n=%zu\n", name, pc(50), pc(99), v.size());
    };
    std::printf("Wire-to-wire breakdown (%d ticks attempted, %zu completed full round):\n",
                kN, total_ns.size());
    report("Hop 1 (MD send -> gateway RX)", hop1_ns);
    report("Processing (gateway internal)", proc_ns);
    report("Hop 2 (order send -> exch RX)", hop2_ns);
    report("TOTAL (wire-to-wire)", total_ns);

    std::puts(
        "\nKya seekha:\n"
        "  - Poora breakdown, HAR hop kernel-timestamped -- 'kahan time gaya'\n"
        "    (network transit vs OUR OWN processing) explicitly separable hai,\n"
        "    na ki ek single opaque number.\n"
        "  - 'Processing' stage yahan trivial hai (demo) -- REAL strategy mein\n"
        "    yehi woh number hai jo 37/14's latency budget mein SABSE ZYAADA\n"
        "    tumhare control mein hota (network/kernel dusre factors hain).\n"
        "  - Yeh EXACTLY woh 'timestamp har stage pe, EK clock domain se' pattern\n"
        "    hai jo real HFT wire-to-wire rigs use karte (30's audit-tracked\n"
        "    'wire-to-wire latency measurement' row) -- yahan poori tarah built.");
    std::printf("\n[NOTE] Windows/MinGW dev box pe likha gaya, Linux/WSL pe verify karo.\n"
                "Loopback + software timestamps -- real NIC/PTP pe numbers bahut kam\n"
                "honge. Numbers is machine pe NAHI nape gaye.\n");
    return 0;
}

/* ============================================================
 * EXPECTED (typical Linux, loopback, software TS) -- NAHI napa gaya
 * ------------------------------------------------------------
 * Wire-to-wire breakdown (1000 ticks attempted, ~960 completed full round):
 *   Hop 1 (MD send -> gateway RX)  : p50     3000   p99     9000   n=960
 *   Processing (gateway internal)  : p50      200   p99      800   n=960
 *   Hop 2 (order send -> exch RX)  : p50     3000   p99     9000   n=960
 *   TOTAL (wire-to-wire)           : p50     6500   p99    19000   n=960
 *
 * Processing is a TINY fraction of the total here (demo logic does ~
 * nothing) -- in a real strategy this ratio inverts: the two network
 * hops shrink (real NIC + kernel bypass, ~100-300 ns each) while
 * processing becomes the dominant, and most CONTROLLABLE, cost.
 * ============================================================ */
