// 06_multicast_receiver.linux.cpp
// ============================================================
// Multicast receiver: ek group (e.g. 239.1.2.3:9300) join karo
// (IP_ADD_MEMBERSHIP), datagrams receive karo. Ek chhota sender bhi
// andar (alag thread) taaki example self-contained ho.
// Yeh market-data distribution ka backbone hai.
// ============================================================
//  LINUX-ONLY (POSIX sockets, ip_mreqn).
//  Linux / WSL pe:
//      g++ -std=c++20 -O2 -Wall -Wextra -pthread 06_multicast_receiver.linux.cpp -o mcast
//      ./mcast 239.1.2.3 9300 lo        # WSL: loopback pe multicast; real: NIC name
//  Real multicast: `ip maddr`, `netstat -g` se membership dekho.
// ============================================================

#define _GNU_SOURCE 1   // glibc: expose MAP_POPULATE/ip_mreqn/accept4/recvmmsg/CLOCK_* under strict -std=c++20
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <atomic>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/socket.h>

int main(int argc, char** argv) {
    const char*  group = argc > 1 ? argv[1] : "239.1.2.3";
    const uint16_t port = static_cast<uint16_t>(argc > 2 ? std::atoi(argv[2]) : 9300);
    const char*  ifname = argc > 3 ? argv[3] : "lo";       // interface for membership
    const int    count  = argc > 4 ? std::atoi(argv[4]) : 200;

    // ---------- receiver socket ----------
    int rfd = ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (rfd < 0) { perror("socket"); return 1; }
    int one = 1;
    ::setsockopt(rfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);   // multiple receivers same port
    // (Linux: SO_REUSEPORT bhi -- alag processes same group padh sakte)

    sockaddr_in bindaddr{};
    bindaddr.sin_family = AF_INET;
    bindaddr.sin_port   = htons(port);
    // Portable: INADDR_ANY pe bind, phir group join. (Kuch setups group addr pe bind karte.)
    bindaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (::bind(rfd, reinterpret_cast<sockaddr*>(&bindaddr), sizeof bindaddr) < 0) { perror("bind"); return 1; }

    // ---------- join the group on a specific interface ----------
    ip_mreqn mreq{};
    if (::inet_pton(AF_INET, group, &mreq.imr_multiaddr) != 1) { std::fprintf(stderr, "bad group\n"); return 1; }
    mreq.imr_address.s_addr = htonl(INADDR_ANY);
    mreq.imr_ifindex = static_cast<int>(::if_nametoindex(ifname));    // 0 = kernel picks
    if (::setsockopt(rfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof mreq) < 0) {
        perror("IP_ADD_MEMBERSHIP (interface sahi hai? -- `ip link` se naam dekho)");
        return 1;
    }
    std::printf("joined %s:%u on if=%s (ifindex %u)\n", group, port, ifname, mreq.imr_ifindex);

    // ---------- sender thread (self-contained demo) ----------
    std::atomic<bool> go{false};
    std::thread tx([&] {
        int sfd = ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
        // TX interface + TTL + loopback (taaki same host ka receiver bhi sune)
        ip_mreqn txif{}; txif.imr_ifindex = mreq.imr_ifindex;
        ::setsockopt(sfd, IPPROTO_IP, IP_MULTICAST_IF, &txif, sizeof txif);
        int ttl = 1;   ::setsockopt(sfd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof ttl);
        int lb  = 1;   ::setsockopt(sfd, IPPROTO_IP, IP_MULTICAST_LOOP, &lb, sizeof lb);
        sockaddr_in to{}; to.sin_family = AF_INET; to.sin_port = htons(port);
        ::inet_pton(AF_INET, group, &to.sin_addr);
        while (!go.load()) std::this_thread::yield();
        for (int i = 1; i <= count; ++i) {
            char msg[32]; int n = std::snprintf(msg, sizeof msg, "tick %d", i);
            ::sendto(sfd, msg, static_cast<size_t>(n), 0,
                     reinterpret_cast<sockaddr*>(&to), sizeof to);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        ::close(sfd);
    });

    go.store(true);
    char buf[128];
    int got = 0;
    timeval tv{2, 0};
    ::setsockopt(rfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);      // 2s idle -> exit
    for (;;) {
        sockaddr_in src{}; socklen_t sl = sizeof src;
        ssize_t n = ::recvfrom(rfd, buf, sizeof buf - 1, 0,
                               reinterpret_cast<sockaddr*>(&src), &sl);
        if (n < 0) { if (errno == EINTR) continue; break; }          // timeout -> done
        buf[n] = '\0';
        if (++got <= 3 || got == count)
            std::printf("  rx from %s: \"%s\"\n", inet_ntoa(src.sin_addr), buf);
    }
    tx.join();

    // leave (cleanup)
    ::setsockopt(rfd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof mreq);
    ::close(rfd);

    std::printf("\n  total received = %d / %d\n", got, count);
    std::puts(
        "Kya seekha:\n"
        "  - Multicast: sender EK baar bhejta ek group address (224.0.0.0/4) pe;\n"
        "    network (switch, IGMP snooping) copies deta har joined receiver ko.\n"
        "    N subscribers, sender ka kaam O(1). Yehi exchange market-data feed.\n"
        "  - IP_ADD_MEMBERSHIP se NIC/switch ko 'main is group ka traffic chahta'\n"
        "    (IGMP report). Bina join, kernel woh datagrams drop kar deta.\n"
        "  - SO_REUSEADDR/SO_REUSEPORT: ek hi host pe kai processes same feed\n"
        "    padh sakte (har strategy apna copy).\n"
        "  - HFT: A/B feeds -- do alag multicast groups (redundant paths). Receiver\n"
        "    dono se padhta, seq number se arbitrate karta (jo pehle aaye).");
    std::printf("\n[NOTE] loopback (`lo`) pe multicast kaam karta hai testing ke liye;\n"
                "asli deployment me NIC + switch IGMP config. Numbers TYPICAL.\n");
    return 0;
}

/* ============================================================
 * EXPECTED (typical Linux, if=lo) -- NAHI napa gaya
 * ------------------------------------------------------------
 * joined 239.1.2.3:9300 on if=lo (ifindex 1)
 *   rx from 127.0.0.1: "tick 1"
 *   rx from 127.0.0.1: "tick 2"
 *   rx from 127.0.0.1: "tick 3"
 *   rx from 127.0.0.1: "tick 200"
 *
 *   total received = 200 / 200
 *
 * `netstat -g` / `ip maddr show` me group membership dikhega jab tak process zinda.
 * Real switch pe IGMP snooping galat ho to receiver ko kuch nahi milta (classic bug).
 * ============================================================ */
