# 17 — Exercises: networking

## Prerequisites
- Poora folder 30 (`01`–`16`)

## Kaise use karein
- Part A: behaviour predict karo, phir Linux/WSL pe verify.
- Part B: find-the-bug — har snippet mein ek networking bug hai.
- Part C: design — architecture decisions, HFT context.
- Part D: challenge — build + measure. Real numbers likho (CLAUDE.md Rule 2).

> Examples `*.linux.cpp` hain — is Windows box pe compile nahi honge
> (`build.ps1 folder 30-NETWORKING` unhe SKIP karta hai). Linux/WSL:
> `g++ -std=c++20 -O2 -pthread file.linux.cpp -o file && ./file`

---

## Part A — Predict the behaviour

### A1
Client: `send(fd, "ORDER", 5); send(fd, "|", 1); send(fd, "1000", 4);`
Server: `char b[64]; int n = recv(fd, b, 64, 0);`
`TCP_NODELAY` is **off** on both. What does `n` and `b` most likely look like on
the **first** `recv`, and what's the latency?

<details><summary>Answer</summary>

The first `send("ORDER", 5)` goes out as segment 1. `send("|")` and
`send("1000")` are small and segment 1 is unACKed → **Nagle holds them**. The
server receives "ORDER" (`n=5`, `b="ORDER"`), has no reply → **delayed ACK** (up
to ~40 ms). After the ACK, Nagle releases "|1000". So the first `recv` gets 5
bytes; the rest arrives ~40 ms later. Fix: `TCP_NODELAY=1` on both ends, and one
`writev({"ORDER|1000"})` instead of three sends. (`04`)
</details>

### A2
A UDP socket with default `SO_RCVBUF`. A sender blasts 200k 64-byte datagrams as
fast as possible over loopback; the receiver does `recvfrom` in a loop with a
100 µs `sleep` between calls. What happens?

<details><summary>Answer</summary>

The receiver can't keep up — it processes ~10k/s (100 µs/packet) while the
sender does millions/s. The socket receive buffer (default ~208 KB → ~3000
64-byte datagrams) fills, then the kernel **silently drops** the rest. `recvfrom`
never errors; you just see a large gap between the highest sequence number and
the count received. `cat /proc/net/udp` drops column / `netstat -su` "receive
buffer errors" confirm. Fix: no `sleep`, `recvmmsg` batching, big `SO_RCVBUF`,
pinned receiver. (`05`)
</details>

### A3
`epoll` with `EPOLLIN | EPOLLET` on a non-blocking client fd. The peer sends
100 KB. Your handler does exactly one `recv(fd, buf, 8192)` per event and
returns. The peer sends nothing more. State of the connection?

<details><summary>Answer</summary>

Stuck. ET notifies only on the empty→non-empty transition. You read 8 KB and
left 92 KB in the socket buffer; there's no new edge, so `epoll_wait` won't
report the fd again until the peer sends *more*. The 92 KB (and whatever request
it contains) sit unprocessed indefinitely. Fix: on every `EPOLLIN`, loop `recv`
until `EAGAIN`. (`08`, `10`)
</details>

### A4
`select(maxfd + 1, &rfds, ...)` in a chat server. It works fine, then starts
corrupting memory / crashing when the server passes ~1100 concurrent
connections. Why?

<details><summary>Answer</summary>

`fd_set` is a fixed 1024-bit bitmask (`FD_SETSIZE`). Once an accepted fd is
≥ 1024, `FD_SET(fd, &rfds)` writes past the bitmask into adjacent memory →
corruption. `select` can't be fixed for this — move to `poll` (array, no limit)
or `epoll`. (`09`)
</details>

### A5
Two processes each `IP_ADD_MEMBERSHIP` for `239.1.2.3:9300`, both with
`SO_REUSEADDR` **but not `SO_REUSEPORT`**, on Linux. What happens on the second
`bind`, and on packet delivery?

<details><summary>Answer</summary>

