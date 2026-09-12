# 06 — Multicast: IGMP, groups, market data ka backbone

## Prerequisites
- `05-udp.md` (UDP, loss handling, A/B feeds)
- `02-ip-basics.md` (addressing, TTL, multi-NIC)

## Yeh topic abhi kyun
Exchange market data ka distribution model **IP multicast** hai: exchange ek
baar bhejta hai ek group address pe, aur network (switches) har subscriber ko
copy pahunchata hai. Ek feed handler likhne ke liye tumhe group **join** karna,
sahi **interface** pe, aur A/B feeds arbitrate karna aana chahiye. Example `06`
ek self-contained join+receive demo hai.

---

## Multicast address space

`224.0.0.0` – `239.255.255.255` (`224.0.0.0/4`, "class D"). A packet to such an
address is delivered to **every host that has joined that group**.

| Range | Use |
|---|---|
| `224.0.0.0/24` | link-local control (never routed) — e.g. `224.0.0.1` all-hosts, `224.0.0.22` IGMPv3 |
| `232.0.0.0/8` | SSM (Source-Specific Multicast) |
| `239.0.0.0/8` | **administratively scoped** (private, like RFC1918) — most in-house / exchange feeds live here |

Each multicast IP maps to a MAC address `01:00:5e:xx:xx:xx` (low 23 bits of the
IP). The NIC filters incoming frames by this multicast MAC (or goes promiscuous
/ all-multi if overloaded).

---

## Joining a group — `IP_ADD_MEMBERSHIP`

```cpp
int fd = socket(AF_INET, SOCK_DGRAM, 0);
int one = 1;
setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);   // many receivers, same port
setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &one, sizeof one);   // Linux: many processes too

sockaddr_in a{};
a.sin_family = AF_INET;
a.sin_port   = htons(GROUP_PORT);
a.sin_addr.s_addr = htonl(INADDR_ANY);      // (some setups bind to the group addr)
bind(fd, (sockaddr*)&a, sizeof a);

ip_mreqn mreq{};
inet_pton(AF_INET, "239.1.2.3", &mreq.imr_multiaddr);
mreq.imr_address.s_addr = htonl(INADDR_ANY);
mreq.imr_ifindex = if_nametoindex("eth0");   // WHICH interface joins -- crucial on multi-NIC
setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof mreq);
```

`IP_ADD_MEMBERSHIP` does two things:
1. Tells the **kernel** "deliver `239.1.2.3:PORT` datagrams to this socket".
2. Sends an **IGMP membership report** on `imr_ifindex` so the upstream
   switch/router starts forwarding that group's traffic to this port.

**Leave:** `IP_DROP_MEMBERSHIP`. Also happens automatically on `close()`.

### SSM (Source-Specific) — `IP_ADD_SOURCE_MEMBERSHIP` / `MCAST_JOIN_SOURCE_GROUP`
Join `(group, source_IP)` — only accept multicast from that specific sender.
Exchanges increasingly use SSM (`232/8`): no rogue sender can inject into your
feed, and switch state is simpler. Prefer it when the venue supports it.

---

## IGMP — how the network knows to forward

- **IGMPv2/v3:** hosts send **membership reports** to say "I want group X";
  routers periodically send **queries**; a host stops reporting → after a
  timeout the router prunes that group from that port.
- **IGMP snooping** (switch feature): the switch listens to IGMP and only floods
  a group's frames to ports that reported interest — instead of flooding all
  ports. **If IGMP snooping is misconfigured, you get the classic bug:** either
  no traffic (snooping on, your report not seen) or the whole feed flooding
  every port (snooping querier missing → switch floods).
- **IGMPv3** needed for SSM.

```bash
ip maddr show           # per-interface group memberships (link + IP)
netstat -g              # same, groups per interface
cat /proc/net/igmp      # kernel IGMP state
```

---

## The interface matters (multi-NIC HFT box)

On a box with `eth0` (market data) and `eth1` (order entry):
- `imr_ifindex` **must** be `eth0` — join on the wrong interface and no IGMP
  report goes out the NIC that's actually connected to the feed → nothing
  arrives.
- Set `IP_MULTICAST_IF` on the **sending** side to pick the egress interface.
- `rp_filter` on `eth0` should be loose (`2`) or off — strict RPF drops
  multicast whose reply path would be `eth1` (`02`).
- Routing: a route for the multicast range out `eth0` (`ip route add
  239.0.0.0/8 dev eth0`) so the kernel picks the right interface.

---

## Sending multicast

