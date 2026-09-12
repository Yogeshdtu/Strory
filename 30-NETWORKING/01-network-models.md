# 01 — Network models: OSI, TCP/IP stack, encapsulation

## Prerequisites
- `29-LINUX-SYSTEMS` (kernel vs userspace, syscalls, NIC IRQs)
- `05-OPERATORS` / bit stuff helps for headers

## Yeh folder kyun
Market data network se aata hai. Orders network se jaate hain. Ek HFT trade ka
latency budget mein sabse bada chunk **wire time + stack time** hota hai. Yeh
folder: kaise ek byte tumhare `send()` se exchange tak jaata, har layer kitni
latency jodti hai, aur usse kaise kam karte hain (`TCP_NODELAY`, UDP, multicast,
`epoll`, kernel bypass). Pehle map: layers.

## Yeh topic abhi kyun
Har baaki lesson ek layer ya ek layer ka problem hai. Pehle poori tasveer —
kaunsa header kis order mein lagta, kaun kya karta, kahan latency chhupti.

---

## OSI (7 layer) vs TCP/IP (jo actually chalta)

| OSI | TCP/IP (practical) | Kya | HFT-relevant unit |
|---|---|---|---|
| 7 Application | Application | HTTP, FIX, ITCH, OUCH, tumhara protocol | order/quote message |
| 6 Presentation | (app ka hissa) | encoding, TLS | serialization cost |
| 5 Session | (app ka hissa) | connection state | — |
| 4 Transport | **Transport** | **TCP / UDP** — ports, reliability (ya nahi) | segment / datagram |
| 3 Network | **Internet** | **IP** — addressing, routing | packet |
| 2 Data Link | **Link** | Ethernet — MAC, framing, switch | frame |
| 1 Physical | Link | copper/fiber, SFP, PHY | bits on wire |

OSI ek teaching model hai; asli internet **4-layer TCP/IP** hai. HFT mein tumhe
mainly **Transport (TCP/UDP)** aur **Link (Ethernet + NIC)** dikhega, IP beech
mein.

---

## Encapsulation — har layer apna header lagati

Jab tum `send(fd, order, 40)` karte ho, woh 40 bytes ke aage-peeche headers
lagte hain, layer by layer:

```
  App:   [ Order message (40 B) ]
  TCP:   [ TCP hdr 20 B ][ Order 40 B ]                       <- segment
  IP:    [ IP hdr 20 B ][ TCP hdr 20 B ][ Order 40 B ]        <- packet
  Eth:   [ Eth hdr 14 B ][ IP 20 ][ TCP 20 ][ Order 40 ][ FCS 4 B ]   <- frame (98 B on wire)
         + 7 B preamble + 1 B SFD + 12 B inter-frame gap (wire overhead)
```

To 40 bytes ka order ~98 bytes ka frame ban jaata (~2.5× overhead), aur
~138 bytes "wire time" (preamble + IFG ke saath). **10 Gbps pe 138 B ≈ 110 ns
serialization**; 40 Gbps pe ~28 ns. Yeh floor hai — usse tez nahi ho sakte ek
frame ke liye.

Receiving side pe ulta: NIC frame padhta → Eth header strip → IP → TCP → app ko
40 bytes.

---

## Ek `send()` se wire tak — poora path (TCP, kernel)

```
send(fd, buf, n)                             [userspace]
  -> syscall trap (~300 ns, 29/02)           [ring 3 -> 0]
  -> copy buf into socket send buffer (skb)  [user->kernel copy]
  -> TCP: segment banao, seq/ack, checksum (often NIC offload), congestion window check
  -> IP: route lookup, src/dst IP, TTL
  -> Nagle check (TCP_NODELAY off? hold karo -- 04)
  -> qdisc (traffic control queue)
  -> NIC driver: descriptor ring me daalo, doorbell
  -> NIC DMA reads frame from RAM, serializes onto wire
  ================= WIRE =================
  -> switch (store-and-forward ~300ns-1us, ya cut-through ~50-100ns)
  -> exchange NIC -> their stack -> their matching engine
```

Har arrow ek latency contribution. HFT optimization = in arrows ko chhota ya
delete karna: syscall (bypass), copy (zero-copy / bypass), Nagle (`NODELAY`),
qdisc (`pfifo_fast` / XDP), switch (cut-through), stack (kernel bypass).

