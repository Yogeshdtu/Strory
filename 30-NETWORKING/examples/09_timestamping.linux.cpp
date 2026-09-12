// 09_timestamping.linux.cpp
// ============================================================
// SO_TIMESTAMPING: kernel (aur, NIC support ho to, HARDWARE) har packet ka
// receive timestamp deta -- ancillary data (cmsg) me. Isse "packet NIC pe
// kab aaya" vs "app ne kab padha" ka gap (kernel+sched latency) milta.
// ============================================================
//  LINUX-ONLY (linux/net_tstamp.h, SO_TIMESTAMPING, recvmsg cmsg).
//  Linux / WSL pe (software TS hamesha; hardware TS ke liye NIC + PTP):
//      g++ -std=c++20 -O2 -Wall -Wextra -pthread 09_timestamping.linux.cpp -o tstamp
//      ./tstamp 9500
//  HW TS check: `ethtool -T eth0`
// ============================================================

#define _GNU_SOURCE 1   // glibc: expose MAP_POPULATE/ip_mreqn/accept4/recvmmsg/CLOCK_* under strict -std=c++20
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <chrono>
#include <thread>
#include <atomic>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <linux/net_tstamp.h>
#include <linux/errqueue.h>

static uint64_t mono_ns() {
    return static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
}
static uint64_t ts_to_ns(const timespec& t) {
    return static_cast<uint64_t>(t.tv_sec) * 1'000'000'000ull + static_cast<uint64_t>(t.tv_nsec);
}

int main(int argc, char** argv) {
    const uint16_t port = static_cast<uint16_t>(argc > 1 ? std::atoi(argv[1]) : 9500);

    int fd = ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0) { perror("socket"); return 1; }
    int one = 1; ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);

    sockaddr_in a{}; a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_ANY); a.sin_port = htons(port);
    if (::bind(fd, reinterpret_cast<sockaddr*>(&a), sizeof a) < 0) { perror("bind"); return 1; }

    // Enable RX timestamping. RX_SOFTWARE always; RX_HARDWARE + RAW_HARDWARE
    // sirf tab kaam karega jab NIC support kare aur driver enable ho.
    int flags = SOF_TIMESTAMPING_RX_SOFTWARE | SOF_TIMESTAMPING_SOFTWARE
              | SOF_TIMESTAMPING_RX_HARDWARE | SOF_TIMESTAMPING_RAW_HARDWARE;
    if (::setsockopt(fd, SOL_SOCKET, SO_TIMESTAMPING, &flags, sizeof flags) < 0)
        perror("SO_TIMESTAMPING (software still works via SO_TIMESTAMPNS fallback)");

    // self-sender
    std::atomic<bool> go{false};
    std::thread tx([&]{
        int s = ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
        sockaddr_in to{}; to.sin_family = AF_INET; to.sin_port = htons(port);
        ::inet_pton(AF_INET, "127.0.0.1", &to.sin_addr);
        while (!go.load()) std::this_thread::yield();
        for (int i = 1; i <= 100; ++i) {
            uint64_t x = mono_ns();
            ::sendto(s, &x, sizeof x, 0, reinterpret_cast<sockaddr*>(&to), sizeof to);
            std::this_thread::sleep_for(std::chrono::milliseconds(3));
        }
        ::close(s);
    });
    go.store(true);

    char data[64];
    char ctrl[256];
    uint64_t sum_gap = 0, n_gap = 0, worst = 0;
    int got = 0;
    timeval tv{2,0}; ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);

    for (;;) {
        iovec iov{data, sizeof data};
        msghdr msg{};
        msg.msg_iov = &iov; msg.msg_iovlen = 1;
        msg.msg_control = ctrl; msg.msg_controllen = sizeof ctrl;

        ssize_t n = ::recvmsg(fd, &msg, 0);
        uint64_t app_ns = mono_ns();
        if (n < 0) { if (errno == EINTR) continue; break; }   // timeout -> done
        ++got;

        uint64_t kernel_ns = 0;
        for (cmsghdr* c = CMSG_FIRSTHDR(&msg); c; c = CMSG_NXTHDR(&msg, c)) {
            if (c->cmsg_level == SOL_SOCKET && c->cmsg_type == SO_TIMESTAMPING) {
                // scm_timestamping: [0]=software, [1]=deprecated, [2]=raw hardware
                auto* t = reinterpret_cast<scm_timestamping*>(CMSG_DATA(c));
                uint64_t sw = ts_to_ns(t->ts[0]);
                uint64_t hw = ts_to_ns(t->ts[2]);
                kernel_ns = hw ? hw : sw;
            }
        }
        if (kernel_ns) {
            // NOTE: kernel TS CLOCK_REALTIME base, app_ns steady_clock base -- alag
            // epochs. Yahan sirf demo; asli use me dono ko ek base pe lo (ya
            // dono steady). Delta ka *variation* (jitter) meaningful hai.
            uint64_t gap = app_ns > kernel_ns ? app_ns - kernel_ns : 0;
            if (gap) { sum_gap += gap; ++n_gap; if (gap > worst) worst = gap; }
        }
    }
    tx.join();
    ::close(fd);

    std::printf("received %d packets\n", got);
    if (n_gap)
        std::printf("kernel-RX -> app-recv gap:  mean ~%llu ns  worst ~%llu ns  (n=%llu)\n"
                    "  (epochs differ -- treat as relative/jitter, not absolute)\n",
                    (unsigned long long)(sum_gap / n_gap), (unsigned long long)worst,
                    (unsigned long long)n_gap);

    std::puts(
        "\nKya seekha:\n"
        "  - SO_TIMESTAMPING kernel/HW ka RX timestamp cmsg me deta -> tumhe pata\n"
        "    chalta 'packet NIC/kernel pe kab aaya' vs 'meine recv() kab kiya'.\n"
        "    Woh gap = kernel processing + queue + scheduler wakeup latency.\n"
        "  - HARDWARE timestamps (NIC PHY pe, `ethtool -T eth0` se check) sabse\n"
        "    accurate -- software TS me already kernel jitter mila hota.\n"
        "  - HFT use: (a) exchange se latency measure (their send TS vs your HW RX\n"
        "    TS), (b) internal pipeline stage-by-stage latency, (c) PTP clock sync\n"
        "    (hardware TS + ptp4l -> sub-us wall-clock to exchange, 14).\n"
        "  - TX timestamping (SOF_TIMESTAMPING_TX_*) via MSG_ERRQUEUE -- 'packet\n"
        "    wire pe kab gaya'.");
    std::printf("\n[NOTE] loopback pe hardware TS nahi milega (no NIC PHY); software TS\n"
                "gap TYPICAL ~us-scale. Real NIC + PTP pe sub-us. Numbers NAHI nape gaye.\n");
    return 0;
}

/* ============================================================
 * EXPECTED (typical Linux, loopback, software TS) -- NAHI napa gaya
 * ------------------------------------------------------------
 * received 100 packets
 * kernel-RX -> app-recv gap:  mean ~4000 ns  worst ~45000 ns  (n=100)
 *   (epochs differ -- treat as relative/jitter, not absolute)
 *
 * Real HW-timestamped NIC + pinned busy-poll receiver: gap ~hundreds of ns,
 * tight. The whole point: measure where the microseconds go.
 * ============================================================ */
