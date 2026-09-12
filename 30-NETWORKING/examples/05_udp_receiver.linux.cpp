// 05_udp_receiver.linux.cpp
// ============================================================
// UDP receiver: socket -> bind -> recv() loop. Sequence number se
// loss / reorder detect, aur one-way delay estimate (sender ka send_ns
// vs local recv time -- clocks synced na ho to sirf jitter meaningful).
// ============================================================
//  LINUX-ONLY (POSIX sockets, recvmmsg).
//  Linux / WSL pe:
//      g++ -std=c++20 -O2 -Wall -Wextra 05_udp_receiver.linux.cpp -o udp_rx
//      ./udp_rx 9200
//      # phir example 04:  ./udp_tx 127.0.0.1 9200 100000
// ============================================================

#define _GNU_SOURCE 1   // glibc: expose MAP_POPULATE/ip_mreqn/accept4/recvmmsg/CLOCK_* under strict -std=c++20
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <chrono>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#pragma pack(push, 1)
struct Packet { uint64_t seq; uint64_t send_ns; char pad[48]; };
#pragma pack(pop)
static_assert(sizeof(Packet) == 64);

static uint64_t now_ns() {
    return static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
}

int main(int argc, char** argv) {
    const uint16_t port = static_cast<uint16_t>(argc > 1 ? std::atoi(argv[1]) : 9200);
    const uint64_t expect = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 100000;

    int fd = ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0) { perror("socket"); return 1; }

    int one = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    int rcvbuf = 8 * 1024 * 1024;                         // bada -> burst drop kam
    ::setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof rcvbuf);

    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    a.sin_port = htons(port);
    if (::bind(fd, reinterpret_cast<sockaddr*>(&a), sizeof a) < 0) { perror("bind"); return 1; }

    std::printf("UDP rx on :%u  (expecting ~%llu packets, Ctrl-C to stop early)\n",
                port, (unsigned long long)expect);

    // recvmmsg: ek syscall me kai datagrams -> per-packet syscall overhead kam
    constexpr int B = 32;
    Packet bufs[B];
    iovec  iov[B];
    mmsghdr msgs[B];
    std::memset(msgs, 0, sizeof msgs);
    for (int i = 0; i < B; ++i) {
        iov[i].iov_base = &bufs[i];
        iov[i].iov_len  = sizeof(Packet);
        msgs[i].msg_hdr.msg_iov = &iov[i];
        msgs[i].msg_hdr.msg_iovlen = 1;
    }

    uint64_t got = 0, lost = 0, reordered = 0, last_seq = 0;
    uint64_t jitter_sum = 0, jitter_n = 0, prev_owd = 0;

    for (;;) {
        int n = ::recvmmsg(fd, msgs, B, MSG_WAITFORONE, nullptr);
        if (n < 0) { if (errno == EINTR) continue; perror("recvmmsg"); break; }
        uint64_t rx = now_ns();
        for (int i = 0; i < n; ++i) {
            if (msgs[i].msg_len != sizeof(Packet)) continue;
            const Packet& p = bufs[i];
            ++got;
            if (last_seq != 0) {
                if (p.seq > last_seq + 1) lost += (p.seq - last_seq - 1);
                else if (p.seq <= last_seq) { ++reordered; }
            }
            if (p.seq > last_seq) last_seq = p.seq;

            // one-way "delay" (clocks unsynced -> absolute meaningless, delta = jitter)
            uint64_t owd = rx - p.send_ns;
            if (prev_owd) { uint64_t d = owd > prev_owd ? owd - prev_owd : prev_owd - owd;
                            jitter_sum += d; ++jitter_n; }
            prev_owd = owd;
        }
        if (last_seq >= expect) break;
    }
    ::close(fd);

    double lossp = got + lost > 0 ? 100.0 * static_cast<double>(lost) / static_cast<double>(got + lost) : 0.0;
    std::printf("\n  received = %llu\n  lost (seq gaps) = %llu  (%.3f%%)\n"
                "  reordered/dup = %llu\n  mean inter-arrival jitter = %.0f ns\n",
                (unsigned long long)got, (unsigned long long)lost, lossp,
                (unsigned long long)reordered,
                jitter_n ? static_cast<double>(jitter_sum) / static_cast<double>(jitter_n) : 0.0);

    std::puts(
        "\nKya seekha:\n"
        "  - recv() sirf ek pura datagram deta (ya EAGAIN). Boundaries preserved\n"
        "    (TCP ke ulate) -- half packet kabhi nahi.\n"
        "  - Loss = seq gap. UDP ise detect/fix nahi karta -- app ki zimmedari.\n"
        "  - recvmmsg se ek syscall me B datagrams -> ~B-guna kam syscall overhead\n"
        "    high packet-rate pe (market data burst).\n"
        "  - SO_RCVBUF bada rakho: kernel socket queue chhoti ho to burst me\n"
        "    kernel silently drop karta (netstat -su 'receive buffer errors').");
    std::printf("\n[NOTE] loopback pe loss ~0; real feed pe SO_RCVBUF / core-busy se drop.\n");
    return 0;
}

/* ============================================================
 * EXPECTED (typical Linux, loopback, with example 04) -- NAHI napa gaya
 * ------------------------------------------------------------
 * UDP rx on :9200  (expecting ~100000 packets, Ctrl-C to stop early)
 *
 *   received = 100000
 *   lost (seq gaps) = 0  (0.000%)
 *   reordered/dup = 0
 *   mean inter-arrival jitter = 900 ns
 *
 * Real network / high rate / small SO_RCVBUF: loss 0.01-2%, jitter us-scale.
 * `cat /proc/net/udp` , `netstat -su` se kernel-side drop counters.
 * ============================================================ */
