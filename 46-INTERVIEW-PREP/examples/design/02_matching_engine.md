# Design problem — Limit order book + matching engine

> Companion code: `../06_top_of_book.cpp` (the L2 book skeleton).
> Full reference: course folders 39 (order book), 40 (matching engine),
> 44 (`L2Book` + `FastVenue`).

---

## Prompt

"Design an order book that consumes add/cancel/trade events and maintains
per-instrument bids and asks. Extend it to a **matching engine** that
accepts incoming aggressive orders and produces fills."

---

## 1. Clarify

- **Book only, or matching?** (Book = maintain state from a feed;
  matching = also cross incoming orders against resting ones.)
- **L2 (aggregated per level) or L3 / MBO (every individual order)?**
  L3 needed for queue-position modelling and exact matching.
- **Instruments:** many → one single-threaded pipeline per shard.
- **Price range:** bounded tick grid (yes — exchanges have a tick size
  and price bands).
- **Order types to match:** Limit, Market, IOC, FOK? Self-trade
  prevention?
- **Determinism:** replay must be byte-identical.
- **Throughput / latency:** ~M msg/s; per-update target tens of ns.

## 2. Data model

- Prices as **integer ticks** (scale ×100) — exact, hashable, no float.
- Per instrument, per side: a flat `std::array<int64, kLevels>` of
  aggregate quantity, indexed by `(tick − base)`.
- Cached `best_bid_idx`, `best_ask_idx`.
- **L3 / matching:** each level also holds a **FIFO** of `{order_id,
  qty}` (a `std::vector<Entry>` pre-`reserve`d, or an intrusive list in a
  pool). A dense `order_id → {side, level, slot}` index (`std::vector`,
  direct-indexed) for O(1) cancel.

```
bid_qty[tick]  ask_qty[tick]        // L2 aggregate
bid_fifo[tick] ask_fifo[tick]       // L3: FIFO<Entry> per level
loc[order_id] -> {side, tick, slot} // O(1) cancel
best_bid_idx, best_ask_idx          // cached BBO
```

## 3. Operations

| Op | Logic | Cost |
|---|---|---|
| **Add** | `arr[idx] += qty`; push FIFO; update BBO if new best | O(1) |
| **Cancel** | id index → level → `arr[idx] -= qty`, mark slot dead; if touch level emptied, re-walk to next non-empty (BBO move) | O(1) + bounded re-walk |
| **Trade** (from feed) | consume `qty` from the touch FIFO front(s); re-walk if depleted | O(fills) |
| **Match** (aggressive order) | sweep opposite side best→limit, fill FIFO fronts in price-time order, one fill per (level, segment); remainder rests (Limit) / cancels (IOC) / voids all (FOK) | O(levels swept) |

`resolve_cross()` — while `best_bid ≥ best_ask`, drop the smaller of the
two crossed touch levels and re-walk. Invariant guard; the real fix is
applying updates fully.

## 4. Hot path

Add/cancel: one array write + maybe a 1–2-tick BBO re-walk. No allocation
(FIFOs pre-reserved or pooled). Single-threaded per instrument → no lock,
no coherence traffic. Measured ~15–70 ns/message (`43/14`, `44`).

## 5. Matching correctness — the hard parts

- **Price-time priority:** fill the *oldest* order at the best price
  first (FIFO per level).
- **FOK:** must be *all or nothing* — a naive "precheck total available
  qty then fill" **breaks** once combined with self-trade prevention
  (some of that qty is your own and gets skipped). Check availability
  *excluding* STP-skipped orders, or do a dry run. (Real finding from
  folder 40.)
- **Self-trade prevention:** cancel-newest / cancel-oldest / cancel-both
  when an incoming order would match your own resting order.
- **Determinism:** no map iteration affecting output; fixed tie-breaks.

## 6. Failure modes

| Failure | Handling |
|---|---|
| Cancel for unknown / already-cancelled id (late, duplicate) | ignore, count |
| Book crosses | `resolve_cross()` + investigate upstream (lost stale-order pull? unapplied trade?) |
| Qty underflow (bad cancel sequence over-subtracts) | clamp to 0, assert, log; root-cause upstream |
| Price outside the tick grid | reject / clamp; a real venue enforces price bands |

## 7. Trade-offs

- **Flat array** (O(1), contiguous, needs a bounded price range) vs
  **`std::map<price, Level>`** (any range, but a tree node malloc/free +
  rebalance + pointer chase per message — ~25× slower book stage;
  `43/17` C1: a *sorted vector* is also worse than the map under HFT
  add/cancel churn because of the O(n) memmove).
- **L2** (less memory/work) vs **L3** (queue position, exact matching,
  more update cost).
- **Aggregate-only `FastVenue`** (fast sweep) needs a **per-level FIFO**
  anyway to keep per-order state consistent with later cancels — an
  aggregate-only sweep drifts (real folder-44 bug: 3 shares over
  4706 → fixed by adding the FIFO).

## 8. Measure / verify

- BBO diffed against a brute-force `std::map` reference over a replayed
  session (0 mismatches / ~118k messages — `44`).
- Matching engine **fuzz-tested** against an independent reference
  implementation (0 disagreements across 30k random commands — `40`).
- `rdtsc` per-op latency histogram; CI replay + regression threshold.
- `template<class Venue>` naive-vs-optimized: prove the trading output
  (fills, qty, P&L, position) is **byte-identical** across N seed/config
  combos **before** reporting any speedup (`44`).
