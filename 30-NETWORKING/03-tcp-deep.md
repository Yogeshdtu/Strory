# 03 — TCP deep: handshake, sequence numbers, retransmission, congestion

## Prerequisites
- `02-ip-basics.md`
- `01-network-models.md` (encapsulation, byte order)

## Yeh topic abhi kyun
Order entry aur drop-copy connections aksar **TCP** hote — reliable, ordered
delivery chahiye ("mera order pahuncha ya nahi" pe koi shak nahi). Par TCP ke
andar bahut machinery hai (handshake, retransmit timers, congestion window,
Nagle) jo latency spikes de sakti. `04` un latency issues pe focus karega; yeh
lesson TCP ka mental model banata hai taaki woh spikes samajh aayein.

---

## Connection = 4-tuple + state machine

Ek TCP connection unique hai: **(src IP, src port, dst IP, dst port)**. Kernel
har connection ka ek TCB (control block) rakhta with a state:

```
CLOSED -> (connect) SYN_SENT -> ESTABLISHED
CLOSED -> (listen)  LISTEN -> (SYN rcvd) SYN_RCVD -> ESTABLISHED
ESTABLISHED -> (close) FIN_WAIT_1 -> FIN_WAIT_2 -> TIME_WAIT -> CLOSED
ESTABLISHED -> (rcv FIN) CLOSE_WAIT -> LAST_ACK -> CLOSED
```

`ss -tan` se live states dekho.

---

## 3-way handshake (connection setup)

```
client                          server
  |  SYN  seq=X                    |     "connect karna hai, mera ISN = X"
  |------------------------------->|
  |            SYN-ACK seq=Y ack=X+1     "theek, mera ISN = Y, tumhara X mila"
  |<------------------------------|
  |  ACK  ack=Y+1                  |     "tumhara Y mila -- connection ESTABLISHED"
  |------------------------------->|
  |  ... data flows both ways ...  |
```

**Cost: 1 RTT** before you can send data (client can piggyback data on the 3rd
ACK with TCP Fast Open, rarely used). To a colocated exchange gateway ~µs; but a
**reconnect mid-session costs you that RTT** plus slow-start.

- **ISN (Initial Sequence Number)** randomized (security).
- **Options** in SYN: MSS (`02`), window scale, SACK permitted, timestamps.

## Connection teardown + TIME_WAIT

`close()` → FIN exchange (each side sends FIN, gets ACK). The side that closes
first (usually the client) sits in **TIME_WAIT** for **2×MSL** (~60 s on Linux)
— so late-arriving segments from the old connection don't corrupt a new one on
the same 4-tuple.

- **Problem:** a client rapidly reconnecting to the same server 4-tuple runs out
  of ephemeral ports / hits TIME_WAIT buildup.
- **Fixes:** `SO_REUSEADDR` (rebind despite TIME_WAIT), server-side don't-close-
  first, `SO_LINGER {1,0}` (send RST, skip TIME_WAIT — drops unsent data, use
  carefully), `net.ipv4.ip_local_port_range` widen. HFT gateways keep **one
  long-lived connection** — reconnect is an exceptional event.

---

## Sequence numbers, ACKs, and the sliding window

- Every byte has a **sequence number**. `seq` in a segment = seq of its first
  byte. `ack` = "next byte I expect" (cumulative).
