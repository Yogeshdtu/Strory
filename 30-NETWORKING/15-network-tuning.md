# 15 — Network tuning: NIC rings, offloads, coalescing

## Prerequisites
- `29-LINUX-SYSTEMS` file 15 (IRQ affinity, softirqs), file 18 (tuning checklist)
- `05-udp.md` (kernel drops), `11-socket-options.md`

## Yeh topic abhi kyun
Socket options aur boot params ke baad, ek layer aur hai: **NIC + driver + kernel
network stack** ki settings. Galat coalescing ya GRO on = tumhare tuned CPU cores
ke bawajood market data 50–100 µs late. Yeh lesson `ethtool` + sysctl network knobs
— folder 29 ke `18` checklist ka networking hissa, expanded.

---

## NIC ring buffers (`ethtool -g` / `-G`)

The NIC has RX and TX **descriptor rings** in host memory. RX ring full (app /
softirq behind) → NIC drops incoming frames (`rx_missed`/`rx_no_buffer`).

```bash
ethtool -g eth0                          # current / max ring sizes
ethtool -G eth0 rx 4096 tx 4096          # bump toward the max
```
- **Bigger RX ring = more burst absorption** before drops — cheap insurance for
  market-data bursts. Downside: a very large ring can add latency under load
  (frames sit longer) and use more memory; 2048–4096 is a common sweet spot.
- Check drops: `ethtool -S eth0 | grep -iE 'drop|miss|no_buf|fifo|overrun'`.

## Interrupt coalescing (`ethtool -c` / `-C`)

The NIC waits up to `rx-usves` µs or `rx-frames` frames before raising an RX
interrupt — fewer interrupts, more throughput, **more latency** (the first
packet of a lull waits for the timer).

```bash
ethtool -c eth0
ethtool -C eth0 adaptive-rx off adaptive-tx off rx-usves 0 rx-frames 1 tx-usves 0
```
- **HFT: minimum coalescing.** `adaptive-rx off` (deterministic), small/zero
  `rx-usves`. Accept a higher interrupt rate for lower, predictable latency.
- **Adaptive** coalescing lets the driver tune by load — non-deterministic,
  turn it off.
- If you're kernel-bypassing that NIC (`13`), coalescing is moot (you poll).

## Offloads (`ethtool -k` / `-K`)

| Offload | What | HFT |
|---|---|---|
| **GRO** (Generic Receive Offload) | driver merges small incoming packets into one big one before the stack | **off** — merging = per-packet latency + jitter (`01`) |
| **LRO** (Large Receive Offload) | hardware version of GRO, even more aggressive, can't be undone for forwarding | **off** |
| **GSO/TSO** (segmentation offload, TX) | stack hands a big buffer, NIC splits into MSS segments | usually **on** (TX-side, helps throughput, small latency cost); some shops disable for determinism |
| **RX/TX checksum** | NIC computes checksums | **on** (frees CPU, no latency cost) |
| **RX/TX VLAN** | NIC handles VLAN tags | on if you use VLANs |
| **rx-fcs / rxvlan** | pass FCS / raw | niche |

```bash
ethtool -k eth0                          # list
ethtool -K eth0 gro off lro off          # the important two for RX latency
```

## RSS / RPS / RFS — spreading receive work

- **RSS** (Receive Side Scaling): the NIC hashes each flow to one of N RX queues,
  each with its own IRQ → parallel receive across cores. Set queue count to your
  housekeeping/network core count; pin each queue's IRQ (`29/15`).
  ```bash
  ethtool -L eth0 combined 4              # 4 RX/TX queues
  ethtool -x eth0                         # show the RSS indirection table
  ```
- **RPS** (software RSS): kernel redistributes softirq processing across cores.
  HFT: usually **off** for hot NIC queues (`echo 0 > .../rps_cpus`) — you want
  softirq on the same housekeeping core as the IRQ, not scattered onto hot
  cores.
- **RFS** (Receive Flow Steering): steers a flow's softirq to the core running
  the app that owns it. Can help *if* the app isn't pinned; HFT pins everything,
  so RFS is usually off/irrelevant.
- **aRFS** (accelerated RFS): NIC-level flow steering to the app's core — useful
  with a large connection count, less so for a single pinned feed.

## Flow control / pause frames (`ethtool -a` / `-A`)

Ethernet PAUSE frames let a congested receiver tell the switch to stop sending.
Sounds good, but it introduces **head-of-line blocking** at the switch (one slow
port pauses everyone) and unpredictable delay.

```bash
ethtool -A eth0 rx off tx off            # common in HFT: no pause frames
```
Debatable — some environments keep RX pause on to avoid drops. Know it exists;
decide deliberately.

## qdisc (traffic control on TX)

