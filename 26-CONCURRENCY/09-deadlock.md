# 09 — Deadlock

## Prerequisites
- `07-mutex.md`, `08-lock-guards.md`
- [`examples/04_deadlock.cpp`](examples/04_deadlock.cpp)

## Yeh topic abhi kyun
Deadlock = do (ya zyada) threads hamesha ke liye ek doosre ka wait kar rahe hain,
koi aage nahi badh sakta. Program hang. Yeh concurrent code ka doosra bada bug
(race ke baad). Achhi khabar: iske conditions well-defined hain, aur usme se koi
**ek** todo to deadlock namumkin.

---

## The classic: opposite lock order

```cpp
std::mutex A, B;

// Thread 1                     // Thread 2
lock(A);                        lock(B);
lock(B);   // waits for T2      lock(A);   // waits for T1
   ...                             ...
```

Timeline:
```
T1: lock A ✓ ... lock B  → BLOCKED (T2 holds B)
T2: lock B ✓ ... lock A  → BLOCKED (T1 holds A)
```
Neither can proceed. **Deadlock.**

`examples/04` demonstrates this with `std::timed_mutex` + `try_lock_for` so it
*detects* the impasse and backs off instead of hanging (10 near-deadlocks per
run).

---

## The 4 Coffman conditions — all must hold

| # | Condition | Meaning |
|---|---|---|
| 1 | **Mutual exclusion** | a resource is held exclusively (a mutex) |
| 2 | **Hold and wait** | a thread holds one resource while waiting for another |
| 3 | **No preemption** | a resource can't be forcibly taken from a thread |
| 4 | **Circular wait** | a cycle: T1 waits for T2's resource, T2 waits for T1's |

**Break any one → no deadlock possible.**

---

## Fixes

### Fix 1 — consistent lock order (break circular wait) — most common

Every thread that needs both `A` and `B` locks them in the **same order**
(e.g. by address, or by a documented hierarchy).

```cpp
void transfer(Account& from, Account& to) {
    std::mutex* first  = &from.m_ < &to.m_ ? &from.m_ : &to.m_;   // lower address first
    std::mutex* second = &from.m_ < &to.m_ ? &to.m_   : &from.m_;
    std::lock_guard l1(*first);
    std::lock_guard l2(*second);
    ...
}
```

`examples/04` "FIX A": everyone locks `fx1` then `fx2` — no cycle possible.

### Fix 2 — `std::scoped_lock` / `std::lock` (break hold-and-wait)

Acquire **all** the mutexes **at once** with a deadlock-avoidance algorithm (try
one, try the rest, on failure release all and retry). No global order needed.

```cpp
std::scoped_lock lk(from.m_, to.m_);   // C++17 — deadlock-free, order-independent
```

`examples/04` "FIX B".

### Fix 3 — lock hierarchy (enforced ordering)

A `HierarchicalMutex` that records a "level" and refuses (throws) if you try to
lock a higher-or-equal level than one you already hold — turns a lock-order bug
into an immediate exception instead of a hang.

### Fix 4 — `try_lock` + back-off (break hold-and-wait, at a cost)

```cpp
for (;;) {
    std::unique_lock l1(A);
    std::unique_lock l2(B, std::try_to_lock);
    if (l2.owns_lock()) break;          // got both
    l1.unlock();                         // release A, yield, retry
    std::this_thread::yield();
}
```
Risk: **livelock** (two threads politely backing off forever) — add jitter/backoff.

### Fix 5 — don't take two locks (best)

Restructure so only one lock is ever held, or use a single lock for the combined
state, or message-passing (no shared locks at all).

---

## Other deadlock shapes

- **Self-deadlock** — a non-recursive mutex locked twice on one thread (file 07
  trap 2): `f()` locks `m_`, calls `g()`, `g()` locks `m_`.
