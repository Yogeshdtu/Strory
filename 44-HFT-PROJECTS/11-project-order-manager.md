# 11 — Project 10: Order Manager (OMS) + execution simulator

## Prerequisites
- `10-project-risk-engine.md`
- `07-project-object-pool.md` (generation handles — OMS uses them)

## Yeh topic abhi kyun

Risk ne order ko pass kiya. Ab kisi ko us order ka **lifecycle** track
karna hai: bheja gaya → exchange ne acknowledge kiya → partially filled →
fully filled / rejected / cancelled. Yeh OMS ka kaam. Aur `ExecutionSimulator`
us order ko "the venue" (folder 40's `MatchingEngine`) pe chalata aur fills
wapas deta.

## `OrderManager` (`mh_order_manager.hpp`)

State machine:
```
   submit()          New
      │
   on_ack()          Acked
      │
   on_fill(q)   ──▶  PartiallyFilled  ──(filled >= orig)──▶  Filled   [retired]
      │
   on_reject()  ──▶  Rejected   [retired]
   on_unfilled_cancel() ──▶ Cancelled / PartiallyFilled   [retired]  (IOC remainder void)
```

- Each order lives in an `ObjectPool<OmsOrder>` (project 6) — pooled
  storage, **generation-checked handles**. `ClientOrderId -> Handle` index.
- On retire (`Filled`/`Rejected`/`Cancelled`): `pool.release(handle)`,
  erase from index, `--live_`. The slot recycles; a late message for that
  `ClientOrderId` finds nothing → safely ignored.
- `all_settled()` → `live_ == 0`: every submitted order eventually left the
  live set. This is the **accounting invariant**.

## `ExecutionSimulator` — "the venue"

Wraps folder 40's `MatchingEngine` (see `05-project-matching-engine.md`).
`send(req, cl_id, filled) -> std::vector<Fill>`. Our IOC order crosses the
mirrored market book; each `Trade` becomes a `Fill` at the maker price;
remainder is void (`last_status() != Filled` → OMS `on_unfilled_cancel`).

## Measure (`10_order_manager.cpp`)

### State machine
```
  [ok]   submit -> New
  [ok]   on_ack -> Acked
  [ok]   partial fill -> PartiallyFilled
  [ok]   full fill -> retired (get -> nullptr)
  [ok]   reject -> retired
  [ok]   IOC partial + unfilled-cancel -> retired
```

### Full-run accounting (120k messages through the whole pipeline)
```
full run: submitted=~240 orders (~4800 qty), filled ~4700 qty
  [ok]   every submitted order settled (live == 0)
  [ok]   filled qty <= submitted qty (no phantom fills)
```

Two invariants that must **always** hold:
1. **`live_ == 0` at the end** — no order is left dangling in an
   intermediate state. If it's not zero, some order got a `submit` but
   never a terminal event — a bug (lost ack, lost fill, missed reject).
2. **`filled_qty <= submitted_qty`** — you can't fill more than you asked
   for. If it's greater, the venue or the fill-accounting is
   double-counting.

## HFT relevance

- The OMS is the **source of truth** for "what orders do I have working and
  what's my exposure." Risk, P&L, and reconciliation all read from it.
- Real OMSes handle: cancel/replace (amend), order chaining, venue-specific
  order types, sequence-number tracking per session, drop-copy
  reconciliation, and recovery after a disconnect (re-request working
  orders from the exchange).
- The generation-handle pattern (project 6) matters here specifically:
  exchanges send **async** acks/fills, sometimes out of order, sometimes
  after your local order object has been recycled. The handle check turns
  "apply a stale fill to the wrong order" into "safely drop it."

## ⚠️ Traps

### Trap 1 — leaking orders (live_ never returns to 0)
An order gets `submit`ed, then a fill path is missed (venue returned
`Filled` but OMS was only told about partial fills). `all_settled()` catches
it in tests; in production it shows as phantom exposure.

### Trap 2 — applying a fill to a recycled slot
`ClientOrderId -> OmsOrder*` (raw pointer) instead of `-> Handle` → late
fill for a retired order lands on whatever order now owns that slot. Use
the handle + `get()` (project 6, trap 3).

### Trap 3 — state transitions out of order
`on_fill` arrives before `on_ack` (exchange filled instantly). The state
machine must handle `New -> PartiallyFilled` directly, not assume `Acked`
first.

### Trap 4 — trusting `orig_qty` from the fill
Fills report *fill* qty, not order qty. Track `filled += fill.qty` against
the order's own `orig_qty`; don't reconstruct it from fills.

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Fills always arrive after the ack | Instant fills → `New -> PartiallyFilled` with no `Acked` |
| OMS can store `OmsOrder*` in the index | Recycled slot + late message = wrong-order bug; store `Handle` |
| "live_ != 0 at end" is a test artifact | It's a real leak — an order with no terminal state |
| filled_qty slightly > submitted is rounding | It's double-counting — a real accounting bug |

## Exercises

1. `10_order_manager.cpp` mein `on_ack` ke bina seedha `on_fill` call karo.
   State kya hoti?
   <details><summary>Answer</summary>
   `on_fill` sees state `New`, applies the fill → `PartiallyFilled` (or
   `Filled`). The `on_ack`-first assumption is not baked in — good,
   because instant fills are real.
   </details>

2. OMS ke index ko `unordered_map<ClientOrderId, OmsOrder*>` bana do
   (Handle ki jagah raw pointer). Full-run mein kaunsa check fail ho sakta?
   <details><summary>Answer</summary>
   Ek order retire → slot recycle → agla `acquire` wahi memory → purana
   pointer ab naye order ko point karta. Agar us pehle order ke id pe koi
   late `on_fill` aata (yahan sim mein synchronous hai to shayad nahi, par
   async venue mein), woh fill galat order pe apply → `filled_qty` ya
   `position` galat. Handle + gen check isko nullptr banata.
   </details>

3. `on_unfilled_cancel` ko remove kar do (IOC remainder ko retire mat
   karo). `all_settled()` pe asar?
   <details><summary>Answer</summary>
   IOC orders jo poore fill nahi hue woh `PartiallyFilled`/`Acked` mein
   phanse rahenge → `live_` kabhi 0 nahi hoga → `all_settled()` fail.
   IOC ka remainder void hota — OMS ko batana zaroori.
   </details>

## Interview questions

1. OMS ka state machine — states aur transitions. Instant fill kaise
   handle?
2. Exchange async out-of-order acks/fills bhejta — OMS kaise safe rehta
   (generation handle)?
3. "Every submitted order settles" invariant kya catch karta?
4. `filled_qty <= submitted_qty` violate hone ka matlab?

## Next
→ [`12-project-mini-hft-engine.md`](12-project-mini-hft-engine.md)
