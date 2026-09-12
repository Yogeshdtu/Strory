// 10_latency_measure.linux.cpp
// ============================================================
// Round-trip latency histogram: UDP ping-pong between two threads over
// loopback, blocking recv vs busy-poll recv (MSG_DONTWAIT spin).
// p50/p99/p99.9 + a crude ASCII histogram. Yeh "measure, phir optimize"
// ka networking version hai.
// ============================================================
//  LINUX-ONLY (POSIX sockets, recvfrom MSG_DONTWAIT).
//  Linux / WSL pe:
//      g++ -std=c++20 -O2 -Wall -Wextra -pthread 10_latency_measure.linux.cpp -o latmeas && ./latmeas
// ============================================================

#define _GNU_SOURCE 1   // glibc: expose MAP_POPULATE/ip_mreqn/accept4/recvmmsg/CLOCK_* under strict -std=c++20
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <chrono>
#include <thread>
#include <atomic>
#include <algorithm>
#include <vector>
#include <cmath>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

static uint64_t now_ns() {
    return static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
}

static int bind_udp(uint16_t& port) {
    int fd = ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    sockaddr_in a{}; a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK); a.sin_port = htons(port);
    ::bind(fd, reinterpret_cast<sockaddr*>(&a), sizeof a);
    socklen_t l = sizeof a; ::getsockname(fd, reinterpret_cast<sockaddr*>(&a), &l);
    port = ntohs(a.sin_port);
    return fd;
}

// echo responder thread
static void responder(uint16_t port, std::atomic<bool>& stop, bool busy) {
    uint16_t p = port; int fd = bind_udp(p);
    sockaddr_in peer{}; socklen_t pl = sizeof peer;
    char b[64];
    while (!stop.load(std::memory_order_relaxed)) {
        ssize_t n = ::recvfrom(fd, b, sizeof b, busy ? MSG_DONTWAIT : 0,
                               reinterpret_cast<sockaddr*>(&peer), &pl);
        if (n < 0) { if (busy && (errno == EAGAIN)) continue; if (errno == EINTR) continue; break; }
        ::sendto(fd, b, static_cast<size_t>(n), 0, reinterpret_cast<sockaddr*>(&peer), pl);
    }
    ::close(fd);
}

static void run(const char* tag, bool busy, uint16_t resp_port) {
    uint16_t myp = 0; int fd = bind_udp(myp);
    sockaddr_in to{}; to.sin_family = AF_INET; to.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    to.sin_port = htons(resp_port);

    constexpr int WARM = 5000, N = 200000;
    char b[64] = "ping";
    std::vector<uint32_t> lat; lat.reserve(N);

    for (int i = 0; i < WARM + N; ++i) {
        uint64_t t0 = now_ns();
        ::sendto(fd, b, 5, 0, reinterpret_cast<sockaddr*>(&to), sizeof to);
        for (;;) {
            ssize_t n = ::recvfrom(fd, b, sizeof b, busy ? MSG_DONTWAIT : 0, nullptr, nullptr);
            if (n >= 0) break;
            if (busy && errno == EAGAIN) continue;   // spin
            if (errno == EINTR) continue;
            break;
        }
        uint64_t dt = now_ns() - t0;
        if (i >= WARM) lat.push_back(static_cast<uint32_t>(std::min<uint64_t>(dt, 0xFFFFFFFFu)));
    }
    ::close(fd);

    std::sort(lat.begin(), lat.end());
    auto q = [&](double p){ return lat[static_cast<size_t>(p * (lat.size() - 1))]; };
    std::printf("\n[%s]  n=%zu\n", tag, lat.size());
    std::printf("  p50=%u ns  p90=%u ns  p99=%u ns  p99.9=%u ns  max=%u ns\n",
                q(0.50), q(0.90), q(0.99), q(0.999), lat.back());

    // crude log-ish histogram
    const uint32_t buckets[] = {500,1000,2000,4000,8000,16000,32000,64000,128000,~0u};
    size_t cnt[10] = {0};
    for (uint32_t v : lat) { for (int k = 0; k < 10; ++k) if (v <= buckets[k]) { ++cnt[k]; break; } }
    for (int k = 0; k < 10; ++k) {
        double pct = 100.0 * static_cast<double>(cnt[k]) / static_cast<double>(lat.size());
        std::printf("  <=%7u ns | %5.1f%% | ", buckets[k], pct);
        for (int s = 0; s < static_cast<int>(pct / 2); ++s) std::putchar('#');
        std::putchar('\n');
    }
}

int main() {
    std::puts("UDP loopback ping-pong round-trip latency\n");

    { std::atomic<bool> stop{false};
      uint16_t rp = 0; { uint16_t t=0; int f=bind_udp(t); rp=t; ::close(f); }  // reserve a port number
      std::thread r(responder, rp, std::ref(stop), false);
      run("blocking recv", false, rp);
      stop.store(true);
      // nudge responder out of blocking recv
      int j = ::socket(AF_INET, SOCK_DGRAM, 0); sockaddr_in a{}; a.sin_family=AF_INET;
      a.sin_addr.s_addr=htonl(INADDR_LOOPBACK); a.sin_port=htons(rp);
      ::sendto(j,"x",1,0,reinterpret_cast<sockaddr*>(&a),sizeof a); ::close(j);
      r.join();
    }
    { std::atomic<bool> stop{false};
      uint16_t rp = 0; { uint16_t t=0; int f=bind_udp(t); rp=t; ::close(f); }
      std::thread r(responder, rp, std::ref(stop), true);
      run("busy-poll recv (MSG_DONTWAIT spin)", true, rp);
      stop.store(true); r.join();
    }

    std::puts(
        "\nKya seekha:\n"
        "  - Blocking recv: har round-trip me responder ko wake karna padta\n"
        "    (futex/sched wakeup ~1-5 us) -> p50 upar, tail bada.\n"
        "  - Busy-poll: dono side spin karte, koi sleep/wake nahi -> p50 ~half,\n"
        "    tail kaafi tight. CPU 100% jal raha -- HFT box pe woh core waise bhi\n"
        "    busy-poll kar raha hota.\n"
        "  - Yeh 'measure -> baseline -> ek cheez badlo -> re-measure' loop hai:\n"
        "    aage kar sakte -- threads pin karo (jitter girega), SO_BUSY_POLL,\n"
        "    recvmmsg batching, aur end me kernel bypass (~100-300 ns RT).\n"
        "  - p99.9 tail loopback pe bhi OS scheduler se -- threads pinned +\n"
        "    isolated core pe woh ~p50 ke paas aa jaata (29/11).");
    std::printf("\n[NOTE] loopback numbers TYPICAL hain, aapke box pe NAHI nape gaye.\n");
    return 0;
}

/* ============================================================
 * EXPECTED (typical Linux, loopback, unpinned) -- NAHI napa gaya
 * ------------------------------------------------------------
 * [blocking recv]  n=200000
 *   p50=9000 ns  p90=13000 ns  p99=28000 ns  p99.9=120000 ns  max=1500000 ns
 *
 * [busy-poll recv (MSG_DONTWAIT spin)]  n=200000
 *   p50=3500 ns  p90=5000 ns  p99=9000 ns  p99.9=22000 ns  max=180000 ns
 *
 * busy-poll ~2-3x better p50 and much tighter tail. Pinning + isolated cores
 * shrink the tail further. Kernel bypass would put RT at ~100-300 ns.
 * ============================================================ */
