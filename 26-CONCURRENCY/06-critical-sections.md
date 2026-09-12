# 06 — Critical sections & mutual exclusion

## Prerequisites
- `05-race-conditions.md`

## Yeh topic abhi kyun
Data race ka core reason: do threads ek **shared invariant** ko ek saath modify
karte hue usse temporarily-inconsistent chhod dete hain. **Critical section** =
code ka woh region jo ek waqt mein sirf ek thread execute kare. **Mutual
exclusion** = usse enforce karne ka mechanism. Yeh ek chhota conceptual file hai —
mechanisms (mutex, lock guards) agli files mein.

---

## Critical section

> **Critical section** = code ka ek block jo ek shared resource ko access/modify
> karta hai, aur jise ek waqt mein sirf **ek** thread run kare — warna invariant
> toot jaata.

```cpp
// shared: balance_, ledger_ — ek saath consistent rehne chahiye
void Account::apply(const Txn& t) {
    // ---- critical section START ----
    balance_ += t.amount;
    ledger_.push_back(t);
    // ---- critical section END ----
}
```

Agar Thread A `balance_` update karke `ledger_` update karne se pehle ruk jaaye,
aur Thread B `read()` kare → B ko `balance_` naya par `ledger_` purana dikhega —
**broken invariant**. Critical section ka kaam: is window ko atomic-feel dena.

---

## Mutual exclusion — 4 properties a solution needs

1. **Mutual exclusion** — ek waqt mein at most ek thread critical section mein.
2. **Progress** — agar koi thread CS mein nahi hai aur koi enter karna chahta hai,
   selection deferred nahi hona chahiye indefinitely.
3. **Bounded waiting / no starvation** — ek thread ko forever wait nahi karna
   chahiye (fairness).
4. **No assumptions about speed / number of CPUs** — solution correct rahe kisi
   bhi timing pe.

