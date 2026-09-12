# 04 — TCP latency issues: Nagle, delayed ACK, and friends

## Prerequisites
- `03-tcp-deep.md` (handshake, ACKs, cwnd)

## Yeh topic abhi kyun
TCP correctness ke liye bana hai, latency ke liye nahi. Uske andar chaar-paanch
mechanisms hain jo aaram se ~40 ms ka spike de dete hain ek chatty request/reply
pattern pe. HFT mein order path pe TCP use hota hai — to yeh spikes **poison**
hain. Yeh lesson: har ek ko naam do, samjho, aur band karo. Example `03` Nagle
wala classic demo hai.

---

## The big one: Nagle + delayed ACK deadlock

### Nagle's algorithm (sender side, RFC 896)
"Jab tak ek unACKed small segment outstanding hai, naya small segment mat
bhejo — chhoti writes ko ek bade segment mein jama karo."

Rationale (1984): telnet jaise apps jo har keystroke pe 1 byte bhejte the →
41-byte frame for 1 byte payload → network flood. Nagle coalesces.

### Delayed ACK (receiver side, RFC 1122)
"ACK ko turant mat bhejo — up to ~40 ms wait karo, is umeed mein ki tumhare paas
bhi reply data hoga jispe ACK piggyback ho jaaye." Linux: `~40 ms` max, often
less; every 2nd full segment ACKed immediately.

### The deadlock
Request/reply pattern, chhote messages, `TCP_NODELAY` off:

```
client: write(header)  -> segment 1 sent
client: write(body)    -> Nagle HOLDS it (segment 1 unACKed)
server: receives segment 1 (partial message) -> can't reply yet -> delayed ACK timer starts
        ... ~40 ms pass ...
server: delayed ACK fires -> ACKs segment 1
client: ACK received -> Nagle releases -> sends body
server: now has full message -> replies
```

**~40 ms per round-trip**, on a link where the RTT is microseconds. Example `03`
reproduces this: `TCP_NODELAY` off → p90/p99 ~40 ms; on → ~tens of µs.

### Fixes (do all)
1. **`TCP_NODELAY = 1` on every socket.** Disables Nagle. This alone fixes the
   common case.
2. **One write per message.** `writev({header, body})` instead of two `write`s —
   the OS sends one segment, and there's no "second small write" for Nagle to
   hold anyway.
3. **`TCP_QUICKACK`** (Linux, per-call, resets itself) — suppress delayed ACK on
   the receiver. Belt-and-suspenders; `TCP_NODELAY` on both ends is usually
   enough.

---

## Other TCP latency sources

### `TCP_CORK` (opposite of NODELAY)
`setsockopt(TCP_CORK, 1)` tells TCP to **not send partial segments** until you
uncork or the segment is full — deliberate batching. Useful for building one
packet from many `write`s (then uncork). **Not for HFT hot paths** — it's the
latency-for-throughput trade you're avoiding. Know it exists so you don't leave
it on by accident.

### Slow start after idle (`03`)
Bursty order flow, idle between bursts → `cwnd` reset → first burst slow-started.
`net.ipv4.tcp_slow_start_after_idle=0`.

### RTO minimum on loss (`03`)
Lost last-segment, no dup-ACKs → ~200 ms before retransmit. Not tunable below
the kernel minimum easily. Design for low loss; app-level heartbeat to detect
stalls faster.

### Small socket buffers → head-of-line + throughput cap
`SO_SNDBUF` too small → `send()` blocks / `EAGAIN` sooner; `SO_RCVBUF` too small
→ window shrinks → sender throttled. For order flow (tiny messages) rarely an
issue; for drop-copy/snapshot streams it can be.

### Head-of-line blocking (inherent to TCP)
One lost segment blocks **all** later bytes on that connection until it's
recovered — even if those later bytes are independent messages. This is
*fundamental* to TCP's ordered stream. If independent message streams can't
tolerate one blocking the others → separate connections, or UDP + app ordering
(`05`), or QUIC (not common in HFT).

