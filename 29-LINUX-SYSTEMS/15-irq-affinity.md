# 15 — IRQ affinity: interrupts, softirqs, NIC IRQs

## Prerequisites
- `11-cpu-affinity.md` (isolation, housekeeping cores)
- `14-numa.md` (NIC NUMA node)
- `10-cpu-scheduling.md` (preemption sources)

## Yeh topic abhi kyun
Tumne hot core isolate kar diya (`11`), scheduler ko door rakha (`10`), page
faults khatam kiye (`13`) — par ek aur cheez tumhare hot thread ko interrupt kar
sakti hai: **hardware interrupts (IRQs)**. NIC har batch of packets pe interrupt
maarta, aur agar woh interrupt tumhare hot core pe deliver hua → tumhara thread
ruka, IRQ handler chala, softirq chala. HFT: saare IRQs ko **housekeeping cores**
pe pin karo, hot cores ko IRQ-free rakho.

---

## Interrupt → hard IRQ → softirq → (thread)

Jab NIC ke paas packets aate:

```
  NIC ---(MSI-X interrupt)---> CPU core X
       CPU running thread ko ROKO, save state
       -> hard IRQ handler (driver): minimal -- "packets aaye", schedule softirq
       -> return, thread resume  (hard IRQ: ~1-5 us stall on core X)
  ...phir, jald hi, core X pe (ya ksoftirqd/X):
       -> softirq NET_RX: packets ko protocol stack se guzaaro (IP, TCP/UDP),
          socket buffers mein daalo, waiting app ko wake  (~10s of us of work)
```

Dono steps (hard IRQ + softirq) **jis core pe interrupt aaya usi pe** chalte
hain by default. Agar woh core tumhara hot trading core hai → double jitter:
handler ka time + cache pollution + possible thread preemption.

**RPS/RFS** (Receive Packet Steering / Flow Steering) softirq processing ko
dusre cores pe distribute kar sakte (software), par best: hardware level pe IRQ
ko sahi core pe deliver karao.

---

## IRQ affinity — `/proc/irq/N/smp_affinity`

```bash
# kaunsa IRQ kaunsa device, kaunse core pe kitni baar
cat /proc/interrupts        # columns = CPUs; rows = IRQs; NIC rows dhoondho (eth0-TxRx-0 ...)

# IRQ 129 ko sirf CPU 0 aur 1 pe deliver karo (bitmask: 0b0011 = 3)
echo 3 > /proc/irq/129/smp_affinity
# ya list form:
echo 0-1 > /proc/irq/129/smp_affinity_list

# saari NIC queue IRQs ek script se housekeeping cores pe:
for irq in $(grep 'eth0' /proc/interrupts | awk '{print $1}' | tr -d ':'); do
    echo 0-1 > /proc/irq/$irq/smp_affinity_list
done
```

- Multi-queue NIC: har RX/TX queue ka apna IRQ (`eth0-TxRx-0`..`-N`). Sabko
  housekeeping cores pe spread karo (ya ek dedicated "network" core).
- **`irqbalance` daemon disable karo** — warna woh periodically tumhari affinity
  ko override karega: `systemctl disable --now irqbalance`.
- `irqaffinity=0-1` boot cmdline se default affinity (naye IRQs bhi) set hoti.

---

## Softirq control

Hard IRQ sahi core pe aaya, par softirq processing:

- Default: softirq usi core pe jahan hard IRQ aaya → agar IRQ housekeeping core
  pe, softirq bhi wahin. Good.
- Overload pe kernel `ksoftirqd/N` (per-core kernel thread) pe defer karta — woh
  bhi us core pe. `ksoftirqd` ko `rcu_nocbs`/isolation se hot core se door
  rakho (`11`).
- `/proc/softirqs` — per-core softirq counts (NET_RX, NET_TX, TIMER, SCHED,
  RCU...). Hot core ki NET_RX/TIMER rows ~flat honi chahiye.

---

## The other interrupt sources on a hot core

| Source | Kaise band/kam karein |
|---|---|
| **Timer tick** (`LOC` in `/proc/interrupts`) | `nohz_full=<core>` — 1 runnable task pe tick off (`11`) |
| **Scheduler IPI** (`RES`) | isolation + pinning (koi migration/wakeup-balance nahi) |
| **TLB shootdown IPI** (`TLB`) | trading hours mein `mmap`/`munmap`/`mprotect` mat karo (`07`) |
| **Function-call IPI** (`CAL`) | kernel cross-core work — mostly from the above |
| **NMI / perf** | `perf` ko hot core pe run mat karo; watchdog `nmi_watchdog=0` |
| **Machine check (`MCE`)** | rare; hardware |
| **RCU callbacks** | `rcu_nocbs=<core>` (`11`) |

`watch -n1 'cat /proc/interrupts'` — do snapshots ka diff. Hot core ka column
ideally sirf occasional `LOC` (agar nohz nahi) aur kuch nahi.

---

## Internal working

