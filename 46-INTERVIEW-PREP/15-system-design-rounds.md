# 15 — System design rounds: matching engine / feed handler / risk system

## Prerequisites
Folders `37`–`44`, lesson `14`. This is a whiteboard/verbal round, 45–60
min, one problem.

---

## How to run a design round (the arc)

```
1. CLARIFY (2–5 min)   scope, scale, what's in/out, who's the client
2. CONSTRAINTS         throughput, latency target (p50/p99.9), instruments,
                       hardware, determinism, correctness bar
3. INTERFACES          inputs (feed frames), outputs (orders, book snapshots),
                       the API each component exposes
4. COMPONENTS + FLOW   draw the boxes, the data structures, the threading model
5. HOT PATH            walk one event end to end; name every cost
6. BOTTLENECK + TUNE   where's the latency? apply folder-36/43 techniques
7. FAILURE MODES       loss, gap, crash, slow consumer, bad input — what happens
8. TRADE-OFFS          "I chose X over Y because...; if the constraint were Z I'd..."
9. MEASURE             how you'd benchmark and regression-test it
```

Interviewer grades: did you **clarify before designing**, is the data
model sound, do you know where the nanoseconds/microseconds go, do you
handle failure, and can you defend choices.

**Talk out loud. Draw. State assumptions. Give the simple version first,
then refine.** (`01`.)

---

## Design 1 — Feed handler (UDP multicast market data)

### Clarify
Which protocol family (ITCH-style binary incrementals + periodic
snapshot)? One instrument or many? Throughput (say ~5 M msg/s peak)?
Latency target (wire-to-normalized-event p99.9 < ~1 µs)? A/B feeds?
Downstream is in-process (SPSC) or another process (shared memory)?

### Components
```
NIC (busy-poll, isolated core)
  → frame reassembly + A/B arbitration (key on seq)
  → gap detector (next-expected; buffer ahead; request retransmit / snapshot)
  → decoder (fixed-offset fields, memcpy+bswap, no alloc, span over rx buffer)
  → normalizer (venue message → internal event enum + POD)
  → publish: SPSC ring (in-process) or seqlock'd shared-memory ring (cross-process)
```

### Hot path costs
NIC DMA → poll sees descriptor (~ns) → parse frame (bounds-check once,
decode ~1–2 ns/field) → arbitration (seq compare, dedupe) → enqueue POD
(~20 ns). No syscalls, no allocation, no locks.

### Failure modes
- **Single-path loss** → A/B arbitration covers it, zero recovery
  latency.
- **Both-path gap** → buffer ahead packets, request retransmit for the
  missing range; if unrecoverable, resync from the next snapshot and
  replay incrementals.
- **Slow consumer** → ring fills → drop + count + alert (never block the
  receive core). Design the ring large enough for the worst burst.
- **Malformed frame** → bounds check rejects it, count it, keep going;
  never trust a length field unchecked.

### Trade-offs
Busy-poll (100% core) for latency vs epoll (idle CPU) for many feeds;
in-process SPSC (fastest) vs shared-memory (multiple strategy processes,
isolation) vs a socket (simplest, slowest).

---

## Design 2 — Order book + matching engine

### Clarify
Just maintain a book from a feed, or a **matching** engine (an exchange
simulator / internal crossing)? L2 or L3? How many instruments? Price
range bounded? Determinism required (yes — for replay)?

### Data model
- Per instrument: `bid_qty[]`, `ask_qty[]` — flat `int64` arrays indexed
  by `(price_tick − base)`; cached `best_bid`, `best_ask` indices.
- L3 / matching: per level a **FIFO** (`std::vector<Entry>` or an
  intrusive list) of `{order_id, qty}`; a dense `order_id → {level,
  slot}` index for O(1) cancel.
- Prices as **integer ticks** (scale, e.g. ×100) — exact, no float.