- **Lock + condition variable misuse** — `cv.wait(lk)` without the predicate, or
  notifying without holding the lock during the state change → missed wakeup (a
  "logical" deadlock — file 11).
- **Lock + join** — Thread 1 holds `m_` and `join()`s Thread 2; Thread 2 needs
  `m_` to finish. Don't `join()` while holding a lock the joinee needs.
- **Lock + blocking I/O** — hold a lock, block on a socket read that never
  completes → everyone waiting on that lock is stuck too (file 06).
- **ABBA across subsystems** — subsystem X locks its mutex then calls into Y which
  locks Y's; elsewhere Y calls into X. Document a cross-subsystem lock order or
  don't call across while holding a lock.

---

## Detecting deadlocks

| Tool | How |
|---|---|
| **It hangs** | The crudest signal — attach `gdb`, `thread apply all bt`, see two threads blocked in `__lll_lock_wait` on different mutexes |
| **`std::timed_mutex` + `try_lock_for`** | Turn a hang into a logged "couldn't acquire in N ms" (`examples/04`) — good for detecting in tests/CI |
| **ThreadSanitizer** | `-fsanitize=thread` detects lock-order inversions (potential deadlocks) even if they didn't actually deadlock this run |
| **Helgrind** (Valgrind) | Lock-order-violation detection |
| **A lock hierarchy** | Converts a violation into an immediate throw with a stack trace |

---

## > **HFT relevance**
> - **The hot path takes zero locks → cannot deadlock.** Data flows over lock-free
>   queues (folder 28). This isn't just for speed — it removes the entire deadlock
>   risk class from the latency-critical code.
> - **On the control plane, where locks exist:** one documented global lock order
>   (by subsystem, enforced with a lock hierarchy in debug builds), or
>   `std::scoped_lock` for any multi-lock operation. Code review checks: "does any
>   path lock two mutexes, and in a consistent order?"
> - **Never call across a subsystem boundary while holding a lock** — release
>   first, or use a message. Cross-subsystem ABBA is the deadlock that shows up
>   in production at 3am.
> - **TSan in CI** flags lock-order inversions before they hang a live system.
> - **`try_lock_for` + fallback** on any hot-ish path that must touch a shared
>   resource but must never block forever (`examples/04`).

---

## Hands-on

```bash
./build.ps1 26-CONCURRENCY/examples/04_deadlock.cpp
```

Shows the opposite-order pattern (detected via timeouts), consistent-order fix,
and `std::scoped_lock` fix. Then:
- Replace `std::timed_mutex` + `try_lock_for` with plain `std::mutex` + `lock()`
  → the program **hangs** → `Ctrl-C`, then attach `gdb -p <pid>` and
  `thread apply all bt` to see the two blocked threads.
- Add a self-deadlock: a function holding `mA` that calls another function which
  also locks `mA`.

---

## ⚠️ Traps

### Trap 1 — locking two mutexes in different orders on different paths
The ABBA deadlock. Consistent order, or `std::scoped_lock`.

### Trap 2 — self-deadlock via a helper
`outer()` locks `m_`, calls `inner()` which locks `m_` → deadlock (non-recursive).
Factor so the lock is taken once.

### Trap 3 — `join()` / blocking call while holding a lock
The thing you're waiting on may need that lock. Release before waiting.

### Trap 4 — `try_lock` back-off without jitter → livelock
Two threads back off in lock-step forever. Randomized backoff.

### Trap 5 — calling a user callback while holding a lock
The callback might re-enter your API and try to lock the same mutex → deadlock,
or lock a different one → ABBA. Call callbacks with locks released.