### `qdisc` / traffic control on the TX path
Default `pfifo_fast` / `fq_codel` add a queue. Usually negligible, but a
misconfigured shaper (`tc`) or `fq` pacing can add latency. HFT: simple qdisc,
no shaping on the trading interface.

### Interrupt coalescing / GRO / LRO (receive side)
Not TCP itself, but they delay when TCP even *sees* the segment (`01`, `15`).
`ethtool -C` low coalescing, `ethtool -K eth0 gro off lro off`.

---

## Internal working — how to *see* it

```bash
# example 03 dekho, phir:
sudo tcpdump -i lo -ttt -n 'tcp port <port>'    # inter-packet gaps -- ~40ms gaps = Nagle/delACK
ss -tin                                          # 'nodelay' flag? cwnd, rtt, unacked
nstat -az | grep -iE 'DelayedACK|TCPTimeouts'
```

`tcpdump` with `-ttt` (delta timestamps) is the giveaway: a request/reply loop
showing consistent ~40 ms gaps between a data segment and its ACK = Nagle +
delayed-ACK.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `TCP_NODELAY` on one end only
Both ends matter. If the server has Nagle on and does two small writes for its
reply, you get the stall in the other direction. Set it on **every** socket,
client and server, listening socket's accepted fds too.

### Trap 2 — setting `TCP_NODELAY` on the listening socket and expecting it to inherit
Accepted sockets **do** inherit most options from the listener on Linux
(including `TCP_NODELAY`), but be explicit — set it right after `accept4()` too.
Don't rely on inheritance across platforms.

### Trap 3 — "I set `TCP_NODELAY`, why still slow?"
Other causes: delayed ACK on the peer (needs `TCP_QUICKACK` or the peer's
`NODELAY`), slow-start-after-idle, a lost packet (RTO), GRO/coalescing on RX, or
it's not TCP at all (scheduler wakeup, `29/10`).

### Trap 4 — leaving `TCP_CORK` on
Set for a batched build, forgot to uncork → every message now waits. Audit for
stray `TCP_CORK`.

### Trap 5 — two writes per message
Even with `TCP_NODELAY`, two syscalls > one, and without it you've recreated the
Nagle trigger. `writev` / build the frame in one buffer.

### Trap 6 — blaming TCP for a scheduler problem
p99.9 spikes of ~1–10 ms with `TCP_NODELAY` on and no retransmits → that's the
OS scheduler waking your thread, not TCP. Pin + isolate (`29/11`), busy-poll
(`08`).

---

## > **HFT relevance**

> - **`TCP_NODELAY` on every socket, both ends.** Non-negotiable. Example `03`
>   is the "us vs 40 ms" demo.
> - **One `writev` per message** — header + body in one segment, no second
>   small write.
> - **`net.ipv4.tcp_slow_start_after_idle=0`** for bursty order flow.
> - **App-level heartbeat / sequence acks** so a stalled connection is detected
>   in ms, not seconds (RTO).
> - **Separate connections** for independent critical streams so one packet loss
>   doesn't head-of-line-block the others; or move market data to UDP/multicast
>   (`05`, `06`).
> - **RX side:** GRO/LRO off, low interrupt coalescing (`15`) — so TCP sees the
>   segment as early as possible.
> - **p99.9 tail with all of the above still bad?** It's the scheduler, not TCP
>   — go to `29/10`, `29/11`, and busy-poll (`08`).

---

## Hands-on

