# Mock transcript — System design round (~45 min)

> Problem: "Design an order book that consumes an ITCH-style feed and
> exposes best bid/offer and depth. Equities, one exchange."

---

**Candidate:** Before I design — a few questions. Is this just maintaining
the book from the feed, or also **matching** incoming orders?

**Interviewer:** Just maintaining it from the feed for now.

**Candidate:** L2 aggregated per price level, or L3 with every individual
order?

**Interviewer:** L2 is fine.

**Candidate:** One instrument or many?

**Interviewer:** Design for one; tell me how you'd scale to many.

**Candidate:** And is the price range bounded — a tick grid with price
bands?

**Interviewer:** Yes.

**Candidate:** Throughput and latency target?

**Interviewer:** Peak a few million messages a second. Per-update latency
should be tens of nanoseconds.

**Candidate:** And does a recorded session need to replay
byte-identically?

**Interviewer:** Yes, assume so.

**Candidate:** Okay. Those constraints — millions of messages a second,
tens of nanoseconds, deterministic replay — rule out per-message
allocation and locks, and push me toward a flat data structure.

**Data model.** Prices as integer ticks, scale times 100 — exact, no
float. For one instrument, two flat arrays: `int64 bid_qty[N]` and
`int64 ask_qty[N]`, indexed by `price_tick minus base`, holding aggregate
quantity per level. I cache `best_bid_idx` and `best_ask_idx`. And a
dense `order_id to {side, tick, qty}` index — a `std::vector` direct-
indexed by a compact order id — so cancels are O(1).

**Operations.** Add: `arr[idx] += qty`, and if this price is better than
the cached best, update the cached index. Cancel: look up the order in
the id index, subtract its quantity from its level, mark it dead; if that
was the touch level and it's now empty, re-walk outward to the next
non-empty level to move the BBO. Trade from the feed: consume quantity
from the touch level, re-walk if it empties.

**Cost.** Add is one array write plus maybe a one-or-two-tick BBO update.
Cancel is O(1) plus a bounded re-walk only when the touch empties, and
the walk distance is just how far the BBO moved. No allocation after
construction. Single-threaded, so no locks.

**Interviewer:** Why not `std::map<price, Level>`?

**Candidate:** Every add and cancel would be a tree node malloc and free
plus a rebalance, all pointer-chasing — measured about 25 times slower
for the book stage. And a sorted `std::vector` isn't the answer either:
HFT order flow is constant add and cancel near the touch, so you'd be
doing an O(n) memmove of all the levels beyond the insertion point on
every message. The flat array indexed by tick wins because the price
range is bounded, so the index is just arithmetic.

**Interviewer:** How do you keep the book from crossing — bid greater
than or equal to ask?

**Candidate:** If updates are applied fully and correctly it doesn't
cross. As a safety guard I'd add a `resolve_cross` step: while best bid is
at or above best ask, drop the smaller of the two crossed touch levels
and re-walk. But that's a symptom guard — the real fix is upstream,
making sure stale orders get pulled and trades that consume a level are
applied. I actually hit this exact bug once: the cached BBO only moved
one direction and stale far orders were never pulled, so the book drifted
crossed as the mid moved.

**Interviewer:** Scaling to many instruments?

**Candidate:** Partition instruments into groups — by hash, or by
liquidity to balance load — and run one independent single-threaded
pipeline per group, each pinned to its own isolated core with its own
feed subscription filter. No shared mutable state between cores. If I
need cross-instrument logic like portfolio risk, that runs on a separate
core and gets updates over SPSC queues.

**Interviewer:** How would you verify it's correct?

**Candidate:** Run a brute-force reference — literally a `std::map`-based
book — alongside the real one over a replayed capture, and assert the BBO
matches on every message. Folder-style, I'd expect zero mismatches over a
hundred-thousand-message session. Plus an `rdtsc` latency histogram per
operation, and a CI job that replays a fixed session and fails if p50,
p99, or p99.9 regress past a threshold.

---

## Rubric

| Dimension | Weak | This candidate |
|---|---|---|
| Clarify first | jumps to a design | asked 6 scoping questions before drawing |
| Data model | `std::map` everywhere | flat array by tick, cached BBO, dense id index, integer prices |
| Justifies choices | "it's faster" | map = per-msg alloc + chase (~25×); sorted vector = O(n) memmove on churn |
| Failure modes | ignores | crossing → `resolve_cross` + names the real upstream cause (from experience) |
| Scaling | "add threads" | shard instruments across isolated cores, shared-nothing, SPSC for cross-cutting |
| Verification | "it'll be fast" | reference `std::map` diff + `rdtsc` histogram + CI replay regression gate |

**Bar for a strong hire:** clarifies before designing, reaches for a flat
array not a map, can defend it, handles the crossing invariant, and has a
concrete correctness-and-latency verification plan.