### Trap 6 — `std::lock(a, b)` then `std::lock_guard(a)` without `adopt_lock`
Locks `a` twice → self-deadlock. `std::adopt_lock` or `std::scoped_lock`.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "deadlock is random / hard to prevent" | 4 well-defined conditions; break any one (usually: consistent order) |
| "`scoped_lock` just locks multiple mutexes" | It also uses a deadlock-avoidance ordering algorithm |
| "self-deadlock needs two threads" | One thread re-locking a non-recursive mutex deadlocks itself |
| "`try_lock` + retry can't deadlock, so it's safe" | It can **livelock** — add backoff/jitter |
| "holding a lock during a callback is fine" | The callback can re-enter and deadlock |
| "the hot path uses fine-grained locks carefully" | The hot path should use **no** locks — lock-free queues |

---

## Exercises

1. **Which Coffman condition** does each fix break: (a) consistent lock order,
   (b) `std::scoped_lock`, (c) `try_lock` + backoff, (d) a single combined lock?

   <details><summary>Answer</summary>

   (a) circular wait. (b) hold-and-wait (acquires all at once). (c) hold-and-wait
   (releases on failure). (d) circular wait *and* hold-and-wait (only one lock —
   no cycle, no waiting-while-holding-another).
   </details>

2. **Fix the ABBA:**
   ```cpp
   void move(Node& a, Node& b) {
       std::lock_guard la(a.m);
       std::lock_guard lb(b.m);
       ...
   }
   ```
   Two threads call `move(x, y)` and `move(y, x)`.

   <details><summary>Answer</summary>

   `std::scoped_lock lk(a.m, b.m);` — locks both deadlock-free. Or order by
   address: lock `&a.m < &b.m ? a.m : b.m` first, then the other.
   </details>

3. **Self-deadlock:** `void Cache::put(K k, V v){ std::lock_guard lk(m_); evict_if_full();
   map_[k]=v; } void Cache::evict_if_full(){ if (map_.size() > cap_) { std::lock_guard
   lk(m_); map_.erase(...); } }` — bug and fix.

   <details><summary>Answer</summary>

   `put()` holds `m_`, calls `evict_if_full()` which locks `m_` again on the same
   thread → deadlock (`std::mutex` non-recursive). Fix: `evict_if_full()` assumes
   the lock is already held (document it, don't lock inside), or factor the
   erase into a `_locked` helper that both call while holding the lock once.
   </details>

4. **Callback deadlock:** `void Bus::publish(const Msg& m){ std::lock_guard lk(m_);
   for (auto& sub : subs_) sub(m); }` — a subscriber calls `bus.subscribe(...)`.
   What happens, fix?

   <details><summary>Answer</summary>

   `subscribe()` tries to lock `m_`, which `publish()` holds on the same thread →
   deadlock. Fix: copy `subs_` under the lock, release the lock, then invoke the
   callbacks: `std::vector<Fn> local; { std::lock_guard lk(m_); local = subs_; }
   for (auto& f : local) f(m);`.
   </details>

5. **Detect in CI:** you can't reliably reproduce a suspected deadlock. Two
   approaches to surface it.

   <details><summary>Answer</summary>

   (1) Run the concurrency suite under `-fsanitize=thread` — TSan flags lock-order
   inversions even when they don't actually deadlock. (2) Add a debug-build
   `HierarchicalMutex` (or wrap `std::mutex`) that records held-lock levels/order
   per thread and `abort()`s with a stack trace on a violation. Also: watchdog
   threads that `abort()` if a critical section is held longer than a threshold.
   </details>

---

## Interview questions

1. Deadlock kya hai — ek minimal 2-thread 2-mutex example.
2. Coffman ki 4 conditions — sab zaroori kyun?
3. Har condition ko todne ka ek fix.
4. `std::scoped_lock` deadlock se kaise bachata (algorithm)?
5. Self-deadlock — ek thread, ek non-recursive mutex — kaise?
6. `try_lock` + backoff ka risk (livelock)?
7. Lock holding + callback / `join` / blocking I/O — kyun khatarnak?
8. Deadlock detect karne ke tools (gdb bt, TSan, timed_mutex, lock hierarchy).

---

## Next
→ [`10-shared-mutex.md`](10-shared-mutex.md)