```bash
# Linux pe -- example 03: Nagle on vs off, chatty pattern
g++ -std=c++20 -O2 -pthread 30-NETWORKING/examples/03_nagle_demo.linux.cpp -o /tmp/nagle && /tmp/nagle

# the 40ms gaps in a capture
sudo tcpdump -i lo -ttt -n 'tcp port <port>' | head -40

# is a live connection using nodelay?
ss -tino dst <peer-ip>

# slow-start-after-idle
sysctl net.ipv4.tcp_slow_start_after_idle
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Nagle sirf bandwidth ki baat" | + delayed-ACK = ~40 ms latency on request/reply |
| "`TCP_NODELAY` ek end pe kaafi" | dono ends; accepted fds bhi |
| "`TCP_NODELAY` set kiya = fixed" | delayed-ACK / slow-start / RTO / GRO / scheduler bhi |
| "`TCP_CORK` aur `TCP_NODELAY` same knob" | opposite: CORK batches, NODELAY flushes |
| "ek message = do writes theek hai" | 2 syscalls + Nagle trigger; use `writev` |
| "p99.9 spike = TCP" | often scheduler wakeup — pin/isolate/busy-poll |

---

## Exercises

1. `TCP_NODELAY` is on. A request/reply loop still shows occasional ~40 ms
   round-trips. What's left?

   <details><summary>Answer</summary>

   Delayed ACK on the **peer**. `TCP_NODELAY` disables Nagle on *your* sender,
   but if the peer receives your segment, has no reply data yet, and its ACK is
   delayed ~40 ms — and meanwhile you're waiting for that ACK before your
   *next* small write (or the peer is waiting on your ACK) — you still stall.
   Fix: `TCP_NODELAY` on the peer too, and/or `TCP_QUICKACK` after each `recv`
   on the receiver. Also: one `writev` per message so there's never a held
   "second small write".
   </details>

2. Why does sending header and body as two `write()`s risk the Nagle stall even
   though each is a separate call?

   <details><summary>Answer</summary>

   `write(header)` sends segment 1. `write(body)` is now a *small* segment while
   segment 1 is still unACKed → Nagle holds it until segment 1 is ACKed (which,
   with delayed ACK, is ~40 ms). One `writev({header, body})` sends a single
   segment containing both — nothing is held, and there's no "second small
   write" to trigger Nagle in the first place.
   </details>

3. What is `TCP_CORK` and when would you *deliberately* use it (not on an HFT
   hot path)?

   <details><summary>Answer</summary>

   `TCP_CORK` tells TCP to buffer and not send partial segments until you
   uncork or a full MSS accumulates — the opposite of `TCP_NODELAY`. Use it
   when you're assembling one logical packet from several `write`s and want
   exactly one segment on the wire (e.g. an HTTP response header block +
   `sendfile` body): cork, write header, `sendfile`, uncork. On an HFT order
   path you never want this — you want each message out immediately.
   </details>

4. Head-of-line blocking on a TCP connection carrying two independent message
   streams — describe it and one fix.

   <details><summary>Answer</summary>

   TCP delivers bytes strictly in order. If a segment carrying stream-A data is
   lost, every later byte on that connection — including stream-B messages that
   arrived fine — is held in the kernel until the lost segment is retransmitted
   and recovered. Stream B is blocked by stream A's loss. Fix: put the two
   streams on **separate TCP connections** (independent sequence spaces), or use
   UDP with application-level ordering per stream (`05`), or a multi-stream
   transport (QUIC/SCTP — rare in HFT).
   </details>

5. After a tuning pass (`TCP_NODELAY`, `writev`, `slow_start_after_idle=0`,
   GRO off), p50 is great but p99.9 is still ~2 ms. Where do you look next?

   <details><summary>Answer</summary>

   Not TCP anymore. ~2 ms spikes with clean TCP counters = the OS scheduler
   waking your thread (`29/10`): the thread was preempted or descheduled and
   took a scheduler quantum to come back. Fixes: pin the thread to an isolated
   core (`29/11`), `nohz_full`, IRQs off that core (`29/15`), and busy-poll the
   socket (`08`) so there's no sleep/wake at all. Check
   `nonvoluntary_ctxt_switches` on the thread.
   </details>

---

## Interview questions

1. Nagle + delayed ACK — how they interact to produce a ~40 ms stall.
2. Three fixes for the Nagle/delayed-ACK stall.
3. `TCP_NODELAY` vs `TCP_CORK` — opposite knobs; when each.
4. `writev` for a header+body message — why better than two `write`s.
5. Slow-start-after-idle — effect on bursty order flow, the sysctl.
6. TCP head-of-line blocking — cause, and when it forces separate connections.
7. `TCP_NODELAY` is on and p99.9 is still bad — list non-TCP causes.

---

## Next
→ [`05-udp.md`](05-udp.md)