With `SO_REUSEADDR` alone, both can `bind` the multicast group:port (multicast
addresses are treated leniently for `SO_REUSEADDR`). Packet delivery: on Linux,
without `SO_REUSEPORT`, a multicast datagram to that group:port is delivered to
**all** matching bound sockets (multicast fan-out semantics) — so actually both
receive every packet, which is often what you want for multiple strategies.
`SO_REUSEPORT` would instead *load-balance* (hash) datagrams to one socket each —
which you'd want for sharded workers, not for "every strategy sees the whole
feed". Know which semantics you need. (`06`, `11`)
</details>

### A6
An order connection is idle 8 seconds between bursts. `TCP_NODELAY` is on. The
first order of each burst has ~5× the latency of the rest. TCP cause?

<details><summary>Answer</summary>

Slow-start-after-idle: after an idle period longer than the RTO, Linux resets
`cwnd` to the initial window, so the first burst is slow-started (a few segments
per RTT until it ramps). Fix: `sysctl -w net.ipv4.tcp_slow_start_after_idle=0`,
and/or a keepalive trickle so the connection is never truly idle. (`03`, `04`)
</details>

### A7
You enable `SO_TIMESTAMPING` with `SOF_TIMESTAMPING_RX_HARDWARE |
SOF_TIMESTAMPING_RAW_HARDWARE` and read `ts[2]` from the cmsg. The values are all
zero, but `ts[0]` (software) is populated. Why?

<details><summary>Answer</summary>

Hardware timestamping isn't actually enabled on the NIC. `SO_TIMESTAMPING` asks
the socket layer for stamps, but you also need the `SIOCSHWTSTAMP` ioctl (or
`hwstamp_ctl -i eth0 -r 1`, or a running PTP daemon) to turn on the NIC's PHY
timestamping and set the RX filter to "all packets". Without it, the HW slots
stay zero and you silently get software stamps only. Also check `ethtool -T
eth0` actually lists HW RX support, and that you're on a real NIC (loopback has
no PHC). (`14`)
</details>

---

## Part B — Find the bug

### B1
```c
ssize_t n = recv(fd, buf, len, 0);
if (n < 0) { perror("recv"); close(fd); }
process(buf, n);            // <-- ?
```

<details><summary>Answer</summary>

`n == 0` (peer closed cleanly) isn't handled — it falls through to
`process(buf, 0)` and keeps looping on a dead connection. And on `n < 0` with
`errno == EAGAIN`/`EINTR` you shouldn't `close` — that's "try again". And
`process(buf, n)` runs even after `n < 0` (you `perror`+`close` but don't
`return`/`continue`). Fix: `if (n == 0) { close; return; }` `if (n < 0) { if
(errno==EAGAIN||errno==EINTR) return; perror; close; return; }` then
`process(buf, n)`. (`07`)
</details>

### B2
```c
int fd = socket(AF_INET, SOCK_STREAM, 0);
struct sockaddr_in a = {0};
a.sin_family = AF_INET;
a.sin_port = 9099;                          // <-- ?
a.sin_addr.s_addr = INADDR_ANY;             // <-- ?
bind(fd, (struct sockaddr*)&a, sizeof a);
```

<details><summary>Answer</summary>

Missing byte-order conversion. `a.sin_port = htons(9099)` (wire is big-endian;
raw `9099` binds to port 35619 on x86). `a.sin_addr.s_addr = htonl(INADDR_ANY)`
— `INADDR_ANY` is 0 so `htonl` is a no-op here, but it's wrong style and breaks
for any non-zero address. Also no `SO_REUSEADDR` (rebind after restart will hit
`EADDRINUSE` for ~60 s), and `bind`'s return isn't checked. (`01`, `11`)
</details>

### B3
```c
// epoll edge-triggered server, on writable:
if (ev.events & EPOLLOUT) {
    ssize_t k = send(fd, conn->out, conn->out_len, MSG_NOSIGNAL);
    if (k > 0) { conn->out += k; conn->out_len -= k; }
    // leave EPOLLOUT registered
}
```

<details><summary>Answer</summary>

