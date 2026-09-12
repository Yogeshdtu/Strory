# 06 — DPDK: poll mode drivers, hugepages, cores

## Prerequisites
- `05-ef-vi.md`

## Yeh topic abhi kyun

ef_vi (05) EK vendor (Solarflare/Xilinx) ke NIC tak limited hai. **DPDK**
(Data Plane Development Kit) ISI idea ko **vendor-agnostic** banata —
Intel, Mellanox, aur kai aur NIC vendors DPDK-compatible poll-mode
drivers dete. Yeh HFT/networking ka sabse widely-adopted bypass
framework hai.

---

## Poll-mode driver — interrupt ka opposite

Normal NIC driver: packet aata → **interrupt** fire hota → CPU context-
switch karta → driver ISR chalta → kernel ko deta. DPDK ka **poll-mode
driver (PMD)**: koi interrupt HAI HI NAHI — ek dedicated core
CONTINUOUSLY NIC ke descriptor ring ko POLL karta ("kya naya packet
aaya?"), 41/07's busy-spin trade-off yahan HARDWARE level pe.

```
Interrupt-driven (normal):  packet -> IRQ -> context switch -> ISR -> app
                             (latency: interrupt-handling overhead)

Poll-mode (DPDK):           dedicated core LAGATAAR check karta
                             (latency: ~0, PAR core 100% burn hota,
                              hamesha, chahe packet aaye ya na aaye)
```

---

## NIC OS se poori tarah HATANA

```bash
# NIC ko kernel driver se UNBIND karke DPDK-compatible driver se BIND karo:
dpdk-devbind.py --bind=vfio-pci <PCI-address-of-NIC>
```

Iske baad, us NIC ko **kernel bilkul nahi dekh sakta** — `ifconfig`,
`ip link`, `ethtool` sab kuch us interface ke liye USELESS ho jaate
(kernel ke pass ab woh device HAI hi nahi, driver-level pe). Sirf DPDK
application usse access kar sakti.

---

## Hugepages — kyun zaroori

DPDK **hugepages** (2MB ya 1GB pages, normal 4KB ki jagah) use karta —
kam pages = kam TLB entries chahiye poore memory footprint ke liye =
kam TLB misses (32-CACHE-MEMORY-PERFORMANCE ka hugepages concept, yahan
applied). DPDK ke rings/buffers (jo continuously access hote hot path
pe) hugepages mein pre-allocate hote — 36's "pre-fault, dry-run = zero-
fault steady state" principle yahan EXTREME scale pe.

---

## Conceptual shape

```cpp
rte_eal_init(argc, argv);          // DPDK environment abstraction layer setup
// ... port config, mempool setup (buffer pools, hugepage-backed) ...
struct rte_mbuf* pkts[MAX_BURST];
uint16_t n = rte_eth_rx_burst(port, queue, pkts, MAX_BURST);   // POLL -- non-blocking, batch
for (uint16_t i = 0; i < n; ++i) {
    // process pkts[i]->buf_addr ...
    rte_pktmbuf_free(pkts[i]);
}
```

`rte_eth_rx_burst` — notice **"burst"**: DPDK naturally BATCH karta
(ek call mein multiple packets), 41/05's Disruptor-batching principle
yahan bhi.

---

## Trade-offs

| Fayda | Cost |
|---|---|
| Vendor-agnostic (Intel/Mellanox/etc.) | Poora networking stack (agar TCP/IP chahiye) KHUD likhna/library integrate karna |
| Batched, zero-copy, huge-page-backed | Kam se kam 1 CORE 100% dedicated (poll loop) |
| Widely adopted (community, libraries) | Hugepage/kernel config zaroori (`vfio-pci`/`igb_uio`, boot params) |
| Fine-grained control (queue-per-core, RSS) | NIC OS se poori tarah GAYAB — koi aur process usse touch nahi kar sakta |

---

## ⚠️ Traps / Common mistakes

### Trap 1 — DPDK ko "bas ek library import" samajhna
DPDK poore SYSTEM SETUP (hugepages configured, NIC unbound from kernel,
dedicated isolated cores, 41/08's core-pinning) ki maang karta — yeh ek
DEPLOYMENT commitment hai, sirf code-level dependency nahi.

### Trap 2 — sochna DPDK "free TCP/IP" deta
DPDK sirf **poll-mode packet I/O** deta — agar TCP chahiye,
alag library (jaisa F-Stack, mTCP) ya khud likhna padta.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| DPDK "ef_vi jaisa hi, bas generic" hai | Similar IDEA (poll-mode, bypass), ALAG API/ecosystem, vendor-agnostic |
| DPDK free mein TCP/IP deta | Sirf packet I/O; TCP/IP alag layer (khud ya library) |
| DPDK ek "add-on" hai | Poora deployment commitment (hugepages, NIC unbind, dedicated cores) |

---

## Exercises

1. Ek team apna NIC vendor abhi decide nahi kar payi (multiple vendors
   evaluate kar rahe). Kaunsa bypass option (ef_vi vs DPDK) unke liye
   safer starting point hai?
   <details><summary>Answer</summary>
   DPDK -- vendor-agnostic hai (multiple NIC vendors support karte),
   isliye vendor-decision baad mein bhi flexible rehta. ef_vi Solarflare/
   Xilinx-specific commitment hai upfront.
   </details>

---

## Interview questions

1. Poll-mode driver interrupt-driven se kaise alag hai?
2. Hugepages DPDK mein kyun zaroori hain?
3. DPDK aur ef_vi ka trade-off fark (vendor-lock ke angle se).

---

## Next
→ [`07-busy-poll-sockets.md`](07-busy-poll-sockets.md)
