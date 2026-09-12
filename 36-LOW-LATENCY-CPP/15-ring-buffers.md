# 15 — Ring buffers: design, power-of-2, cache-friendly indices

## Prerequisites
- **`28-LOCK-FREE/01-02`** (the SPSC ring built + the full optimization
  ladder), **`27-ATOMICS`** (acquire/release, `memory_order`)
- `20-ALGORITHMS-DSA/07` (ring buffer as a queue)
- `examples/09_ring_buffer.cpp`

## Yeh topic abhi kyun
A fixed-capacity ring buffer is *the* HFT inter-thread primitive: hot thread
→ housekeeping/logger/aggregator thread, feed handler → strategy, strategy →
order gateway. Folder 28 built and benchmarked it (the ladder: naive →
+padding → +cached-index). Yahan the clean final form, the design knobs,
aur "kab ring galat tool hai".

---

## The design, decision by decision

### 1. Fixed capacity, power of two
```cpp
static_assert((Cap & (Cap - 1)) == 0, "power of two");
buf_[head & (Cap - 1)] = v;                 // & mask, not %  (no divider, no branch)
```
Power-of-two → index wrap is `& (Cap-1)` (one AND). Non-pow2 → `%` (~20-40
cyc divider — folder 34/08) or a branch. Always pow2.

### 2. Monotonic counters, not wrapped indices
`head_` and `tail_` are **ever-increasing** `size_t` counters; you mask only
when indexing. `size = head - tail`, `full = (head - tail) == Cap`,
`empty = head == tail`. No "one slot wasted to distinguish full/empty",
and **no ABA** (the counters never repeat in any practical run — 2⁶⁴ ops).

### 3. Two atomics, release-store / acquire-load — no CAS
```cpp
// producer:
buf_[h & mask] = v;                              // write payload
head_.store(h + 1, std::memory_order_release);   // publish  (payload happens-before this)
// consumer:
size_t h = head_.load(std::memory_order_acquire);// see the publish -> see the payload
out = buf_[t & mask];
tail_.store(t + 1, std::memory_order_release);
```
SPSC (one producer, one consumer) → no CAS needed, wait-free in practice.
MPMC → Vyukov bounded queue (per-cell sequence numbers — 28/03).

### 4. Cache the opposite index (the real ~1.5× win)
```cpp
// producer keeps a private copy of tail_; only re-reads the real tail_
// when its cached view says "full":
if (h - cached_tail_ >= Cap) {
    cached_tail_ = tail_.load(std::memory_order_acquire);   // cross-core read — rare
    if (h - cached_tail_ >= Cap) return false;              // really full
}
```
Without this, every `push` reads the consumer's `tail_` cache line → a
cross-core coherence miss per op. With it, the producer only pays that when
it *thinks* it's full — normally never (28/02 measured this as the ladder's
biggest single step, ~1.3–1.6×).

### 5. Pad the shared fields
`alignas(64)` on `head_`, `tail_`, `buf_` → each on its own cache line → no
false sharing between producer and consumer (lesson 11). `09_ring_buffer.cpp`
does exactly this; `sizeof(ring)` grows but the ping-pong is gone.

### 6. Full-policy at the producer
`try_push` returns `false` on full. The producer **must not block** on the
hot path — it drops (and counts the drop, which is itself a signal), applies
backpressure upstream, or the ring is sized so full never happens in
practice (lesson 05). Blocking on a full ring on the hot path is the
opposite of what you want.

---

## Measured (`09_ring_buffer.cpp`, is box, interleaved push/pop)

```
             p50      p99      p99.9      max
  try_push   20 ns   30 ns   30 ns   ~8 us
  try_pop    20 ns   30 ns   30 ns   ~19 us
```
A few ns of real work per op: one relaxed load, `& mask`, a store, a
release-store. No `%`, no branch mispredict, no allocation, no lock. (max =
OS-interrupt noise on the per-op timer, unpinned box.)

---

## Kab ring buffer GALAT tool hai (lesson 24 preview)