Two bugs: (1) `EPOLLOUT` is never removed when `out_len` hits 0 → `epoll_wait`
returns the fd writable every iteration (a socket with send-buffer room is
always writable) → 100% CPU spin. When `conn->out_len == 0`, `epoll_ctl(ep,
EPOLL_CTL_MOD, fd, &ev_without_EPOLLOUT)`. (2) `k < 0` unhandled: `EAGAIN` (buffer
full again — fine, wait), `EINTR` (retry), other (close). (3) `conn->out += k`
mutates the base pointer — need a separate offset or you can't free/reset the
buffer. (`10`)
</details>

### B4
```c
// UDP multicast receiver, multi-NIC box
ip_mreqn mreq = {0};
inet_pton(AF_INET, "239.1.2.3", &mreq.imr_multiaddr);
// mreq.imr_ifindex left 0
setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof mreq);
```

<details><summary>Answer</summary>

`imr_ifindex == 0` lets the kernel pick the interface for the IGMP membership
report. On a multi-NIC box that can be the wrong NIC (e.g. `eth1` order-entry
instead of `eth0` market-data), so the switch port connected to the feed never
starts forwarding the group → no packets, and `tcpdump -i eth0` shows nothing.
Fix: `mreq.imr_ifindex = if_nametoindex("eth0")` (the market-data NIC). Also
check `rp_filter=2` on that NIC and a route for `239.0.0.0/8` out it. (`06`,
`02`)
</details>

### B5
```c
// "read exactly a 40-byte message"
char msg[40];
ssize_t n = recv(fd, msg, sizeof msg, MSG_WAITALL);
dispatch(msg);
```

<details><summary>Answer</summary>

`MSG_WAITALL` blocks until 40 bytes arrive **or** the connection ends / a signal
hits. If the peer sends 39 bytes and stalls (or the real message is shorter, or
`EINTR`), this hangs indefinitely (or returns short with no clear handling).
Also no error/`n==0` check, and if the fd is non-blocking `MSG_WAITALL` is
ignored (returns what's available). Robust: loop `recv` accumulating into `msg`
with your own completion check and a timeout (`poll`), handle `n==0` (closed),
`EAGAIN`, `EINTR`. And do proper framing — assuming every message is exactly 40
bytes on a stream socket only works if the protocol guarantees it. (`07`)
</details>

### B6
```c
// TCP client, non-blocking connect
fcntl(fd, F_SETFL, O_NONBLOCK);
connect(fd, &addr, len);
// wait for writable via epoll...
if (ev.events & EPOLLOUT) {
    start_sending(fd);          // <-- ?
}
```

<details><summary>Answer</summary>

"Writable" after a non-blocking `connect` doesn't mean "connected" — it can also
fire on **failure** (`ECONNREFUSED`, `ETIMEDOUT`, `EHOSTUNREACH`). You must check
`getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len)`: `err == 0` → connected;
non-zero → that's the failure errno, close and handle (retry/alert). Sending on
a failed socket → `EPIPE`/`ECONNRESET` and confusion. (`07`, `08`)
</details>

---

## Part C — Design

### C1
Design the transport for (a) market-data ingest, (b) order entry, (c) internal
"risk check" request/response between two processes on the same box, (d) shipping
logs to a remote collector. For each: protocol, and one key setting.

<details><summary>Answer</summary>

(a) **UDP multicast**, A/B groups, sequence numbers; key: big `SO_RCVBUF` +
`net.core.rmem_max`, busy-poll receiver (→ kernel bypass). (b) **TCP**, one
long-lived connection per venue; key: `TCP_NODELAY` + `writev` framing + app
heartbeat + `TCP_USER_TIMEOUT`. (c) **Unix domain socket** (`SOCK_SEQPACKET`) or
better a **shared-memory ring** (`29/08`) if it's on the hot path — no kernel
stack, ~ns; key: lock-free SPSC, no shared mutex. (d) **TCP** to the collector,
but **off the hot path** — a background thread reads a lock-free log ring and
`writev`s batches; key: never block the trading threads, drop-oldest on
backpressure. (`05`, `03`, `29/08`, `29/09`)
</details>

### C2
Your feed handler occasionally reports gaps on feed A. Walk through the recovery
design so the hot loop never stalls.

<details><summary>Answer</summary>

(1) Hot loop consumes A **and** B into one sequencer; a gap on A is usually
filled by B's copy (arbitrate by sequence, drop dups) — most "A gaps" never
become real gaps. (2) A real gap (both feeds missed `1000–1004`): the hot loop
records `{1000,1004}` in a small gap list and **keeps consuming live data** —
never blocks. (3) A separate **recovery thread** watches the gap list: if a gap
is older than a threshold and still unfilled, it requests a retransmit
(`1000–1004`) over the venue's TCP retransmit service, or waits for the next
**snapshot** on the snapshot group. (4) Recovered packets are merged back by
sequence; the book is marked "recovering" meanwhile and the strategy can choose
to widen/pull quotes. (5) If a gap can't be filled within a deadline → mark the
book stale / go flat and alert. (`05`, `06`, `16`)
</details>

### C3
Tick-to-trade is 15 µs. Budget: NIC+wire ~1 µs, your code ~1 µs, and ~13 µs
"somewhere in the OS". Give the diagnosis-and-fix order.

<details><summary>Answer</summary>

(1) **HW RX timestamps** (`14`) to split the 13 µs: HW-RX-TS → kernel-sees → app
`recv` → your first instruction. (2) If most is **HW-RX → app**: kernel RX path
+ wakeup → blocking `recv`? switch to **busy-poll** (`08`); core not isolated /
`nohz_full`? (`29/11`); NIC IRQ/softirq on a hot core / high coalescing / GRO
on? (`29/15`, `15`); then **kernel bypass** for the feed (`13`) removes most of
what's left. (3) If a chunk is **send-side**: `TCP_NODELAY`, `writev`, or bypass
TX. (4) If it's **jittery** rather than constant: page faults (warm-up, `mlock`,
`29/13`), THP collapse (`29/12`), C-state/turbo (`29/06`), NUMA (`29/14`),
cgroup throttle (`29/17`). Change one, re-measure, attribute. (`13`, `29`)
</details>

