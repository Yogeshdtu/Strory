# 11 — Socket options: TCP_NODELAY, SO_REUSEPORT, SO_RCVBUF, SO_BUSY_POLL

## Prerequisites
- `04-tcp-latency-issues.md` (Nagle), `05-udp.md` (kernel drops)
- `08-blocking-vs-nonblocking.md` (busy-poll)

## Yeh topic abhi kyun
`setsockopt` se tum kernel ko batate ho har socket ko kaise handle kare. HFT
mein ~6 options matter karte, aur unhe **startup pe, har socket pe, hard-coded**
set karna best practice hai (config drift se bacho). Example `08` sabko read/set
karta hai.

---

## The HFT short list

| Option | Level | Value | Kyun |
|---|---|---|---|
| **`TCP_NODELAY`** | `IPPROTO_TCP` | `1` | Nagle off. #1. Every socket, both ends. (`04`) |
| **`SO_RCVBUF`** | `SOL_SOCKET` | large (MBs) | market-data burst headroom; fewer kernel drops (`05`) |
| **`SO_SNDBUF`** | `SOL_SOCKET` | large-ish | avoid `send` blocking / `EAGAIN` under burst |
| **`SO_REUSEADDR`** | `SOL_SOCKET` | `1` | rebind despite TIME_WAIT; fast restart |
| **`SO_REUSEPORT`** | `SOL_SOCKET` | `1` | N processes/threads, one port; kernel load-balances (Linux) |
| **`SO_BUSY_POLL`** | `SOL_SOCKET` | `~50` (µs) | `recv`/`epoll_wait` spins in-kernel before sleeping (`08`) |
| **`TCP_QUICKACK`** | `IPPROTO_TCP` | `1` | suppress delayed ACK (per-call, resets) (`04`) |
| **`SO_BINDTODEVICE`** | `SOL_SOCKET` | `"eth0"` | pin socket to a NIC on multi-homed box (`02`) |
| **`IP_TOS` / `SO_PRIORITY`** | `IPPROTO_IP` / `SOL_SOCKET` | DSCP / prio | mark packets for QoS on the switch |
| **`SO_TIMESTAMPING`** | `SOL_SOCKET` | flags | HW/SW RX/TX timestamps (`14`) |

```cpp
int one = 1;
setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,  &one, sizeof one);
setsockopt(fd, SOL_SOCKET,  SO_REUSEADDR, &one, sizeof one);
setsockopt(fd, SOL_SOCKET,  SO_REUSEPORT, &one, sizeof one);
int rb = 8 << 20; setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rb, sizeof rb);
```

---

## Buffer sizing — `SO_RCVBUF` / `SO_SNDBUF` and the `*mem_max` cap

- The value you set is **capped by `net.core.rmem_max` / `wmem_max`** (`29/06`).
  Ask for 16 MB but `rmem_max` is 208 KB → you get 208 KB (×2 for bookkeeping).
  **Raise the sysctls first** (`15`).
- `getsockopt(SO_RCVBUF)` returns roughly **2× what you set** — the kernel
  doubles it to account for its own overhead (skb metadata). Don't be confused.
- Auto-tuning: TCP has `net.ipv4.tcp_rmem`/`tcp_wmem` (min/default/max) and
  grows the window dynamically — **setting `SO_RCVBUF` explicitly disables
  auto-tuning** for that socket. For a known workload (market data) that's fine;
  for general TCP, sometimes let it auto-tune.
- **UDP has no auto-tuning** — `SO_RCVBUF` is your only lever against socket-
  buffer drops; size it for your worst burst (rate × burst-duration × packet-
  size), with margin.

---

## `SO_REUSEPORT` — the scaling primitive

Multiple sockets (in different threads or processes) can `bind()` the **same**
IP:port if all set `SO_REUSEPORT`. The kernel then:
- **TCP:** hashes each incoming connection to one of the listening sockets → N
  independent accept queues → no shared-listen-fd contention, no thundering
  herd. Each worker `accept`s its own connections.
- **UDP:** hashes each datagram (by 4-tuple) to one socket → N receivers share
  the load of one multicast group / one port.

HFT: each strategy process opens its own `SO_REUSEPORT` socket on the
market-data port → the kernel fans the feed out to all of them, each with its
own buffer and its own pinned core. (Careful: rebalancing when a socket is added/
removed can briefly misroute; `SO_REUSEPORT` + `EBPF` reuseport program for
deterministic steering in advanced setups.)

---

## `SO_BUSY_POLL` — kernel-side spin

