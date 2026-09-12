# 02 — IP basics: IPv4/IPv6, routing, MTU, fragmentation

## Prerequisites
- `01-network-models.md` (encapsulation, layers)

## Yeh topic abhi kyun
IP transport (TCP/UDP) ke neeche ki layer hai — addressing aur "yeh packet
kahan bhejun". HFT mein tum IP ko zyada nahi chhedte (setup ke baad routing
static hoti), par **MTU / fragmentation** aur **routing table** samajhna zaroori
hai: ek galat MTU ya ek slow route tumhare P99 mein dikhega, aur multicast
(`06`) IP-level cheez hai.

---

## IPv4 address + subnet

`10.20.30.40/24`:
- 32-bit address, 4 octets. `/24` = pehle 24 bits **network**, last 8 **host**.
- Subnet mask `255.255.255.0`. Network `10.20.30.0`, broadcast `10.20.30.255`,
  usable hosts `.1`–`.254`.
- Same subnet ke hosts **direct** (Ethernet, ARP se MAC resolve). Alag subnet →
  **gateway** (router) ke through.

Special ranges: `127.0.0.0/8` loopback, `10/8` `172.16/12` `192.168/16` private
(RFC1918), `224.0.0.0/4` **multicast** (`06`), `169.254/16` link-local.

## IPv6 (briefly)
128-bit, `2001:db8::1`. HFT internally aksar abhi IPv4 (simplicity, tooling),
par exchanges/venues IPv6 support karte. Concepts same; no broadcast (multicast +
anycast), no fragmentation by routers (sender's job), bigger headers (40 B fixed
vs 20 B).

---

## Routing — "yeh packet kis interface se, kis next-hop ko"

Har packet ke liye kernel **routing table** dekhta (longest-prefix match):

```bash
ip route
#   default via 10.20.30.1 dev eth0                 <- 0.0.0.0/0, gateway
#   10.20.30.0/24 dev eth0 proto kernel scope link  <- local subnet, direct
#   10.99.0.0/16 via 10.20.30.2 dev eth0            <- specific route to venue net
ip route get 203.0.113.5      # is destination ke liye kaunsa route chunega
```

- **Longest prefix wins:** `10.99.5.5` matches `10.99.0.0/16` (more specific)
  over `default`.
- **Direct (scope link):** destination same subnet → ARP for its MAC → send.
- **Via gateway:** destination elsewhere → send to gateway's MAC, gateway
  forwards.
- HFT: routes are **static and minimal** — one route to the market-data network,
  one to the order-entry network, maybe a management default. No dynamic routing
  protocols (OSPF/BGP) on trading hosts.

### Multi-homed HFT box
Aksar 2+ NICs: `eth0` = market data (multicast in), `eth1` = order entry (TCP to
gateway), `eth2` = management. Source-based routing / policy routing (`ip rule`)
ensures order packets leave the right NIC with the right source IP.

---

## MTU — maximum transmission unit

MTU = largest IP packet (payload after Ethernet header) an interface will send.
Standard Ethernet: **1500 bytes**. Jumbo frames: up to ~9000.

```bash
ip link show eth0 | grep mtu           # mtu 1500
```

- TCP negotiates **MSS** (Max Segment Size) = MTU − IP hdr − TCP hdr = 1500 − 20
  − 20 = **1460** typically. Har TCP segment ≤ MSS.
- UDP: agar tumhara datagram + headers > MTU → **fragmentation** (neeche).

