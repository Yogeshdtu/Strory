# 13 — Kernel bypass: DPDK, Onload, ef_vi, VMA, AF_XDP

## Prerequisites
- `29-LINUX-SYSTEMS` file 01 (ring 0/3), file 02 (syscall cost), file 15 (IRQs/softirqs)
- `12-zero-copy.md`, `08-blocking-vs-nonblocking.md` (busy-poll)

## Yeh topic abhi kyun
Ab tak humne kernel network stack ke andar latency kam ki — `TCP_NODELAY`, UDP,
`epoll`, busy-poll, zero-copy. Kernel bypass ka idea alag hai: **poora kernel
stack hi hata do** data path se. NIC ki packet rings ko seedha userspace mein
map karo, wahi se poll karo. Ek `sendto()` syscall (~1–3 µs) ban jaata ek memory
write + doorbell (~100–300 ns). Yeh HFT ka single biggest network latency lever
hai. Yeh lesson overview — code nahi, kyunki har framework ka apna API hai aur
hardware-specific hai.

---

## Kernel path vs bypass — what's actually removed

**Kernel RX path (per packet):** NIC IRQ → hard IRQ handler → softirq (NAPI) →
IP/UDP/TCP processing → socket buffer enqueue → wake the app → `recv()` syscall
→ user↔kernel copy. ~1–5 µs + scheduler jitter.

**Bypass RX path:** NIC DMAs the frame into a **userspace-visible ring** → your
poll loop reads the descriptor → you parse the packet. ~100–300 ns, no IRQ, no
softirq, no syscall, no copy, no wakeup.

You give up: the kernel's TCP/IP stack (you bring your own, or use the
framework's), the kernel's socket API for that traffic, and one or more CPU
cores permanently spinning.

---

## The families

| Framework | Model | Stack | Notes |
|---|---|---|---|
| **DPDK** | full NIC ownership; PMD (poll-mode driver) in userspace | **you provide** (or use `dpdk` sample stacks / F-Stack / VPP) | vendor-neutral, huge ecosystem. NIC bound to `vfio-pci` — kernel can't use it. Hugepages, core pinning mandatory. Best raw throughput/latency, most work. |
| **Solarflare/Xilinx OpenOnload** | `LD_PRELOAD` shim — intercepts socket calls | **kernel-compatible userspace TCP/UDP** | your existing sockets code runs unchanged; Onload does bypass under the hood. NIC still usable by kernel (shared). The "just works" option — Solarflare hardware. |
| **ef_vi** (Solarflare/Xilinx) | low-level API, raw frames | **you provide** | lower than Onload, more control, more code. Used when Onload's stack isn't enough. |
| **Mellanox/NVIDIA VMA / XLIO** | `LD_PRELOAD` shim, like Onload | kernel-compatible | for Mellanox/ConnectX NICs. |
| **AF_XDP** (in-kernel, "half bypass") | XDP program + a userspace ring (`UMEM`) | you provide (or partial) | **in mainline Linux**, no proprietary driver. NIC stays with the kernel; XDP redirects selected flows to your ring. Zero-copy mode with supported drivers. Lower ceiling than DPDK/ef_vi, far less setup — the pragmatic starting point. |
| **RDMA / RoCE** | different beast — one-sided memory ops | n/a | not for exchange feeds (they're Ethernet multicast); used for intra-datacenter low-latency messaging. |

---

## What you take on

| Cost | Detail |
|---|---|
| **Your own protocol handling** | ARP, ICMP, fragmentation, and (for TCP) the whole state machine — either implement, or use the framework's userspace stack (Onload/VMA give you one; DPDK/ef_vi you bring one). For UDP multicast market data it's simpler (parse Ethernet/IP/UDP headers, done). |
| **Dedicated cores** | one or more cores spinning 100% polling the NIC ring, forever. Isolated (`29/11`), no other threads. |
| **Hugepages + pinned memory** | packet buffers (`mbuf`/`UMEM`) in hugepages, `mlock`ed (`29/12`, `29/13`). |
| **NIC ownership / driver binding** | DPDK: NIC removed from the kernel (`vfio-pci`), `ip`/`tcpdump`/`ethtool` don't see it. Onload/VMA/AF_XDP: shared, less disruptive. |
| **Debuggability** | `tcpdump` doesn't see bypassed traffic (unless the framework offers a mirror — DPDK `pdump`, Onload `onload_tcpdump`, AF_XDP you can tee). New tooling to learn. |
| **Hardware lock-in** | ef_vi/Onload = Solarflare/Xilinx; VMA = Mellanox; DPDK/AF_XDP = broad but NIC-feature-dependent. |
| **Portability / CI** | can't run the real path on a dev laptop; need representative NICs in test. |

---

## Where each fits in an HFT stack

- **Market-data ingest (UDP multicast):** the highest-value bypass target. DPDK
  or ef_vi poll-mode; parse Eth/IP/UDP in ~tens of ns; feed the decoder. A/B
  arbitration (`06`) in userspace. This alone can cut ~2–4 µs off tick-to-trade.
- **Order entry (TCP):** bypass with a userspace TCP stack (Onload, or a
  purpose-built minimal TCP). Fewer messages, but the syscall + stack on the
  send path still matters.
- **Everything else (control, logging, monitoring, drop-copy):** stays on the
  kernel stack — no benefit to bypassing, and you want `tcpdump`/`ss` there.

**Adoption ladder:** kernel stack + all the tuning from `29/18` and this folder
→ `SO_BUSY_POLL` → `AF_XDP` for the feed → full DPDK/ef_vi when the last
microseconds pay for the engineering.

---

## AF_XDP — the pragmatic middle

- Ships in mainline Linux (kernel ≥ 4.18, better ≥ 5.x). No proprietary driver.
- You load a small **XDP** BPF program at the NIC driver's earliest hook; it
  `XDP_REDIRECT`s chosen packets into an `AF_XDP` socket backed by a userspace
  memory region (`UMEM`) — a ring of frames.
- **Zero-copy mode** with supported drivers (i40e, ice, mlx5, ...): the NIC DMAs
  straight into `UMEM`. Otherwise a copy mode (still fast).
- The rest of the traffic on that NIC continues through the normal kernel stack —
  you only divert the flows you want.
- Latency: better than the kernel socket path, not as low as DPDK/ef_vi (still
  an in-kernel hop for the redirect), but **massively** less setup and no NIC
  seizure. Good first bypass step.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — bypassing before doing the basics
Kernel bypass on an untuned box (no core isolation, no IRQ affinity, THP on,
mitigations churning) leaves most of the jitter in place. Do `29/18` +
`TCP_NODELAY` + busy-poll first; measure; then bypass for the last few µs.

### Trap 2 — underestimating the stack you now own
"We'll just bypass TCP" → you now need retransmit, RTO, congestion control,
window management, TIME_WAIT, corner cases — a multi-quarter project. Use
Onload/VMA's stack, or keep TCP on the kernel and only bypass the UDP feed.

### Trap 3 — forgetting `tcpdump` is blind
Bypassed traffic doesn't show in `tcpdump`/`ss`/`/proc/net`. Set up the
framework's capture (`dpdk-pdump`, `onload_tcpdump`, an AF_XDP tee, or a
switch SPAN port) **before** you need it in an incident.

