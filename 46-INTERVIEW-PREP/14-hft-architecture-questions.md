# 14 — Layer 13: HFT architecture (feed handler, order book, full pipeline)

## Prerequisites
Folders `37`–`44`. This Layer is the bridge into the system-design round
(`15`). Know each component's job, its data structure, and its failure
modes.

---

## A — The pipeline

### A1. Draw the tick-to-trade pipeline and name each stage's job.
<details><summary>Answer</summary>
```
NIC → Feed decoder → Order book → Strategy → Risk → Order gateway → NIC
```
**Feed decoder** — parse UDP multicast frames (A/B arbitrate, seq gap
detect) into normalized events. **Order book** — maintain per-instrument
bids/asks from add/cancel/trade events; expose BBO + depth. **Strategy** —
consume book updates, decide to quote/hedge/cancel. **Risk** — pre-trade
checks (fat-finger, position, price collar, rate limit, kill switch).
**Order gateway** — encode + send on the TCP session, track OMS state
(New→Acked→Filled...). Single-threaded per instrument or shared-nothing
stages on pinned cores. (`44` `MiniHftEngine`, `37/12`.)
</details>

### A2. Why single-threaded (per instrument) is often the right call.
<details><summary>Answer</summary>
No locks, no cache-coherence traffic, deterministic ordering, trivially
testable/replayable (byte-identical). One instrument's book+strategy+risk
fits in L1/L2. You scale by **sharding instruments across cores**, not by
threading one instrument. Cross-instrument logic (portfolio risk) runs
separately, asynchronously. (`44`, `41`, `36/19`.)
</details>

### A3. What makes the pipeline **deterministic**, and why does that
matter?
<details><summary>Answer</summary>
No wall-clock reads in logic — every timestamp comes from the event
stream; fixed RNG seeds; no unordered map iteration affecting output; no
data races. So a recorded input session **replays byte-identical**. That
matters for: debugging (reproduce a production incident offline),
regression testing (did this change alter any fill?), and `rr`-style
time-travel debugging. (`44`, `45/09`.)
</details>

---

## B — Order book

### B1. Order book data structure — the HFT design.
<details><summary>Answer</summary>
Prices live in a bounded tick range → **flat array indexed by (price −
base)**, one slot per price level holding aggregate qty (L2) or a FIFO of
orders (L3). Cache **best_bid / best_ask** indices; on a touch-level
depletion, re-walk to the next non-empty level. A dense `order_id →
{level, qty}` index (`vector`, direct-indexed) for O(1) cancel. No
`std::map`, no per-message allocation. (`39`, `43/14`, `44` `L2Book`.)
</details>

### B2. L2 vs L3 order book — difference.
<details><summary>Answer</summary>
**L2** — aggregated: per price level, total quantity (and maybe order
count). Enough for most strategies. **L3 / MBO** (market-by-order) — every
individual resting order with its id and queue position. Needed for queue-
position modelling and exact matching simulation; more memory and update
work. (`38`, `39`, `40`.)
</details>

### B3. How do you keep the book from "crossing" (bid ≥ ask)?
<details><summary>Answer</summary>
It shouldn't cross if updates are applied correctly and completely. In
practice: a `resolve_cross()` step — while best_bid ≥ best_ask, drop the
smaller of the two crossed touch levels and re-walk — enforces the
invariant. The real fix is upstream (don't lose stale-order pulls, apply
trades that consume levels). This exact latent bug surfaced in this
course between folder 43 and 44 and was fixed in both the sim and the
book. (`44`, `39/16`.)
</details>