```cpp
int usves = 50;
setsockopt(fd, SOL_SOCKET, SO_BUSY_POLL, &usves, sizeof usves);
```
A blocking `recv` / `epoll_wait` will **poll the NIC driver in the kernel** for
up to 50 µs before it actually sleeps. Near-busy-poll latency, but it **falls
back to sleeping when idle** (doesn't burn the core 24/7). Needs
`CAP_NET_ADMIN` for values, or set the system default via `net.core.busy_poll` /
`net.core.busy_read` (`29/06`). Good middle ground for connections that are hot
but not hot enough to justify a dedicated spinning core.

---

## Keepalive (for control connections)

```cpp
setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof one);
int idle = 10, intvl = 3, cnt = 3;
setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE,  &idle,  sizeof idle);   // start probing after 10s idle
setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof intvl);  // probe every 3s
setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT,   &cnt,   sizeof cnt);    // 3 failed probes -> dead
```
Detects a silently-dead peer (cable pull, crash) in ~19 s instead of ~2 hours
(default). HFT order connections usually also run an **application-level
heartbeat** (faster, and it exercises the actual message path).

`TCP_USER_TIMEOUT` — how long unACKed data may remain before the connection is
declared dead (independent of keepalive); set it to a few seconds on order
connections so a stalled link fails fast instead of retrying for minutes.

---

## `SO_LINGER` — close() behaviour

```cpp
linger lg{1, 0};                    // on=1, timeout=0
setsockopt(fd, SOL_SOCKET, SO_LINGER, &lg, sizeof lg);
```
`{1, 0}` → `close()` sends a **RST** immediately, skips the FIN handshake and
**TIME_WAIT** — but **discards any unsent data**. Used for fast reconnect cycling
where you don't care about in-flight bytes. `{1, N}` → `close()` blocks up to N
seconds trying to flush, then RST. Default `{0, 0}` → graceful FIN, background
flush, TIME_WAIT.

---

## Inheritance

Accepted sockets on Linux **inherit** `SO_RCVBUF`/`SO_SNDBUF`, `TCP_NODELAY` (and
most options) from the **listening** socket at `accept` time. Still **set them
explicitly** on each accepted fd — inheritance rules differ across
options/kernels/platforms, and being explicit is a one-liner.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `TCP_NODELAY` not on every socket
One missed accepted fd or one direction → ~40 ms stalls (`04`). Set it in the
socket-setup helper that *every* fd goes through.

### Trap 2 — `SO_RCVBUF` set, still dropping
`net.core.rmem_max` caps it. `sysctl -w net.core.rmem_max=134217728` first
(`15`). Verify with `getsockopt` (expect ~2× your value).

### Trap 3 — confused by `getsockopt(SO_RCVBUF)` doubling
Kernel returns ~2× the set value (bookkeeping overhead). Not a bug.

### Trap 4 — `SO_REUSEPORT` without understanding rebalancing
Adding/removing a listener re-hashes the distribution → some in-flight
connections/datagrams can land on the wrong socket briefly. For deterministic
steering use an eBPF `SO_ATTACH_REUSEPORT_EBPF` program, or a fixed set of
sockets opened at startup and never changed.

### Trap 5 — `SO_LINGER {1,0}` losing data
It sends RST and drops unsent bytes. Fine for "cycle this dead connection", a
data-loss bug for "cleanly close after sending a logout message". Send + confirm
first, then close.

### Trap 6 — `SO_BUSY_POLL` on every socket
It spins a kernel poll loop per blocking call — great for the hot feed, wasteful
(CPU) for 500 idle control connections. Apply selectively.

### Trap 7 — setting options after `connect`/data flow and expecting retroactive effect
`TCP_NODELAY` mid-stream still helps going forward, but `SO_RCVBUF` after the
window has grown, or `TCP_MAXSEG` after the handshake, may not do what you
expect. Set options **before** `connect`/`listen` where the option's semantics
are connection-setup-time.

---

## > **HFT relevance**

> - **A single `configure_socket(fd)` helper** every socket passes through:
>   `TCP_NODELAY=1`, `SO_REUSEADDR=1`, `SO_REUSEPORT=1`, big `SO_RCVBUF`/
>   `SO_SNDBUF`, `SO_BUSY_POLL` (hot ones), keepalive + `TCP_USER_TIMEOUT` (order
>   connections), `SO_BINDTODEVICE` (multi-NIC). Hard-coded, asserted, logged.
> - **Raise `net.core.rmem_max`/`wmem_max` (and `tcp_rmem`/`tcp_wmem`)** in the
>   box sysctls (`29/18`) so the socket values aren't silently capped.
> - **`SO_REUSEPORT`** so each strategy process has its own market-data socket +
>   buffer + pinned core; kernel fans out.
> - **`SO_BUSY_POLL`** for hot-but-not-dedicated-core connections; full
>   userspace busy-poll (`08`) or kernel bypass (`13`) for the truly critical
>   feed/order path.
> - **App-level heartbeat + `TCP_USER_TIMEOUT`** so a stalled order link fails in
>   seconds, not the default minutes.

---

## Hands-on

```bash
# Linux pe -- example 08: defaults + HFT settings + readback
g++ -std=c++20 -O2 30-NETWORKING/examples/08_socket_options.linux.cpp -o /tmp/so && /tmp/so

# the sysctl caps
sysctl net.core.rmem_max net.core.wmem_max net.ipv4.tcp_rmem net.ipv4.tcp_wmem

# a live socket's options
ss -tio dst <peer>            # nodelay, cwnd, rtt, rcv/snd space
ss -m -p sport = :9099        # memory: rb (rcvbuf), tb (sndbuf), r (used)

# busy poll defaults
sysctl net.core.busy_poll net.core.busy_read
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`TCP_NODELAY` listener pe set = sab fds" | inherits usually, but set on each accepted fd too |
| "`SO_RCVBUF` set = mila" | capped by `net.core.rmem_max`; raise sysctl first |
| "`getsockopt(SO_RCVBUF)` doubled = bug" | kernel bookkeeping overhead; expected |
| "`SO_REUSEPORT` = free scaling, no caveats" | rebalancing on add/remove; use fixed set / eBPF |
| "`SO_LINGER {1,0}` = clean fast close" | RST + drops unsent data |
| "`SO_BUSY_POLL` everywhere" | per-call kernel spin — selective, hot sockets only |

---

## Exercises

1. You set `SO_RCVBUF` to 16 MB on a UDP market-data socket, but still see drops
   in `/proc/net/udp`. Two things to check.

   <details><summary>Answer</summary>

   (1) `net.core.rmem_max` — if it's the default (~208 KB), your 16 MB request
   was silently capped. `sysctl -w net.core.rmem_max=134217728`, then re-set the
   socket option, then `getsockopt` to confirm (~2× your value). (2) The
   receiver isn't draining fast enough — even a big buffer overflows if the
   consuming thread is preempted or doing heavy work inline. Pin/isolate the
   receiver (`29/11`), use `recvmmsg`, move NIC IRQs off its core (`29/15`).
   </details>

2. Why set `TCP_NODELAY` on the accepted socket even though Linux says it's
   inherited from the listener?

   <details><summary>Answer</summary>

   Inheritance rules vary by option, kernel version, and platform, and it's a
   single `setsockopt` call. Being explicit removes any doubt and makes the
   intent visible in the code. The cost of forgetting it (a ~40 ms Nagle stall)
   is far worse than the cost of a redundant call. Best practice: every fd goes
   through one `configure_socket()` helper that sets it unconditionally.
   </details>

3. `SO_REUSEPORT` on 4 strategy processes reading one multicast group. One
   process restarts and briefly some packets go to the wrong process. Why, and
   the mitigation?

   <details><summary>Answer</summary>

   The kernel's `SO_REUSEPORT` group hashes datagrams across the *current* set
   of sockets. When a socket leaves/joins (process restart), the hash buckets
   re-map, so datagrams that would have gone to socket X briefly go to socket Y
   until things settle. Mitigations: (1) attach an eBPF program
   (`SO_ATTACH_REUSEPORT_EBPF`) that steers deterministically by a field you
   control; (2) open a fixed set of sockets at startup owned by a supervisor and
   pass them to workers (`SCM_RIGHTS`, `29/09`) so the set never changes; (3)
   accept that market-data consumers each independently sequence/dedup, so a few
   misrouted packets are harmless (each process just sees a gap that the feed's
   own recovery handles).
   </details>

4. What does `TCP_USER_TIMEOUT` add over `SO_KEEPALIVE`, for an order
   connection?

   <details><summary>Answer</summary>

   `SO_KEEPALIVE` probes an **idle** connection to detect a dead peer.
   `TCP_USER_TIMEOUT` bounds how long **unacknowledged sent data** may linger
   before the kernel declares the connection dead — i.e. it acts when you're
   actively sending and not getting ACKs (a stalled link mid-trade). Without it,
   TCP retransmits with exponential backoff for ~15+ minutes. Set it to a few
   seconds on an order connection so a stall becomes a fast, detectable failure
   you can react to (failover, alert), not a silent multi-minute hang.
   </details>

5. When would you *not* want to set `SO_RCVBUF` explicitly on a TCP socket?

   <details><summary>Answer</summary>

   Setting it explicitly **disables TCP receive-window auto-tuning** for that
   socket (the kernel would otherwise grow/shrink the window based on the
   bandwidth-delay product it measures). For a general-purpose TCP connection
   with unknown/variable RTT and bandwidth, auto-tuning usually does better than
   a fixed guess. For a known workload — a colocated order connection (tiny
   messages, µs RTT) or a market-data stream with a characterized burst profile
   — an explicit size sized to the worst case is fine and more predictable.
   </details>

---

## Interview questions

1. The ~6 HFT-relevant socket options and one reason for each.
2. `SO_RCVBUF` — the `net.core.rmem_max` cap and the `getsockopt` doubling.
3. `SO_REUSEPORT` — TCP vs UDP behaviour; the rebalancing caveat.
4. `SO_BUSY_POLL` — what it does, when to use it vs full busy-poll.
5. `SO_LINGER {1,0}` — effect on `close()`, the data-loss risk.
6. `SO_KEEPALIVE` + `TCP_KEEP*` vs `TCP_USER_TIMEOUT` vs app heartbeat.
7. Option inheritance from listener to accepted fd — why still set explicitly.

---

## Next
→ [`12-zero-copy.md`](12-zero-copy.md)