- Modern NICs **MSI-X** use karte — har queue ka apna interrupt vector, jise
  kernel ek specific core ke local APIC pe route karta (`smp_affinity`).
- **Interrupt coalescing** (`ethtool -C eth0 rx-usves N rx-frames M`): NIC ek
  interrupt maarne se pehle N µs ya M packets wait karta → fewer interrupts,
  higher throughput, par +latency (packet baitha rehta jab tak coalesce timer
  na fire ho). HFT: **low coalescing** (`rx-usves 0` / very small) — latency >
  interrupt-rate efficiency. Ya kernel-bypass (`30/13`) jahan coalescing bypass.
- **Adaptive coalescing** (`adaptive-rx on`) driver ko load ke hisaab se tune
  karne deta — HFT: **off**, fixed low values (deterministic).
- IRQ handler top-half minimal (ack + schedule); bottom-half (softirq/NAPI) does
  the real work, poll-mode under load (NAPI) so it's not 1 interrupt/packet.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `irqbalance` chal raha
Tumne `smp_affinity` set kiya, 30s baad `irqbalance` ne waapas spread kar diya —
including hot cores pe. `systemctl disable --now irqbalance` pehle.

### Trap 2 — sirf IRQ pin kiya, softirq bhool gaye
IRQ housekeeping core pe, par RPS config ne softirq ko sab cores pe steer kiya
(including hot). `/sys/class/net/eth0/queues/rx-*/rps_cpus` ko bhi housekeeping
mask do (ya `00` = disable RPS, softirq stays where IRQ landed).

### Trap 3 — high interrupt coalescing "for stability"
`rx-usves 50` = har packet ~50 µs baitha reh sakta NIC pe interrupt ka wait
karte. Market data latency ke liye disaster. Low/zero coalescing, adaptive off.

### Trap 4 — NIC doosre NUMA node pe (double whammy)
IRQ ko node-0 housekeeping core pe pin kiya, par NIC node 1 ka → IRQ handler
node 0 pe chalta par packet data node 1 memory mein → remote (`14`). NIC ke
apne node ke cores pe IRQ pin karo.

### Trap 5 — `nmi_watchdog` on
Har core pe periodic NMI (hard lockup detector). Hot core pe ek aur jitter
source. `sysctl kernel.nmi_watchdog=0` ya cmdline `nmi_watchdog=0`.

### Trap 6 — `smp_affinity` set hone ke baad verify na karna
Kuch IRQs "managed" hote (kernel-controlled affinity, driver ne set kiya) —
`echo` silently ignore ho sakta (`smp_affinity` write EIO / no effect). `cat`
karke confirm, aur `/proc/interrupts` diff se actually check kaunse core pe
count badh raha.

### Trap 7 — housekeeping cores overloaded
Saare IRQs + `ksoftirqd` + logging + monitoring 2 housekeeping cores pe → woh
saturate → NET_RX softirq late → market data late even though hot core idle.
Enough housekeeping cores do; heavy NIC → dedicated network core(s).

---

## > **HFT relevance**

> - **Hot cores are interrupt-free.** Every NIC queue IRQ pinned to housekeeping
>   (or a dedicated network) core; `irqbalance` disabled; `irqaffinity=` on
>   cmdline for defaults; `nohz_full` kills the timer tick; `nmi_watchdog=0`.
> - **NIC settings:** low/zero RX coalescing, `adaptive-rx off`, enough RX ring
>   descriptors (`ethtool -G`), RSS queues aligned to the NIC's NUMA node's
>   cores.
> - **Or bypass the kernel entirely** (`30/13`): DPDK/ef_vi poll the NIC ring
>   from userspace — no IRQ, no softirq, no syscall. IRQ tuning matters most for
>   the *kernel* network path and for everything you can't bypass.
> - **Verify continuously:** a monitor that diffs `/proc/interrupts` and
>   `/proc/softirqs` and alerts if a hot core's IRQ/softirq counts move.
> - **NIC on the hot pipeline's NUMA node**, IRQs on that node's housekeeping
>   cores (`14`).

---

## Hands-on

