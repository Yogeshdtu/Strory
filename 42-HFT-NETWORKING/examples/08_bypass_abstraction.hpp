// 08_bypass_abstraction.hpp
// ============================================================
// Ek "receiver" abstraction jo APPLICATION CODE ko backend-agnostic
// banata -- chahe packets kernel sockets se aayein, ya Onload/ef_vi/
// DPDK jaisi kernel-bypass library se, application ka poll-loop code
// EXACTLY SAME rehta (03-kernel-bypass-overview.md).
//
// LINUX-ONLY (KernelSocketReceiver POSIX sockets use karta). Yeh header
// khud .linux.cpp files se hi include hota (01, 03, 06) -- Windows-
// verified side pe kabhi include NAHI hota, isliye build.ps1 checkall
// ise kabhi touch nahi karta (jaisa 29/30's headers).
//
// SCOPE: sirf ek "identical interface" DIKHANA hai. Onload/ef_vi/DPDK
// backends yahan ACTUALLY implement nahi kiye gaye (vendor SDK + special
// hardware chahiye, jo is course ke scope se bahar hai -- 04/05/06
// lessons unka API/design *explain* karte, code nahi maangte).
// ============================================================
#pragma once

#include <cstdint>
#include <cstring>
#include <cstdio>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/socket.h>
#include <unistd.h>

// Ek receive-hui packet ka view -- koi copy/allocation, sirf ek pointer +
// length is buffer ke andar jo caller-owned hai (zero-copy-friendly shape,
// 36's principle yahan bhi).
struct RxPacket {
    const std::uint8_t* data = nullptr;
    std::size_t          len  = 0;
};

// ============================================================
//  INetworkReceiver -- application isi interface ke against likhta.
// ============================================================
class INetworkReceiver {
public:
    virtual ~INetworkReceiver() = default;

    // Non-blocking. true = ek packet mila (`out` populate hua), false =
    // abhi kuch nahi (caller decide karta: spin phir try karo, ya sleep).
    virtual bool try_receive(RxPacket& out) = 0;

    // Backend-specific "warm up" -- kernel-socket ke liye no-op,
    // DPDK/ef_vi jaisi libraries ke liye yahan hugepages/rings/queues
    // pre-allocate + touch hote (36's page-fault-avoidance principle,
    // hot path shuru hone se PEHLE).
    virtual void warmup() {}

    virtual const char* backend_name() const = 0;
};

// ============================================================
//  KernelSocketReceiver -- REAL, working backend (plain POSIX multicast
//  socket). Baseline jiske against bypass backends compare hote (03).
// ============================================================
class KernelSocketReceiver final : public INetworkReceiver {
public:
    KernelSocketReceiver(const char* group, std::uint16_t port, const char* ifname) {
        fd_ = ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
        if (fd_ < 0) { std::perror("socket"); return; }

        int one = 1;
        ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);

        sockaddr_in bindaddr{};
        bindaddr.sin_family      = AF_INET;
        bindaddr.sin_port        = htons(port);
        bindaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        if (::bind(fd_, reinterpret_cast<sockaddr*>(&bindaddr), sizeof bindaddr) < 0) {
            std::perror("bind"); return;
        }

        ip_mreqn mreq{};
        if (::inet_pton(AF_INET, group, &mreq.imr_multiaddr) != 1) {
            std::fprintf(stderr, "bad group address\n"); return;
        }
        mreq.imr_address.s_addr = htonl(INADDR_ANY);
        mreq.imr_ifindex        = static_cast<int>(::if_nametoindex(ifname));
        if (::setsockopt(fd_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof mreq) < 0) {
            std::perror("IP_ADD_MEMBERSHIP"); return;
        }
        mreq_  = mreq;
        ready_ = true;
    }

    ~KernelSocketReceiver() override {
        if (fd_ >= 0) {
            if (ready_) ::setsockopt(fd_, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq_, sizeof mreq_);
            ::close(fd_);
        }
    }

    bool try_receive(RxPacket& out) override {
        if (!ready_) return false;
        const ssize_t n = ::recv(fd_, buf_, sizeof buf_, 0);   // socket already O_NONBLOCK
        if (n <= 0) return false;   // n<0: EAGAIN (nothing yet) or real error -- caller doesn't care which for the hot path
        out.data = reinterpret_cast<const std::uint8_t*>(buf_);
        out.len  = static_cast<std::size_t>(n);
        return true;
    }

    const char* backend_name() const override { return "kernel-socket"; }

    // Kernel-bypass-specific tuning knobs ka RAW ACCESS -- e.g. SO_BUSY_POLL
    // (02-busy-poll-sockets.md) is fd pe seedha set kiya jaa sakta.
    int fd() const { return fd_; }
    bool ok() const { return ready_; }

private:
    int       fd_    = -1;
    bool      ready_ = false;
    ip_mreqn  mreq_{};
    char      buf_[2048]{};
};

// ============================================================
//  Stub backends -- 04/05/06 lessons ka "yeh REAL implementation kaisi
//  dikhti" reference. Vendor SDK (OpenOnload / ef_vi / DPDK) ke bina
//  compile/run NAHI ho sakte -- isliye sirf INTERFACE-SHAPE + comments,
//  koi fake/misleading "working" code nahi (Rule 4 -- claim verifiable ho).
// ============================================================

// OnloadReceiver: application UNCHANGED rehta -- Onload
// `LD_PRELOAD=libonload.so ./program` se poore socket() family calls
// ko intercept karta, USER-SPACE mein poora TCP/UDP stack replicate
// karta (04-solarflare-onload.md). Isliye is course mein "OnloadReceiver"
// class ki ZAROORAT hi nahi -- KernelSocketReceiver ka WOHI code, Onload
// ke saath run karne pe, transparently bypass ho jaata. Yeh khud is
// abstraction ka POINT hai: application code kabhi badalta hi nahi.

// EfViReceiver: ef_vi RAW API hardware queues seedha expose karta --
// NO socket() call hi nahi hota. Conceptual shape (05-ef-vi.md):
//   ef_vi vi; ef_vi_receive_post(&vi, buf, id);      // pre-post a buffer
//   ef_vi_receive_get(&vi, &pkt);                     // poll for arrival
// Yeh course mein NAHI implement kiya -- Solarflare/Xilinx NIC + driver
// + `ef_vi` SDK chahiye.

// DpdkReceiver: poll-mode driver, NIC ko OS se poori tarah HATA deta
// (`vfio-pci`/`igb_uio` bind), hugepages + lock-free rings (06-dpdk-
// intro.md). Conceptual shape:
//   rte_eal_init(...); rte_eth_rx_burst(port, queue, pkts, MAX_BURST);
// Yeh course mein NAHI implement kiya -- DPDK SDK + hugepage-configured
// kernel + (real hardware pe) DPDK-supported NIC chahiye.
