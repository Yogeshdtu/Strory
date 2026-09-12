# 11 — Layer 10: networking (TCP/UDP, multicast, epoll, kernel bypass)

## Prerequisites
Folders `30-NETWORKING`, `42-HFT-NETWORKING`. Market data is UDP
multicast; order entry is TCP. Know both paths and how to make them fast.

---

## A — TCP vs UDP

### A1. Market data UDP multicast kyun, order entry TCP kyun?
<details><summary>Answer</summary>
**Market data:** one-to-many, high rate, latency-critical, and a lost
packet is stale in microseconds anyway → **UDP multicast** (one send,
switch replicates to all subscribers; no per-client state; no
retransmit delay). Gaps handled by sequence numbers + a separate
recovery/snapshot channel. **Order entry:** you *must* not lose or
reorder an order, you need an ack → **TCP** (reliable, ordered), tuned
with `TCP_NODELAY` etc. (`30`, `38`, `42`.)
</details>

### A2. `TCP_NODELAY` — what it disables, why HFT wants it.
<details><summary>Answer</summary>
Disables **Nagle's algorithm**, which buffers small writes until an ACK
returns or enough data accumulates (to reduce tiny packets). Nagle +
delayed-ACK can add ~40 ms of latency to a small request/response. HFT
sends small order messages immediately → always `TCP_NODELAY`. (`30`,
`42`.)
</details>

### A3. Delayed ACK — what is it, interaction with Nagle.
<details><summary>Answer</summary>
The receiver delays sending a bare ACK (up to ~40–200 ms) hoping to
piggyback it on outgoing data. If the sender has Nagle on and is waiting
for that ACK before sending more small data → deadlock-ish stall. Fix on
the sender: `TCP_NODELAY`. `TCP_QUICKACK` on the receiver disables delayed
ACK (resets itself, must re-set). (`30`, `42`.)
</details>

### A4. TCP head-of-line blocking — kya hai?
<details><summary>Answer</summary>
TCP delivers bytes **in order**. One lost segment → all subsequent
segments wait in the kernel buffer until the retransmit fills the gap,
even though they arrived. For a multiplexed stream (many logical messages
on one connection) a single loss stalls everything behind it. UDP has no
HOL blocking (but no reliability). QUIC / SCTP address this differently.
(`30`, `42`.)
</details>

### A5. `SO_REUSEPORT` — use case.
<details><summary>Answer</summary>
Multiple sockets bound to the same port; the kernel load-balances
incoming connections/datagrams across them — one per worker thread, each
with its own accept/receive queue → no shared accept lock, better
multicore scaling. Common in high-throughput servers and multi-threaded
UDP receivers. (`30`.)
</details>

---

## B — Multicast & feed handling

### B1. Multicast join — the API path.
<details><summary>Answer</summary>
`socket(AF_INET, SOCK_DGRAM)` → `setsockopt(SO_REUSEADDR)` → `bind` to
the port (INADDR_ANY or the group) → `setsockopt(IP_ADD_MEMBERSHIP,
{group_addr, interface_addr})` to join. IGMP tells the switch to forward
that group to this port. `IP_MULTICAST_LOOP`, source-specific multicast
(`IP_ADD_SOURCE_MEMBERSHIP`) for filtering. (`30`, `42`.)
</details>

### B2. A/B feed arbitration — kya aur kyun.
<details><summary>Answer</summary>
Exchanges publish the same market data on **two** independent multicast
feeds (A and B) via separate infrastructure. The handler listens to both,
keys on sequence number, and takes whichever packet arrives first,
discarding the duplicate. Survives a single-path loss with **zero**
recovery latency. Gap on both → request a retransmit / snapshot. (`38`,
`42`.)
</details>

### B3. Sequence gap detection + recovery.
<details><summary>Answer</summary>
Each message has a monotonically increasing sequence number. Handler
tracks "next expected"; `seq > expected` → gap (buffer the ahead
packets, request retransmit for the missing range, or fall back to a
periodic **snapshot** channel and replay incrementals from there);
`seq < expected` → duplicate, drop. Determinism: never guess, always
recover from a known-good point. (`38`, `44` MarketDataSimulator has
monotonic seq.)
</details>

### B4. Why parse market data without allocating or copying?
<details><summary>Answer</summary>
Rate is millions of msgs/sec; any per-message `malloc` / `std::string` /
copy adds latency and jitter (page faults, cache pollution). Parse
**in place** over the receive buffer with a `std::span` / `const
std::byte*`, bounds-check once per frame, decode fixed-offset fields with
`memcpy` + `bswap` (or `bit_cast`). Measured: portable shift-parse ≈
`memcpy`+`bswap` at `-O2` (compiler combines it) (`38`, `43/13`, `44`
`parse_v1`/`parse_v3`).
</details>

