# Design problem — Market data feed handler

> Worked answer in the `15-system-design-rounds.md` arc:
> clarify → constraints → interfaces → components → hot path → bottleneck
> → failure modes → trade-offs → measure.

---

## Prompt

"Design a market data feed handler. It receives an ITCH-style binary
incremental feed over UDP multicast and produces normalized book-update
events for downstream strategy code."

---

## 1. Clarify

- **Scope:** parse + normalize + publish. Book maintenance is a separate
  component (`02_matching_engine.md`). Recovery/snapshot channel in scope?
  (assume yes — you must handle gaps).
- **Instruments:** many (thousands) → shard, or one process per group.
- **Feeds:** A/B redundant multicast groups.
- **Downstream:** in-process (SPSC ring) or a separate process
  (shared-memory ring)?
- **Scale:** peak ~5 M msg/s aggregate; bursty at the open.
- **Latency target:** wire → normalized event, p99.9 < ~1 µs.
- **Determinism:** a recorded session must replay byte-identical.

## 2. Constraints → design forces

Rate + latency + determinism ⇒ **no allocation, no syscalls, no locks on
the hot path**; parse in place; fixed-size POD events; single-threaded
receive per shard on a pinned isolated core.

## 3. Interfaces

```
Input:  raw UDP datagrams (one or more messages per datagram, length-prefixed)
Output: SPSC ring of  struct MdEvent {
            uint64 seq; uint64 ts_ns; uint32 instrument;
            enum {Add,Cancel,Trade,...} type;
            int64 px_ticks; uint32 qty; uint8 side; ...
        };   // trivially copyable, <= 48 bytes
```

## 4. Components + data flow

```
NIC (busy-poll, isolated core, IRQ coalescing off / kernel bypass)
  │
  ▼  recv into a fixed ring of receive buffers (no per-packet alloc)
Frame splitter        (walk length-prefixed messages within the datagram)
  │
  ▼
A/B arbitration       (key on seq; take first arrival; drop the duplicate)
  │
  ▼
Gap detector          (next_expected; seq>expected → buffer ahead + request
                       retransmit / fall back to snapshot channel and replay;
                       seq<expected → duplicate, drop)
  │
  ▼
Decoder               (fixed-offset fields; memcpy + __builtin_bswap / bit_cast;
                       bounds-check the frame ONCE; span over the rx buffer)
  │
  ▼
Normalizer            (venue message → MdEvent POD; instrument id lookup via a
                       flat table, not a hash map)
  │
  ▼
Publish               (SPSC ring for in-process; seqlock'd shared-memory ring
                       for cross-process; full → drop + count, never block)
```

## 5. Hot path — one message end to end

NIC DMA → poll sees a descriptor (~ns) → frame split (pointer math) →
A/B seq compare + dedupe (~ns) → decode (bounds-check once, ~1–2 ns/field,
`bswap`) → normalize (flat id lookup, POD fill) → `try_push` to the ring
(~20 ns). **Zero** allocations, syscalls, locks.

## 6. Bottleneck & tuning

- Parsing: portable shift-and-or ≈ `memcpy`+`bswap` at `-O2` (compiler
  fuses it) — don't hand-SIMD without measuring (`43/13`).
- The publish ring: `alignas(64)` indices + cached opposite index (the
  biggest single win, `03_spsc_ring_buffer.cpp`).
- Instrument lookup: flat array indexed by a compact id, not
  `unordered_map` (node chase).
- Receive: busy-poll on a dedicated core; NIC coalescing off; consider
  `ef_vi` / DPDK if kernel-stack latency dominates the budget.

## 7. Failure modes

| Failure | Handling |
|---|---|
| Single-path loss (A or B) | A/B arbitration covers it — zero recovery latency |
| Both-path gap | buffer ahead packets; request retransmit for the range; if unrecoverable, resync from the next snapshot and replay incrementals |
| Slow consumer | ring fills → drop + count + alert; size the ring for the worst open-burst |
| Malformed frame / bad length | bounds check rejects; count; continue — never trust a length field unchecked |
| Duplicate (seq < expected) | drop |
| Clock | no wall-clock reads; `ts_ns` comes from the feed / a NIC hardware timestamp |

## 8. Trade-offs

- **Busy-poll** (100% of a core, lowest latency) vs **epoll** (idle CPU,
  µs wakeup) — HFT picks busy-poll for the hot feed.
- **In-process SPSC** (fastest) vs **shared-memory ring** (multiple
  strategy processes, fault isolation) vs **a socket** (simplest,
  slowest).
- **Kernel sockets** vs **Onload** (transparent, sockets API) vs
  **ef_vi/DPDK** (sub-µs, you own the stack, less portable).
- **One process, sharded threads** vs **one process per instrument
  group** (isolation vs shared caches/config).

## 9. Measure

`rdtsc` checkpoints per stage → an always-on HdrHistogram keyed to a git
commit. CI replays a recorded multicast capture (pcap) and fails the
build if p50/p99/p99.9 regress past a threshold. Every optimization PR
shows the before/after histogram. Correctness: normalized event stream
diffed against a reference decoder over the same capture (0 differences).
