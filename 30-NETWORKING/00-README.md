# 30 — NETWORKING (PHASE 19)

## Prerequisites
`29-LINUX-SYSTEMS`

## Yeh folder kyun
Market data network se aata hai. Orders network se jaate hain. Poora HFT game
**network latency** ka hai.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-network-models.md` | OSI, TCP/IP stack, encapsulation, headers |
| 02 | `02-ip-basics.md` | IPv4/IPv6, routing, MTU, fragmentation |
| 03 | `03-tcp-deep.md` | **TCP** — handshake, sequence numbers, retransmission, congestion control |
| 04 | `04-tcp-latency-issues.md` | **Nagle's algorithm, delayed ACK** — HFT ka classic dushman |
| 05 | `05-udp.md` | UDP — connectionless, unreliable, **kyun HFT market data UDP hai** |
| 06 | `06-multicast.md` | **Multicast** — IGMP, groups, market data ka backbone |
| 07 | `07-sockets-api.md` | `socket`, `bind`, `listen`, `accept`, `connect`, `send`/`recv` |
| 08 | `08-blocking-vs-nonblocking.md` | Blocking, non-blocking, `O_NONBLOCK`, `EAGAIN` |
| 09 | `09-select-poll.md` | `select`, `poll` — aur unki limitations |
| 10 | `10-epoll-deep.md` | **`epoll` deep dive** — level vs edge triggered, event loop design |
| 11 | `11-socket-options.md` | `TCP_NODELAY`, `SO_REUSEPORT`, `SO_RCVBUF`, `SO_BUSY_POLL` |
| 12 | `12-zero-copy.md` | `sendfile`, `splice`, `MSG_ZEROCOPY`, io_uring intro |
| 13 | `13-kernel-bypass-intro.md` | **Kernel bypass** kya hai — DPDK, Onload, ef_vi, VMA ka overview |
| 14 | `14-timestamping.md` | `SO_TIMESTAMPING`, hardware timestamps, PTP clock sync |
| 15 | `15-network-tuning.md` | NIC tuning, ring buffers, offloads, coalescing |
| 16 | `16-building-servers.md` | Echo server → epoll server → UDP multicast receiver |
| 17 | `17-exercises.md` | Practice + network programs |

## Examples

| File | Kya |
|---|---|
| `examples/01_tcp_echo_server.cpp` | Basic TCP server |
| `examples/02_tcp_client.cpp` | TCP client |
| `examples/03_nagle_demo.cpp` | Nagle ka latency pe asar — measured |
| `examples/04_udp_sender.cpp` | UDP sender |
| `examples/05_udp_receiver.cpp` | UDP receiver |
| `examples/06_multicast_receiver.cpp` | Multicast group join + receive |
| `examples/07_epoll_server.cpp` | Poora epoll event loop |
| `examples/08_socket_options.cpp` | Options ka asar |
| `examples/09_timestamping.cpp` | Hardware timestamps |
| `examples/10_latency_measure.cpp` | Round-trip latency histogram |

## Time
3–4 hafte

## Status
✅ **COMPLETE** (Batch 9 — PHASE 19). 16 lessons + `17-exercises.md` + 10
examples.

- Examples sab **`*.linux.cpp`** (POSIX sockets, `epoll`, multicast,
  `SO_TIMESTAMPING`, `recvmmsg`). Windows/MinGW pe compile nahi hote —
  `./build.ps1 folder 30-NETWORKING` inhe `SKIP (linux-only)` karta hai (10/10
  skipped, 0 fail). Linux/WSL pe verify karo.
- Benchmark numbers har example ke `EXPECTED OUTPUT` block mein **TYPICAL Linux
  loopback** figures hain, explicitly "not measured on your machine" labelled
  (CLAUDE.md Rule 2 — is box pe socket code chal hi nahi sakta). Asli numbers
  `17-exercises.md` Part D mein khud Linux pe lo.
- `build.ps1` ka `.linux.cpp` skip-hook (folder 29 mein add kiya) yahan bhi
  apply hota.

## Next
→ [`../31-CPU-ARCHITECTURE/00-README.md`](../31-CPU-ARCHITECTURE/00-README.md)