```cpp
int s = socket(AF_INET, SOCK_DGRAM, 0);
ip_mreqn txif{}; txif.imr_ifindex = if_nametoindex("eth0");
setsockopt(s, IPPROTO_IP, IP_MULTICAST_IF, &txif, sizeof txif);   // egress interface
int ttl = 1;   setsockopt(s, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof ttl);   // 1 = stay on subnet
int loop = 0;  setsockopt(s, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof loop); // don't echo to self
sockaddr_in to{}; to.sin_family = AF_INET; to.sin_port = htons(PORT);
inet_pton(AF_INET, "239.1.2.3", &to.sin_addr);
sendto(s, buf, n, 0, (sockaddr*)&to, sizeof to);
```

- **TTL:** each router hop decrements it; `1` = never leaves the local segment.
  Exchange feeds use small, deliberate TTLs to scope distribution.
- **LOOP:** whether the sending host's own joined sockets see the packet.
  Default on; turn off for a pure sender.

---

## A/B feeds — the HFT recovery model (recap `05`)

The exchange publishes the **same sequence-numbered stream** on **two multicast
groups** (`239.1.2.3` "A" and `239.1.2.4` "B") over **physically separate
network paths** (different switches, often different NICs on your box).

Your feed handler:
- Joins both A and B.
- Keeps one `expected_seq`.
- For each incoming packet (from either group): if `seq == expected`, deliver
  and advance; if `seq > expected`, gap; if `seq <= last`, it's the duplicate
  from the other feed — drop.
- A packet lost on A almost always arrives on B (independent paths) → gap filled
  with **zero round-trips**.

Only when *both* feeds miss a sequence do you fall back to the snapshot /
retransmit channel (`05`).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — joining on the wrong interface
`imr_ifindex = 0` (kernel picks) on a multi-NIC box → IGMP report may go out the
wrong NIC → switch never forwards the feed to you. Always specify the
market-data interface explicitly.

### Trap 2 — IGMP snooping misconfig
No querier on the VLAN → switch either floods everything or (with snooping)
forwards nothing. Symptom: `tcpdump -i eth0` shows the group... or shows
nothing. Work with the network team; verify with `ip maddr` + switch counters.

### Trap 3 — `SO_REUSEADDR`/`SO_REUSEPORT` missing
Second process/socket trying to bind the same group:port → `EADDRINUSE`.
Multiple strategies each want their own receive socket for the same feed → both
options needed.

### Trap 4 — `rp_filter` dropping multicast
Strict reverse-path filter on the RX interface drops multicast whose source
isn't routable back out that same interface (`02`). Set `rp_filter=2` on `eth0`.

### Trap 5 — no A/B arbitration
Single feed → every gap is a snapshot/retransmit round-trip (slow, and a burst
of gaps can cascade). Join both feeds and arbitrate by sequence.

### Trap 6 — `IP_MULTICAST_LOOP` confusion in tests
Local sender + local receiver in one box: if `LOOP` is off, the receiver sees
nothing and you think join failed. For self-contained tests (example `06`) leave
`LOOP` on.

### Trap 7 — not renewing membership / relying on it forever
IGMP membership is soft state (queries + reports). Normally the kernel handles
renewal, but a flapping link or a switch reboot can drop you silently — monitor
that packets are still arriving (a "no data for N ms" watchdog on the feed).

---

## > **HFT relevance**

> - **Market data feed handler = join A + B multicast groups on the market-data
>   NIC, arbitrate by sequence number, recover via snapshot/retransmit only when
>   both miss.**
> - **Explicit interface** (`imr_ifindex`) on every join; explicit
>   `IP_MULTICAST_IF` on every send. Never let the kernel guess on a multi-NIC
>   box.
> - **Prefer SSM** (`232/8`, `MCAST_JOIN_SOURCE_GROUP`) where the venue offers
>   it — source filtering in the network, no rogue injection.
> - **`SO_REUSEPORT`** so each strategy process can have its own receive socket
>   on the same feed, and the kernel spreads load.
> - **Feed watchdog:** alert if no packet on A *or* B for N ms — a silently
>   pruned membership or a dead path is an outage.
> - **Kernel drops still apply** — big `SO_RCVBUF`, big NIC rings, `recvmmsg`,
>   pinned isolated receiver, IRQ affinity (`05`, `29/15`). Then kernel bypass
>   (`13`).

---

## Hands-on