```bash
# NIC IRQs kaunse core pe
grep -E 'eth0|ens|enp' /proc/interrupts

# ek IRQ ki affinity
cat /proc/irq/129/smp_affinity_list

# irqbalance status
systemctl status irqbalance

# saare eth0 IRQs housekeeping (0-1) pe
for irq in $(grep -E 'eth0' /proc/interrupts | awk -F: '{print $1}' | tr -d ' '); do
  echo 0-1 > /proc/irq/$irq/smp_affinity_list 2>/dev/null && echo "IRQ $irq -> 0-1"
done

# coalescing dekho / set karo
ethtool -c eth0
ethtool -C eth0 adaptive-rx off rx-usves 0 rx-frames 1     # aggressive low latency

# hot core pe interrupts aa rahe? (do snapshots)
cat /proc/interrupts > /tmp/i1; sleep 5; cat /proc/interrupts > /tmp/i2
diff <(cut -c1-60 /tmp/i1) <(cut -c1-60 /tmp/i2)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "core isolate kiya, ab koi interrupt nahi" | IRQs abhi bhi deliver ho sakte; `smp_affinity` se pin |
| "IRQ pin kiya, ho gaya" | `irqbalance` reset kar dega; softirq/RPS bhi align karo |
| "high coalescing = stable = achha" | packets NIC pe baithe rehte; +latency; HFT me low/zero |
| "softirq apne aap dusre core pe" | default: IRQ wale core pe; RPS/isolation se steer |
| "managed IRQ ki affinity echo se badal dunga" | kuch IRQs kernel-controlled; write no-op — verify |
| "NIC ka NUMA node koi matter nahi karta" | IRQ core + packet memory node match hone chahiye |

---

## Exercises

1. `/proc/interrupts` mein `eth0-TxRx-3` ka count sirf CPU 6 pe badh raha, aur
   CPU 6 tumhara hot strategy core hai. 2-step fix.

   <details><summary>Answer</summary>

   (1) `systemctl disable --now irqbalance` (warna reset). (2) `echo 0-1 >
   /proc/irq/<that-irq>/smp_affinity_list` (housekeeping cores). Verify:
   `/proc/interrupts` diff — CPU 6 ka count freeze, CPU 0/1 pe badhna shuru.
   Bonus: `rps_cpus` for that queue ko bhi `0-1` (ya `0` to disable RPS) taaki
   softirq bhi CPU 6 na chhue.
   </details>

2. IRQ housekeeping core pe pin kar diya, par hot core pe abhi bhi `/proc/
   interrupts` ki `LOC` row badh rahi hai. Kya, fix?

   <details><summary>Answer</summary>

   `LOC` = local APIC timer interrupt (scheduler tick). Isse `smp_affinity` se
   nahi hata sakte — `nohz_full=<hot core>` boot param chahiye, aur us core pe
   **sirf 1 runnable task** (`nr_running == 1`), tabhi tick rukti (`11`).
   Do busy threads = tick wapas.
   </details>

3. NIC `ethtool -C eth0` dikhta `rx-usves: 100, adaptive-rx: on`. Market data
   feed pe kya asar, HFT setting kya?

   <details><summary>Answer</summary>

   `rx-usves 100` = NIC ek RX interrupt maarne se pehle 100 µs (ya frame limit)
   tak wait — pehla packet 100 µs tak NIC buffer mein baitha reh sakta before
   the kernel even sees it. `adaptive-rx on` = load-dependent, non-deterministic.
   HFT: `adaptive-rx off rx-usves 0 rx-frames 1` (interrupt per packet-ish, or
   very small), accepting higher interrupt rate for minimum latency. Better
   still: kernel bypass, where you poll and coalescing is moot.
   </details>

4. Housekeeping cores 0–1 pe saare IRQs + ksoftirqd + logging daemon + Prometheus
   node_exporter. Burst ke dauraan market data ~200 µs late, chahe hot core idle.
   Kyun, fix.

   <details><summary>Answer</summary>

   Housekeeping cores saturated → NET_RX softirq (jo yahin chalta) ko CPU nahi
   mil raha turant → packets kernel queue mein baithe → app ko late deliver.
   Hot core idle hone se koi fayda nahi kyunki data uski socket buffer tak
   pahuncha hi nahi. Fix: (a) dedicated network core(s) sirf NIC IRQ+softirq ke
   liye, logging/monitoring ko alag cores ya `SCHED_IDLE`. (b) `net.core.
   netdev_budget`/`netdev_max_backlog` tune. (c) more housekeeping cores.
   (d) kernel bypass for the feed.
   </details>

5. Kernel-bypass (DPDK) use kar rahe ho feed ke liye. IRQ affinity tuning ab
   bhi matter karta hai kya?

   <details><summary>Answer</summary>

   Bypassed NIC/queue ke liye nahi — DPDK PMD userspace se poll karta, us queue
   pe koi IRQ nahi. Par: (a) baaki NICs / management interface / disk / other
   devices ke IRQs abhi bhi hain — unhe hot cores se door rakho. (b) Timer,
   IPI, RCU, watchdog abhi bhi apply. (c) Agar hybrid (kuch traffic kernel path
   pe) → wahan IRQ tuning zaroori. Bypass ek path ko hatata, poore box ko IRQ-
   tuning se free nahi karta.
   </details>

---

## Interview questions

1. Hard IRQ vs softirq — kaun kya karta, dono kaunse core pe by default.
2. `/proc/irq/N/smp_affinity` — kaise set, `irqbalance` ka role.
3. Interrupt coalescing — throughput vs latency trade-off, HFT setting.
4. Timer tick (`LOC`) ko hot core se kaise hataao?
5. NIC NUMA node + IRQ affinity — dono ko kyun align karna.
6. RPS/RFS — kya karte, hot core se kaise door rakho.
7. Kernel bypass ke baad kaunsi interrupt-tuning abhi bhi relevant.

---

## Next
→ [`16-clocks-and-timers.md`](16-clocks-and-timers.md)