**Receive path** bhi symmetric (NIC IRQ → softirq → TCP/IP → socket buffer →
`recv()` copy → wakeup), aur usme scheduler wakeup latency chhupi hoti (`29/15`,
`29/10`).

---

## Byte order — network is big-endian

Multi-byte fields wire pe **big-endian** ("network byte order"). x86 little-
endian. Isi liye `htons`/`htonl` (host→network) aur `ntohs`/`ntohl`:

```cpp
addr.sin_port = htons(9099);              // 16-bit port
addr.sin_addr.s_addr = htonl(INADDR_ANY); // 32-bit IPv4
uint32_t seq_wire = htonl(msg.seq);       // apne protocol fields bhi
```

Bhoolne pe: port "9099" wire pe "0x8B23" ki jagah "0x238B" → connection kahin
aur, ya bind fail. Exchange protocols (ITCH, OUCH, FIX-binary) ka apna endianness
spec padho — kuch big-endian, kuch little (SBE).

---

## Latency ka rough breakdown (single trade, colocated, kernel path)

| Stage | ~time |
|---|---|
| `recv()` syscall + copy + wakeup | ~1–5 µs |
| kernel RX stack (IP/UDP) | ~0.5–2 µs |
| your decode + strategy + encode | budget: ~100 ns – 1 µs |
| `send()` syscall + TCP/IP + copy | ~1–3 µs |
| NIC serialization (small frame, 10G) | ~0.1 µs |
| switch (cut-through) | ~0.05–0.1 µs |
| **kernel-path round-trip in-box overhead** | **~5–15 µs** |
| **kernel-bypass equivalent** | **~0.5–2 µs** |

Kernel bypass (`13`) is single biggest lever — it deletes the syscall + copy +
stack + wakeup for the data path.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — "network latency = ping time"
`ping` ICMP RTT includes kernel + wire dono taraf. Application-level latency
(your `send` to their ack) alag — aur usme tumhare stack ka time bhi.

### Trap 2 — endianness bhoolna
`htons`/`htonl` sirf socket structs pe nahi — apne wire protocol ke har
multi-byte field pe. Aur exchange ka spec check karo (kuch LE binary).

### Trap 3 — "MTU 1500 to mera 40-byte message ek frame"
Haan ek frame, par overhead: 40 B payload → 98 B frame → ~138 B wire. Small
messages pe overhead % bada. Batching (multiple messages / frame) throughput
badhata par latency vs batch trade-off.

### Trap 4 — layer skip karke sochna
"Main bas TCP se baat kar raha" — par TCP ke neeche IP routing, uske neeche
Ethernet + switch + NIC queues. Ek slow switch ya ek full NIC ring buffer
tumhare "TCP latency" mein dikhega.

### Trap 5 — offloads ko free maan lena
Checksum/segmentation NIC pe offload hote (GRO/GSO/TSO) — throughput ke liye
achha, **latency ke liye bura** (packets NIC pe aggregate hone ka wait). HFT:
GRO/LRO off (`15`).

---

## > **HFT relevance**

> - **Every layer is a latency line item.** This folder walks down the stack
>   removing them: `TCP_NODELAY` (Nagle), UDP instead of TCP (no handshake/
>   retransmit), multicast (one send → N receivers), `epoll` (O(ready) not
>   O(all)), busy-poll (no wakeup), and finally kernel bypass (no syscall/copy/
>   stack).
> - **Wire time is a hard floor.** A small frame at 10G is ~0.1 µs to
>   serialize; at 40/100G less. You can't beat physics for one frame — you beat
>   it with proximity (colocation), fewer hops (cut-through switches), and
>   fewer bytes.
> - **Colocation** puts your box in the exchange's datacenter — RTT to the
>   matching engine drops from ms (across a city) to ~µs. Everything else is
>   noise until you've done this.
> - **The receive path hides scheduler jitter** — NIC IRQ → softirq → wakeup.
>   `29/11` (pinning/isolation) and `29/15` (IRQ affinity) are part of the
>   network latency story, not separate.

---

## Hands-on

