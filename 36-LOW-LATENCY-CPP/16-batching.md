# 16 — Batching and amortization — and the latency trade-off

## Prerequisites
- `01-latency-throughput-jitter.md`, `35-PROFILING-BENCHMARKING/05` (tail)
- `examples/10_batching.cpp`

## Yeh topic abhi kyun
Batching ek fixed per-operation cost ko N items pe amortize karta →
throughput badhta. Par har item ko "batch ready" hone ka wait karna padta →
**latency badhti**. Yeh classic trade-off, measured as a curve, aur "kab
batch karo, kab bilkul nahi."

---

## Where the fixed cost is

Batching pays off when there's a **per-operation overhead** independent of
how many items you process:

| Fixed cost | Batch primitive |
|---|---|
| a **syscall** (`write`, `sendmmsg`, `io_uring` submit) | `writev` / `sendmmsg` / one io_uring submit for N |
| a **lock** acquire/release | take the lock once, do N updates |
| a **cache-cold** data structure touch | process N items while it's hot |
| **DMA / disk** transfer setup | one big transfer vs N small |
| an **RPC** round trip | one request carrying N operations |
| **timestamp / bookkeeping** per call | amortized over the batch |
| **branch mispredict** on a mode check | check the mode once per batch |

If your per-item path has **no** such fixed cost (e.g. a pure arithmetic
transform), batching does nothing — don't add the complexity.

---

## Measured (`10_batching.cpp`, is box — a non-inlinable per-batch setup + per-item work)

```
  batch B    ns / item    throughput M/s    head-of-line ns
  1          6.71         149               6.7
  2          4.09         245               12.3
  4          2.84         353               19.8
  8          2.17         462               32.5
  16         1.83         545               56.9
  32         1.66         603               104.5
  64         1.58         634               200.4
  256        1.54         649               787.4
  1024       1.51         661               3095.2
```

- **ns/item falls** as B grows (the fixed `batch_setup` amortized) — but the
  gain **flattens** by B ≈ 32–64 (setup is essentially amortized away;
  1.66 → 1.51 from B=32 to B=1024 is a rounding error).
- **head-of-line latency grows ~linearly** with B — an item arriving right
  after a batch closed waits for ~B more items, then the whole batch is
  processed. At B=1024 that's **3 µs** vs **6.7 ns** at B=1.

**Sweet spot** for a throughput-bound stage: the knee, ~B=16–32 here — most
of the throughput, a fraction of the latency cost.

---

## The decision

```
  Is this stage THROUGHPUT-bound (a real risk of a queue backing up under
  peak load)?
        |
    NO  ── B = 1. Process each item the instant it arrives. Batching only
        |   adds head-of-line latency for zero benefit.
        |
    YES ── batch, but:
        |   - pick B at the amortization KNEE (measure), not "as big as possible"
        |   - use OPPORTUNISTIC batching: take whatever is already queued
        |     (B = current depth), NEVER wait to fill a batch
        |   - carve out urgent items (an order, a risk trip) to a B=1 fast lane
```

### Opportunistic batching (the HFT pattern)
```cpp
// drain the ring: process everything available NOW as one batch,
// but never block waiting for more
std::size_t n = ring.pop_bulk(buf, MAX_BATCH);   // returns 0..MAX_BATCH
if (n) process_batch(buf, n);                      // amortize setup over n
```
Under light load `n` is small (often 1) → low latency. Under a burst `n` is
large → the amortization kicks in exactly when you need the throughput.
You never *add* latency by waiting.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — waiting to fill a fixed batch
`while (batch.size() < 32) batch.push(recv());` → an item can wait for 31
more to arrive. Under low load that's unbounded latency. Opportunistic only.

### Trap 2 — batching a stage that isn't throughput-bound
Your feed decoder does 400 ns/msg and peak is 200k msgs/sec (8% util) →
batching adds head-of-line latency for a throughput problem you don't have.

### Trap 3 — B as big as possible
The amortization curve flattens (knee ~16–32 here). Beyond the knee you're
just adding latency. Pick B at the knee, measured.

### Trap 4 — one queue for urgent + bulk
An order stuck behind a batch of 500 quotes. Separate lanes: urgent B=1,
bulk opportunistic-batched.

