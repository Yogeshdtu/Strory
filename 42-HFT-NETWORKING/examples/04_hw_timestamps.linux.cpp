// 04_hw_timestamps.linux.cpp
// ============================================================
// 30/09 ne RX timestamp dikhaya, PAR compare kiya APP's steady_clock se
// -- ALAG clock domains (kernel CLOCK_REALTIME vs app monotonic), isliye
// woh comparison sirf "relative jitter" tha, absolute latency NAHI.
//
// Yeh file DONO taraf KERNEL timestamps leta -- TX timestamp (sender ke
// socket ki ERROR QUEUE se, MSG_ERRQUEUE) AUR RX timestamp (receiver ke
// normal recvmsg cmsg se) -- DONO SAME clock domain (kernel's) se, isliye
// unka difference ek GENUINELY meaningful "wire+kernel latency" number
// deta (08-hardware-timestamping.md, 14-wire-to-wire-measurement.md).
// ============================================================
//  LINUX-ONLY (SO_TIMESTAMPING TX + RX, MSG_ERRQUEUE, linux/net_tstamp.h).
//  Linux / WSL pe (software TS hamesha kaam karta; hardware TS ke liye
//  NIC + driver support chahiye -- `ethtool -T eth0` se check karo):
//      g++ -std=c++20 -O2 -Wall -Wextra -pthread 04_hw_timestamps.linux.cpp -o hwts
// ============================================================

#define _GNU_SOURCE 1
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <linux/net_tstamp.h>
#include <linux/errqueue.h>

struct Msg { std::uint32_t seq; };

static std::uint64_t ts_to_ns(const timespec& t) {
    return static_cast<std::uint64_t>(t.tv_sec) * 1'000'000'000ull + static_cast<std::uint64_t>(t.tv_nsec);
}

