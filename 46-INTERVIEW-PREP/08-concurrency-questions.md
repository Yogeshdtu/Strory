# 08 — Layer 7: threads, mutexes, condition variables, deadlock

## Prerequisites
Folder `26-CONCURRENCY`, `45/08`. HFT is mostly shared-nothing, but you
must be fluent in the primitives and their failure modes.

---

## A — Threads & basics

### A1. `std::thread` — join vs detach, aur na karo to?
<details><summary>Answer</summary>
`join()` — caller blocks until thread finishes. `detach()` — thread runs
independently, no handle. Agar `std::thread` object destruct ho **without**
join/detach → `std::terminate()`. So: hamesha join ya detach (RAII wrapper
like `std::jthread` C++20 jo dtor pe join karta, ya a scope guard).
(`26/03`.)
</details>

### A2. `std::jthread` `std::thread` se kaise better?
<details><summary>Answer</summary>
(1) Destructor **automatically joins** (no `terminate` footgun). (2)
Built-in **`std::stop_token`** — cooperative cancellation
(`request_stop()`, thread checks `stop_requested()`). Modern default.
(`26/`.)
</details>

### A3. Race condition kya hai — precise definition.
<details><summary>Answer</summary>
Do (ya zyada) threads same memory location ko access karte, at least ek
**write** hai, aur unke beech koi **happens-before** relationship nahi
(no synchronization ordering them). Result: undefined behaviour (C++
data race). Symptom: non-deterministic wrong values, load/thread-count
dependent. (`27/01`, `45/12` B1.)
</details>

### A4. Race condition vs data race — same cheez?
<details><summary>Answer</summary>
Related but not identical. **Data race** = the C++ UB above (unsynchronized
conflicting access). **Race condition** = broader: a correctness bug from
timing/ordering even *with* synchronization — e.g. two threads each doing
`if (!exists(k)) insert(k)` under separate locks, or a check-then-act
(TOCTOU). All data races are race conditions; not vice versa. (`26/05`.)
</details>

---

## B — Mutexes & locks

### B1. `std::mutex`, `lock_guard`, `unique_lock`, `scoped_lock` — kab
kaunsa?
<details><summary>Answer</summary>
`mutex` — the lock itself. `lock_guard<mutex>` — simplest RAII, lock in
ctor, unlock in dtor, no unlock/relock. `unique_lock` — RAII + can
unlock/relock, deferred lock, timed lock, move — needed with
`condition_variable`. `scoped_lock(m1, m2, ...)` — locks **multiple**
mutexes deadlock-free (uses `std::lock`'s avoidance algorithm). Default:
`lock_guard` (or `scoped_lock` for one too). (`26/06`.)
</details>

### B2. Deadlock — Coffman's 4 conditions, aur break kaise.
<details><summary>Answer</summary>
(1) Mutual exclusion, (2) hold-and-wait, (3) no preemption, (4) circular
wait. **Break any one:** consistent **global lock order** (breaks
circular wait — most common fix); `std::scoped_lock`/`std::lock` for
multi-lock; lock hierarchy with assertions; `try_lock` + backoff
(livelock risk); or just don't hold two locks. (`26/09`, `45/08`.)
</details>

### B3. gdb se deadlock kaise diagnose?
<details><summary>Answer</summary>
`gdb -p <pid>` → `thread apply all bt`. Signature: ≥2 threads in
`__lll_lock_wait` / `pthread_mutex_lock`, and their stacks show each
holding a lock the other wants. `print *(pthread_mutex_t*)addr` →
`__owner` field tells which thread holds it. One thread waiting, rest
idle → missing unlock (early return / exception between manual lock/
unlock). (`45/08`.)
</details>

### B4. `std::recursive_mutex` — kab, aur kyun aksar code smell?
<details><summary>Answer</summary>
Same thread ko mutex baar-baar lock karne deta (count-based). Use case:
ek locked public method jo dusre locked public method ko call kare.
**Smell** kyunki: aksar iska matlab locking design confused hai (kaunsa
layer lock leta unclear), invariants across the "recursive" call hold
karna mushkil, aur costlier than a plain mutex. Better: private
unlocked helpers, public methods lock once. (`26/`.)
</details>

### B5. Reader-writer lock (`std::shared_mutex`) — trade-off.
<details><summary>Answer</summary>
Multiple concurrent readers **or** one writer. Good when reads
vastly dominate and the critical section is long. But: acquire is
**2–5× costlier** than a plain mutex (more state), writer starvation
possible, and for short critical sections a plain mutex or an atomic /
seqlock wins. HFT: often a **seqlock** for read-mostly single-writer
data (readers never block, ~2500–3500× faster than `shared_mutex` under
contention — `41`). (`26/`, `27`.)
</details>

### B6. Spinlock vs mutex — kab spinlock?
<details><summary>Answer</summary>
Spinlock — busy-wait (no syscall), good when critical section is **very
short** and contention low and you're **pinned** (won't be descheduled
holding it). Mutex — sleeps the thread (syscall, ~1 µs to wake), good
when the section is long or the holder may block. HFT hot path with
pinned cores + tiny sections → spinlock (or lock-free). A spinlock that
gets preempted while held is a disaster (other cores spin for a whole
timeslice). (`26/`, `28`.)
</details>

