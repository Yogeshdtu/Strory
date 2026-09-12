# 10 — IRQ pinning, avoiding interrupt jitter

## Prerequisites
- `09-nic-tuning.md`
- `41-HFT-CONCURRENCY/08-core-pinning-strategy.md` (affinity vs
  isolation ka distinction — yeh lesson USSE NIC IRQs pe apply karta)

## Yeh topic abhi kyun

09 ne NIC-level settings tune kiye. Yeh lesson ek layer upar jaata:
**NIC ka interrupt KIS CORE pe handle hota**, aur woh core tumhare
hot-path threads (41/08 se pinned) se KYUN alag hona CHAHIYE.

---

## Problem: IRQ aur hot-path thread SAME core share karein

```
Core 3: [matching-engine thread, PINNED, spinning]  <- latency-critical
Core 3: [NIC's RX interrupt ALSO lands here]         <- SAME core!

Jab NIC interrupt fire hota, OS matching-engine thread ko PREEMPT karta
(interrupt handling zaroori hai) -- matching-engine thread ab RUK jaata,
jitna der interrupt-handler chalta.
```

Yeh EXACTLY 41/08's "affinity ≠ isolation" finding ka EK SOURCE hai — ek
thread pinned hone ke bawajood, agar SAME core pe IRQs bhi aate rahein,
woh thread STILL preempted hota (bas kisi doosre USER thread se nahi,
KERNEL interrupt-handler se).

---

## Fix: RSS queues + per-queue IRQ pinning

Modern NICs **RSS** (Receive Side Scaling) support karte — multiple RX
queues, har ek ka apna IRQ, traffic HASH-based queues ke beech split
hota (jaisa 5-tuple hash — src/dst IP+port+protocol).

```bash
ethtool -l eth0                       # kitni queues available hain
cat /proc/interrupts | grep eth0      # har queue ka IRQ number
echo 4 > /proc/irq/<IRQ_NUM>/smp_affinity_list   # is IRQ ko core 4 pe PIN karo
```

**Strategy**: NIC IRQs ko **HOUSEKEEPING cores** pe pin karo (jahan
koi latency-critical thread nahi chal raha), **hot-path threads ko
ALAG, isolated cores** pe (41/08). Do groups KABHI overlap nahi karne
chahiye.

```
Core 0-1: housekeeping (OS, background) + NIC IRQs yahan pinned
Core 2:   ISOLATED -- matching engine thread (NO IRQs yahan)
Core 3:   ISOLATED -- feed receiver thread (NO IRQs yahan)
```

---

## RPS OFF for hot queues

**RPS** (Receive Packet Steering) ek SOFTWARE-level re-steering hai
(kernel packet ko EK core se DOOSRE core pe "migrate" karta, load-
balance ke liye) — yeh EXTRA processing hai, extra latency add karta.
Hot queues (jo already RSS se sahi core pe pahunch rahi) ke liye RPS
**OFF** rakho:

```bash
echo 0 > /sys/class/net/eth0/queues/rx-0/rps_cpus
```

---

## `/proc/interrupts` — verify karo

```bash
watch -n1 'grep eth0 /proc/interrupts'
```

Agar IRQ count kisi WRONG core pe badh raha hai (jahan tumne pin nahi
kiya), affinity setting effective NAHI hui — kuch drivers/systems mein
`irqbalance` daemon RUNNING hone se manual affinity OVERRIDE ho jaati
(isliye production HFT boxes pe `irqbalance` **disable** kiya jaata).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `irqbalance` daemon chalte rehna dena
`irqbalance` PERIODICALLY IRQ affinity ko "rebalance" karta (load ke
hisaab se) — yeh manual pinning ko OVERRIDE kar sakta bina warning ke.
Production HFT box pe: `systemctl disable irqbalance`.

### Trap 2 — RSS queue count ko cores se MISMATCH rakhna
Agar 8 RSS queues hain par sirf 4 dedicated housekeeping cores, kuch
queues ka IRQ HOT-PATH cores pe hi girega (koi aur jagah nahi). Queue-
count ko housekeeping-core-count ke hisaab se configure karo.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Thread pinning (41/08) akele kaafi hai | IRQs bhi SAME core pe aa sakte -- unhe bhi alag pin karo |
| RPS "extra parallelism" deta, hamesha achha | Extra software-level steering, latency-critical queues ke liye OFF rakho |
| `irqbalance` helpful hai (automatic) | Production HFT mein manual pinning ko override kar sakta -- disable karo |

---

## Hands-on

```bash
# 07_nic_tuning.sh ka step 5 isi ko cover karta:
sudo ./07_nic_tuning.sh eth0
```

---

## Exercises

1. Ek matching-engine thread `Core 2` pe pinned hai, aur usi core pe
   ABHI BHI occasional latency spikes dikh rahe. `irqbalance` chal raha
   hai. Kya check karoge?
   <details><summary>Answer</summary>
   `cat /proc/interrupts | grep eth0` -- dekho kya IRQs Core 2 pe aa
   rahe hain (irqbalance ne unhe wahan move kar diya ho sakta, chahe
   pehle manually alag pin kiya ho). Fix: `irqbalance` disable karo,
   phir IRQ affinity dobara SET karo.
   </details>

---

## Interview questions

1. NIC IRQ SAME core pe pinned thread ke saath hone se kya problem hoti?
2. RSS kya hai, aur per-queue IRQ pinning kaise kaam karta?
3. RPS kya hai, aur latency-critical queues ke liye OFF kyun rakhte?

---

## Next
→ [`11-tcp-for-order-gateways.md`](11-tcp-for-order-gateways.md)
