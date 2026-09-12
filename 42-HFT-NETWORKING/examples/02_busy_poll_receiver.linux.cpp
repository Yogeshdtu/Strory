// 02_busy_poll_receiver.linux.cpp
// ============================================================
// SO_BUSY_POLL -- a KERNEL-level busy-poll (the kernel's own NAPI poll
// routine spins for a bounded microsecond budget before falling back to
// interrupt-driven wakeup). This is DIFFERENT from 30/10's manual
// MSG_DONTWAIT spin (that's APPLICATION-level busy-poll -- works on
// ANY socket/interface, including loopback). SO_BUSY_POLL needs the
// NIC DRIVER to implement `ndo_busy_poll` -- on loopback/virtual
// interfaces it is typically a silent NO-OP (07-busy-poll-sockets.md).
// ============================================================
//  LINUX-ONLY (SO_BUSY_POLL, Linux >= 3.11).
//  Linux / WSL pe:
//      g++ -std=c++20 -O2 -Wall -Wextra -pthread 02_busy_poll_receiver.linux.cpp -o busypoll
//      ./busypoll
//  Real NIC support check:  ethtool -k eth0 | grep busy-poll
// ============================================================

#define _GNU_SOURCE 1
#include "08_bypass_abstraction.hpp"

#include <cstdio>
#include <cstring>

#ifndef SO_BUSY_POLL
#define SO_BUSY_POLL 46   // glibc headers usually define this; fallback for older ones
#endif

int main() {
    KernelSocketReceiver rx("239.1.2.5", 9401, "lo");
    if (!rx.ok()) { std::fprintf(stderr, "receiver setup FAILED\n"); return 1; }

    // Ask the kernel to busy-poll for up to 50 microseconds before
    // falling back to interrupt-driven wakeup for THIS socket.
    int budget_us = 50;
    const int rc = ::setsockopt(rx.fd(), SOL_SOCKET, SO_BUSY_POLL, &budget_us, sizeof budget_us);
    if (rc < 0) {
        std::perror("SO_BUSY_POLL setsockopt");
        std::puts("(kernel too old, or CONFIG_NET_RX_BUSY_POLL not built -- socket still");
        std::puts(" works normally, just without this hint.)");
    } else {
        std::printf("SO_BUSY_POLL set: %d us budget requested\n", budget_us);
    }

    // Read back what the kernel actually stored (some kernels clamp it).
    int got = 0; socklen_t gl = sizeof got;
    if (::getsockopt(rx.fd(), SOL_SOCKET, SO_BUSY_POLL, &got, &gl) == 0)
        std::printf("kernel reports SO_BUSY_POLL = %d us\n", got);

    std::puts(
        "\nKya seekha:\n"
        "  - SO_BUSY_POLL ek PER-SOCKET hint hai -- 'is socket pe kuch aane ka\n"
        "    wait karte waqt, kernel apna NAPI poll khud, bounded microseconds\n"
        "    ke liye, chalao (interrupt ka wait mat karo).'\n"
        "  - Yeh sirf tab kaam karta jab NIC DRIVER `ndo_busy_poll` implement\n"
        "    kare -- zyaadatar modern datacenter NICs (Intel ixgbe/i40e, Mellanox\n"
        "    mlx5, Solarflare sfc) karte. Loopback (`lo`) jaisi VIRTUAL interfaces\n"
        "    pe typically NO-OP hota -- koi real NAPI poll-routine hai hi nahi.\n"
        "  - Isliye is example ka 'success' setsockopt call SUCCEED karna hai --\n"
        "    is machine (aur loopback) pe MEASURABLE latency difference NAHI\n"
        "    milega (03's benchmark isi ko explicitly demonstrate karta).\n"
        "  - `net.core.busy_poll` / `net.core.busy_read` sysctls -- SYSTEM-WIDE\n"
        "    defaults; per-socket SO_BUSY_POLL unhe override karta.");
    std::printf("\n[NOTE] Windows/MinGW dev box pe likha gaya, Linux/WSL pe chalao. Real\n"
                "NIC (loopback nahi) pe hi busy-poll ka asli latency-fayda measure hoga.\n");
    return 0;
}

/* ============================================================
 * EXPECTED (typical Linux, if=lo) -- NAHI napa gaya
 * ------------------------------------------------------------
 * SO_BUSY_POLL set: 50 us budget requested
 * kernel reports SO_BUSY_POLL = 50 us
 *
 * (setsockopt SUCCEEDS on any modern kernel -- the budget is accepted
 * regardless of driver support. Whether it does anything is invisible
 * from userspace; only a latency benchmark on REAL hardware with a
 * busy-poll-capable driver would show a difference. On loopback, 03's
 * benchmark should show ~0 measurable difference between "busy-poll on"
 * and "busy-poll off" for exactly this reason.)
 * ============================================================ */