### B4. Matching engine — price-time priority, what "match" does.
<details><summary>Answer</summary>
An incoming aggressive order sweeps the opposite side from the best price
inward, filling resting orders in **FIFO order per level** (price-time
priority), until filled or its limit price is passed. Remainder rests
(limit) or cancels (IOC) or the whole thing cancels if not fully filled
(FOK). Generates trade events. Determinism + correctness (self-trade
prevention, FOK's all-or-nothing) are the hard parts. (`40`, `44`.)
</details>

---

## C — Risk & OMS

### C1. Pre-trade risk checks — name 5, all O(1).
<details><summary>Answer</summary>
(1) **Fat-finger** — order qty / notional above a cap. (2) **Price
collar** — order price within N bps of a reference (mid). (3) **Position
limit** — projected position after a full fill within bounds. (4)
**Message rate limit** — rolling-window order count (backpressure: drop,
don't kill). (5) **Kill switch** — after K real breaches, block
everything. All constant-time, on the hot path, before the order leaves.
(`37/13`, `44` `RiskEngine`.)
</details>

### C2. OMS state machine — the states.
<details><summary>Answer</summary>
`New → Acked → PartiallyFilled → Filled` (terminal), or `→ Rejected` /
`→ Cancelled` (terminal). Each order is a record in a pool with a
`ClientOrderId → handle` index. Invariants: no leaked orders (`live == 0`
at end), `filled_qty ≤ submitted_qty` (no phantom fills). Handles instant
fills (New→PartiallyFilled without Acked) and IOC remainders. (`44`
`OrderManager`.)
</details>

### C3. A late exchange ack arrives for an order slot you've already
recycled. What prevents a bug?
<details><summary>Answer</summary>
**Generation-checked handles.** The order record lives in an
`ObjectPool<OmsOrder>`; the handle is `{index, generation}`. On release,
`generation++`. The stale ack's handle has the old generation →
`pool.get(handle)` returns `nullptr` → the ack is safely dropped instead
of corrupting a reused slot. (`44`, `45/12` A3.)
</details>

---

## D — Cross-cutting

### D1. How is latency measured and regression-tested in production?
<details><summary>Answer</summary>
Hardware timestamps at the NIC (wire in / wire out) + `rdtsc`
checkpoints per internal stage → an always-on latency histogram
(HdrHistogram) keyed to a git commit. CI replays a recorded market
session and fails the build if p50/p99/p99.9 regress beyond a threshold.
Every optimization PR must show the before/after histogram. (`35/16`,
`42/14`, `43`.)
</details>

### D2. Where does the async logger sit, and why?
<details><summary>Answer</summary>
Off the hot path: the hot thread enqueues a fixed-size POD (~20–40 ns,
no format, no lock, no syscall) into an SPSC ring; a background thread
(pinned to a housekeeping core) formats and writes (binary, to an mmap'd
file). Queue full → drop + increment a counter (the logger must never be
able to stall the hot path). (`41/09`, `45/10`.)
</details>

### D3. FPGA vs software — where's the line?
<details><summary>Answer</summary>
FPGA handles the most latency-critical, fixed logic: wire parsing, simple
filters, sometimes a "fast cancel" or a tick-to-trade for a trivial
strategy — deterministic sub-microsecond, no OS. Software (C++) handles
everything complex/changing: full book, non-trivial strategy, risk, OMS,
research iteration. Many shops: FPGA for the fast path + a C++ "slow"
path that can override. (`37`, `42`.)
</details>

### D4. How would you shard 5000 instruments across 16 cores?
<details><summary>Answer</summary>
Partition instruments into 16 groups (by hash, or by liquidity to balance
load); each core runs an independent single-threaded pipeline (decode →
book → strategy → risk → gateway) for its group, with its own feed
subscription filter. No shared mutable state between cores. A separate
core aggregates cross-instrument/portfolio risk asynchronously via SPSC
queues. Pin all, isolate, idle SMT siblings. (`36/19`, `41`, `44`.)
</details>

---

## Interview tips for Layer 13

- Be able to **draw** the pipeline and give each stage's data structure
  + failure mode in one breath.
- Order book = flat array by tick + cached BBO + dense id index; **not**
  `std::map`.
- Single-threaded per instrument, shard across cores — say this and why
  (no locks, deterministic, fits in cache).
- "Deterministic replay" is a recurring theme — connect it to debugging
  and regression testing.
- Everything measured; every latency claim has a histogram behind it.

## Next
→ [`15-system-design-rounds.md`](15-system-design-rounds.md)