- **You don't need to decouple.** If A can just *call* B directly (same
  thread, no queuing needed), a function call is ~free vs a ring hop
  (a store + a load + a cache-line handoff + the consumer's polling latency).
- **You need ordering/priority across message classes.** A single FIFO ring
  can't prioritize an urgent order over a batch of quotes. Multiple rings +
  a selection policy, or a priority structure.
- **The consumer is slower than the producer, sustained.** The ring fills,
  you drop — a ring is a *shock absorber* for bursts, not a fix for a
  throughput deficit. Fix the consumer.
- **Variable-size messages.** A ring of fixed slots wastes space for small
  messages / can't hold big ones. Use a ring of *offsets* into a separate
  byte arena, or a bip-buffer (contiguous variable-length ring).
- **You need MPMC and low contention.** Vyukov works but every op is a CAS;
  if you can shard to per-producer SPSC rings + one consumer, do that.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — non-power-of-two capacity
`% Cap` = a divider per op, or a branch. Always pow2 + `& (Cap-1)`.

### Trap 2 — relaxed everywhere
The payload write must **happen-before** the index publish → `release` on
the producer's `head_.store`, `acquire` on the consumer's `head_.load`.
Relaxed → the consumer can see the new `head_` before the payload write
lands → reads garbage (27).

### Trap 3 — no opposite-index cache
Every op reads the other side's atomic → a cross-core miss per op. Cache it
(knob 4).

### Trap 4 — producer blocks on full
`while (!try_push(v)) {}` on the hot path → the producer stalls on the
consumer. Drop + count, or size the ring so it can't fill.

### Trap 5 — sharing a slot's line
Small `T`, near-empty ring → producer's write to `buf_[h]` and consumer's
read of `buf_[t]` share a line (lesson 11 ex 2). Batch, size generously, or
line-pad slots for tiny `T` under heavy contention.

### Trap 6 — a ring where a call would do
Decoupling has a cost (the hop + polling latency). Use a ring to cross a
thread boundary or absorb a burst, not by reflex.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "any capacity is fine" | power of two → `& mask`; else divider/branch |
| "wrap the indices" | monotonic counters, mask only on index → no ABA, no wasted slot |
| "relaxed atomics are faster and fine" | payload must happen-before the publish → release/acquire |
| "read the other index each op" | cache it — the ladder's biggest win |
| "block until there's space" | drop + count / size it big enough; never block the producer |
| "use a ring to be safe" | only to cross threads / absorb bursts; a call is cheaper |

---

## Exercises

1. Ek SPSC ring `try_push` mein tumne performance ke liye release ko relaxed
   kar diya: `head_.store(h + 1, std::memory_order_relaxed);`. Single test
   run pass hota hai. Yeh kyun ek time-bomb hai (x86 pe aur ARM pe)?

   <details><summary>Answer</summary>

   The producer does `buf_[h & mask] = v; head_.store(h+1, relaxed);`. With
   `relaxed`, there is **no ordering guarantee** between the payload write
   and the index store — the compiler or CPU may make the `head_` update
   visible to the consumer *before* `buf_[h & mask] = v` lands.
   - **On x86 (TSO)**: stores are not reordered with other stores, so *the
     CPU* won't reorder them — but **the compiler still can** (relaxed gives
     the optimizer permission to move/merge). And there's no guarantee for
     the consumer's *load* side either. It "works" in a simple test because
     the compiler happened not to reorder and x86's store order saved you.
   - **On ARM/POWER (weak)**: the CPU freely reorders the two stores → the
     consumer loads the incremented `head_`, then reads `buf_[t & mask]`
     which the producer **hasn't written yet** → **garbage payload**, or a
     torn value. Intermittent, load-dependent, undebuggable.
   Fix: `head_.store(h+1, memory_order_release)` on the producer and
   `head_.load(memory_order_acquire)` on the consumer — this establishes
   happens-before: everything the producer did before the release (the
   payload write) is visible to the consumer after the acquire. That's the
   whole correctness argument of the lock-free ring.
   </details>

2. Ek ring producer→consumer strategy pe har market-data message deta.
   Profiling dikhata strategy thread ka p99 acha hai par **end-to-end**
   (wire → order) p99 bura, aur ring ki depth aksar 200+ (Cap 1024). Kya ho
   raha, aur ring ka size badhana fix hai?

   <details><summary>Answer</summary>

   Ring depth staying at 200+ means the **consumer (strategy) can't keep up
   with the producer (feed handler)** on a sustained basis — messages are
   queuing. Each message now waits behind ~200 others → +200 × (per-message
   processing time) of pure queue latency added to end-to-end, even though
   the strategy's *own* per-message p99 looks fine (it's measuring from
   dequeue, not from arrival — coordinated-omission-style blind spot,
   35/05).
   **Increasing the ring size does NOT fix it** — it makes it *worse*: a
   bigger ring just lets the backlog grow deeper before you drop, so queue
   latency grows. A ring absorbs **bursts** (transient producer > consumer);
   it can't fix a **sustained** throughput deficit.
   Real fixes: (a) make the consumer faster (profile the strategy's
   per-message path — this whole folder); (b) shed load — drop stale market
   data (only the latest quote per symbol matters; conflate); (c) parallelize
   the consumer (shard symbols across N strategy threads, N SPSC rings); (d)
   do less per message (incremental book updates, bounded work — lesson 01).
   Also: measure end-to-end from **arrival timestamp**, and alert on ring
   depth as a leading indicator.
   </details>

---

## Interview questions

1. Power-of-two capacity — why (`& mask` vs `%`).
2. Monotonic counters vs wrapped indices — no-ABA, no wasted slot.
3. Release/acquire on the two indices — the happens-before argument.
4. Cached opposite index — what it removes, why it's the biggest win.
5. Producer on full — why not block; the policies.
6. Four situations where a ring buffer is the wrong tool.

---

## Next
→ [`16-batching.md`](16-batching.md)
