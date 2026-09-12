# 10 — `std::shared_mutex` (reader-writer locks)

## Prerequisites
- `07-mutex.md`, `08-lock-guards.md`

## Yeh topic abhi kyun
`std::mutex` sab ko serialize karta — even readers jo ek doosre se conflict nahi
karte. `std::shared_mutex` (C++17) deta hai: **kai readers ek saath**, ya **ek
writer akela**. Read-mostly data (config, routing table, symbology) ke liye
faayda ho *sakta* hai — par ek catch hai jo aksar isse worthless bana deta hai.

---

## API

```cpp
#include <shared_mutex>

std::shared_mutex sm;

// WRITER — exclusive lock (like a normal mutex)
{
    std::unique_lock lk(sm);           // or std::lock_guard
    table_[key] = value;               // no reader or other writer can be in
}

// READER — shared lock
{
    std::shared_lock lk(sm);           // many readers can hold this simultaneously
    auto it = table_.find(key);        // no writer can be in; other readers can
    return it == table_.end() ? Value{} : it->second;
}
```

- **`std::shared_lock<std::shared_mutex>`** — acquires the *shared* (read) mode.
- **`std::unique_lock` / `std::lock_guard`** — acquires the *exclusive* (write) mode.
- Multiple `shared_lock` holders OK at once. A `unique_lock` waits for all readers
  to leave, then holds alone.

---

## When it helps — and when it doesn't

**Helps when:**
- Reads **vastly** outnumber writes (99%+).
- The critical section (the read) is **long enough** that reader parallelism
  actually matters (dozens of ns+, not a single pointer load).
- You have many cores doing concurrent reads.

**Doesn't help (or hurts) when:**
- The critical section is tiny — a `std::shared_mutex` is **2–5× more expensive to
  acquire** than a `std::mutex` (it maintains a reader count, often with a
  cmpxchg loop or an internal mutex). If the read is "load one pointer," the lock
  overhead dwarfs the work → a plain `std::mutex` (or better, atomics) wins.
- Writes are frequent — writers starve behind a stream of readers (or readers
  starve behind writers, depending on the fairness policy), and the reader-count
  bookkeeping cache-line-bounces between cores anyway (same as false sharing).
- Contention is high — the shared counter word is itself a contention point.

**Rule of thumb:** `shared_mutex` is a niche tool. Measure. Very often the better
answer is **an immutable snapshot + atomic pointer swap** (readers do a single
atomic load, zero locking; the writer builds a new object and swaps).

---

## The snapshot alternative (usually better)

```cpp
std::atomic<std::shared_ptr<const RoutingTable>> g_table;   // C++20 atomic<shared_ptr>

// reader — one atomic load, no lock, no waiting
std::shared_ptr<const RoutingTable> t = g_table.load();
route(t->lookup(dest));

// writer — build a new table, publish atomically
auto next = std::make_shared<RoutingTable>(*g_table.load());
next->update(...);
g_table.store(std::move(next));
```

- Readers **never block**, never see a torn state, cost = one atomic load + a
  refcount bump.
- The writer's old table stays alive until the last reader using it drops its
  `shared_ptr` (safe reclamation, for free).
- Downside: readers may see a slightly stale table for a moment (usually fine),
  and each write allocates a new copy (fine if writes are rare).

For lock-free read-mostly without `shared_ptr` overhead: **RCU** (read-copy-update)
or hazard pointers (folder 28) — same idea, manual reclamation.

---

## Fairness / starvation

`std::shared_mutex`'s policy is **implementation-defined**:
- libstdc++ (glibc rwlock) — configurable; default can let readers starve writers
  (`PTHREAD_RWLOCK_PREFER_READER_NP`) unless writer-preference is set.
- A steady stream of readers with reader-preference → a writer waits indefinitely.
- Writer-preference → new readers block once a writer is waiting → reader latency
  spikes.

You don't control this portably. Another reason to prefer the snapshot pattern
where the semantics are yours.

---

## Andar kya hota hai

- A `std::shared_mutex` holds a state word encoding "N readers" or "1 writer" (plus
  waiter flags). `lock_shared()` — cmpxchg-loop to increment the reader count if
  no writer; on contention, spin then futex-wait. `lock()` — wait for readers to
  hit 0, set the writer bit.
- The reader-count word is written by **every** `lock_shared`/`unlock_shared` → it
  cache-line-bounces between reading cores exactly like false sharing. So "many
  parallel readers" still serialize on that one word's coherence traffic.
- `std::atomic<std::shared_ptr<T>>` (C++20) — may be lock-free or use a small
  internal lock table depending on the platform; check
  `std::atomic<std::shared_ptr<T>>::is_lock_free()`. Even when not lock-free, the
  critical section is tiny and bounded.

---

## > **HFT relevance**
> - **`std::shared_mutex` is rarely the right tool on anything hot** — the
>   acquire cost + the reader-count cache bouncing make it slower than a plain
>   `std::mutex` for short critical sections, and it can't beat lock-free.
> - **Read-mostly hot data (config, symbology, risk limits, routing) →
>   immutable snapshot + atomic pointer swap.** Readers: one atomic load. Writer
>   (rare): build a new const object, `store()` it. This is the standard HFT
>   pattern for "the strategy needs to read X every tick, ops updates X once a
>   minute."
> - **Where you genuinely have long reads and rare writes on a cold-ish path**
>   (a big in-memory analytics table queried by a reporting thread), `shared_mutex`
>   is acceptable — measure it.
> - **RCU / seqlock** (folder 28) for the lock-free read-mostly case without
>   `shared_ptr` refcount traffic.