Historical solutions (Peterson's algorithm, Dekker's, Lamport's bakery) — pure
software, do flags + a turn variable. Modern reality: **hardware atomic
instructions** (`lock xchg`, `lock cmpxchg`, LL/SC) → `std::atomic`, `std::mutex`.

---

## Critical section kaise chhota rakhein

**Rule: critical section ke andar sirf shared-state ka access. Baaki sab bahar.**

```cpp
// ❌ CS bada — I/O, allocation, computation sab lock ke andar
void bad(const Request& r) {
    std::lock_guard lk(m_);
    auto parsed = parse(r);          // CPU work — lock held
    log_to_disk(parsed);             // I/O — lock held (!!)
    shared_map_[parsed.key] = parsed.value;
}

// ✅ CS chhota — sirf shared write
void good(const Request& r) {
    auto parsed = parse(r);          // outside
    auto entry  = build_entry(parsed);
    {
        std::lock_guard lk(m_);
        shared_map_[entry.key] = entry.value;   // only the shared bit
    }
    log_to_disk(entry);              // outside
}
```

Lock ke andar I/O / syscall / allocation / long computation = **contention** —
baaki threads wait karte, throughput girta, latency spike. HFT mein ek lock jo
disk pe likhe wale thread ke peeche block ho jaaye = a millisecond stall.

---

## Serialization cost

Ek critical section poore parallel program ka **serial fraction** badhata hai
(Amdahl — file 01). Agar 8 threads ek CS ke through jaate hain jo har call mein
100 ns lagta, to woh 800 ns/round-of-8 = a serial bottleneck jo scaling cap
karta.

`examples/03`: 16M locked `++counter` = 1248 ms (mutex, fully serialized). Same
work with no shared write (per-thread + combine) = **1.9 ms**. The critical
section *was* the program.

---

## > **HFT relevance**
> - **The hot path has no critical sections** — it shares no mutable state with
>   other threads. Data flows in/out over lock-free SPSC queues (folder 28).
> - **Where a lock is unavoidable** (a control-plane update, a rarely-touched
>   table): keep the critical section to a handful of instructions — build
>   everything outside, `swap`/assign a pointer inside, do I/O/logging outside.
> - **Prefer "publish an immutable snapshot"** — the writer builds a new `const`
>   object off to the side and atomically swaps a pointer; readers never enter a
>   critical section at all (folder 28).
> - **Contention is jitter** — a lock that occasionally waits behind a slow holder
>   is a tail-latency spike. Measure lock hold time and wait time; if either is
>   non-trivial on a hot path, redesign.

---

## Hands-on

```bash
./build.ps1 fast 26-CONCURRENCY/examples/03_mutex_fix.cpp   # CS = the whole program (1248 ms)
```

Take `examples/03` (a) and move a tiny bit of "work" (a multiply, a `sleep_for(0)`)
*inside* the `lock_guard` scope — watch the time balloon as the critical section
grows. Then shrink it back.

---

## ⚠️ Traps

### Trap 1 — I/O or syscall inside the critical section
```cpp
{ std::lock_guard lk(m_); fprintf(log, "...\n"); update(shared_); }   // ⚠️ log I/O under lock
```
Do the I/O outside; only the shared write is critical.

### Trap 2 — allocation inside the critical section
`shared_vec_.push_back(x)` can allocate (realloc) under the lock. `reserve` up
front, or build outside and `swap` in.

### Trap 3 — long computation inside
`{ lk; result = expensive(shared_input_); shared_out_ = result; }` — compute into
a local outside, then a tiny locked assignment.

### Trap 4 — one giant lock for unrelated state
Coarse-grained locking serializes things that don't conflict. Shard the state /
one lock per independent invariant (file 07, 09 — beware deadlock).

### Trap 5 — forgetting a code path that also touches the shared state
*Every* access to the shared invariant must be inside a critical section with the
*same* lock. One unlocked read = a data race.

### Trap 6 — a "critical section" that isn't actually mutually exclusive
Two different mutexes protecting the same data → no mutual exclusion. One lock per
invariant, used everywhere.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "just wrap the whole function in a lock" | Only the shared-state access is critical; keep it minimal |
| "I/O under a lock is fine, it's rare" | It blocks every waiter for the I/O duration — a stall |
| "one big lock is simpler and correct" | Correct but serializes unrelated work; hurts scaling |
| "reads outside the lock are okay if writes are locked" | A read racing a write is UB — protect all access |
| "the critical section is small, contention is fine" | Even 100 ns × N threads is a serial bottleneck (Amdahl) |
| "two mutexes on the same data = extra safety" | No mutual exclusion at all — must be the *same* lock |

---

## Exercises

1. **Shrink it:** rewrite so only the shared write is in the critical section.
   ```cpp
   void add(const Order& o) {
       std::lock_guard lk(m_);
       auto v = validate(o);          // pure, no shared state
       auto id = next_id_++;          // shared
       book_[id] = build_level(v);    // build_level pure; book_ shared
       audit_.write(id, v);           // I/O
   }
   ```

   <details><summary>Answer</summary>

   ```cpp
   void add(const Order& o) {
       auto v = validate(o);
       auto level = build_level(v);
       long id;
       { std::lock_guard lk(m_); id = next_id_++; book_[std::move(id)] = std::move(level); }
       audit_.write(id, v);
   }
   ```
   Only `next_id_++` and the `book_` insert are locked; validation, `build_level`,
   and the audit I/O are outside. (Better still: pre-`reserve` `book_` so the
   insert doesn't allocate under the lock.)
   </details>

2. **Serial fraction:** 8 threads, each does 1 µs of independent work then a
   critical section that takes 200 ns. What's the max throughput vs 1 thread?

   <details><summary>Answer</summary>

   Per thread per iteration: 1 µs parallel + 0.2 µs serialized. With 8 threads,
   the serial part is 8 × 0.2 = 1.6 µs per "round." So a round takes
   `max(1 µs, 1.6 µs) = 1.6 µs` for 8 units of work → ~5× a single thread's 1 µs
   for 1 unit... roughly 5× speedup, not 8×. The 200 ns CS caps it.
   </details>

3. **Missing path:** a `Stats` struct is updated under `m_` in `record()` but read
   without a lock in `snapshot()`. What's wrong and the two fixes.

   <details><summary>Answer</summary>

   `snapshot()`'s unlocked read races with `record()`'s write → data race / UB
   (torn values). Fix: (1) take `m_` in `snapshot()` too (copy out under the lock).
   (2) Make `Stats` a set of `std::atomic` fields, or publish an immutable snapshot
   pointer that `snapshot()` loads atomically.
   </details>

4. **Coarse vs fine:** one `std::mutex` protects both `positions_` and
   `market_data_cache_`, which are never used together. Cost, and the fix.

   <details><summary>Answer</summary>

   Threads updating `positions_` block threads updating `market_data_cache_` even
   though they don't conflict — false serialization. Fix: one mutex per
   independent invariant (`pos_m_`, `md_m_`). If some operation *does* touch both,
   establish a fixed lock order (file 09) to avoid deadlock.
   </details>

5. **Snapshot pattern:** sketch how to let many readers see a config with zero
   locking while one writer updates it.

   <details><summary>Answer</summary>

   Store `std::shared_ptr<const Config>` in a `std::atomic<std::shared_ptr<const
   Config>>` (C++20) — or an `std::atomic<const Config*>` with careful reclamation.
   Readers `load()` the current pointer and use it (it stays valid via the
   shared_ptr refcount). The writer builds a brand-new `Config`, then `store()`s
   the new pointer. No reader ever waits; no reader sees a half-updated `Config`.
   </details>

---

## Interview questions

1. Critical section kya hai — kis problem ko solve karta?
2. Mutual exclusion solution ke 4 required properties.
3. Critical section chhota kyun rakhein — I/O/alloc/compute lock ke andar ka cost?
4. Critical section aur Amdahl's law ka rishta.
5. Do alag mutex ek hi data pe — kya galat?
6. "Publish an immutable snapshot" — readers ko lock se kaise bachata hai?
7. Har shared-state access same lock ke andar kyun zaroori (ek unlocked read = ?)?

---

## Next
→ [`07-mutex.md`](07-mutex.md)
