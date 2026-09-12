# 05 — UDP: connectionless, unreliable, and why HFT market data uses it

## Prerequisites
- `03-tcp-deep.md` (so you can contrast)
- `02-ip-basics.md` (fragmentation, MTU)

## Yeh topic abhi kyun
Market data feeds — the firehose of every quote and trade from an exchange — are
almost always **UDP** (usually multicast, `06`). At first that seems backwards:
you'd want *reliable* delivery of price data. The reason UDP wins here is exactly
what this lesson is about: for data that's worthless once stale, TCP's
retransmit-and-wait is the wrong trade.

---

## UDP = IP + ports + a checksum. That's it.

```
  [ IP hdr 20 B ][ UDP hdr 8 B ][ payload ]
  UDP hdr: src port (2) | dst port (2) | length (2) | checksum (2)
```

No handshake, no connection state, no sequence numbers, no ACKs, no
retransmission, no flow control, no congestion control, no ordering. You call
`sendto()`, the datagram goes to IP, and that's the end of the kernel's
involvement. It might arrive, might not, might arrive out of order, might arrive
twice.

**What UDP *does* give you:**
- **Message boundaries.** One `sendto` = one datagram = one `recvfrom`. Never
  half a message, never two merged (contrast TCP's byte stream).
- **Ports** for multiplexing (which socket gets it).
- **A checksum** (optional in IPv4, mandatory IPv6) — corrupt datagrams are
  dropped, not delivered.
- **Multicast / broadcast** — one send, many receivers (`06`).
- **Minimal latency** — no stack machinery between your `send` and the wire.

---

## `connect()` on a UDP socket (yes, you can)

```cpp
int fd = socket(AF_INET, SOCK_DGRAM, 0);
connect(fd, &dst, sizeof dst);        // no packets sent -- just sets default peer
send(fd, buf, n, 0);                  // now send() works (no sendto address needed)
recv(fd, buf, n, 0);                  // and only datagrams FROM dst are delivered
```

- No handshake — `connect` just records the default destination and filters
  incoming datagrams to that source.
- **Slightly faster per packet:** the kernel resolves the route once, not per
  `sendto`.
- **You get `ECONNREFUSED`** on `recv` if the peer sent an ICMP port-unreachable
  — useful signal you don't get with unconnected UDP.
- HFT senders often `connect()` the UDP socket for this per-packet saving.

---

## Why market data is UDP (the actual reasons)

| Reason | Explanation |
|---|---|
| **Stale data is useless** | A quote from 5 ms ago has no value. TCP would retransmit it and **head-of-line block** all newer quotes behind it (`04`). You'd rather skip it and take the next one. |
| **One-to-many** | An exchange feeds thousands of subscribers. TCP is point-to-point — thousands of connections, thousands of retransmit state machines. UDP multicast: send once, the network fans out (`06`). |
| **No sender-side back-pressure** | A slow subscriber can't slow down the exchange (TCP flow control would). The exchange sends at line rate; keeping up is your problem. |
| **Predictable latency** | No congestion window, no Nagle, no RTO. Latency = wire + minimal stack. |
| **Recovery is a separate, cheaper channel** | Gaps are recovered via a **snapshot** feed or a **retransmission request** service — on demand, out of band, not blocking the live feed. |

**The trade you accept:** you will occasionally miss packets, and *you* must
detect and recover them.

---

## Handling loss and reorder yourself

Every serious UDP feed protocol carries a **sequence number** per packet (per
channel). Your receiver:

```
expected = last_seq + 1
on packet(seq):
  if seq == expected:        deliver; last_seq = seq
  elif seq >  expected:      GAP of (seq - expected) -- deliver, mark hole, trigger recovery
  elif seq <= last_seq:      duplicate or late -- drop (or use for gap-fill)
```

**Recovery mechanisms (exchange-specific):**
- **A/B (redundant) feeds:** the exchange sends the *same* stream on two
  independent multicast groups via different network paths. You subscribe to
  both, arbitrate by sequence number, take whichever packet arrives first. A
  gap on A is usually filled by B. This is the primary defense (`06`).
- **Snapshot feed:** a periodic full state dump on a separate group — you join
  it, wait for a snapshot with `seq >= your gap`, and re-sync.
- **Retransmission request (TCP):** a request/response service where you ask
  "resend packets 1000–1005". Slower; last resort.

**Example `04`/`05`** show the sender/receiver pattern with a sequence number and
gap/reorder detection.

---

## Kernel-side drops — the silent killer

The datagram made it to your NIC, but you still lost it:
- **Socket receive buffer full** (`SO_RCVBUF` too small, app not draining fast
  enough) → kernel drops, increments a counter, tells you nothing on `recv`.
- **NIC ring buffer overflow** (RX descriptors exhausted, softirq behind) →
  dropped before the socket.

```bash
netstat -su | grep -iE 'receive buffer errors|packet receive errors'
cat /proc/net/udp        # column 'drops' per socket
ethtool -S eth0 | grep -iE 'rx_dropped|rx_missed|rx_no_buffer|fifo'
ss -uepm                 # per-socket buffer usage
```

**Fixes:** large `SO_RCVBUF` + `net.core.rmem_max` (`11`, `15`), big NIC RX
rings (`ethtool -G`), drain fast (`recvmmsg` batching, dedicated pinned
receiver thread, busy-poll), IRQ affinity off hot cores (`29/15`).

---

## `recvmmsg` / `sendmmsg` — batch the syscalls

One syscall, up to N datagrams:

```cpp
struct mmsghdr msgs[32];
// ... set up iovecs pointing at 32 buffers ...
int n = recvmmsg(fd, msgs, 32, MSG_WAITFORONE, nullptr);
for (int i = 0; i < n; ++i) process(bufs[i], msgs[i].msg_len);
```

At a high packet rate (market data bursts), this cuts per-packet syscall
overhead ~N-fold. Example `05` uses it.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — expecting reliability
UDP will lose, reorder, and duplicate. If your code assumes "every packet
arrives once, in order", it's wrong. Sequence numbers + gap detection + a
recovery path, always.

### Trap 2 — datagram > MTU → fragmentation
One lost fragment = whole datagram lost, plus reassembly cost/DoS surface
(`02`). Keep payload + headers ≤ path MTU.

### Trap 3 — silent kernel drops
`recv` never tells you a packet was dropped in the socket buffer. Monitor
`/proc/net/udp` drops, `netstat -su`, `ethtool -S`. A rising drop count during
bursts = too-small buffers or a too-slow / preempted receiver.

### Trap 4 — one `recvfrom` per packet at high rate
Per-packet syscall (~hundreds of ns) becomes the bottleneck in a burst.
`recvmmsg` batches; a busy-poll loop avoids the wakeup.

### Trap 5 — assuming your test (loopback) represents production
`lo` doesn't drop, doesn't reorder, has a 64 KB MTU. Your gap-handling code is
never exercised. Test with `tc netem` (`loss`, `reorder`, `duplicate`) or a real
lossy link.

### Trap 6 — no A/B arbitration
Relying on a single feed → every gap needs a snapshot/retransmit round-trip
(slow). Subscribe to both A and B feeds and arbitrate by sequence — most gaps
vanish for free.

### Trap 7 — treating `connect()`ed UDP as a "connection"
It's still unreliable and stateless — `connect` only sets a default peer and a
source filter. No handshake happened, no delivery guarantee gained.

---

## > **HFT relevance**

> - **Market data = UDP multicast (`06`).** Sequence-numbered; you detect gaps.
> - **A/B feed arbitration is the primary recovery** — two multicast groups,
>   different paths, take the first-arriving packet per sequence number. Cheap,
>   fast, no round-trip.
> - **Snapshot feed** for cold start and large gaps; **retransmit request
>   (TCP)** as a last resort.
> - **Fight kernel drops:** big `SO_RCVBUF` + `net.core.rmem_max`, big NIC RX
>   rings, `recvmmsg` batching, a dedicated **pinned, isolated** receiver thread
>   (`29/11`), IRQs off that core (`29/15`), busy-poll (`08`). Monitor
>   `/proc/net/udp` drops continuously.
> - **`connect()` the UDP socket** for the small per-packet route-resolution
>   saving.
> - **The endgame is kernel bypass (`13`)** — DPDK/ef_vi poll the NIC ring in
>   userspace, and the socket buffer / syscall / drop problem disappears.

---

## Hands-on

```bash
# Linux pe -- examples 04 + 05
g++ -std=c++20 -O2 30-NETWORKING/examples/05_udp_receiver.linux.cpp -o /tmp/rx
g++ -std=c++20 -O2 30-NETWORKING/examples/04_udp_sender.linux.cpp   -o /tmp/tx
/tmp/rx 9200 100000 & sleep 0.3 ; /tmp/tx 127.0.0.1 9200 100000 5

# inject loss/reorder to actually exercise gap handling
sudo tc qdisc add dev lo root netem loss 1% reorder 2% delay 50us
# ... run again, watch the receiver's lost/reordered counts ...
sudo tc qdisc del dev lo root

# kernel-side drop counters
watch -n1 'netstat -su | grep -iE "receive buffer|receive errors"'
cat /proc/net/udp
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "UDP fast but sabhi packets aate hain" | loss/reorder/dup expected; you handle it |
| "TCP hi reliable data ke liye" | for stale-able data, TCP's retransmit blocks newer data |
| "`recv` batayega packet gira" | silent; monitor `/proc/net/udp`, `netstat -su` |
| "loopback test kaafi hai" | no loss/reorder; use `tc netem` |
| "`connect()` UDP = reliable connection" | just a default peer + source filter |
| "ek feed kaafi" | A/B redundant feeds = most gaps filled free |

---

## Exercises

1. Why would retransmitting a lost market-data packet (TCP-style) actually make
   things *worse* for a trading strategy?

   <details><summary>Answer</summary>

   TCP delivers in order, so the retransmitted old packet would **head-of-line
   block** every newer quote/trade behind it until it's recovered (~1 RTT best
   case, ~200 ms if RTO). Meanwhile the market has moved — you'd be processing a
   stale price while blind to the current one. With UDP you skip the gap, keep
   consuming fresh data, and recover the hole out-of-band if you even need it.
   Freshness beats completeness for live prices.
   </details>

2. Your UDP receiver shows 0 loss on loopback but 3% loss in production during
   the open. First things to check?

   <details><summary>Answer</summary>

   (1) Kernel-side drops: `cat /proc/net/udp` drops column, `netstat -su`
   "receive buffer errors", `ethtool -S eth0` `rx_dropped`/`rx_missed`. (2)
   `SO_RCVBUF` — is it large, and is `net.core.rmem_max` large enough to not cap
   it? (3) Is the receiver thread keeping up — pinned, isolated, using
   `recvmmsg`, not doing heavy work inline? (4) NIC RX ring size (`ethtool -G`).
   (5) IRQ affinity — is NIC IRQ/softirq colliding with the receiver's core
   (`29/15`)? Loopback exercises none of this, which is why it looked clean.
   </details>

3. Design gap recovery for a single-feed UDP protocol with per-packet sequence
   numbers. What are the pieces?

   <details><summary>Answer</summary>

   (1) Track `last_in_order_seq`; on `seq > expected`, record the hole(s) and
   keep delivering fresh data (don't block). (2) A **snapshot** channel (often a
   separate multicast group or a TCP request) giving full state with a sequence
   watermark — join/request it, discard live packets ≤ watermark, resume. (3) A
   **retransmit request** service (usually TCP) for small, recent gaps: "send me
   1000–1004". (4) Dedup: drop `seq <= last_in_order_seq`. (5) A gap that can't
   be filled within a deadline → mark the book stale / go flat. (Adding a second
   redundant feed — A/B — makes most of this rare; see `06`.)
   </details>

4. `recvmmsg` vs a `recvfrom` loop — when does the difference matter, and by how
   much?

   <details><summary>Answer</summary>

   It matters under **burst** — when many datagrams are queued and you're
   syscall-bound. Each `recvfrom` is a syscall (~200–500 ns trap + copy);
   `recvmmsg` amortizes one trap over up to N datagrams. At 32/batch you cut
   per-packet syscall overhead ~30×. At low rates (one packet, then idle) it's
   neutral — you still block/wake once. The bigger win at low rate is busy-poll
   (no wake at all).
   </details>

5. You test with `tc qdisc add dev lo root netem loss 1% reorder 2%` and your
   receiver's "lost" count roughly matches but "reordered" is 0. Bug?

   <details><summary>Answer</summary>

   Likely a bug in the reorder detection, or the reorder is being *counted as
   loss + duplicate*. Check the logic: a reordered packet arrives with `seq <
   last_seq` (if `last_seq` was already advanced by a later packet) — that
   branch must increment "reordered", not be silently dropped or double-counted
   as a gap when the later packet first arrived. Also `netem reorder` needs a
   `delay` to have something to reorder against — `netem loss 1% reorder 2%
   delay 50us` (the example's hands-on uses this). Verify with `tcpdump` that
   packets are actually arriving out of order on the wire.
   </details>

---

## Interview questions

1. What UDP provides vs what it doesn't (vs TCP).
2. Why is exchange market data UDP (multicast) and not TCP — the real reasons.
3. Sequence numbers + gap detection — the receiver algorithm.
4. A/B feed arbitration — how it recovers loss without a round-trip.
5. Kernel-side UDP drops — where they happen, how to see them, how to reduce.
6. `recvmmsg` — what it batches, when it matters.
7. `connect()` on a UDP socket — what it does and doesn't give you.

---

## Next
→ [`06-multicast.md`](06-multicast.md)