```bash
tc qdisc show dev eth0
tc qdisc replace dev eth0 root pfifo_fast   # simple FIFO, no shaping
# or 'mq' (multiqueue, one qdisc per HW queue) which is the modern default
```
- Default (`fq_codel` / `fq`) adds AQM + pacing — generally fine, but `fq`'s
  pacing can add latency. HFT trading interface: `pfifo_fast` or `mq`, **no
  shapers**, no `netem`.

## sysctl (network) — recap + additions (`29/18`)

```ini
net.core.rmem_max = 134217728            # so SO_RCVBUF isn't capped (05, 11)
net.core.wmem_max = 134217728
net.core.rmem_default = 16777216
net.core.netdev_max_backlog = 250000    # per-CPU backlog before the stack (burst)
net.core.netdev_budget = 600            # packets per softirq poll
net.core.netdev_budget_usves = 4000
net.core.busy_poll = 50                 # kernel-side busy poll (08, 11)
net.core.busy_read = 50
net.core.default_qdisc = fq             # or pfifo_fast on trading iface
net.ipv4.udp_mem = 8388608 12582912 16777216
net.ipv4.udp_rmem_min = 8192
net.ipv4.tcp_slow_start_after_idle = 0  # (03)
net.ipv4.tcp_low_latency = 1            # legacy; mostly a no-op on modern kernels
net.ipv4.conf.eth0.rp_filter = 2        # multi-NIC / multicast (02)
net.ipv4.igmp_max_memberships = 512     # if joining many multicast groups (06)
```

---

## ⚠️ Traps / Common mistakes

### Trap 1 — GRO/LRO left on
Market-data packets merged/delayed at the NIC/driver → per-packet latency + burst
jitter. `ethtool -K eth0 gro off lro off`.

### Trap 2 — adaptive / high interrupt coalescing
`adaptive-rx on` or `rx-usves 100` → first packet of a lull waits ~100 µs.
`adaptive-rx off`, small/zero `rx-usves`.

### Trap 3 — RX ring too small
Default (256–512) + market-data burst → `rx_missed` drops even with a big
`SO_RCVBUF` (the drop is upstream of the socket). `ethtool -G eth0 rx 4096`.

### Trap 4 — RPS scattering softirq onto hot cores
Default or misconfigured `rps_cpus` sends receive softirq to your isolated
trading cores → jitter. `echo 0 > /sys/class/net/eth0/queues/rx-*/rps_cpus` for
hot queues; keep softirq with the IRQ on housekeeping cores (`29/15`).

### Trap 5 — RSS queue count ≠ IRQ affinity plan
16 RSS queues, IRQs spread everywhere including hot cores. Set
`ethtool -L eth0 combined <housekeeping core count>` and pin each queue IRQ
explicitly.

### Trap 6 — `fq` pacing on the trading interface
`fq` qdisc paces TX — adds small, variable delay. `pfifo_fast`/`mq` on the
interface carrying orders.

### Trap 7 — tuning the wrong NIC
Multi-NIC box: you tuned `eth0` but market data comes on `eth2`. Confirm which
interface (`ip -br addr`, `tcpdump`), tune that one; keep management NIC
untouched.

### Trap 8 — settings not persisted
`ethtool` changes are lost on reboot / link down-up / driver reload. Persist via
udev rules, `NetworkManager`/`networkd` link config, or a `systemd` oneshot
service (`29/18`).

---

## > **HFT relevance**

> - **RX latency knobs (do all, on the market-data NIC):** `gro off lro off`,
>   `adaptive-rx off`, `rx-usves 0 rx-frames 1`, `ring rx 4096`, RSS queues =
>   housekeeping core count with IRQs pinned there (`29/15`), RPS off for hot
>   queues, pause frames off (deliberate).
> - **`net.core.rmem_max`/`wmem_max` large** so socket buffers aren't capped;
>   `netdev_max_backlog` + `netdev_budget` up for burst absorption.
> - **`pfifo_fast`/`mq` qdisc**, no shapers, on the trading interface.
> - **Persist everything** (udev / systemd oneshot) — `ethtool` settings don't
>   survive a link flap.
> - **Verify by effect:** `ethtool -S` drop counters flat during the open,
>   `/proc/net/udp` drops zero, `/proc/interrupts` diff clean on hot cores, feed
>   jitter (example `05`/`10`) within budget. An unverified `ethtool` line is a
>   guess.
> - **All moot if you kernel-bypass that NIC (`13`)** — you own the ring; but
>   the *other* NICs and the box still need this.

---

## Hands-on