```bash
# ek packet ke headers dekho
sudo tcpdump -i lo -X -c 1 'tcp port 9099' &
printf 'hello' | nc 127.0.0.1 9099        # example 01 server chalao pehle

# frame size math
python3 -c "p=40; print('payload',p,'-> frame', 14+20+20+p+4, '-> wire', 14+20+20+p+4+7+1+12)"

# stack ka time roughly: loopback RTT (example 02) vs UDS RTT (29/09) vs shm (29/05)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "OSI 7 layers actually chalte" | practical stack 4-layer TCP/IP |
| "network latency = ping" | ping includes both kernels; app latency alag |
| "40 bytes = 40 bytes on wire" | +58 B headers/FCS +20 B wire overhead |
| "offloads sirf achhe" | throughput haan, latency nahi (aggregation wait) |
| "endianness sirf socket structs" | har multi-byte wire field; exchange spec padho |

---

## Exercises

1. Ek 16-byte order message. Ethernet frame kitne bytes, wire pe kitne, aur
   10 Gbps pe serialize karne mein kitna time?

   <details><summary>Answer</summary>

   Frame: 14 (Eth) + 20 (IP) + 20 (TCP) + 16 (payload) + 4 (FCS) = **74 bytes**
   (below 64-byte minimum? no, 74 > 64, OK). Wire: + 7 preamble + 1 SFD + 12
   IFG = **94 bytes**. 10 Gbps = 1.25 GB/s → 94 / 1.25e9 ≈ **75 ns**. (UDP
   would save 12 B: 8-byte UDP hdr vs 20-byte TCP.)
   </details>

2. `htons` bhoolke tumne `addr.sin_port = 9099` set kiya x86 pe. Kya port
   actually use hoga?

   <details><summary>Answer</summary>

   9099 decimal = `0x238B`. Wire wants big-endian; x86 stores it little-endian
   as bytes `8B 23`. Interpreted as network-order that's `0x8B23` = **35619**.
   So you'd bind/connect to port 35619, not 9099. `htons(9099)` byte-swaps to
   `0x8B23` in memory so the wire sees `23 8B` = 9099.
   </details>

3. Kernel path RT in-box overhead ~5–15 µs hai, wire time ~0.1 µs. Optimization
   effort kahan lagaoge?

   <details><summary>Answer</summary>

   In-box overhead — it's 50–150× the wire time. That means: kernel bypass
   (delete syscall + copy + stack + wakeup → ~0.5–2 µs), busy-poll instead of
   blocking, pinned isolated cores (no wakeup jitter), no allocation/logging on
   the path. Wire time you address structurally (colocation, cut-through switch,
   fewer bytes) — not by code.
   </details>

4. GRO (Generic Receive Offload) on hai. Market-data feed pe kya asar, HFT
   setting?

   <details><summary>Answer</summary>

   GRO NIC/driver ko multiple small incoming packets ko ek bade "super-packet"
   mein merge karne deta before handing to the stack — fewer trips up the
   stack, better throughput. Par: packets NIC/driver pe **wait** karte merge
   hone ke liye → per-packet latency badhti aur jittery. HFT: `ethtool -K eth0
   gro off lro off` (`15`). Har market-data packet ko turant chahiye, merged
   nahi.
   </details>

5. Colocation ke bina RTT ~5 ms (across city), colocated ~10 µs. 500× farak.
   Yeh kis layer ka farak hai?

   <details><summary>Answer</summary>

   Physical distance / propagation delay + number of hops (routers, switches)
   at the Link/Network layers. Light in fiber ~5 µs/km, plus per-hop
   store-and-forward + queuing. Colocation puts you in the same building as
   the matching engine — one or two cut-through switches, meters of fiber.
   No amount of code tuning closes a 5 ms geographic gap; you move the box.
   </details>

---

## Interview questions

1. OSI vs TCP/IP — practical stack ke layers.
2. Encapsulation — ek order message pe kaunse headers, kis order mein?
3. Wire overhead — 40-byte payload actually kitne bytes on wire, kyun.
4. Network byte order — kya, `htons`/`htonl` kab.
5. Ek `send()` se wire tak ke ~6 stages.
6. Kernel path vs kernel bypass — round-trip overhead ka farak.
7. Colocation — kya latency problem solve karta, kaunsi layer.

---

## Next
→ [`02-ip-basics.md`](02-ip-basics.md)
