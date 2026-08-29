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
⏳ **Yeh folder abhi syllabus stage pe hai.** Upar ki file list poori plan hai —
content agle batch mein aayega.

## Next
→ [`../43-HFT-OPTIMIZATION/00-README.md`](../43-HFT-OPTIMIZATION/00-README.md)