### Trap 4 — DPDK NIC seizure surprise
`dpdk-devbind` moves the NIC to `vfio-pci` — it vanishes from `ip link`,
`ethtool`, the kernel. If that's your only NIC / your management path, you've
locked yourself out. Dedicate a separate NIC; keep management on a kernel NIC.

### Trap 5 — no hugepages / not pinned
DPDK/AF_XDP buffers in regular pages → page faults + TLB misses on the hot path
(`29/12`, `29/13`). Reserve hugepages at boot, `mlock` the pools.

### Trap 6 — one core, many jobs
The poll loop must own its core. Sharing it with the decoder or a timer thread
reintroduces scheduling jitter (`29/11`). One core per pipeline stage.

### Trap 7 — assuming bypass fixes NUMA / switch / wire
Bypass removes host stack latency. It does nothing for a cross-socket NIC
(`29/14`), a store-and-forward switch, interrupt coalescing you left on for
other NICs, or geographic distance. Those are separate fixes.

---

## > **HFT relevance**

> - **Kernel bypass is the single biggest network latency lever** — it deletes
>   the syscall + copy + IP/UDP/TCP stack + softirq + wakeup from the data path,
>   turning ~1–5 µs into ~100–300 ns per operation.
> - **Prioritise the market-data feed** (UDP multicast) — simplest to bypass
>   (just header parsing), biggest tick-to-trade win. DPDK or ef_vi, poll-mode,
>   dedicated isolated core, hugepage `mbuf` pools, A/B arbitration in userspace.
> - **Order entry:** Onload/VMA (`LD_PRELOAD`, your socket code unchanged) or a
>   minimal userspace TCP; the send-side syscall + stack still costs.
> - **Keep control/logging/monitoring on the kernel stack** — you want the
>   tooling, and there's no latency payoff.
> - **Do the tuning first** (`29/18`, this folder). Bypass on an untuned box is
>   wasted effort — the jitter you didn't fix is still there.
> - **Start with `AF_XDP`** — mainline, no NIC seizure, no proprietary driver —
>   then go to DPDK/ef_vi when the last microseconds justify the engineering and
>   the hardware lock-in.
> - **Set up capture/observability for the bypassed path before go-live.**

---

## Hands-on (conceptual — real bypass needs specific NICs)

```bash
# what NIC / driver do you have?
lspci | grep -i ethernet
ethtool -i eth0                       # driver, firmware
ethtool -T eth0                       # timestamping caps (relevant for 14)

# AF_XDP support probe
ip link set dev eth0 xdpgeneric obj /usr/lib/bpf/xdp-pass.o sec xdp 2>/dev/null && echo "XDP loads"
ls /sys/fs/bpf 2>/dev/null

# DPDK: bindable NICs (do NOT bind your management NIC)
dpdk-devbind.py --status 2>/dev/null || echo "install dpdk to inspect"

# Onload (Solarflare): would run as
#   onload --profile=latency ./your_existing_socket_app
```