// Pulls the TX-completion timestamp for the packet we just sent, from
// the socket's error queue. `expect_seq` correlates it (payload is
// looped back in the error-queue message), since we're sending one at
// a time (no need for SOF_TIMESTAMPING_OPT_ID pairing in this simple
// paced demo).
static bool get_tx_timestamp(int fd, std::uint32_t expect_seq, std::uint64_t& out_ns) {
    char data[64]; char ctrl[256];
    for (int tries = 0; tries < 200; ++tries) {   // TX completion is async -- brief retry
        iovec iov{data, sizeof data};
        msghdr msg{}; msg.msg_iov = &iov; msg.msg_iovlen = 1;
        msg.msg_control = ctrl; msg.msg_controllen = sizeof ctrl;
        const ssize_t n = ::recvmsg(fd, &msg, MSG_ERRQUEUE);
        if (n < 0) { std::this_thread::sleep_for(std::chrono::microseconds(20)); continue; }
        if (static_cast<std::size_t>(n) < sizeof(Msg)) continue;
        Msg got{}; std::memcpy(&got, data, sizeof got);
        if (got.seq != expect_seq) continue;   // stale/mismatched -- keep looking (rare in this paced demo)
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

int main() {
    constexpr std::uint16_t port = 9403;
    constexpr int kN = 2000;

    // ---- sender socket: TX timestamping enabled ----
    int tfd = ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    int txflags = SOF_TIMESTAMPING_TX_SOFTWARE | SOF_TIMESTAMPING_SOFTWARE
                | SOF_TIMESTAMPING_TX_HARDWARE | SOF_TIMESTAMPING_RAW_HARDWARE;
    if (::setsockopt(tfd, SOL_SOCKET, SO_TIMESTAMPING, &txflags, sizeof txflags) < 0)
        std::perror("SO_TIMESTAMPING (TX)");

    // ---- receiver socket: RX timestamping enabled ----
    int rfd = ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    int one = 1; ::setsockopt(rfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    sockaddr_in a{}; a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK); a.sin_port = htons(port);
    if (::bind(rfd, reinterpret_cast<sockaddr*>(&a), sizeof a) < 0) { std::perror("bind"); return 1; }
    int rxflags = SOF_TIMESTAMPING_RX_SOFTWARE | SOF_TIMESTAMPING_SOFTWARE
                | SOF_TIMESTAMPING_RX_HARDWARE | SOF_TIMESTAMPING_RAW_HARDWARE;
    if (::setsockopt(rfd, SOL_SOCKET, SO_TIMESTAMPING, &rxflags, sizeof rxflags) < 0)
        std::perror("SO_TIMESTAMPING (RX)");
    timeval rcv_tv{2, 0};   // safety net -- never hang forever on a lost datagram
    ::setsockopt(rfd, SOL_SOCKET, SO_RCVTIMEO, &rcv_tv, sizeof rcv_tv);

    sockaddr_in to{}; to.sin_family = AF_INET; to.sin_port = htons(port);
    ::inet_pton(AF_INET, "127.0.0.1", &to.sin_addr);

    std::vector<double> lat_ns; lat_ns.reserve(kN);
    int matched = 0;

    for (std::uint32_t seq = 0; seq < static_cast<std::uint32_t>(kN); ++seq) {
        Msg m{seq};
        ::sendto(tfd, &m, sizeof m, 0, reinterpret_cast<sockaddr*>(&to), sizeof to);

        // RX side -- pick up the datagram + its kernel RX timestamp
        char rdata[64]; char rctrl[256];
        iovec riov{rdata, sizeof rdata};
        msghdr rmsg{}; rmsg.msg_iov = &riov; rmsg.msg_iovlen = 1;
        rmsg.msg_control = rctrl; rmsg.msg_controllen = sizeof rctrl;
        const ssize_t rn = ::recvmsg(rfd, &rmsg, 0);
        if (rn < static_cast<ssize_t>(sizeof(Msg))) continue;

        std::uint64_t rx_ns = 0;
        for (cmsghdr* c = CMSG_FIRSTHDR(&rmsg); c; c = CMSG_NXTHDR(&rmsg, c)) {
            if (c->cmsg_level == SOL_SOCKET && c->cmsg_type == SO_TIMESTAMPING) {
                auto* t = reinterpret_cast<scm_timestamping*>(CMSG_DATA(c));
                const std::uint64_t sw = ts_to_ns(t->ts[0]);
                const std::uint64_t hw = ts_to_ns(t->ts[2]);
                rx_ns = hw ? hw : sw;
            }
        }

        std::uint64_t tx_ns = 0;
        if (rx_ns != 0 && get_tx_timestamp(tfd, seq, tx_ns) && tx_ns != 0 && rx_ns >= tx_ns) {
            lat_ns.push_back(static_cast<double>(rx_ns - tx_ns));
            ++matched;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
    ::close(tfd); ::close(rfd);

    std::printf("matched TX+RX timestamp pairs: %d / %d\n", matched, kN);
    if (!lat_ns.empty()) {
        std::sort(lat_ns.begin(), lat_ns.end());
        auto pc = [&](double p) {
            std::size_t i = static_cast<std::size_t>(p / 100.0 * static_cast<double>(lat_ns.size()));
            return lat_ns[std::min(i, lat_ns.size() - 1)];
        };
        std::printf("TX-kernel-ts -> RX-kernel-ts (SAME clock domain, genuinely meaningful):\n");
        std::printf("  p50 %6.0f   p99 %8.0f   p99.9 %9.0f   max %10.0f  ns\n",
                    pc(50), pc(99), pc(99.9), lat_ns.back());
    }

    std::puts(
        "\nKya seekha:\n"
        "  - 30/09's number APP steady_clock vs KERNEL clock tha -- ALAG epochs,\n"
        "    sirf jitter/relative dekh sakte the. Yahan DONO taraf KERNEL clock\n"
        "    hai (TX aur RX dono SO_TIMESTAMPING se) -- subtraction MEANINGFUL hai.\n"
        "  - MSG_ERRQUEUE: TX completion ek ASYNC event hai (kernel ne packet\n"
        "    driver ko de diya, uske BAAD timestamp aata) -- isliye error-queue se\n"
        "    poll/retry karna padta, seedha sendto() ke return se nahi milta.\n"
        "  - Real NIC + PTP: `ts[2]` (raw hardware) populate hota, jo actual WIRE\n"
        "    time hai (driver/kernel processing delay se pehle) -- loopback pe\n"
        "    `ts[2]` hamesha 0 hota (koi PHY hai hi nahi), `ts[0]` (software) pe\n"
        "    fallback hota is demo mein.");
    std::printf("\n[NOTE] Windows/MinGW dev box pe likha gaya, Linux/WSL pe verify karo.\n"
                "Loopback pe sirf SOFTWARE timestamps milenge -- HARDWARE ke liye real\n"
                "NIC (aur PTP-synced clock) chahiye. Numbers is machine pe NAHI nape gaye.\n");
    return 0;
}

/* ============================================================
 * EXPECTED (typical Linux, loopback, software TS) -- NAHI napa gaya
 * ------------------------------------------------------------
 * matched TX+RX timestamp pairs: 1987 / 2000
 * TX-kernel-ts -> RX-kernel-ts (SAME clock domain, genuinely meaningful):
 *   p50    3200   p99     9500   p99.9    24000   max     58000  ns
 *
 * (Real NIC + hardware timestamping + PTP sync: this same measurement
 * would show hundreds of ns, not microseconds -- the gap here is mostly
 * loopback-stack + kernel-queueing overhead, not "wire" time.)
 * ============================================================ */
