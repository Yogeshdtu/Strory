# 09 — Layer 8: atomics, memory ordering, happens-before

## Prerequisites
Folders `27-ATOMICS-MEMORY-MODEL`, `28-LOCK-FREE`. This separates
"knows threads" from "knows the C++ memory model" — a real HFT filter.

---

## A — Atomics fundamentals

### A1. `std::atomic<int>` — kya guarantee karta jo `volatile int` nahi?
<details><summary>Answer</summary>
`atomic` — (1) operations **indivisible** (no torn reads/writes), (2)
**memory ordering** control (fences / visibility to other threads), (3)
RMW ops (`fetch_add`, `compare_exchange`) atomic. `volatile` sirf
"compiler mat optimize away / reorder w.r.t. other volatiles" — for MMIO,
signal handlers, `setjmp`. `volatile` gives **no** atomicity and **no**
cross-thread ordering. Never use `volatile` for threading. (`27/`.)
</details>

### A2. `is_lock_free()` — kya batata, aur kab false?
<details><summary>Answer</summary>
Batata ki us type ke atomic ops hardware instructions se hote hain (no
internal mutex). `atomic<int>` / `atomic<void*>` almost always lock-free.
Bade types (`atomic<BigStruct>`, `atomic<double>` on some ABIs,
128-bit without `cmpxchg16b`) → compiler ek hidden lock use kar sakta →
`is_lock_free()` false, and then it's not usable in a signal handler /
truly wait-free path. `atomic_ref` bhi. (`27/`.)
</details>

### A3. `compare_exchange_weak` vs `_strong`?
<details><summary>Answer</summary>
Dono: "agar `*this == expected`, `*this = desired` aur return true; warna
`expected = *this`, return false." **weak** spuriously fail kar sakta
(even when equal) — cheaper on LL/SC architectures (ARM, POWER); use in a
**loop** (you're retrying anyway). **strong** never spuriously fails —
use when you're not looping. x86 pe dono same cost (`cmpxchg`). (`27`,
`28`.)
</details>

### A4. `fetch_add` return kya karta?
<details><summary>Answer</summary>
The **previous** value (atomic post-increment style). `x.fetch_add(1)` ≡
`x++` returning the old value. Useful for claiming a unique slot/index.
`x += 1` on an atomic ≡ `fetch_add(1)` but returns the new value.
(`27/`.)
</details>

---

## B — Memory ordering

### B1. The 6 memory orders — name and one-line each.
<details><summary>Answer</summary>
`relaxed` — atomicity only, no ordering. `consume` — (practically
deprecated / treated as acquire) data-dependent ordering. `acquire` — no
reads/writes in this thread can be reordered **before** this load; sees
writes released by another thread. `release` — no reads/writes can be
reordered **after** this store; publishes prior writes. `acq_rel` — both
(for RMW). `seq_cst` — acquire/release **plus** a single total order
across all `seq_cst` ops (default, safest, sometimes a fence cost).
(`27/`.)
</details>

### B2. Acquire-release — the canonical pattern.
<details><summary>Answer</summary>
Producer writes data, then `flag.store(true, release)`. Consumer
`while(!flag.load(acquire)){}` then reads data. The release **publishes**
all writes sequenced before it; the matching acquire load, once it sees
`true`, is guaranteed to see those writes. This is a **happens-before**
edge built without a lock. (`27/`, `41`.)
</details>

### B3. `memory_order_relaxed` — safe use case.
<details><summary>Answer</summary>
Ordering doesn't matter, only atomicity: a **statistics counter**
(`hits.fetch_add(1, relaxed)`), a monotonically-increasing sequence-number
generator, a "stop requested" flag where nothing else is published with
it. **Not** safe for publishing data to another thread (no
happens-before). (`27`, `45` async logger drop-count.)
</details>

### B4. Happens-before — define, aur relaxed atomics isse kyun nahi dete.
<details><summary>Answer</summary>
A happens-before B ⇒ effects of A are visible to B. Built from:
sequenced-before (within a thread) + synchronizes-with (a release store
paired with an acquire load of the same variable that reads that value) +
transitivity + thread start/join. `relaxed` ops are atomic but create
**no** synchronizes-with edge → no happens-before → other writes around
them can be seen out of order / not at all. (`27/`.)
</details>

### B5. `std::atomic_thread_fence` — kab chahiye alag se?
<details><summary>Answer</summary>
When you want the ordering without tying it to a specific atomic
variable's operation — e.g. a `relaxed` store followed by a
`atomic_thread_fence(release)` earlier, or separating the fence from the
flag for performance. Also for the "fence-fence" synchronization pattern.
Rare in application code; seqlocks and some MPMC ring designs use them.
(`27`, `28`.)
</details>

---

## C — Lock-free

### C1. Lock-free vs wait-free vs obstruction-free?
<details><summary>Answer</summary>
**Lock-free** — system-wide progress guaranteed: at least one thread
makes progress in a bounded number of steps (individual threads can
starve). **Wait-free** — **every** thread makes progress in a bounded
number of its own steps (strongest, hardest). **Obstruction-free** —
a thread makes progress if it runs in isolation (weakest). SPSC ring
buffers are effectively wait-free per operation. (`28/`.)
</details>