```bash
# Linux/WSL pe -- example 06 (self-contained sender+receiver, if=lo)
g++ -std=c++20 -O2 -pthread 30-NETWORKING/examples/06_multicast_receiver.linux.cpp -o /tmp/mc
/tmp/mc 239.1.2.3 9300 lo

# real interface + external sender:
#   receiver:  /tmp/mc 239.1.2.3 9300 eth0
#   sender  :  socat -u - UDP4-DATAGRAM:239.1.2.3:9300,ip-multicast-if=eth0

ip maddr show                 # joined groups per interface
netstat -g
cat /proc/net/igmp
sudo tcpdump -i eth0 -n 'multicast and udp port 9300'
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "join = kernel filter only" | also sends an IGMP report so the switch forwards |
| "interface auto-detect ho jayega" | multi-NIC: must set `imr_ifindex` / `IP_MULTICAST_IF` |
| "snooping = fewer packets, always good" | misconfigured = no traffic or full flood |
| "ek feed kaafi" | A/B redundant groups fill most gaps with no round-trip |
| "membership permanent hai" | soft state; link flap / switch reboot can drop it silently |
| "`SO_REUSEADDR` kaafi for multiple receivers" | also `SO_REUSEPORT` on Linux for multi-process |

---

## Exercises

1. On a 2-NIC box (`eth0` = market data, `eth1` = orders), your join "succeeds"
   but no packets arrive. `tcpdump -i eth0` also shows nothing. First checks?

   <details><summary>Answer</summary>

   (1) `imr_ifindex` — did you join on `eth0` or let the kernel pick (0)? If the
   IGMP report went out `eth1`, the switch port for `eth0` never gets the group.
   Set `mreq.imr_ifindex = if_nametoindex("eth0")`. (2) Is there a route sending
   `239.0.0.0/8` out `eth0`? (3) `ip maddr show eth0` — is the group listed? (4)
   Switch side: IGMP snooping / querier on that VLAN. (5) `rp_filter` on `eth0`
   (strict → drops). Since `tcpdump` shows nothing, it's upstream of the socket
   — interface/IGMP/switch, not your app logic.
   </details>

2. Why do exchanges send the *same* stream on two multicast groups over separate
   paths instead of one group with retransmission?

   <details><summary>Answer</summary>

   Independent paths fail independently — a packet dropped on path A (a
   congested switch, a bad optic) almost always still arrives on path B. The
   receiver arbitrates by sequence number and fills the gap with **zero
   round-trips and zero added latency**. A retransmission request is a
   round-trip to a (possibly busy) retransmit server, during which you're
   behind; and a burst of losses can overwhelm it. A/B is cheap insurance;
   retransmit/snapshot is the last resort for when both paths miss.
   </details>

3. `IP_MULTICAST_LOOP` — what does it control, and why does it trip people up in
   local tests?

   <details><summary>Answer</summary>

   It controls whether datagrams a host *sends* to a group are also delivered to
   *that same host's* sockets joined to the group. Default: on. In a
   self-contained test (sender and receiver in one process/box, like example
   `06`) you need it **on** or the receiver sees nothing and you conclude the
   join failed. In production, a pure sender turns it **off** (no point echoing
   to itself), and a pure receiver box isn't sending so it's moot.
   </details>

4. Your feed handler joined group A and B. A packet with `seq=5000` arrives on
   A, then `seq=5000` arrives on B a few µs later. What should happen, and what
   data structure do you need?

   <details><summary>Answer</summary>

   The B copy is a **duplicate** — drop it. You need `last_delivered_seq` (or a
   small window/bitmap of recently seen sequences for out-of-order tolerance).
   Algorithm: on packet `seq`: if `seq == expected` deliver + advance; if `seq >
   expected` it's ahead (gap — deliver, note holes, keep going); if `seq <=
   last_delivered` it's a dup/late — drop. The A/B arbitration is just "feed
   both streams into the same dedup/sequencing logic and take whichever arrives
   first."
   </details>

5. SSM (source-specific multicast) vs ASM (any-source) — one security and one
   operational advantage of SSM.

   <details><summary>Answer</summary>

   Security: with SSM you join `(group, source_IP)`, so the network only
   delivers traffic from the legitimate exchange sender — a rogue host on the
   segment can't inject packets into your feed by sending to the group.
   Operational: SSM (IGMPv3/`232/8`) has simpler network state — no shared
   distribution tree / RP (rendezvous point) machinery, less to misconfigure,
   faster join/leave. Downside: needs IGMPv3 end to end and the venue must
   publish source addresses.
   </details>

---

## Interview questions

1. Multicast address range; `239/8` vs `232/8` (SSM).
2. `IP_ADD_MEMBERSHIP` — the two things it does (kernel filter + IGMP report).
3. IGMP snooping — what it does, the two misconfig failure modes.
4. Why `imr_ifindex` matters on a multi-NIC box.
5. A/B feed arbitration — how it recovers loss without a round-trip.
6. `IP_MULTICAST_TTL` and `IP_MULTICAST_LOOP` — what each controls.
7. SSM vs ASM — advantages of source-specific.

---

## Next
→ [`07-sockets-api.md`](07-sockets-api.md)
