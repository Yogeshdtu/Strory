# 08 — Acquire / release

## Prerequisites
- `07-memory-order-relaxed.md`
- [`examples/04_acquire_release.cpp`](examples/04_acquire_release.cpp)

## Yeh topic abhi kyun
`acquire`/`release` woh order hai jo aap 90% waqt actually chahte ho — ek thread
data banata hai aur "publish" karta hai, doosra "subscribe" karke woh data
**guaranteed** dekhta hai. Yeh SPSC queue, lazy init, `shared_ptr`, RCU — sab ka
core hai. `seq_cst` se sasta, aur exactly is pattern ke liye bana hai.

---

## The one rule

> A **release** store and the **acquire** load that reads its value (or reads a
> later value in that variable's release sequence) form a **synchronizes-with**
> edge. Everything **sequenced-before the release** in the storing thread
> **happens-before** everything **sequenced-after the acquire** in the loading
> thread.

```cpp
int payload = 0;                          // plain, non-atomic
std::atomic<bool> ready{false};

// Producer thread
payload = 42;                                        // (A)
ready.store(true, std::memory_order_release);        // (B)  ── publish

// Consumer thread
while (!ready.load(std::memory_order_acquire)) {}    // (C)  ── subscribe
assert(payload == 42);                               // (D)  ── ALWAYS holds
```

`(B)` synchronizes-with `(C)` ⇒ `(A)` happens-before `(D)`. No data race on
`payload`, and `(D)` sees `42`. Swap either side to `relaxed` → edge gone → `(D)`
races and may read `0`.

---

## Directional barriers (the intuition)

```
        │ earlier ops │
        │             │
  ┌─────▼─────────────▼──┐
  │  release STORE       │   nothing above can sink below this line
  └──────────────────────┘
  ═══════ the "line" other threads observe ═══════
  ┌──────────────────────┐
  │  acquire LOAD         │   nothing below can hoist above this line
  └─────▲─────────────▲──┘
        │ later ops   │
```

- **`release` (store side):** a one-way gate — prior reads/writes cannot move
  *after* it. Writes done before the release are all visible to anyone who
  acquires.
- **`acquire` (load side):** a one-way gate — later reads/writes cannot move
  *before* it. Once you've acquired, you see everything the releaser published.
- They **don't** stop a store *after* the release from being reordered with a
  load *before* the acquire (store-load — that's `seq_cst`'s job; file 06 demo).

---

## `acq_rel` — for RMW that both consumes and publishes

An RMW (`fetch_add`, `exchange`, `compare_exchange`) touches the variable on both
the read and write side:

```cpp
// lock-free stack push (Treiber) — folder 28
Node* n = new Node{val};
n->next = head.load(std::memory_order_relaxed);
while (!head.compare_exchange_weak(n->next, n,
                                  std::memory_order_release,   // success: publish n + n->next
                                  std::memory_order_relaxed))  // failure: just re-read
    ;
```
Here the success side only needs `release` (publish the new node). If the RMW also
needed to **see** data published by whoever it's racing, you'd use `acq_rel`
(acquire the previous releaser's writes, release your own).

Reference-count decrement is the canonical `acq_rel`:
```cpp
if (refcount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
    delete resource;   // acquire: see every other thread's last use before we destroy
}
```

---

## The release sequence (why "or a later value")

After a `release` store, a contiguous run of RMWs (by any thread) on that variable
extends the release's reach: an acquire that reads **any** value in that run still
synchronizes-with the original release. This is what makes `fetch_sub` refcount
chains and `compare_exchange` loops compose correctly. (Plain non-RMW `relaxed`
*stores* by another thread break the sequence.)

---

## `examples/04` — what it shows

- **Demo 1 (publish/subscribe):** producer writes a multi-field payload then
  `release`s a flag; consumer `acquire`s the flag then reads the payload.
  **0 mismatches** over 5000 rounds — the edge holds. (Compare file 07's
  `relaxed` version: also 0 on x86, but *formally* racy; here it's *correct by the
  standard*.)
- **Demo 2 (SPSC hand-off):** `static Message buf[N]`, `std::atomic<uint64_t>
  widx, ridx`. Producer writes `buf[w % N]` then `widx.store(w+1, release)`.
  Consumer spins until `ridx < widx.load(acquire)`, then reads `buf[r % N]`.
  **2,000,000 messages, checksum matches** — a real one-slot-per-index lock-free
  queue built purely from acquire/release. This is the folder-28 SPSC ring in
  miniature.

---

## When acquire/release is NOT enough

- **Store-load ordering across a pair** (Dekker / Peterson mutual exclusion, the
  store-buffering litmus) — needs `seq_cst` (file 06, file 16).
- **Multiple variables needing one global order all threads agree on** (IRIW —
  independent-reads-of-independent-writes; file 16) — `seq_cst`.
- Everything else — publish/subscribe, producer/consumer, lazy init, refcounts,
  lock-free queues/stacks — **acquire/release is the right tool.**

---

## > **HFT relevance**
> - **SPSC / MPSC queue on the hot path**: producer `release`s the write index
>   (publishing the slot payload), consumer `acquire`s it. No `mfence` per message
>   — a `seq_cst` store index would add ~13 ns each here (`examples/06`).
> - **Publish-an-immutable-snapshot**: build a `MarketState` object off to the
>   side, then `ptr.store(newState, release)`; readers `ptr.load(acquire)`. Classic
>   RCU-style publication (folder 28).
> - **Sequence-lock writer**: `seq.store(odd, relaxed); …write…;
>   seq.store(even, release)` — the closing `release` publishes the payload;
>   readers `acquire` the sequence (folder 28 seqlock).
> - **Refcount teardown**: `fetch_sub(1, acq_rel)` before `delete` — miss the
>   acquire and you can `delete` while another core's last read is still in
>   flight.
> - **Don't reach for `seq_cst` "to be safe"** — if the pattern is publish →
>   subscribe, acquire/release is *both* correct and cheaper. Reserve `seq_cst`
>   for genuine multi-variable global-order needs.

---

## Hands-on

```bash
./build.ps1 fast 27-ATOMICS-MEMORY-MODEL/examples/04_acquire_release.cpp
```

Then:
- Demo 1: change the producer's `release` → `relaxed`, keep consumer `acquire`.
  On x86 still "passes" — the bug is invisible. Note that it's now formally UB.
- Demo 2: change `widx.store(..., release)` → `relaxed`. On this box: likely still
  0 mismatches (TSO). On ARM: corruption. This is why you write the order the
  *model* demands.
- `g++ -O2 -S`: `store(release)` on x86 → plain `mov` (TSO gives release for
  free on a store); `load(acquire)` → plain `mov`. The orders are *free* on x86 —
  they only constrain the **compiler**. That's the point: cost 0, correctness
  everywhere.

---

## ⚠️ Traps

### Trap 1 — only one side ordered
`release` store + `relaxed` load (or vice-versa) → **no edge**. Both sides must be
release/acquire (or seq_cst).

### Trap 2 — acquire/release on *different* variables
The edge is between a release and the acquire that **reads that same variable's
value**. Releasing `x` and acquiring `y` synchronizes nothing.

### Trap 3 — expecting store-load ordering
`acquire`/`release` don't stop a later store / earlier load reordering across the
pair. Store-buffering / Dekker needs `seq_cst`.

### Trap 4 — refcount decrement with plain `release`
The *last* decrementer must `acquire` the other threads' final uses before
`delete`. `acq_rel` on the `fetch_sub`.

### Trap 5 — "release publishes only the atomic variable"
It publishes **everything sequenced-before it** in that thread — that's the whole
point. Write the payload first, release second.

### Trap 6 — release *after* you've already let the reader see the data
Ordering is by program order in the storing thread: payload write must be
**before** the release store, or it isn't published.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "acquire/release is a full fence" | One-way each; store-load across the pair is still reorderable (→ `seq_cst`) |
| "release on x86 costs an `mfence`" | Free on x86 — plain `mov`; it constrains only the compiler |
| "any release pairs with any acquire" | Only when the acquire **reads the value** that release (or its release sequence) stored |
| "release publishes just the flag" | Publishes every write sequenced-before it |
| "refcount-- just needs release" | Final decrement needs acquire too — `acq_rel` |
| "acquire/release ⇒ threads agree on a global order" | No — that's `seq_cst`; acq/rel is pairwise |

---

## Exercises

1. **Fix the publish:** `payload = build(); flag.store(true, relaxed);` /
   `while(!flag.load(relaxed)); use(payload);` — minimal change?

   <details><summary>Answer</summary>

   `flag.store(true, std::memory_order_release)` and `flag.load(std::memory_order_acquire)`.
   That creates the synchronizes-with edge so `build()`'s writes happen-before
   `use(payload)`.
   </details>

2. **Which pairs synchronize:** producer does `a.store(1, release)` then
   `b.store(1, release)`. Consumer does `b.load(acquire)` (sees 1) then
   `a.load(acquire)`. Is the consumer guaranteed to see `a == 1`?

   <details><summary>Answer</summary>

   The edge is via `b`: everything sequenced-before `b.store` (including
   `a.store(1)`) happens-before everything after `b.load(acquire)` — so yes, the
   later `a.load(acquire)` sees `1`. (If the consumer read `a` *before* `b`, no
   guarantee.)
   </details>

3. **`acq_rel` or `release`:** (a) Treiber stack push CAS; (b) refcount
   `fetch_sub` before possible delete; (c) a work-stealing deque `steal()` CAS
   that both reads the victim's tasks and claims one.

   <details><summary>Answer</summary>

   (a) `release` on success is enough (you only publish). (b) `acq_rel` — acquire
   others' last uses, release yours. (c) `acq_rel` — you consume the victim's
   published tasks and publish your claim.
   </details>

4. **Still racy?** producer `release`s `ready`; consumer `acquire`s `ready`; but
   consumer reads `payload` **before** the `acquire` load in program order.

   <details><summary>Answer</summary>

   Yes, racy — the acquire only orders operations *sequenced-after* it. A
   `payload` read before the acquire isn't covered. Move the read after the
   acquire load.
   </details>

5. **Cost on x86:** what machine barrier does `store(release)` / `load(acquire)`
   emit on x86-64, and what does that tell you about using them liberally?

   <details><summary>Answer</summary>

   None — plain `mov` both ways (x86 TSO already gives store-release and
   load-acquire). They only restrict compiler reordering. So on x86 acquire/release
   is *free* — use it wherever the pattern is publish/subscribe; it also keeps the
   code correct on weak architectures.
   </details>

---

## Interview questions

1. Release–acquire ka exact guarantee — synchronizes-with se happens-before tak.
2. "Release publishes everything before it" — matlab, aur ordering constraint.
3. `acq_rel` kab — RMW jo consume + publish dono kare, ek example.
4. Release sequence kya hai — "or a later value" ka matlab?
5. Acquire/release store-load reordering rokta hai? (Nahi — kya rokta hai?)
6. x86 pe release/acquire ka machine cost — kya aur kyun (TSO)?
7. Refcount decrement `acq_rel` kyun, `release` kyun kaafi nahi?

---

## Next
→ [`09-seq-cst.md`](09-seq-cst.md)