### C2. ABA problem — kya hai, fix.
<details><summary>Answer</summary>
Thread reads value `A` from a shared pointer, gets preempted; another
thread changes it to `B` then back to `A` (freeing and reallocating the
node). Thread resumes, CAS sees `A`, "succeeds" — but the world changed.
Fixes: **tagged pointers / generation counters** (CAS on `{ptr, tag}`,
tag always increments — `44` `ObjectPool` handles), hazard pointers, RCU,
epoch-based reclamation. (`28/`, `45/12` B6.)
</details>

### C3. SPSC ring buffer — why no CAS needed?
<details><summary>Answer</summary>
Single producer owns the write index, single consumer owns the read
index. Producer does `release` store of the new write index; consumer
does `acquire` load. Each side only *writes* its own index and only
*reads* the other's. No two threads write the same location → plain
atomic load/store with acquire/release is enough, no compare-exchange.
Add `alignas(64)` on the indices (false sharing) + a cached copy of the
opposite index (fewer coherence misses). (`36/15`, `41`, `28`.)
</details>

### C4. Lock-free doesn't mean fast — explain.
<details><summary>Answer</summary>
A contended CAS loop can spin many times, each iteration a
cache-coherence round trip (line bouncing between cores) — sometimes
slower than a well-designed lock. Lock-free's real value is **progress
guarantee** (no thread blocked by a descheduled lock-holder) and
**bounded latency** (no syscall to sleep/wake). Design to reduce
contention (sharding, SPSC) first; lock-free MPMC only when truly needed.
(`28`, `41/`, `43`.)
</details>

### C5. `memory_order` for a spinlock's `lock` and `unlock`?
<details><summary>Answer</summary>
`lock`: `while (flag.exchange(true, acquire)) { /* pause */ }` — **acquire**
so the critical section's reads/writes can't hoist above the lock.
`unlock`: `flag.store(false, release)` — **release** so the critical
section's writes are published before the lock is seen free. `acquire`
on lock, `release` on unlock — the same pairing as any producer/consumer.
(`27`, `28`.)
</details>

---

## D — HFT-flavoured

### D1. Seqlock — kaise kaam karta, kab use.
<details><summary>Answer</summary>
Single writer, many readers, read-mostly data. A sequence counter:
writer does `seq++` (now odd) → write data → `seq++` (now even). Reader:
read `seq` (retry if odd), read data, re-read `seq` — if unchanged and
even, the read was consistent; else retry. Readers **never block or
write**, writer never waits. Great for a frequently-published market-data
snapshot. Caveat: readers can spin under heavy write load; data must be
trivially copyable. (~2500–3500× vs `shared_mutex` under contention,
`41`.) (`27`, `28`.)
</details>

### D2. False sharing — memory-model angle.
<details><summary>Answer</summary>
Two atomics (or an atomic and other hot data) on the **same 64-byte cache
line**, written by different cores → every write invalidates the other
core's copy → the line ping-pongs (coherence traffic), 10–100× slower on
that access even though the program is correct. `perf c2c` shows HITM.
Fix: `alignas(64)` + padding so each hot variable owns its line
(`43/08`, `45/12` B9).
</details>

### D3. Why is `seq_cst` sometimes avoided in HFT?
<details><summary>Answer</summary>
`seq_cst` may emit a full memory barrier (`mfence` / `lock`-prefixed op /
`dmb ish`) to maintain the single total order — extra cycles on the hot
path. If acquire/release semantics are sufficient for correctness (they
usually are for a specific producer/consumer pairing), use them. But:
**profile and prove** the ordering is correct — a wrong weakening is a
Heisenbug. Default to `seq_cst`, weaken deliberately with a comment
explaining the happens-before argument. (`27`, `43/16`.)
</details>

### D4. `std::atomic<double>` for a price — issues?
<details><summary>Answer</summary>
(1) May not be lock-free on all ABIs. (2) `fetch_add` on floating types
is C++20 only. (3) Float comparison / accumulation error is still a
problem (`45/12` C7). HFT stores prices as **integer ticks**
(`atomic<int64_t>`) — lock-free, exact, and the fixed-point discussion
from `43/09` applies.
</details>

---

## Interview tips for Layer 8

- If you can explain **acquire-release + happens-before** with the
  producer-writes-data-then-flag example, you're ahead of most
  candidates.
- `volatile` ≠ atomic — say this unprompted if `volatile` comes up.
- `relaxed` is for counters/flags with nothing else published alongside.
- Lock-free: know it's a **progress guarantee**, not a speed guarantee;
  contended CAS can be slower than a lock.
- SPSC ring: "no CAS because each side owns one index; acquire/release on
  the two indices; `alignas(64)` + cached opposite index."
- Seqlock is the HFT power move for read-mostly single-writer data.

## Next
→ [`10-linux-questions.md`](10-linux-questions.md)