---

## Hands-on

```bash
# sketch a benchmark:
# 1. std::mutex guarding a map, 8 reader threads + 1 writer
# 2. std::shared_mutex, shared_lock for readers
# 3. std::atomic<std::shared_ptr<const Map>> snapshot
# time each; vary read critical-section size (find() vs a bigger scan)
```

- With a tiny read (`find` one key): `std::mutex` ≈ or beats `std::shared_mutex`.
- With a big read (scan 1000 entries): `std::shared_mutex` pulls ahead (real
  reader parallelism).
- The snapshot version beats both for reads and never blocks — at the cost of a
  copy per write.

---

## ⚠️ Traps

### Trap 1 — reaching for `shared_mutex` for a tiny read
The acquire overhead exceeds the work. `std::mutex`, atomics, or a snapshot.

### Trap 2 — expecting linear reader scaling
The reader-count word cache-bounces between cores → coherence traffic serializes
much of the "parallel" reads.

### Trap 3 — writer starvation
Reader-preference policy + a steady read stream → the writer never gets in.
Implementation-defined; you can't fix it portably.

### Trap 4 — `std::lock_guard` for the reader
`std::lock_guard lk(sm)` takes the **exclusive** lock (no `shared_lock` deduction).
Readers must use `std::shared_lock lk(sm)`.

### Trap 5 — upgrading a shared lock to unique
Not supported (deadlock-prone). Drop the `shared_lock`, take a `unique_lock`, and
**re-check** your assumptions (state may have changed).

### Trap 6 — using it where the data is actually write-heavy
Both readers and writers pay the bookkeeping cost; a plain `std::mutex` is
simpler and often faster.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`shared_mutex` is a free speedup for read-mostly data" | 2–5× costlier to acquire than `std::mutex`; only wins with long reads |
| "N readers → N× read throughput" | The reader-count word cache-bounces; coherence traffic serializes it |
| "readers never wait" | They wait for an in-progress or (writer-preference) pending writer |
| "`lock_guard` on a `shared_mutex` takes a read lock" | It takes the **exclusive** lock; use `shared_lock` for reads |
| "you can upgrade a read lock to a write lock" | Not supported — drop and re-acquire, re-validate |
| "it's the go-to for config that's read every tick" | Snapshot + atomic pointer swap is — zero reader locking |

---

## Exercises

1. **Helps or not:** (a) a config map read once per request by 16 threads, updated
   hourly; (b) a per-tick counter read+written by 8 threads; (c) a 50k-row
   analytics table scanned by a reporting thread, appended-to daily.

   <details><summary>Answer</summary>

   (a) `shared_mutex` *could* help, but a snapshot + atomic pointer is better
   (zero reader locking). (b) No — write-heavy and tiny; `std::atomic` or shard.
   (c) Yes — long reads, rare writes, one reader thread anyway; `shared_mutex`
   (or a snapshot) is fine.
   </details>

2. **Cost:** why is `std::shared_lock` acquisition more expensive than
   `std::mutex::lock` for an uncontended lock?

   <details><summary>Answer</summary>

   `std::mutex` uncontended = one atomic CAS (0→1). `shared_mutex` must
   atomically increment a reader count while checking no writer holds/waits —
   typically a cmpxchg *loop* on a wider state word, plus more branches. And that
   word is then written on unlock too.
   </details>

3. **Snapshot pattern:** write the reader and writer for `std::atomic<std::shared_ptr
   <const std::vector<Rule>>> g_rules;`.

   <details><summary>Answer</summary>

   ```cpp
   // reader
   auto rules = g_rules.load();          // atomic load + refcount bump
   apply(*rules);
   // writer (rare)
   auto next = std::make_shared<std::vector<Rule>>(*g_rules.load());  // copy
   next->push_back(newRule);
   g_rules.store(std::move(next));       // publish
   ```
   Readers never block; the old vector is freed when the last reader's `shared_ptr`
   dies.
   </details>

4. **Starvation:** with reader-preference, describe how a writer can wait forever,
   and one mitigation.

   <details><summary>Answer</summary>

   As long as at least one reader always holds the shared lock (a new reader
   arrives before the last one leaves), the writer's `lock()` never sees reader
   count hit 0. Mitigation: writer-preference policy (new readers block once a
   writer waits) — but that spikes reader latency. Better: batch writes, or use a
   snapshot so the writer never needs exclusive access to the read path.
   </details>

5. **Upgrade:** you hold a `shared_lock`, discover you need to modify. Why can't
   you just "upgrade," and what do you do?

   <details><summary>Answer</summary>

   Upgrading isn't supported because two readers both trying to upgrade would each
   wait for the other to release → deadlock. You must release the `shared_lock`,
   acquire a `unique_lock`, and then **re-read** the state you based your decision
   on (another writer may have changed it in the gap).
   </details>

---

## Interview questions

1. `std::shared_mutex` — shared vs exclusive mode, kaunsa guard.
2. Kab faayda hai, kab nahi (critical section size, write frequency)?
3. `shared_mutex` acquire cost `std::mutex` se zyada kyun?
4. "N readers → N× throughput" kyun galat (reader-count cache bouncing)?
5. Writer starvation — kaise hota, fairness policy implementation-defined kyun matter?
6. Immutable snapshot + atomic pointer swap — readers ko lock se kaise bachata?
7. Shared lock ko unique mein "upgrade" kyun nahi kar sakte?

---

## Next
→ [`11-condition-variables.md`](11-condition-variables.md)