### Operations
- **Add** — `arr[idx] += qty`; push to FIFO; update BBO if new best.
- **Cancel** — index → level → subtract qty, mark slot dead; if the
  touch level emptied, re-walk to the next non-empty (BBO move).
- **Trade / match** — aggressive order sweeps opposite side best→limit,
  consuming FIFO fronts, one fill per (level, segment); remainder rests /
  cancels (IOC) / voids all (FOK); emit trade events.
- `resolve_cross()` invariant guard.

### Hot path costs
Add/cancel: one array write + maybe a short BBO re-walk (bounded by how
far the touch moved). No allocation (FIFO vectors pre-`reserve`d or a
pool). ~15–70 ns/message measured (`43/14`, `44`).

### Failure modes
Cancel for an unknown id (late/duplicate) → ignore, count. Book crosses →
`resolve_cross`. Qty underflow (over-subtraction from a bad cancel
sequence) → clamp + assert + log; investigate upstream.

### Trade-offs
Flat array (O(1), contiguous, needs a bounded price range) vs
`std::map` (unbounded range, but per-message alloc + pointer chase, ~25×
slower). L2 (less memory/work) vs L3 (queue position, exact matching).

---

## Design 3 — Pre-trade risk gateway

### Clarify
Which checks? Per-order latency budget (it's on the hot path — target
tens of ns)? Position tracked here or fed in? What's the action on a
breach — reject, or reject + kill?

### Checks (all O(1), before the order is sent)
| Check | Logic |
|---|---|
| Fat-finger | `qty ≤ max_qty && qty*price ≤ max_notional` |
| Price collar | `abs(price − mid) ≤ band_bps * mid / 10000` |
| Position limit | `abs(position ± qty) ≤ max_position` (projected full fill) |
| Message rate | rolling-window count `≤ max_msgs` → drop (backpressure), **not** a kill-switch breach |
| Kill switch | after K *real* breaches → block all orders until manual reset |

`on_fill()` updates the real position. State is a handful of integers —
fits in a cache line, no allocation, no locks (single-threaded per
instrument).

### Failure modes
- Rate-limit path must be distinct from the kill counter (a burst
  shouldn't kill you).
- Stale `mid` (feed gap) → collar check uses a bad reference → widen or
  suspend quoting on stale data.
- Kill switch tripped → clear, loud alert; require human reset.

### Trade-offs
Strict collars = fewer bad orders but more missed quotes near fast
moves. Position check on projected *full* fill (conservative) vs
*expected* fill (tighter but riskier).

---

## Design 4 — Full tick-to-trade (the integration)

Chain Designs 1–3 + strategy + order gateway + OMS + async logging.
Single-threaded per instrument (or shared-nothing stages), pinned +
isolated cores, SPSC queues between any threads. Deterministic (no wall
clock). Every stage `rdtsc`-checkpointed into an always-on histogram
keyed to a commit; CI replays a recorded session and fails on
regression. This is exactly folder 44's `MiniHftEngine` — use it as your
worked reference. Talk through: one market message → book update →
strategy decision → risk check → order encoded → sent, naming every cost.

---

## Rubric (what a strong answer looks like)

| Dimension | Weak | Strong |
|---|---|---|
| Clarify | jumps to a design | pins scope, scale, latency target, determinism first |
| Data model | `std::map`/`unordered_map` everywhere | flat arrays by tick, cached BBO, dense id index, integer prices |
| Threading | "add threads + a mutex" | shared-nothing, pinned cores, SPSC queues, no hot-path lock |
| Hot path | vague | walks one event, names every ns/µs cost |
| Failure | ignores it | loss/gap/crash/slow-consumer/bad-input each handled |
| Trade-offs | one true way | "X over Y because…; if constraint Z then…" |
| Measurement | "it'll be fast" | histogram keyed to a commit, CI replay, before/after |

---

## Next
→ [`16-brainteasers-and-probability.md`](16-brainteasers-and-probability.md)