```bash
ip -br addr                                        # which iface is which
ethtool -i eth0 ; ethtool -g eth0 ; ethtool -c eth0 ; ethtool -k eth0 | grep -E 'gro|lro|tso|gso'
ethtool -S eth0 | grep -iE 'drop|miss|no_buf|fifo|error'

# apply HFT RX settings (market-data NIC)
sudo ethtool -K eth0 gro off lro off
sudo ethtool -C eth0 adaptive-rx off adaptive-tx off rx-usves 0 rx-frames 1
sudo ethtool -G eth0 rx 4096 tx 4096
sudo ethtool -L eth0 combined 2
for q in /sys/class/net/eth0/queues/rx-*/rps_cpus; do echo 0 | sudo tee $q; done

# qdisc
tc qdisc show dev eth0
sudo tc qdisc replace dev eth0 root mq

# watch drops during a load test
watch -n1 'ethtool -S eth0 | grep -iE "drop|miss"; echo ---; grep Udp: -A1 /proc/net/snmp'
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "GRO/offloads sirf achhe" | GRO/LRO delay per-packet latency; off for market data |
| "coalescing = stability" | packets wait for the timer; min/zero for HFT |
| "big `SO_RCVBUF` = no drops" | NIC RX ring can drop upstream of the socket — size that too |
| "RPS spreads load = good" | scatters softirq onto hot cores; off for hot queues |
| "`ethtool` change permanent" | lost on reboot / link flap / driver reload — persist it |
| "tuned one NIC = done" | tune the market-data NIC specifically; verify by drop counters |

---

## Exercises

1. Market-data drops appear only during the open, `SO_RCVBUF` is 32 MB and
   `net.core.rmem_max` is 128 MB. Where else could the drop be?

   <details><summary>Answer</summary>

   Upstream of the socket buffer: (1) the **NIC RX ring** — `ethtool -S eth0 |
   grep -iE 'rx_missed|rx_no_buffer|fifo'`; bump `ethtool -G eth0 rx 4096`. (2)
   **`netdev_max_backlog`** — the per-CPU queue between the NIC and the stack;
   raise it. (3) The **softirq not getting CPU** — its core is busy/preempted
   (`29/15`), or `netdev_budget` too low so it can't drain in one poll. (4)
   **GRO** stalling packets. The socket buffer is only the last stage; the open
   burst can overrun any earlier one.
   </details>

2. Why turn `adaptive-rx` off even though it's "smart"?

   <details><summary>Answer</summary>

   Adaptive coalescing changes the interrupt timer/frame thresholds based on
   observed load — so your per-packet latency depends on recent traffic and
   varies unpredictably. HFT wants **deterministic** latency: fixed, minimal
   coalescing (`rx-usves 0`/small, `rx-frames 1`), so every packet is handled
   the same way regardless of load. You trade a higher interrupt rate (fine on a
   dedicated network core) for predictability.
   </details>

3. RPS is enabled and your isolated trading core shows rising `NET_RX` in
   `/proc/softirqs`. Connection?

   <details><summary>Answer</summary>

   RPS (software receive steering) is configured to spread receive-softirq
   processing across a CPU set that includes your isolated trading core. So even
   though the NIC IRQ is pinned to a housekeeping core, RPS hands the softirq
   work (protocol processing) to the trading core → jitter on your hot thread.
   Fix: `echo 0 > /sys/class/net/eth0/queues/rx-*/rps_cpus` for the hot NIC's
   queues (disable RPS; softirq stays where the IRQ landed), or set the mask to
   housekeeping cores only.
   </details>

4. Your `ethtool -K eth0 gro off` works, but after a network blip the drops and
   latency spikes return. Why?

   <details><summary>Answer</summary>

   `ethtool` settings are **not persistent** — a link down/up, a driver reload
   (`modprobe -r`/`-a`), a `NetworkManager` reconnect, or a reboot resets them
   to driver defaults (GRO back on). Persist them: a udev rule
   (`ACTION=="add", SUBSYSTEM=="net", NAME=="eth0", RUN+="/sbin/ethtool -K
   eth0 gro off lro off"`), a `systemd` oneshot service ordered after the
   interface, or your network manager's link-config hooks. Then verify after a
   deliberate `ip link set eth0 down/up`.
   </details>

5. When does none of this NIC tuning matter?

   <details><summary>Answer</summary>

   When you **kernel-bypass** that NIC (`13`): DPDK/ef_vi own the NIC's rings and
   you poll them from userspace, so coalescing, GRO, RPS, and the kernel qdisc
   are all bypassed. (You do still size the NIC's descriptor rings and pick RSS
   queues within the framework, and every *other* NIC on the box — management,
   the order-entry NIC if it's still on the kernel stack — still needs this
   tuning.)
   </details>

---

## Interview questions

1. NIC RX ring — what fills it, what dropping there looks like, how to size.
2. Interrupt coalescing — throughput vs latency, HFT setting, adaptive.
3. GRO/LRO — what they do, why off for market data.
4. RSS vs RPS vs RFS — hardware vs software steering; HFT choices.
5. Pause frames — the head-of-line-blocking risk.
6. Which qdisc for a trading interface and why.
7. Persisting `ethtool` settings across link flaps / reboots.

---

## Next
→ [`16-building-servers.md`](16-building-servers.md)