For actually building: DPDK `l2fwd`/`l3fwd` samples, `libxdp`/`xdp-tools`
tutorials, or Onload's docs with Solarflare hardware. This lesson is the map;
the implementation is hardware-specific and lives outside this repo's build.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "bypass = just faster sockets" | you remove the kernel stack; you own the protocol + a spinning core |
| "bypass fixes all latency" | host stack only — not NUMA, switch, coalescing, distance |
| "DPDK is the only way" | AF_XDP (mainline), Onload/VMA (`LD_PRELOAD`), ef_vi — spectrum |
| "our socket code just gets faster" | true for Onload/VMA shims; DPDK/ef_vi = rewrite |
| "tcpdump still works" | blind to bypassed traffic; set up framework capture |
| "bypass first, tune later" | untuned box keeps its jitter; tune (`29/18`) first |

---

## Exercises

1. Your tick-to-trade is 12 µs. A breakdown shows ~4 µs in the kernel RX path
   (IRQ→softirq→stack→wakeup→`recv`). What does bypassing the feed realistically
   save, and what's left?

   <details><summary>Answer</summary>

   Bypass turns that ~4 µs RX path into ~0.1–0.3 µs (NIC DMA into a userspace
   ring + your poll loop reading a descriptor). Net saving ~3.5–4 µs. What's
   left: your decode + strategy + order-encode (your code budget), the send-side
   syscall + stack (unless you also bypass TX), NIC serialization + switch + wire
   (physics), and any NUMA/coalescing/scheduling issues you didn't fix. Bypass
   is huge but it's one term in the sum.
   </details>

2. Why is UDP multicast market data the easiest thing to bypass, and TCP order
   entry the hardest?

   <details><summary>Answer</summary>

   For the UDP feed you only need to parse Ethernet + IP + UDP headers and hand
   the payload to your decoder — no connection state, no reliability, no ARP
   beyond the sender's MAC. For TCP you must reimplement (or license) the entire
   stack: handshake, sequence/ack, sliding window, retransmission + RTO,
   congestion control, TIME_WAIT, PMTUD, corner cases — a large, correctness-
   critical project. That's why teams bypass the feed first (DPDK/ef_vi) and
   either keep order-entry TCP on the kernel, use Onload/VMA's ready-made
   userspace stack, or write a deliberately minimal TCP.
   </details>

3. A team runs DPDK on their only NIC and then can't SSH into the box. What
   happened, and the fix?

   <details><summary>Answer</summary>

   `dpdk-devbind` moved the NIC from its kernel driver to `vfio-pci` so DPDK can
   own it — the kernel no longer has a network interface, so SSH (and `ip`,
   `ethtool`, `tcpdump`) are gone. Fix: always dedicate a **separate** NIC to
   DPDK and keep management/monitoring on a kernel-driven NIC. Recover the
   locked-out box via console/IPMI and `dpdk-devbind --bind=<kernel driver>
   <pci-id>`.
   </details>

4. Where does `AF_XDP` sit between the kernel socket path and full DPDK, and why
   is it a good first step?

   <details><summary>Answer</summary>

   `AF_XDP` is in mainline Linux: a small XDP/BPF program at the driver's
   earliest hook redirects chosen packets into a userspace-mapped ring (`UMEM`),
   with zero-copy on supported drivers. It removes most of the kernel stack for
   those flows but keeps one in-kernel hop (the redirect), so latency is better
   than sockets, not as low as DPDK/ef_vi. It's a good first step because: no
   proprietary driver, no NIC seizure (other traffic still uses the kernel),
   works on commodity NICs, and far less setup — you can measure the benefit
   before committing to DPDK's operational cost and hardware lock-in.
   </details>

5. You bypassed the feed but p99.9 tick-to-trade barely improved. List the
   likely culprits.

   <details><summary>Answer</summary>

   The box wasn't tuned first (`29/18`): (a) the poll core isn't isolated /
   `nohz_full` → still preempted and ticked; (b) IRQs from *other* NICs/devices
   land on hot cores (`29/15`); (c) THP `khugepaged` collapses (`29/12`); (d)
   page faults because the packet pools aren't hugepage-backed + `mlock`ed
   (`29/13`); (e) NIC on the wrong NUMA node (`29/14`); (f) the decode/strategy
   threads share cores or allocate/log on the hot path; (g) turbo/C-state
   frequency jitter (`29/06`). Bypass removed the stack latency; the *jitter*
   was elsewhere.
   </details>

---

## Interview questions

1. What exactly does kernel bypass remove from the RX path? Rough latency delta.
2. DPDK vs Onload vs ef_vi vs AF_XDP — model and trade-offs of each.
3. What do you take on when you bypass TCP (vs a UDP feed)?
4. Why keep control/logging/monitoring on the kernel stack?
5. `tcpdump` and bypassed traffic — the observability problem and fixes.
6. The adoption ladder: tuning → busy-poll → AF_XDP → DPDK/ef_vi.
7. Bypass doesn't fix which latency sources? (NUMA, switch, coalescing, distance, scheduling)

---

## Next
→ [`14-timestamping.md`](14-timestamping.md)