### Path MTU Discovery (PMTUD)
Sender MTU-sized packet bhejta `DF` (Don't Fragment) bit set. Beech ka router
jiska link chhota → packet drop + ICMP "Fragmentation Needed" (type 3 code 4)
bhejta → sender chhota MSS use karta. Agar firewall ICMP block kare → **PMTUD
black hole**: connection hang (bade packets silently drop, koi feedback nahi).
Classic "chhota data theek, bada data hang" bug.

---

## Fragmentation — bada packet, chhota link

IP packet > next-hop MTU:
- **IPv4:** router (ya sender) packet ko fragments mein tod deta; receiver ki
  IP layer reassemble karti (needs ALL fragments; ek bhi khoya → poora packet
  gaya, timeout ~30 s).
- **IPv6:** routers fragment nahi karte — sender ko PMTUD karke chhota bhejna
  padta, warna drop.

**Fragmentation is bad for HFT:**
- Ek fragment khoya → poora datagram lost (UDP pe koi retransmit nahi → market
  data gap).
- Reassembly buffer + timer = latency + memory + a DoS surface.
- Out-of-order fragments, reassembly at line rate = CPU.

**Rule:** UDP market-data datagrams ko MTU ke andar rakho (payload ≤ ~1472 for
1500 MTU, minus your protocol headers). Exchange feeds isko design karte hain —
har ITCH/multicast packet MTU-safe. Apne internal UDP protocols bhi.

---

## ARP — IP se MAC

Same-subnet send ke liye kernel ko destination ka **MAC address** chahiye:
```bash
ip neigh          # ARP cache: IP -> MAC, state (REACHABLE/STALE/...)
```
- Cache miss → ARP request (broadcast) → reply → cache (~minutes).
- **First-packet latency spike:** ARP cache STALE/expired → agla packet ARP
  resolve ka wait (~ms). HFT: **static ARP entries** (`ip neigh add ... nud
  permanent`) for the gateway and known peers, so it never resolves during
  trading.

---

## Internal working / gotchas

- Routing decision **per packet** (route cache mostly removed in modern kernels;
  FIB lookup is fast, ~tens of ns, but non-zero).
- `rp_filter` (reverse-path filter, `net.ipv4.conf.*.rp_filter`): kernel drops
  packets arriving on an interface it wouldn't route the reply out of. Multi-NIC
  HFT boxes: set to `2` (loose) or `0`, else legitimate multicast/asymmetric
  traffic gets dropped silently.
- `net.ipv4.ip_forward` = 0 on trading hosts (not a router).
- TTL (IPv4) / Hop Limit (IPv6): decremented per hop; 0 → drop + ICMP. Multicast
  TTL 1 = "don't leave this subnet" (`06`).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — PMTUD black hole
Firewall drops ICMP → large packets vanish, small ones work → connection
"randomly" hangs on big transfers. Fix: allow ICMP type 3, or clamp MSS
(`iptables ... TCPMSS --clamp-mss-to-pmtu`), or lower MTU.

### Trap 2 — UDP datagram > MTU → fragmentation
One lost fragment = whole datagram lost (no UDP retransmit). Keep UDP payload +
headers ≤ MTU. Check exchange feed spec; design internal protocols the same.

### Trap 3 — stale ARP → first-trade latency spike
Gateway's ARP entry expires overnight; market open → first order waits for ARP.
Static `ip neigh` entries for gateway + peers.

### Trap 4 — `rp_filter` dropping multicast on a multi-NIC box
Market data on `eth0`, replies would route out `eth1` → strict `rp_filter`
drops it. Set `rp_filter=2` (loose) on the receiving interface.

### Trap 5 — wrong source IP on a multi-homed box
Order leaves via `eth1` but with `eth0`'s source IP → exchange rejects / ACLs
drop. Use `ip rule` / `SO_BINDTODEVICE` / bind the socket to the right local IP.

### Trap 6 — assuming loopback has MTU 1500
`lo` MTU is usually **65536**. Local UDP tests won't fragment even at 60 KB —
then the real NIC does. Test with realistic sizes and the real interface.

---

## > **HFT relevance**

> - **Static everything.** Static routes (2–3), static ARP for gateway + peers,
>   no routing daemons. First packet after market open must not wait on ARP or
>   route resolution.
> - **MTU-safe UDP.** Every market-data and internal UDP datagram fits in one
>   frame — fragmentation on a UDP feed means silent, unrecoverable gaps.
> - **Multi-NIC layout:** market-data NIC (multicast RX), order-entry NIC (TCP
>   to gateway), management NIC. `rp_filter=2`, correct source IP per path,
>   `SO_BINDTODEVICE` where needed.
> - **Jumbo frames** sometimes for internal bulk (snapshot recovery, historical
>   replay) — never assume the exchange link supports them.
> - **PMTUD black holes** are a real outage cause — monitor for it (large
>   messages timing out while small ones succeed).

---

## Hands-on

```bash
ip route ; ip route get 8.8.8.8
ip -s link show eth0            # MTU, errors, drops
ip neigh                       # ARP cache
tracepath 8.8.8.8              # per-hop MTU + latency
ping -M do -s 1472 <peer>      # DF bit, 1472+28=1500 -> should pass; -s 1473 -> "message too long" or drop
cat /proc/sys/net/ipv4/conf/all/rp_filter
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "IP reliable delivery deta" | IP best-effort; reliability TCP mein (ya app) |
| "fragmentation transparent hai" | one lost fragment = whole packet lost; latency + DoS surface |
| "PMTUD hamesha kaam karta" | ICMP blocked → black hole → hangs on large packets |
| "ARP ek baar, phir free" | cache expires; static entries for HFT peers |
| "loopback MTU 1500" | usually 65536; local tests don't fragment |
| "ek NIC kaafi hai" | HFT: separate market-data / order-entry / mgmt NICs |

---

## Exercises

1. Internal UDP protocol: 12-byte header + variable body. 1500 MTU. Max body
   size to avoid fragmentation?

   <details><summary>Answer</summary>

   MTU 1500 − IP header 20 − UDP header 8 = **1472** bytes of UDP payload. Minus
   your 12-byte app header = **1460 bytes** max body. Go over and the IP layer
   fragments; lose one fragment and the whole datagram is gone (no UDP
   retransmit). Many designs cap well under (e.g. 1400) for headroom (VLAN tags,
   tunnels).
   </details>

2. "Small orders go through, large snapshot requests hang." Diagnosis?

   <details><summary>Answer</summary>

   PMTUD black hole. A router on the path has a smaller MTU; it drops the large
   (DF-set) packets and sends ICMP "Fragmentation Needed", but a firewall
   blocks that ICMP → the sender never learns → large packets silently vanish,
   small ones (under the real path MTU) work. Fix: allow ICMP type 3 code 4, or
   MSS-clamp (`iptables -t mangle ... TCPMSS --clamp-mss-to-pmtu`), or manually
   lower the interface MTU.
   </details>

3. Multi-NIC box: market data arrives on `eth0`, nothing gets through to the
   app. `tcpdump -i eth0` shows the multicast packets arriving. Kya check?

   <details><summary>Answer</summary>

   (1) `IP_ADD_MEMBERSHIP` actually done on the right interface (`06`)? (2)
   `rp_filter` — `net.ipv4.conf.eth0.rp_filter` strict (1)? The kernel sees
   multicast on `eth0` but its route back to the source would go out `eth1` →
   strict RPF drops it before the socket. Set `rp_filter=2` (loose) or `0` on
   `eth0`. (3) Firewall (`iptables -L`) dropping the multicast group / port.
   (4) IGMP snooping on the switch not forwarding (`06`).
   </details>

4. HFT box, first order every morning is ~2 ms slower than the rest. Network
   cause?

   <details><summary>Answer</summary>

   Stale ARP for the gateway — the entry went STALE/expired overnight, so the
   first order-entry packet triggers an ARP request/reply round-trip (~ms on a
   quiet LAN, plus the packet is queued meanwhile). Fix: `ip neigh replace
   <gw-ip> lladdr <gw-mac> dev eth1 nud permanent` at startup, so it never
   resolves during trading. (Also warm the path with a dummy packet before
   market open.)
   </details>

5. IPv4 vs IPv6 fragmentation — kaun kya karta, HFT ke liye kya matlab.

   <details><summary>Answer</summary>

   IPv4: any router (or the sender) can fragment a too-big packet; receiver
   reassembles. IPv6: routers **never** fragment — if a packet is too big for a
   link, the router drops it and sends ICMPv6 "Packet Too Big"; the sender must
   do PMTUD and send smaller. For HFT the practical rule is identical either
   way: keep every UDP datagram inside the path MTU so fragmentation never
   happens — on IPv6 an oversize datagram is just dropped, on IPv4 it's a
   fragile reassembly you don't want.
   </details>

---

## Interview questions

1. Subnet mask / prefix — same-subnet vs via-gateway decision.
2. Routing table longest-prefix match — example.
3. MTU vs MSS — relationship, typical values.
4. Fragmentation — IPv4 vs IPv6, why it's bad for a UDP feed.
5. PMTUD black hole — cause and symptom.
6. ARP — when it resolves, why static entries for HFT.
7. `rp_filter` on a multi-NIC box — what it does, what to set.

---

## Next
→ [`03-tcp-deep.md`](03-tcp-deep.md)
