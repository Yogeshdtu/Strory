// 03_nagle_demo.linux.cpp
// ============================================================
// Nagle's algorithm + delayed ACK ka latency pe asar. Ek "write small,
// wait for reply" loop TCP_NODELAY OFF vs ON. OFF pe p99 ~40 ms spike.
// Yeh HFT ka classic dushman hai.
// ============================================================
//  LINUX-ONLY. Server thread + client thread ek hi process me (loopback).
//  Linux / WSL pe:
//      g++ -std=c++20 -O2 -Wall -Wextra -pthread 03_nagle_demo.linux.cpp -o nagle && ./nagle
// ============================================================

#define _GNU_SOURCE 1   // glibc: expose MAP_POPULATE/ip_mreqn/accept4/recvmmsg/CLOCK_* under strict -std=c++20
#include <cstdio>
#include <cstring>
#include <chrono>
#include <thread>
#include <atomic>
#include <algorithm>
#include <vector>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

static uint64_t now_ns() {
    return static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
}

// ---- ek trivial echo server thread ----
static std::atomic<uint16_t> g_port{0};
static void echo_server(std::atomic<bool>& stop) {
    int lfd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    int one = 1; ::setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    sockaddr_in a{}; a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK); a.sin_port = 0;
    ::bind(lfd, reinterpret_cast<sockaddr*>(&a), sizeof a);
    socklen_t al = sizeof a; ::getsockname(lfd, reinterpret_cast<sockaddr*>(&a), &al);
    g_port.store(ntohs(a.sin_port));
    ::listen(lfd, 8);
    int cfd = ::accept4(lfd, nullptr, nullptr, SOCK_CLOEXEC);
    int nd = 1; ::setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, &nd, sizeof nd);  // server side always NODELAY
    char b[512];
    while (!stop.load()) {
        ssize_t n = ::recv(cfd, b, sizeof b, 0);
        if (n <= 0) break;
        ::send(cfd, b, static_cast<size_t>(n), MSG_NOSIGNAL);
    }
    ::close(cfd); ::close(lfd);
}

static void run_client(bool nodelay, const char* tag) {
    while (g_port.load() == 0) std::this_thread::yield();
    int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    sockaddr_in a{}; a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = htons(g_port.load());
    if (::connect(fd, reinterpret_cast<sockaddr*>(&a), sizeof a) < 0) { perror("connect"); return; }

    int flag = nodelay ? 1 : 0;
    ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof flag);

    constexpr int N = 4000;
    std::vector<uint64_t> lat; lat.reserve(N);
    char rx[64];
    // "chatty" pattern: do chhote writes, phir reply ka wait -- Nagle ka worst case
    for (int i = 0; i < N; ++i) {
        uint64_t t0 = now_ns();
        const char h[4] = {'H','D','R','!'};
        const char body[8] = {'p','a','y','l','o','a','d','.'};
        ::send(fd, h, sizeof h, MSG_NOSIGNAL);       // 1st small segment
        ::send(fd, body, sizeof body, MSG_NOSIGNAL); // 2nd small segment -> Nagle holds it
        size_t got = 0;
        while (got < sizeof h + sizeof body) {
            ssize_t n = ::recv(fd, rx + got, sizeof rx - got, 0);
            if (n <= 0) { ::close(fd); return; }
            got += static_cast<size_t>(n);
        }
        lat.push_back(now_ns() - t0);
    }
    ::close(fd);
    std::sort(lat.begin(), lat.end());
    auto q = [&](double p){ return lat[static_cast<size_t>(p*(lat.size()-1))]/1000.0; };  // us
    std::printf("  %-22s  p50=%.1f us  p90=%.1f us  p99=%.1f us  max=%.1f us\n",
                tag, q(0.50), q(0.90), q(0.99), lat.back()/1000.0);
}

int main() {
    std::puts("Chatty request/reply (2 small writes -> wait), 4000 iterations:\n");

    { std::atomic<bool> stop{false}; std::thread s(echo_server, std::ref(stop));
      run_client(false, "TCP_NODELAY = OFF");
      stop.store(true); int j=::socket(AF_INET,SOCK_STREAM,0); sockaddr_in a{}; a.sin_family=AF_INET;
      a.sin_addr.s_addr=htonl(INADDR_LOOPBACK); a.sin_port=htons(g_port.load());
      ::connect(j,reinterpret_cast<sockaddr*>(&a),sizeof a); ::close(j); s.join(); }

    g_port.store(0);
    { std::atomic<bool> stop{false}; std::thread s(echo_server, std::ref(stop));
      run_client(true,  "TCP_NODELAY = ON ");
      stop.store(true); int j=::socket(AF_INET,SOCK_STREAM,0); sockaddr_in a{}; a.sin_family=AF_INET;
      a.sin_addr.s_addr=htonl(INADDR_LOOPBACK); a.sin_port=htons(g_port.load());
      ::connect(j,reinterpret_cast<sockaddr*>(&a),sizeof a); ::close(j); s.join(); }

    std::puts(
        "\nKya hua:\n"
        "  - Nagle (default ON): jab tak pichhla chhota segment ACK na ho jaaye,\n"
        "    naya chhota segment nahi bhejta -- chhoti writes ko coalesce karke\n"
        "    bandwidth bachata. Client ka 2nd send() buffer me ruka rehta.\n"
        "  - Delayed ACK (receiver): ACK ~40 ms tak defer kar sakta (piggyback ki\n"
        "    aas me). Client Nagle ke wajah se 2nd segment nahi bhej raha, server\n"
        "    delayed-ACK ke wajah se ACK nahi bhej raha -> ~40 ms deadlock har\n"
        "    iteration (ya kuch fraction pe).\n"
        "  - TCP_NODELAY = ON: Nagle band, har send() turant wire pe. p99 gir jaata.\n"
        "  - HFT: HAR socket pe TCP_NODELAY. Aur ek request = ek write() (writev se\n"
        "    header+body ek shot me), taaki 'do chhoti writes' wala pattern hi na bane.");

    std::printf("\n[NOTE] loopback numbers TYPICAL hain (delayed-ACK ~40ms Linux default),\n"
                "aapke box pe NAHI nape gaye.\n");
    return 0;
}

/* ============================================================
 * EXPECTED (typical Linux, loopback) -- NAHI napa gaya
 * ------------------------------------------------------------
 * Chatty request/reply (2 small writes -> wait), 4000 iterations:
 *
 *   TCP_NODELAY = OFF       p50=0.1 us  p90=40200.0 us  p99=40800.0 us  max=41000.0 us
 *   TCP_NODELAY = ON        p50=22.0 us  p90=30.0 us   p99=95.0 us     max=1200.0 us
 *
 * OFF: bahut saare iterations ~40 ms (Nagle + delayed-ACK interaction).
 * ON:  clean ~tens of us. Yehi farak HFT me "us vs 40ms" hota hai.
 * (Exact fraction jo 40ms hit karte woh kernel version/timing pe depend.)
 * ============================================================ */
