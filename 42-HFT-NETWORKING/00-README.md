# 42 — HFT NETWORKING (PHASE 30)

## Prerequisites
`41-HFT-CONCURRENCY`, `30-NETWORKING`

## Yeh folder kyun
Wire se wire tak. Yahan har nanosecond count karta hai — aur kernel aapka dushman hai.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-hft-network-path.md` | **Wire → NIC → kernel → app → NIC → wire** — har step ki cost |
| 02 | `02-multicast-receive-path.md` | Market data multicast, group management, filtering |
| 03 | `03-kernel-bypass-overview.md` | Kyun bypass, kaunse options, trade-offs |
| 04 | `04-solarflare-onload.md` | Onload — transparent bypass, LD_PRELOAD model |
| 05 | `05-ef-vi.md` | ef_vi — raw API, lowest latency |
| 06 | `06-dpdk-intro.md` | DPDK — poll mode drivers, hugepages, cores (concepts + minimal example) |
| 07 | `07-busy-poll-sockets.md` | `SO_BUSY_POLL`, kernel busy polling, when it helps |
| 08 | `08-hardware-timestamping.md` | **NIC timestamps**, PTP, measuring true wire time |
| 09 | `09-nic-tuning.md` | Ring buffers, coalescing, offloads (aur kaunse disable karein) |
| 10 | `10-irq-affinity-tuning.md` | IRQ pinning, avoiding interrupt jitter |
| 11 | `11-tcp-for-order-gateways.md` | TCP tuning, `TCP_NODELAY`, connection warm-up, keepalives |
| 12 | `12-switch-and-network-latency.md` | Switch latency, cut-through vs store-forward, cabling |
| 13 | `13-fpga-offload-intro.md` | FPGA ka role — **overview only, yeh alag domain hai** |
| 14 | `14-wire-to-wire-measurement.md` | **True latency measurement** — taps, timestamps, correlation |
| 15 | `15-packet-capture-analysis.md` | tcpdump, wireshark, offline analysis |
| 16 | `16-building-md-receiver.md` | **BUILD: low-latency multicast receiver** |
| 17 | `17-exercises.md` | Practice + network optimization |

## Examples

| File | Kya |
|---|---|
| `examples/01_multicast_receiver.cpp` | **Basic multicast receiver** |
| `examples/02_busy_poll_receiver.cpp` | Busy-poll version |
| `examples/03_receiver_benchmark.cpp` | Latency comparison |
| `examples/04_hw_timestamps.cpp` | Hardware timestamping |
| `examples/05_order_gateway.cpp` | TCP order gateway |
| `examples/06_wire_to_wire.cpp` | End-to-end latency measurement |
| `examples/07_nic_tuning.sh` | NIC tuning script |
| `examples/08_bypass_abstraction.hpp` | Kernel/bypass abstraction layer |

## Time
3 hafte

## Status
✅ **COMPLETE** — 17 lessons (`01`–`17`) + 8 examples (`.hpp`/`.sh` +
6× `.linux.cpp`). `./build.ps1 folder 42-HFT-NETWORKING` → **0 real
fail, 6 correctly SKIPPED (linux-only)**.

This is the first HFT-track folder where MOST of the code is genuinely
Linux-only (multicast sockets, `SO_BUSY_POLL`, `SO_TIMESTAMPING`,
`MSG_ERRQUEUE`, TCP order-gateway tuning) — consistent with
29-LINUX-SYSTEMS/30-NETWORKING's established `.linux.cpp` pattern, since
this dev box is Windows/MinGW with no WSL installed. Every example was
written carefully (reusing 30's already-verified structural patterns),
hand-reviewed for the usual pitfalls (sign-compare, unbounded blocking
recv, empty-vector UB — several real ones caught and fixed), and each
carries an explicit `EXPECTED (... NAHI napa gaya)` block rather than
fabricated "measured" numbers. Central technical contribution: an
`INetworkReceiver` abstraction (`08_bypass_abstraction.hpp`) making the
kernel-socket vs Onload/ef_vi/DPDK choice a config-time decision, not an
application rewrite; a TX+RX same-clock-domain hardware-timestamping
technique that fixes 30/09's "different epochs, relative-only" limitation;
and a full wire-to-wire (tick-to-trade) breakdown across 2 network hops
+ internal processing, all kernel-timestamped.

## Next
→ [`../43-HFT-OPTIMIZATION/00-README.md`](../43-HFT-OPTIMIZATION/00-README.md)
