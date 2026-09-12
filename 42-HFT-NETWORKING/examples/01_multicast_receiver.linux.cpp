// 01_multicast_receiver.linux.cpp
// ============================================================
// Basic multicast receiver -- 30/06 ka SAME core mechanism
// (IP_ADD_MEMBERSHIP), yahan DO cheezein aage badhata:
//   1. `INetworkReceiver` abstraction (08) ke through -- backend
//      (kernel-socket abhi, bypass library baad mein) application code
//      se DECOUPLED hai.
//   2. Ek REALISTIC binary market-data message (seq + timestamp + price
//      + qty, 38-MARKET-DATA jaisa) + SEQUENCE-GAP detection (38/09).
// (02-multicast-receive-path.md)
// ============================================================
//  LINUX-ONLY (POSIX sockets via 08_bypass_abstraction.hpp).
//  Linux / WSL pe:
//      g++ -std=c++20 -O2 -Wall -Wextra -pthread 01_multicast_receiver.linux.cpp -o mcastrx
//      ./mcastrx 239.1.2.4 9400 lo
// ============================================================

#define _GNU_SOURCE 1
#include "08_bypass_abstraction.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <atomic>
#include <chrono>
#include <thread>

struct MdMessage {
    std::uint32_t seq;
    std::uint64_t ts_ns;    // sender's send-time (steady_clock, same-machine demo)
    std::uint32_t price;
    std::uint32_t qty;
};

static std::uint64_t now_ns() {
    return static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
}

int main(int argc, char** argv) {
    const char*    group = argc > 1 ? argv[1] : "239.1.2.4";
    const auto     port  = static_cast<std::uint16_t>(argc > 2 ? std::atoi(argv[2]) : 9400);
    const char*    ifname = argc > 3 ? argv[3] : "lo";
    constexpr int  kCount = 5000;

    KernelSocketReceiver rx(group, port, ifname);
    if (!rx.ok()) { std::fprintf(stderr, "receiver setup FAILED\n"); return 1; }
    std::printf("[%s] joined %s:%u on %s\n", rx.backend_name(), group, port, ifname);

    // 36's SO_RCVBUF tuning -- default kernel buffer chhota ho sakta,
    // bursty market-data feed pe kernel-level drops de sakta agar app
    // thodi der bhi delay ho jaaye consume karne mein.
    int rcvbuf = 4 * 1024 * 1024;
    ::setsockopt(rx.fd(), SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof rcvbuf);

    // ---- self-contained sender thread (deliberately drops a few seqs) ----
    std::atomic<bool> go{false};
    std::thread tx([&] {
        int sfd = ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
        ip_mreqn txif{}; txif.imr_ifindex = static_cast<int>(::if_nametoindex(ifname));
        ::setsockopt(sfd, IPPROTO_IP, IP_MULTICAST_IF, &txif, sizeof txif);
        int ttl = 1; ::setsockopt(sfd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof ttl);
        int lb  = 1; ::setsockopt(sfd, IPPROTO_IP, IP_MULTICAST_LOOP, &lb, sizeof lb);
        sockaddr_in to{}; to.sin_family = AF_INET; to.sin_port = htons(port);
        ::inet_pton(AF_INET, group, &to.sin_addr);

        while (!go.load(std::memory_order_acquire)) std::this_thread::yield();
        for (std::uint32_t seq = 1; seq <= static_cast<std::uint32_t>(kCount); ++seq) {
            if (seq % 777 == 0) continue;   // deliberate drop -- gap detector should catch this
            MdMessage m{seq, now_ns(), 10000 + (seq % 50), 100 + (seq % 400)};
            ::sendto(sfd, &m, sizeof m, 0, reinterpret_cast<sockaddr*>(&to), sizeof to);
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
        ::close(sfd);
    });

    // ---- receive loop (non-blocking, spin -- 41/07's busy-spin trade-off) ----
    go.store(true, std::memory_order_release);
    std::uint32_t expected = 1, received = 0, gaps = 0, gap_total = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    RxPacket pkt;
    // Loop until we've accounted (received OR detected-as-gap) for every seq
    // up through kCount, or the deadline hits -- avoids an off-by-one from
    // trying to precompute "how many drops will there be."
    while (expected <= static_cast<std::uint32_t>(kCount) &&
           std::chrono::steady_clock::now() < deadline) {
        if (!rx.try_receive(pkt)) continue;
        if (pkt.len != sizeof(MdMessage)) continue;   // malformed/short -- ignore for this demo
        MdMessage m;
        std::memcpy(&m, pkt.data, sizeof m);
        ++received;
        if (m.seq != expected) {
            const std::uint32_t missing = m.seq - expected;
            ++gaps;
            gap_total += missing;
        }
        expected = m.seq + 1;
    }
    tx.join();

    std::printf("received=%u  gaps_detected=%u  total_missing_seqs=%u (expected ~%d dropped)\n",
                received, gaps, gap_total, kCount / 777);
    std::puts(
        "\nKya seekha:\n"
        "  - `INetworkReceiver` abstraction (08): yeh receive-loop code KABHI\n"
        "    nahi badlega chahe backend kernel-socket rahe ya kal Onload/ef_vi\n"
        "    ban jaaye -- sirf constructor badalta.\n"
        "  - Sequence-gap detection: `seq != expected` -> woh gap MARKET DATA\n"
        "    ka nahi, TRANSPORT ka packet-loss hai (UDP ki guarantee nahi hoti) --\n"
        "    38/09's exact mechanism, yahan REAL socket pe demonstrate.\n"
        "  - SO_RCVBUF: bursty feed + thodi si app-side delay = kernel-level\n"
        "    silent drop, agar buffer chhota ho. Bada buffer isse absorb karta.");
    std::printf("\n[NOTE] loopback pe run hua; deliberate drops (seq%%777==0) HAMESHA\n"
                "consistent pakde jaane chahiye is design mein. Numbers is machine pe\n"
                "NAHI nape gaye (Windows/MinGW dev box, is file ko Linux/WSL pe verify karo).\n");
    return 0;
}

/* ============================================================
 * EXPECTED (typical Linux, if=lo) -- NAHI napa gaya
 * ------------------------------------------------------------
 * [kernel-socket] joined 239.1.2.4:9400 on lo
 * received=4994  gaps_detected=6  total_missing_seqs=6 (expected ~6 dropped)
 *
 * (5000 seqs total, 6 deliberately skipped at seq 777/1554/2331/3108/3885/
 * 4662 -> 4994 actually sent+received -> gaps_detected=6, each gap missing
 * exactly 1 seq. This count should be exact and reproducible every run,
 * since the skip pattern is deterministic (seq % 777 == 0), not timing-based.)
 * ============================================================ */
