# 15 — tcpdump, wireshark, offline analysis

## Prerequisites
- `14-wire-to-wire-measurement.md`

## Yeh topic abhi kyun

01-14 ne LIVE measurement (timestamps, in-app) cover kiya. Yeh lesson
**post-hoc debugging** ka tool hai — jab kuch galat ho chuka (drops,
unexpected latency, malformed packets), tumhe EXACTLY kya guzra tha
poore packet-level detail mein dekhna hota.

---

## `tcpdump` — capture

```bash
# Multicast market-data traffic capture karo:
sudo tcpdump -i eth0 -w capture.pcap host 239.1.2.4 and port 9400

# Hardware timestamp samet capture (agar NIC/driver support kare):
sudo tcpdump -i eth0 -j adapter_unsynced -w capture.pcap
```

`-w capture.pcap`: **raw packets** file mein save hote (offline analysis
ke liye) — `-n` (no DNS resolution, capture ko slow NAHI karta) aur
`-s 0` (poora packet capture, truncate nahi) production capture mein
common flags hain.

---

## Offline analysis

```bash
tcpdump -r capture.pcap -tttt          # human-readable timestamps ke saath replay
tcpdump -r capture.pcap -c 100 -v      # pehle 100 packets, verbose
wireshark capture.pcap                  # GUI -- filter, follow-stream, statistics
```

**Wireshark** ka fayda: GUI-based filtering (`ip.addr==239.1.2.4 &&
udp.port==9400`), packet-by-packet inspection, **"Follow UDP Stream"**
(poori sequence dekhna), aur **I/O graphs** (throughput/loss visualize
karna time ke saath).

---

## Sequence-gap analysis offline

02's `seq != expected` detection **live** thi. Offline, tumhare paas
**poora capture** hai — tum EXACTLY dekh sakte kaunsa seq number kis
TIME pe missing tha, aur kya woh baad mein RETRANSMIT/RECOVER hua
(38/07's recovery mechanism):

```bash
# Wireshark filter: sirf market-data packets, seq field extract karo
# (custom dissector chahiye agar protocol proprietary hai, 38's ITCH-
#  style format jaisa)
tshark -r capture.pcap -Y "udp.port==9400" -T fields -e frame.time_relative -e data
```

---

## ⚠️ Ek BADA limitation — kernel bypass ke saath

**04's Onload trap yahan wapas aata**: agar traffic **kernel bypass**
(Onload/ef_vi/DPDK) se guzar raha hai, `tcpdump` (jo KERNEL-level
capture hai) usse **DEKH HI NAHI SAKTA** — packets kernel ko touch hi
nahi karte.

| Setup | `tcpdump` kaam karta? |
|---|---|
| Kernel sockets | Haan |
| `SO_BUSY_POLL` | Haan (kernel involved hai, bas differently poll hota) |
| Onload | **NAHI** (Onload-specific tools chahiye — `solar_capture`) |
| ef_vi / DPDK | **NAHI** (khud application/library ko apna capture-hook likhna padta, jaisa DPDK ka `rte_pdump`) |

**Isliye**: kernel-bypass systems mein DEBUGGING ke liye ek **DEDICATED,
SEPARATE capture mechanism** plan karna padta (jaisa NIC ka hardware
port-mirroring feature, "SPAN port," jo bypass se independent hai — NIC
khud packet ki copy ek ALAG monitoring port pe bhejta, jahan normal
tcpdump kaam kar sakta).

---

## Port mirroring (SPAN) — bypass-safe capture

```
Production NIC (bypass) --> [Switch SPAN port] --> Monitoring server (tcpdump yahan)
```

Switch **hardware-level** traffic COPY karta ek dedicated monitoring
port pe — production path (bypass) BILKUL untouched rehta, monitoring
completely SEPARATE hai. Yeh production HFT ka standard debugging setup
hai jab bypass in use ho.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — production bypass system pe `tcpdump` chala ke "kuch nahi mila"
conclude karna
Yeh capture-mechanism ki limitation hai, traffic-absence ka proof nahi.
04/15's SPAN-port pattern use karo.

### Trap 2 — capture file ko production pe LAMBI der chhod dena
`-w` file continuously badhti — disk-full ka risk, aur BADI files
analysis mein slow hoti. `-C` (rotate by size) aur `-W` (rotate count)
flags se bounded capture rakho.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| `tcpdump` HAMESHA sab traffic dikhata | Kernel-bypass traffic INVISIBLE hai isse |
| Wireshark sirf "chhote" captures ke liye hai | Production-scale bhi handle karta, bas GUI large files pe slow ho sakta (CLI `tshark` behtar) |
| Capture file size "matter nahi karti" | Disk-full risk -- rotation (`-C`/`-W`) zaroori production mein |

---

## Hands-on

```bash
# Linux/WSL pe, is repo ke 01_multicast_receiver ko chalate hue:
sudo tcpdump -i lo -w mcast.pcap host 239.1.2.4 and port 9400
# alag terminal mein: ./mcastrx
# phir: tcpdump -r mcast.pcap -tttt
```

---

## Exercises

1. Production system Onload use karta. Team `tcpdump` se ek issue debug
   karne ki koshish karti, kuch nahi dikhta. Sahi approach kya hai?
   <details><summary>Answer</summary>
   `tcpdump` yahan KAAM NAHI karega (Onload traffic bypass karta) --
   ya to Onload-specific tools (`solar_capture`) use karo, ya switch ka
   SPAN/port-mirroring feature use karke traffic ko ek SEPARATE
   monitoring port pe copy karo jahan normal tcpdump chal sake.
   </details>

---

## Interview questions

1. `tcpdump` kis LEVEL pe traffic capture karta, aur is wajah se kya
   MISS kar sakta?
2. Port mirroring (SPAN) kya hai, aur bypass systems ke liye kyun zaroori?
3. Wireshark vs `tshark` — kab kaunsa use karoge?

---

## Next
→ [`16-building-md-receiver.md`](16-building-md-receiver.md)
