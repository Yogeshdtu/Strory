# Examples — Folder 30 (Networking)

> **Sab examples `*.linux.cpp` hain — Linux-only.** POSIX sockets, `epoll`,
> multicast, `SO_TIMESTAMPING`, `recvmmsg` — MinGW/Windows pe compile nahi hote
> (`<sys/socket.h>`, `<sys/epoll.h>` absent; Windows Winsock alag API). `./build.ps1
> folder 30-NETWORKING` inhe **SKIP** karta hai (`SKIP (linux-only)`), aur
> `./build.ps1 checkall` bhi. Linux ya WSL chahiye.

## Compile / run (Linux ya WSL)

```bash
cd 30-NETWORKING/examples
g++ -std=c++20 -O2 -Wall -Wextra -pthread 01_tcp_echo_server.linux.cpp -o echo_srv && ./echo_srv 9099
# pairs:
./echo_srv 9099 &            ; ./tcp_cli 127.0.0.1 9099 20000        # 01 + 02
./udp_rx 9200 100000 &       ; ./udp_tx 127.0.0.1 9200 100000 5      # 05 + 04
./mcast 239.1.2.3 9300 lo                                            # 06 (self-contained)
./epoll_srv 9400 &           ; for i in $(seq 200); do (printf hi | nc -q1 127.0.0.1 9400 &); done   # 07
```

## Examples

| File | Lesson(s) | Kya dikhata hai |
|---|---|---|
| `01_tcp_echo_server.linux.cpp` | 07, 01 | `socket`→`SO_REUSEADDR`→`bind`→`listen`→`accept4`→`recv`/`send_all` loop, one client at a time. Short-write loop, `MSG_NOSIGNAL`, `recv`==0 = FIN, `TCP_NODELAY` on the accepted fd. |
| `02_tcp_client.linux.cpp` | 07, 03 | `getaddrinfo` (v4/v6), `connect`, `TCP_NODELAY`, request/reply round-trip **latency histogram** (p50/p90/p99/p99.9/max). Loopback p50 ~15–30 µs (kernel stack both ways). |
| `03_nagle_demo.linux.cpp` | 04 | Chatty 2-small-writes-then-wait pattern, `TCP_NODELAY` **OFF vs ON**. OFF → p90/p99 ~40 ms (Nagle + delayed-ACK). ON → ~tens of µs. The "us vs 40 ms" demo. |
| `04_udp_sender.linux.cpp` | 05 | `SOCK_DGRAM`, `connect`+`send`, sequence-numbered 64-byte packets, local `ENOBUFS` tracking. Fire-and-forget. |
| `05_udp_receiver.linux.cpp` | 05, 06 | `bind`, big `SO_RCVBUF`, **`recvmmsg` batching**, seq-based loss/reorder/dup detection, inter-arrival jitter. `/proc/net/udp` drops discussion. |
| `06_multicast_receiver.linux.cpp` | 06 | `IP_ADD_MEMBERSHIP` on an explicit interface (`imr_ifindex`), self-contained sender thread (`IP_MULTICAST_IF/TTL/LOOP`), `recvfrom` loop, `IP_DROP_MEMBERSHIP`. Runs on `lo` for testing. |
| `07_epoll_server.linux.cpp` | 10, 09, 08 | Full **edge-triggered** epoll echo server: `epoll_create1`, `accept4` drain-to-`EAGAIN`, `EPOLLIN\|EPOLLET` clients, `recv` drain, `EPOLLHUP/ERR`, `SO_REUSEPORT`. O(ready) vs poll's O(N). |
| `08_socket_options.linux.cpp` | 11 | Read defaults, set the HFT set (`TCP_NODELAY`, `SO_RCVBUF/SNDBUF`, `SO_REUSEADDR/PORT`, `TCP_QUICKACK`, `SO_BUSY_POLL`, keepalive, `SO_LINGER`), read back. Shows the `SO_RCVBUF` doubling + `rmem_max` cap. |
| `09_timestamping.linux.cpp` | 14, 05 | `SO_TIMESTAMPING` (SW + HW flags), `recvmsg` + cmsg parse (`scm_timestamping.ts[0]`/`ts[2]`), kernel-RX → app-recv gap. Clock-domain caveat spelled out. |
| `10_latency_measure.linux.cpp` | 08, 10 | UDP loopback ping-pong RT latency histogram + ASCII buckets, **blocking `recv` vs busy-poll (`MSG_DONTWAIT` spin)**. Busy-poll ~½ p50, much tighter tail. |

## Expected numbers — sab TYPICAL, aapke box pe NAHI nape gaye

Har `.linux.cpp` file ke neeche `EXPECTED OUTPUT` comment block hai with
representative Linux loopback numbers, **explicitly "not measured on your
machine"** labelled. Yeh CLAUDE.md Rule 2 ka honest treatment hai: is Windows
box pe POSIX sockets/epoll chal hi nahi sakte, to numbers fabricate karke
"measured" nahi bola gaya. Asli numbers `17-exercises.md` Part D mein khud Linux
pe lo.

**Ratios pe focus, absolute pe nahi:** loopback absolute latency kernel version,
scheduler, load pe badalta hai. Lesson ratio mein hai — Nagle OFF vs ON
(~40 ms vs ~µs), blocking vs busy-poll (~2× p50 + tail), epoll vs poll
(flat vs O(N)), UDP loss with small vs large `SO_RCVBUF`.

## Notes / jaan-boojh kar cheezein

- **`.linux.cpp` naming** — same convention as folder 29. `build.ps1` skips
  these in `folder`/`checkall` (`SKIP (linux-only)` bucket, not `FAIL (real)`).
- **`#define _GNU_SOURCE 1`** is the first line of every file — strict
  `-std=c++20` otherwise hides glibc extensions (`accept4`, `recvmmsg`,
  `ip_mreqn`, `SO_REUSEPORT`, `MSG_ZEROCOPY`, `SO_TIMESTAMPING` flags). With
  `-std=gnu++20` the `#define` is redundant.
- **No `broken_on_purpose` file.** Har example ek correct program hai jo ek
  networking property demonstrate/measure karta.
- **Self-contained where possible:** `03`, `06`, `09`, `10` run a sender thread
  inside the same process (loopback) so you can run one binary. `01`+`02` and
  `04`+`05` are pairs.
- **`06` on `lo`:** loopback multicast works for testing; real deployment needs
  a NIC + switch IGMP config. Pass a real interface name as arg 3 for that.
- **`09` on loopback:** no hardware timestamps (no NIC PHC) — you get software
  timestamps only; the `EXPECTED` block notes the real-NIC picture.
- **`tc netem`** (`17-exercises.md` D2) is how you actually exercise the
  loss/reorder handling in `05` — loopback alone never drops.
- **WSL2 caveat:** examples run, but scheduler/timer/NIC behaviour depends on
  the Windows host — `02`, `03`, `10` latency tails aren't representative;
  `01`, `07`, `08` (API behaviour) are fine.
