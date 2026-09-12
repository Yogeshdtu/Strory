# 09 — Ring buffers, coalescing, offloads (aur kaunse disable karein)

## Prerequisites
- `08-hardware-timestamping.md`
- `29-LINUX-SYSTEMS` (production tuning checklist ka context)

## Yeh topic abhi kyun

01-08 ne "bypass karo" ka option dikhaya. Yeh lesson (aur 10) us case ke
liye hai jab tum **KERNEL sockets HI use kar rahe ho** (bypass nahi kar
rahe, ya ABHI TAK nahi) — kernel-level NIC settings TUNE karke bhi
significant latency/reliability improvement milta. `07_nic_tuning.sh`
poora checklist hai.

---

## Ring buffers — kitna BADA rakhna

```bash
ethtool -G eth0 rx 4096 tx 4096
```

NIC ka RX ring ek **circular buffer** hai jahan incoming packets DMA
hote, driver/kernel unhe process karne se pehle. **Chhota ring**: burst
traffic mein packets DROP hote (ring full, naye packets ke liye jagah
nahi). **Bada ring**: drops kam, PAR queueing delay badh sakta (packet
ring mein baitha reh sakta processing ka wait karte, 35/05's principle
— throughput ke liye bada, latency ke liye chhota, trade-off).

**HFT approach**: **latency-focused** rehte hue bhi ring ko itna bada
rakho ki NORMAL burst absorb ho jaaye (jaisa market open ka spike) —
`ethtool -S` ke drop counters se VERIFY karo, guess mat karo.

---

## Interrupt coalescing — MIN/OFF

```bash
ethtool -C eth0 rx-usecs 0 rx-frames 1
```

Normal NIC drivers **coalesce** karte — multiple packets ka interrupt
EK saath fire karte (throughput ke liye achha, CPU-interrupt-overhead
kam karta), PAR ISKA MATLAB HAI: pehla packet interrupt ka WAIT karta
jab tak coalescing window (`rx-usecs`) khatam na ho ya `rx-frames`
count na pahunche. HFT ko **turant** har packet chahiye — coalescing
OFF (`rx-usecs 0 rx-frames 1`) karna latency ke liye zaroori hai,
**CPU-interrupt-rate** ke against trade-off (zyaada interrupts = zyaada
CPU cycles interrupt-handling mein).

---

## Offloads — GRO/LRO OFF

**GRO** (Generic Receive Offload) / **LRO** (Large Receive Offload):
kernel/NIC MULTIPLE chhoti packets ko EK BADI "merged" packet mein
combine karta application ko dene se pehle (throughput-optimization,
kam per-packet overhead). HFT ko HAR packet **turant, ALAG-ALAG** chahiye
— GRO/LRO ON hone se pehla packet TAB TAK app ko nahi milta jab tak
merge-window khatam na ho (coalescing jaisa hi PRINCIPLE, higher layer
pe).

```bash
ethtool -K eth0 gro off lro off
```

**TSO/GSO** (TX-side offloads) alag hain — woh OUTGOING bade writes ko
NIC khud chhote packets mein todta; chhoti market-data-style messages
pe unka asar kam hota (already chhoti hain) — inhe explicit EVALUATE
karo, blanket-disable mat karo (unlike GRO/LRO jo RX-side latency ko
directly hit karte).

---

## Pause frames OFF

Ethernet **pause frames** ek congestion-control mechanism hain — agar
receiver ka buffer bharne wala ho, woh sender ko "ROKO" bolta (link-level).
Yeh POORI NIC ko STALL kar sakta ek burst ki wajah se (fairness ke liye
achha, LATENCY ke liye bura — ek slow consumer poori link ko block kar
sakta).

```bash
ethtool -A eth0 rx off tx off
```

---

## Verify BY DROP COUNTERS — feel se nahi

```bash
ethtool -S eth0 | grep -iE 'drop|discard|miss|error'
cat /proc/net/softnet_stat   # column 2 = dropped
```

**35/05's principle yahan bhi**: "lag raha hai" feeling PE tuning mat
karo — counters se PROVE karo drops ho rahe hain (aur kahan), phir
specific fix apply karo, phir counters se RE-VERIFY karo.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — GRO/LRO ko blanket-disable karna, TSO/GSO samet
Different offloads ka RX vs TX asar alag hai — RX-side (GRO/LRO) HFT
ke against, TX-side (TSO/GSO) usually neutral/beneficial hai chhoti
messages ke liye. Sab kuch disable karna zaroori nahi, KUCH counter-
productive bhi ho sakta.

### Trap 2 — production interface pe test kiye bina apply karna
`07_nic_tuning.sh`'s warning: yeh settings NIC ko REBOOT tak (ya driver
reload tak) badalte. STAGING pe pehle verify karo.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Bada ring buffer hamesha behtar hai | Trade-off (drops vs queueing delay) |
| GRO/LRO/TSO/GSO sab "offload" hain, sab off karo | RX-side (GRO/LRO) latency-harmful, TX-side (TSO/GSO) usually neutral |
| Tuning "feel" se verify hoti (lag raha hai/nahi) | Drop-counters se PROVE karo |

---

## Hands-on

```bash
# Real Linux box (root/NET_ADMIN) pe, staging interface pe:
sudo ./07_nic_tuning.sh eth0
```

---

## Exercises

1. Ek team RX ring 8192 se 256 kar deti "latency kam karne" ke liye.
   Kya ho sakta?
   <details><summary>Answer</summary>
   Chhota ring burst traffic ko absorb NAHI kar payega -- market-open
   jaisa spike pe packets DROP honge (ring full). Yeh "latency kam"
   nahi karta, balki RELIABILITY kam karta -- drop-counters se yeh
   turant pakda jaana chahiye.
   </details>

---

## Interview questions

1. Interrupt coalescing kya hai, aur HFT ke liye kyun disable karte?
2. GRO/LRO RX latency ko kaise affect karte?
3. Tuning ko drop-counters se verify karna kyun zaroori hai?

---

## Next
→ [`10-irq-affinity-tuning.md`](10-irq-affinity-tuning.md)