### Trap 5 — batching that breaks ordering/atomicity
If items in a batch have dependencies (a cancel must follow its order),
batch processing must preserve order and handle intra-batch dependencies.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "batching = pure win" | throughput yes, latency (head-of-line) no |
| "wait for a full batch of N" | opportunistic: take what's queued, never wait |
| "bigger batch = better" | the curve flattens at a knee; beyond it, only latency grows |
| "batch everything the same way" | urgent items to a B=1 lane |
| "the stage is slow, batch it" | only if it's *throughput*-bound; else B=1 |

---

## Exercises

1. Tumhare order gateway ko exchange ko orders bhejne hain via a socket
   `send()` (a syscall, ~300 ns fixed). Peak order rate 50k/sec. Ek engineer
   `sendmmsg` se 64-order batches bhejta hai — throughput bahut badh gaya.
   Latency-critical order path pe yeh sahi hai?

   <details><summary>Answer</summary>

   **Almost certainly wrong.** Order sends are the most latency-critical
   path in the system — the whole point is to get *your* order to the
   exchange before someone else's. 50k orders/sec = one every 20 µs. A
   64-order batch means an order can wait for **up to 63 more orders to
   arrive** before it's sent — potentially **~1.26 ms** of pure queuing
   latency added to a path where microseconds decide fills. And 50k/sec
   through a 300-ns syscall is only 1.5% of a single core's syscall
   capacity — there is **no throughput problem** to solve.
   Right approach: **B=1** — `send()` each order the instant the decision is
   made. If the 300-ns syscall itself is in your budget's way, reduce *its*
   cost (kernel bypass / `io_uring` with `SQPOLL` so submit is a memory
   write, not a syscall — lesson 17), not batch. If you have genuinely
   bursty order flow (a signal fires and you fan out 20 orders at once),
   *opportunistic* batching of exactly those 20 (whatever's ready now, no
   waiting) is fine — but never wait to fill a batch on the order path.
   </details>

2. Ek logging stage: hot threads ek SPSC ring mein fixed-size log records
   push karte hain; ek logger thread unhe file mein likhta hai. Logger
   thread `write()` per record karta (~1 µs syscall each). Ring aksar bhar
   jaata burst mein. Batch kaise, aur B kya?

   <details><summary>Answer</summary>

   The logger thread is **throughput-bound** during bursts (records arrive
   faster than 1 `write()`/µs can drain them) and it's **off the hot path**
   (the hot threads only pay the ~2 ns ring push — 35/16). So batching here
   is pure win — it adds latency only to *when the log hits disk*, which
   nobody waits on.
   Approach: **opportunistic bulk drain + `writev`** (or accumulate into a
   large buffer and one `write`):
   ```cpp
   Record buf[MAX];
   size_t n = ring.pop_bulk(buf, MAX);        // whatever's queued now
   if (n) { format_all(buf, n, big_buffer); write(fd, big_buffer, len); }
   ```
   B = "whatever is in the ring" (cap it at some `MAX` so `big_buffer` is
   bounded). One `write()` for `n` records instead of `n` writes → the
   1-µs syscall amortized to ~1 µs / n. During bursts n is large (backlog
   drains fast); when quiet n is 1–2 (still one small write, negligible).
   The ring stops filling because the drain rate is now records/write ×
   writes/sec, not 1 record/µs. Also: `O_APPEND` + a big `write`, and only
   `fsync` on a timer / shutdown, not per batch (fsync is the real spike).
   And size the ring so a full ring drops the *oldest* log records (or
   counts drops) rather than blocking a hot thread.
   </details>

---

## Interview questions

1. When batching pays off — the kinds of fixed per-op cost it amortizes.
2. The throughput vs head-of-line-latency trade-off — the shape of both curves.
3. Opportunistic batching — what it is, why it never adds latency.
4. Choosing B — why "as big as possible" is wrong; the knee.
5. Urgent + bulk in one queue — the problem and the fix (separate lanes).
6. Batching a stage that isn't throughput-bound — why it's pure loss.

---

## Next
→ [`17-syscall-avoidance.md`](17-syscall-avoidance.md)
