// 04_udp_sender.linux.cpp
// ============================================================
// UDP sender: socket(SOCK_DGRAM) -> sendto() har datagram. Koi connection
// nahi, koi handshake nahi, koi retransmit nahi. Ek sequence number
// bhejta hai taaki receiver (example 05) loss/reorder dekh sake.
// ============================================================
//  LINUX-ONLY (POSIX sockets).
//  Linux / WSL pe:
//      g++ -std=c++20 -O2 -Wall -Wextra 04_udp_sender.linux.cpp -o udp_tx
//      ./udp_rx 9200 &            # example 05
//      ./udp_tx 127.0.0.1 9200 100000
// ============================================================

#define _GNU_SOURCE 1   // glibc: expose MAP_POPULATE/ip_mreqn/accept4/recvmmsg/CLOCK_* under strict -std=c++20
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <chrono>
#include <thread>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#pragma pack(push, 1)
struct Packet {
    uint64_t seq;          // 1..N
    uint64_t send_ns;      // sender steady_clock timestamp
    char     pad[48];      // total 64 bytes -- ek market-data-tick jaisa
};
#pragma pack(pop)
static_assert(sizeof(Packet) == 64);

int main(int argc, char** argv) {
    const char*  host = argc > 1 ? argv[1] : "127.0.0.1";
    const uint16_t port = static_cast<uint16_t>(argc > 2 ? std::atoi(argv[2]) : 9200);
    const uint64_t N  = argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 100000;
    const int gap_us  = argc > 4 ? std::atoi(argv[4]) : 20;   // inter-packet gap

    int fd = ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0) { perror("socket"); return 1; }

    // Bada send buffer -> burst me kernel drop na kare
    int sndbuf = 4 * 1024 * 1024;
    ::setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof sndbuf);

    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_port   = htons(port);
    if (::inet_pton(AF_INET, host, &dst.sin_addr) != 1) { std::fprintf(stderr, "bad ip\n"); return 1; }

    // Optional: connect() a UDP socket -> phir send() (sendto ke bina), aur
    // kernel destination ek baar resolve karta -> per-packet thoda sasta.
    ::connect(fd, reinterpret_cast<sockaddr*>(&dst), sizeof dst);

    std::printf("UDP tx -> %s:%u   %llu packets, ~%d us gap\n",
                host, port, (unsigned long long)N, gap_us);

    Packet p{};
    std::memset(p.pad, 0xAB, sizeof p.pad);
    uint64_t sent = 0, edrop = 0;
    for (uint64_t i = 1; i <= N; ++i) {
        p.seq = i;
        p.send_ns = static_cast<uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count());
        ssize_t k = ::send(fd, &p, sizeof p, 0);
        if (k < 0) {
            if (errno == ENOBUFS || errno == EAGAIN) { ++edrop; }   // local buffer full
            else { perror("send"); break; }
        } else {
            ++sent;
        }
        if (gap_us > 0 && (i % 16 == 0)) std::this_thread::sleep_for(std::chrono::microseconds(gap_us));
    }
    ::close(fd);

    std::printf("  sent=%llu   local-ENOBUFS=%llu\n",
                (unsigned long long)sent, (unsigned long long)edrop);
    std::puts(
        "\nKya seekha:\n"
        "  - sendto()/send() bas datagram ko IP layer ko de deta -- koi ACK,\n"
        "    koi retransmit, koi ordering guarantee nahi. Fire and forget.\n"
        "  - 'Loss' do jagah ho sakta: (a) yahan local -- socket send buffer full\n"
        "    (ENOBUFS), (b) network -- switch/NIC queue overflow (silent).\n"
        "  - Receiver ko seq number se hi gaps/reorder pata chalega (example 05).\n"
        "  - HFT market data UDP (aksar multicast, example 06) hai kyunki:\n"
        "    stale data bekaar hai -- ek purane packet ka retransmit (jo TCP\n"
        "    karta) latency badhata bina value ke. Recovery alag channel se\n"
        "    (snapshot / gap-fill request).");
    std::printf("\n[NOTE] TYPICAL behaviour; drop counts network + buffer sizes pe depend.\n");
    return 0;
}

/* ============================================================
 * EXPECTED (typical Linux, loopback) -- NAHI napa gaya
 * ------------------------------------------------------------
 * UDP tx -> 127.0.0.1:9200   100000 packets, ~20 us gap
 *   sent=100000   local-ENOBUFS=0
 *
 * Loopback pe loss ~0. Real network / high rate / small SO_RCVBUF pe
 * receiver side gaps dikhne lagenge (example 05 ka drop%).
 * ============================================================ */