- Receiver advertises a **window** (`rwnd`) = "I have this much buffer free" —
  flow control (fast sender can't drown slow receiver).
- Sender can have up to `min(rwnd, cwnd)` bytes **in flight** (sent, not yet
  ACKed). Window "slides" as ACKs come in.
- **Window scaling** (SYN option): the 16-bit window field × 2^scale, so
  windows > 64 KB (needed for high bandwidth-delay product).

## Retransmission — the reliability engine

- **RTO (Retransmission Timeout):** sender starts a timer per segment. No ACK by
  RTO → retransmit. RTO estimated from **RTT samples** (SRTT + variance), with a
  **minimum ~200 ms** on Linux (`TCP_RTO_MIN`). First retransmit ~200 ms — a
  huge spike for HFT.
- **Fast retransmit:** 3 duplicate ACKs (receiver saw a gap) → sender
  retransmits the missing segment immediately, without waiting for RTO. Much
  faster (~1 RTT).
- **SACK (Selective ACK):** receiver tells the sender exactly which byte ranges
  it got, so the sender retransmits only the holes, not everything after.
- **Exponential backoff:** repeated timeouts double the RTO (200 ms, 400, 800…).
  A brief link blip can stall a connection for seconds.

**HFT implication:** a single lost packet on your order connection can cost
~200 ms (RTO) if it's the last segment (no dup-ACKs to trigger fast retransmit).
This is why order paths want low-loss links, and why some venues also offer a
UDP "drop copy" / heartbeat so you learn about trouble faster.

---

## Congestion control — cwnd

Separate from `rwnd`. The sender's guess at how much the **network** can take.

- **Slow start:** `cwnd` starts small (~10 MSS), doubles every RTT until a loss
  or `ssthresh`.
- **Congestion avoidance:** linear growth (+1 MSS/RTT).
- **On loss:** classic (Reno) halves `cwnd`; **CUBIC** (Linux default) uses a
  cubic function, more aggressive recovery; **BBR** models bandwidth + RTT
  directly (better on lossy/long links).
- **After idle:** `cwnd` can reset to initial (slow start again) —
  `net.ipv4.tcp_slow_start_after_idle=0` disables this (important if your order
  connection is bursty/idle between trades).

For a colocated, low-loss, low-RTT order connection, congestion control barely
engages — but `tcp_slow_start_after_idle` and RTO minimum are the two knobs that
bite.

---

## Nagle + delayed ACK (preview of `04`)

- **Nagle:** don't send a new small segment while a previous small segment is
  unACKed — coalesce small writes. Saves bandwidth, **adds latency**.
- **Delayed ACK:** receiver waits up to ~40 ms before ACKing (hoping to
  piggyback on a reply).
- Together on a request/reply pattern with small messages → ~40 ms stalls.
- **Fix: `TCP_NODELAY` on every socket** (`04`, `11`). This is the single most
  important TCP setting for HFT.

---

## Internal working / observability

```bash
ss -tin                       # per-connection: rtt, cwnd, retrans, rcv/snd buffers, bytes_acked
ss -tan state time-wait | wc -l
nstat -az | grep -i -E 'retrans|timeout|drop'
cat /proc/net/netstat         # TcpExt: retransmits, RTO, dup-acks, SACKs
```

`TCP_INFO` (`getsockopt(fd, IPPROTO_TCP, TCP_INFO, ...)`) gives your program the
same per-connection stats — RTT, `snd_cwnd`, `retrans`, `rttvar`. Log these on
your order connection.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `TCP_NODELAY` off (default)
Small request/reply → Nagle + delayed ACK → ~40 ms spikes. Set `TCP_NODELAY`
on every socket, always (`04`).

### Trap 2 — expecting message boundaries
TCP is a **byte stream**. `send("ABC"); send("DE")` → peer may `recv` "ABCDE" or
"AB" then "CDE". You must frame (length prefix / delimiter). (`07`)

### Trap 3 — RTO minimum surprise
A lost last-segment with no dup-ACKs → ~200 ms RTO before retransmit. You can't
tune this below the kernel minimum easily; design for low loss and add an
application-level heartbeat.

### Trap 4 — slow start after idle
Order connection idle for seconds between trades → `cwnd` collapses → next burst
is slow-started. `net.ipv4.tcp_slow_start_after_idle=0`.

### Trap 5 — TIME_WAIT exhaustion on reconnect loops
Client reconnecting fast to one server 4-tuple → ephemeral ports exhausted /
TIME_WAIT pile-up. Long-lived connections; `SO_REUSEADDR`; don't close-first.

### Trap 6 — small `SO_RCVBUF`/`SO_SNDBUF` throttling throughput
For bulk (snapshots, historical), a small window caps throughput to
`window / RTT`. Enlarge buffers + `net.core.rmem_max`/`wmem_max` (`11`, `15`).

### Trap 7 — assuming `write()` returning = "sent on the wire"
It means "copied into the socket send buffer". Actual transmission depends on
`cwnd`, `rwnd`, Nagle, qdisc. `TCP_INFO` / timestamping (`14`) to know.

---

## > **HFT relevance**

> - **`TCP_NODELAY` on every socket, no exceptions.** #1 setting.
> - **One long-lived order connection** per venue; reconnect is an incident, not
>   a pattern. Keep `SO_REUSEADDR`, don't be the side that closes first.
> - **`net.ipv4.tcp_slow_start_after_idle=0`** — bursty order flow shouldn't
>   re-slow-start.
> - **Low-loss links.** A dropped last-segment = ~200 ms RTO. Colocated, clean
>   fiber, monitored error counters (`ss -ti`, `nstat`).
> - **Application heartbeat** so you detect a stalled connection faster than
>   TCP's timers (seconds).
> - **UDP/multicast for market data** (`05`, `06`) — TCP's retransmit-and-wait
>   is the wrong trade for data that's worthless once stale.
> - **Log `TCP_INFO`** on the order socket — rising `retrans`/`rttvar` is an
>   early warning.

---

## Hands-on

```bash
# handshake dekho
sudo tcpdump -i lo -n 'tcp port 9099 and (tcp[tcpflags] & (tcp-syn|tcp-fin) != 0)' &
# example 01 + 02 chalao

# per-connection internals
ss -tin dst 127.0.0.1

# TIME_WAIT count
ss -tan state time-wait | wc -l

# retransmit / RTO counters
nstat -az | grep -iE 'RetransSegs|TCPTimeouts|TCPFastRetrans|TCPLostRetransmit'

# RTO minimum
ip route show | grep rto_min                     # per-route override possible
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "TCP message-oriented hai" | byte stream; framing tumhari |
| "`write()` return = wire pe gaya" | socket buffer mein copy; cwnd/Nagle/qdisc baaki |
| "loss → fast retransmit hamesha" | 3 dup-ACKs chahiye; last-segment loss → ~200ms RTO |
| "`TCP_NODELAY` optional optimization" | HFT ke liye mandatory; ~40ms spikes without |
| "idle connection stays fast" | `cwnd` resets after idle unless disabled |
| "TIME_WAIT ek bug hai" | correctness feature; manage with reuse + long connections |

---

## Exercises

1. Client sends an order (last segment), it's lost, there's no more data to
   send. How long until retransmit, and why not faster?

   <details><summary>Answer</summary>

   ~200 ms — the RTO minimum on Linux. Fast retransmit needs **3 duplicate
   ACKs**, which the receiver only generates when it receives *later* segments
   and notices the gap. If this was the last segment, there are no later
   segments → no dup-ACKs → the sender must wait for the RTO timer. This is why
   HFT order paths need low loss and an app-level heartbeat/ack, not reliance on
   TCP's timers.
   </details>

2. Throughput of a bulk snapshot download is stuck at ~8 MB/s despite a 10 Gb
   link. RTT is 40 ms. Why?

   <details><summary>Answer</summary>

   Window-limited: throughput ≈ `window / RTT`. 8 MB/s × 0.04 s ≈ 320 KB window.
   The receive window (or send buffer, or `cwnd`) is ~320 KB. Bandwidth-delay
   product for 10 Gb × 40 ms = 50 MB — you'd need a ~50 MB window to fill the
   pipe. Fix: enlarge `SO_RCVBUF`/`SO_SNDBUF` and `net.core.rmem_max`/`wmem_max`,
   ensure window scaling is on. (For colocated µs-RTT order connections this
   never bites; for cross-region bulk it does.)
   </details>

3. Why does `send()` returning success not mean the data is on the wire?

   <details><summary>Answer</summary>

   `send()` copies your bytes into the kernel's socket send buffer and returns.
   Whether/when they hit the wire depends on: the congestion window (`cwnd`),
   the receiver's advertised window (`rwnd`), Nagle (if `TCP_NODELAY` is off,
   small data waits for an ACK), the qdisc/traffic-control queue, and the NIC
   TX ring. To know when it actually left, use TX timestamping (`14`) or infer
   from `TCP_INFO`.
   </details>

4. Your order connection is idle for 5 seconds between bursts, and the first
   order of each burst is slower. TCP cause + fix.

   <details><summary>Answer</summary>

   Slow-start-after-idle: after an idle period longer than the RTO, Linux resets
   `cwnd` to the initial window, so the first burst is slow-started again (only
   a few segments per RTT until it ramps). Fix: `sysctl -w
   net.ipv4.tcp_slow_start_after_idle=0`. Also consider a keepalive/heartbeat
   trickle so the connection is never truly idle.
   </details>

5. You see thousands of connections in TIME_WAIT on your client box. Cause and
   three mitigations.

   <details><summary>Answer</summary>

   The client is opening and closing many short-lived connections to the same
   server (it's the side that closes first, so it holds TIME_WAIT for ~60 s).
   Mitigations: (1) use a **persistent** connection — reconnect only on failure;
   (2) `SO_REUSEADDR` so you can rebind despite TIME_WAIT; (3) let the **server**
   close first where possible (moves TIME_WAIT to the server, which has one
   listening tuple, not the client's ephemeral ports); (4) widen
   `net.ipv4.ip_local_port_range`; (avoid `tcp_tw_recycle` — removed/buggy with
   NAT).
   </details>

---

## Interview questions

1. 3-way handshake — packets, cost, when it bites HFT.
2. Sequence numbers + cumulative ACK + sliding window — how flow control works.
3. RTO vs fast retransmit — when each fires, the ~200 ms minimum.
4. SACK — what problem it solves.
5. `cwnd` vs `rwnd` — congestion vs flow control.
6. TIME_WAIT — why it exists, how to manage it.
7. `TCP_NODELAY` — what Nagle does, why HFT always disables it.

---

## Next
→ [`04-tcp-latency-issues.md`](04-tcp-latency-issues.md)