---

## C — epoll & kernel bypass

### C1. Kernel-bypass ladder — name the rungs.
<details><summary>Answer</summary>
Plain kernel sockets → `SO_BUSY_POLL` / `SO_REUSEPORT` tuning → `AF_XDP`
(XDP sockets, partial bypass) → vendor stacks: **Solarflare Onload**
(transparent, `LD_PRELOAD`, sockets API) → **ef_vi** (Solarflare
low-level API, ~sub-µs) → **DPDK** (full userspace NIC driver, poll-mode,
you own the whole stack). Each rung: less kernel, lower latency, more
code you own + less portability. (`42`, `36/17`.)
</details>

### C2. Busy-poll vs interrupt-driven receive — trade-off.
<details><summary>Answer</summary>
**Interrupt/epoll:** CPU sleeps until a packet arrives → 0% idle CPU but
wakeup latency (interrupt → softirq → schedule → wake, µs). **Busy-poll:**
a core spins reading the NIC ring / calling `recv` → lowest latency
(~ns to see the packet) but **100% of that core** always, more power/heat.
HFT hot receive path = busy-poll on a dedicated isolated core. (`36/16`,
`42`.)
</details>

### C3. NIC interrupt coalescing — what, and HFT setting.
<details><summary>Answer</summary>
The NIC batches multiple packets before raising one interrupt (fewer
interrupts = higher throughput, higher per-packet latency). HFT: **disable
it** (`ethtool -C rx-usecs 0 rx-frames 1`) so each packet is delivered
immediately — or bypass interrupts entirely with busy-poll. (`42`,
`29/15`.)
</details>

### C4. Where do the microseconds go, wire to application?
<details><summary>Answer</summary>
NIC RX → DMA to a ring buffer → interrupt/softirq (or poll) → driver →
kernel netstack (IP/UDP demux, checksum) → socket buffer copy → `recv`
returns to userspace → your parse. Each hop is hundreds of ns; the kernel
netstack + the copy are the big removable chunks → kernel bypass gets
wire-to-app from ~5–10 µs to ~1 µs or less. Hardware timestamping in the
NIC anchors the measurement. (`42/14`, `37/14`.)
</details>

### C5. Hardware timestamping — why, and the clock-domain gotcha.
<details><summary>Answer</summary>
The NIC stamps each packet's arrival time in hardware → removes
software-stack jitter from your latency measurement. Gotcha: TX and RX
timestamps must be in the **same clock domain** (same PHC / synced) or
you're measuring relative jitter, not absolute one-way latency. PTP
(`ptp4l`/`phc2sys`) disciplines the NIC clock. (`42/14`, `30/09`.)
</details>

---

## D — HFT-flavoured

### D1. Order gateway (TCP) tuning checklist — 4 items.
<details><summary>Answer</summary>
`TCP_NODELAY` (no Nagle); pre-established, kept-warm connection (no
connect latency in the hot path, periodic app-level heartbeat); adequate
`SO_SNDBUF` but not so large it hides backpressure; `TCP_QUICKACK` /
mindful of delayed-ACK on the exchange side; pin the sender thread; and
measure round-trip with hardware timestamps. FIX (text) or a binary
order protocol on top. (`42`, `30`.)
</details>

### D2. Why is UDP "unreliable" acceptable for market data but you still
need correctness?
<details><summary>Answer</summary>
Loss and reorder happen, but a retransmitted-late quote is useless in HFT
— so you don't want TCP's blocking recovery. Correctness comes from
**sequence numbers + A/B arbitration + snapshot recovery**: you always
know if you have a complete, in-order view, and you have a defined way to
resync. "Unreliable transport, reliable application protocol." (`38`,
`42`.)
</details>

### D3. `recvmmsg` / `sendmmsg` — what they buy you.
<details><summary>Answer</summary>
Receive/send **multiple** datagrams in **one** syscall → amortizes the
user/kernel transition across a batch. Helps throughput and reduces
syscall overhead under load; adds a little head-of-line latency (you wait
to fill or timeout). A middle rung before full kernel bypass. (`30`,
`36/16-17`.)
</details>

---

## Interview tips for Layer 10

- "MD is UDP multicast, OE is TCP" and *why* (one-to-many + stale-is-
  useless vs must-not-lose) is table stakes.
- `TCP_NODELAY` + the Nagle/delayed-ACK stall story.
- A/B arbitration + sequence gaps + snapshot recovery = "reliable app
  protocol over unreliable transport."
- Kernel-bypass ladder: kernel sockets → busy-poll → Onload → ef_vi →
  DPDK, each trading portability for latency.
- Parse in place, no alloc, no copy — `span` over the receive buffer.

## Next
→ [`12-cpu-cache-questions.md`](12-cpu-cache-questions.md)