### C4
A colleague proposes replacing the kernel TCP stack with a hand-written
userspace TCP for the order connection "to save latency". Argue the trade-offs
and a safer path.

<details><summary>Answer</summary>

Costs: you now own the handshake, sequence/ack, sliding window, retransmission +
RTO, congestion control, TIME_WAIT, PMTUD, and every corner case — a
multi-quarter, correctness-critical project whose first bug corrupts an order or
hangs a connection under load. The latency saved is the send-side syscall +
stack (~1–3 µs) on a path that carries relatively few messages. Safer paths, in
order: (1) `TCP_NODELAY` + `writev` + `SO_BUSY_POLL` + `TCP_USER_TIMEOUT` +
box tuning — often gets you most of the way; (2) **Onload/VMA** — an
`LD_PRELOAD` userspace stack that's kernel-compatible, your socket code
unchanged, vendor-maintained (`13`); (3) bypass only the **UDP market-data
feed** (simple, biggest win) and leave order-entry TCP on the kernel or Onload.
Hand-rolling TCP is rarely the right first move.
</details>

### C5
Design the observability for a kernel-bypassed (DPDK) market-data path — you've
lost `tcpdump`, `ss`, `/proc/net`.

<details><summary>Answer</summary>

(1) **Capture:** `dpdk-pdump` (or a `tee` in the poll loop writing raw frames to
a ring/file, or the framework's mirror) so you can still get a pcap for an
incident. (2) **Counters in your code:** packets/s, bytes/s, drops (ring full),
per-sequence gaps, A vs B contribution, parse errors, per-stage latency
histograms (HW RX TS → decode → publish). Export to your metrics system. (3)
**HW RX timestamps** from the NIC PHC (still available via the framework) +
PTP health (`pmc` offset). (4) **A switch SPAN/mirror port** to a separate
kernel-stack capture box for ground-truth wire traffic independent of your
process. (5) **Watchdogs:** "no packet on A or B for N ms" → alert;
ring-occupancy high-water mark. (6) A **kernel-stack fallback path** (plain
socket join of the same groups on a second NIC/queue) for comparison and
emergency. (`13`, `14`, `15`)
</details>

---

## Part D — Challenge (build + measure)

> Linux/WSL. Real numbers.

### D1 — Nagle costs 40 ms
`examples/03_nagle_demo.linux.cpp` chalao. `TCP_NODELAY` OFF vs ON ke p50/p90/
p99/max likho. Kitne % iterations ~40 ms hit karte OFF pe? Phir example ko badlo:
OFF rakho par do `send`s ki jagah ek `writev({hdr, body})` — Nagle stall abhi
bhi hota hai?

<details><summary>Expected shape</summary>

OFF: p50 tiny (Nagle sometimes doesn't trigger) but p90/p99 ~40 ms — a large
fraction stall. ON: clean ~tens of µs throughout. With one `writev` even with
Nagle OFF-the-setting: no stall — there's no "second small write" for Nagle to
hold, so the message goes out in one segment. Lesson: fix it *both* ways
(`TCP_NODELAY` **and** one write per message). (`04`)
</details>

### D2 — UDP loss under load
`examples/05` + `04`. Loopback pe (no loss) baseline lo. Phir `tc qdisc add dev
lo root netem loss 1% reorder 3% delay 100us` add karke dobara — receiver ke
lost/reordered counts tumhare gap-detection logic se match karte? Phir
`SO_RCVBUF` ko default kar do aur sender ko `gap_us=0` (blast) — kernel-side
drops (`/proc/net/udp`) dikhte?

<details><summary>Expected shape</summary>

Clean loopback: 0/0. With `netem`: "lost" ≈ the injected loss %, "reordered"
> 0 (needs the `delay` to have something to reorder against). Blast + small
`SO_RCVBUF`: `/proc/net/udp` drops climb, receiver's seq-gap count matches. Big
`SO_RCVBUF` + `net.core.rmem_max` raised + `recvmmsg`: drops fall toward 0.
(`05`, `15`)
</details>

### D3 — blocking vs busy-poll
`examples/10_latency_measure.linux.cpp` chalao. Blocking recv vs busy-poll ke
p50/p99/p99.9 likho. Ratio? Phir dono threads ko `taskset -c` se alag isolated
cores pe pin karke dobara — p99.9 kitna tighten hua?

<details><summary>Expected shape</summary>

Busy-poll p50 ~½ of blocking (no wakeup), tail much tighter. Pinning to isolated
cores tightens p99.9 further toward p50 (removes migration + preemption). On a
noisy laptop the tail stays fat regardless — the point is the *direction* and
the ratio. (`08`, `29/11`)
</details>

### D4 — epoll vs poll scaling
Ek load gen likho jo N idle TCP connections khole (N = 100, 1k, 10k) + 10 active.
Ek `poll`-based echo server aur `examples/07` (epoll) — dono ke CPU% aur p99
echo latency measure karo as N badhta hai. Curve kaisa?

<details><summary>Expected shape</summary>

`poll`: CPU and p99 latency grow roughly linearly with N (O(N) copy + scan every
call), even though only 10 connections are active. `epoll`: both stay ~flat —
`epoll_wait` returns only the ~10 ready fds regardless of how many idle ones are
registered. This is the whole reason `epoll` exists (`09`, `10`).
</details>

### D5 — where do the microseconds go
`examples/09_timestamping.linux.cpp` (or add `SO_TIMESTAMPING` to `05`). Measure
the software-RX-TS → app-`recv`-TS gap for 100k packets: p50/p99/max. Then
busy-poll the receiver instead of blocking — how does the gap change? (On a real
NIC with HW timestamping, compare HW-RX-TS → SW-RX-TS → app.)

<details><summary>Expected shape</summary>

Blocking: gap p50 ~few µs, p99/max much larger (wakeup + scheduler). Busy-poll:
p50 drops (no wake), tail tightens. Real NIC: HW-RX → SW-RX is the kernel's
early receive path (~sub-µs to µs); SW-RX → app is wakeup + copy. Kernel bypass
would collapse both. This is the number every other tuning in folders 29–30 is
trying to shrink. (`14`, `08`, `13`)
</details>

---

## Next
→ [`../31-CPU-ARCHITECTURE/00-README.md`](../31-CPU-ARCHITECTURE/00-README.md)