---

## C — Condition variables

### C1. `condition_variable::wait` ko **hamesha** predicate ke saath kyun?
<details><summary>Answer</summary>
`cv.wait(lk, [&]{ return ready; })` — loop-checks the predicate. Handles:
(1) **spurious wakeups** (wait can return without a notify), (2)
**notify-before-wait** (notification lost if you weren't waiting yet — the
predicate catches up), (3) **stolen wakeups** (another thread consumed the
condition). Bare `cv.wait(lk)` is almost always a bug. (`26/`, `45/12` B5.)
</details>

### C2. `notify_one` vs `notify_all`?
<details><summary>Answer</summary>
`notify_one` — wakes one waiting thread (cheaper; use when any one waiter
can make progress and they're interchangeable — e.g. a task queue).
`notify_all` — wakes all (use when the state change is relevant to all,
or waiters have different predicates). Wrong choice → lost wakeup or
thundering herd. (`26/`.)
</details>

### C3. Lock ko notify se pehle ya baad release karna?
<details><summary>Answer</summary>
Predicate ko lock ke andar modify karo. `notify_*` lock ke andar ya
turant baad — dono valid; releasing **before** notify can avoid the woken
thread immediately blocking on the still-held lock ("hurry up and wait"),
a minor optimization. The critical rule: modify the shared state **under
the lock**, then notify. (`26/`.)
</details>

### C4. Producer-consumer with a bounded queue — cv setup?
<details><summary>Answer</summary>
Two condition variables (`not_full`, `not_empty`) + one mutex. Producer:
`wait(lk, [&]{ return q.size() < cap || done; })`, push, `not_empty.notify_one()`.
Consumer: `wait(lk, [&]{ return !q.empty() || done; })`, pop,
`not_full.notify_one()`. For HFT the lock-based version is replaced by a
lock-free SPSC ring (`36/15`, `41`) — but know the cv version cold.
</details>

---

## D — HFT concurrency posture

### D1. HFT systems concurrency ko kaise handle karte — high level?
<details><summary>Answer</summary>
**Shared-nothing / message-passing.** Each stage (feed decode, book,
strategy, risk, order gateway) runs on its **own pinned core**, connected
by **lock-free SPSC queues**. No shared mutable state → no mutexes on the
hot path → no lock contention, no priority inversion, deterministic
latency. Cross-thread "return to owner" for freeing. (`41`, `36/19`, `44`.)
</details>

### D2. Priority inversion — kya hai, HFT ka structural fix?
<details><summary>Answer</summary>
Low-priority thread holds a lock, high-priority thread blocks on it, a
medium-priority thread preempts the low one → high effectively waits on
medium. Classic fixes: priority inheritance / ceiling
(`PTHREAD_PRIO_INHERIT`, `SCHED_FIFO`). **HFT structural fix:** no shared
lock to invert over — shared-nothing design (the async logger, the SPSC
hand-off) is priority-inversion-safe **by construction**. (`41/11`,
`29`.)
</details>

### D3. `std::atomic<bool> flag` as a thread stop signal — enough?
<details><summary>Answer</summary>
For a simple "please stop" flag, yes — `flag.store(true, release)` /
`flag.load(acquire)` (or even relaxed if no other data is published
alongside). But it's **not** a substitute for a mutex/cv when you need to
*wait* (spinning on it burns a core) or when you're publishing more than
the flag. `std::stop_token` / `std::jthread` is the idiomatic modern
form. (`27`, `26`.)
</details>

### D4. `thread_local` — cost and use in HFT.
<details><summary>Answer</summary>
Per-thread storage, initialized on first use per thread. Access has a
small indirection (TLS model dependent — `initial-exec` is fastest,
needs the variable in the main executable / a preloaded lib). Use: per-
thread scratch buffers, per-thread stats counters (summed later — a
sharding pattern that removes contention). Avoid in the very hottest
inner loop if the TLS model forces a function call. (`26`, `41`.)
</details>

---

## Interview tips for Layer 7

- Deadlock: Coffman's 4 + "break circular wait with a global lock order,
  or `std::scoped_lock` for multi-acquire" — plus the gdb signature.
- `cv.wait` **always** with a predicate — and be able to say *why*
  (spurious + lost + stolen wakeups).
- The strong HFT answer to "how do you do concurrency": "shared-nothing,
  pinned cores, lock-free SPSC queues between stages — no mutex on the
  hot path."
- `recursive_mutex` and over-broad `shared_mutex` = design smells; say
  so.

## Next
→ [`09-memory-model-questions.md`](09-memory-model-questions.md)
