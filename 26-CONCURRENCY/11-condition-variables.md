# 11 — Condition variables

## Prerequisites
- `08-lock-guards.md` (`std::unique_lock`), `07-mutex.md`
- [`examples/05_condition_variable.cpp`](examples/05_condition_variable.cpp)

## Yeh topic abhi kyun
Ek thread ko doosre thread ke ek **event** ka wait karna hai — "queue mein data
aaya", "kaam khatam", "shutdown". Spin karna (`while (!ready) {}`) CPU jalata hai.
`std::condition_variable` deta hai: **efficiently sleep until notified**. Par
ismein 2 classic bugs hain (spurious wakeup, lost wakeup) jinke liye ek exact
usage pattern hai.

---

## The pattern (memorize this)

```cpp
std::mutex m;
std::condition_variable cv;
bool ready = false;

// ---- waiter ----
{
    std::unique_lock<std::mutex> lk(m);
    cv.wait(lk, [&]{ return ready; });   // predicate form — the ONLY safe form
    // here: ready == true, and lk is held
    consume();
}

// ---- notifier ----
{
    std::lock_guard<std::mutex> lk(m);   // hold the lock while CHANGING the state
    ready = true;
}
cv.notify_one();                          // notify (lock can be released first)
```

**Three rules:**
1. **Change the shared state under the lock**, then notify.
2. **Wait with a predicate**: `cv.wait(lk, pred)`. Never bare `cv.wait(lk)`.
3. `wait` takes a **`std::unique_lock`** (it needs to unlock/relock).

---

## What `cv.wait(lk, pred)` actually does

```cpp
while (!pred()) {
    lk.unlock();          // release the mutex so a notifier can change the state
    // ... sleep until notified (or spuriously woken) ...
    lk.lock();            // re-acquire
}
// pred() is true, lk is held
```

- **Atomically** unlocks `lk` and starts waiting (no gap where a notify could be
  missed — the "lost wakeup" fix).
- On wake, **re-locks** and **re-checks `pred()`** (the "spurious wakeup" fix — CVs
  can wake for no reason; also handles "woken but the state changed back").
- Returns only when `pred()` is true, holding the lock.

Bare `cv.wait(lk)` (no predicate) has **both** bugs: a notify that fires between
your state check and the wait is lost; a spurious wake returns even though nothing
happened.

---

## The two classic bugs (why the pattern exists)

### Lost wakeup

```cpp
// ❌ BROKEN
// waiter:                          // notifier:
if (!ready)                          ready = true;      // (between the check and wait!)
    cv.wait(lk);                     cv.notify_one();    // no one waiting yet -> lost
// waits forever
```

`notify_one` only wakes threads **currently waiting**. If it fires before the
waiter is inside `wait`, it's gone. The predicate form checks `ready` *after*
re-locking inside `wait`, so it can't miss a state change.

### Spurious wakeup

The OS/implementation may return from `wait` **without any notify**. Bare `wait`
would proceed as if the event happened. The predicate form re-checks and goes
back to sleep.

---

## `notify_one` vs `notify_all`

| | Wakes | Use |
|---|---|---|
| `notify_one()` | one waiting thread (unspecified which) | one item produced → one consumer needed |
| `notify_all()` | all waiting threads | state change relevant to many (shutdown, "phase done", capacity freed for multiple producers) |

Waking all when one would do → a **thundering herd**: all wake, all contend for
the lock, all but one re-check the predicate and go back to sleep — wasted work.
Use `notify_one` when only one waiter can make progress.

---

## Timed wait

```cpp
if (cv.wait_for(lk, 100ms, [&]{ return ready; })) {
    // predicate became true within 100 ms
} else {
    // timed out — predicate still false
}
```

`wait_for` / `wait_until` with a predicate return `pred()`'s value (true = event
happened, false = timeout). Useful for periodic work / shutdown checks.

---

## `std::condition_variable_any`

Works with **any** lock type (not just `std::unique_lock<std::mutex>`) — e.g. a
`std::shared_lock`, a custom lock. Slightly more overhead (an internal mutex).
Default to `std::condition_variable`.

---

## Producer-consumer (the canonical use)

`examples/05` — a bounded queue with **two** CVs:
- `not_empty_` — consumers wait on it; producers `notify_one` after `push`.
- `not_full_` — producers wait on it; consumers `notify_one` after `pop`.
- `close()` sets a flag and `notify_all` on both → waiters wake, see `closed_`,
  and exit gracefully (`pop()` returns `std::nullopt` when drained).

Two CVs (not one) so a producer's notify doesn't needlessly wake other producers.

---

## Andar kya hota hai

- `std::condition_variable` wraps a futex (Linux) / `SRWLOCK`+`CONDITION_VARIABLE`
  (Windows). `wait` — record this thread as a waiter, `unlock` the mutex,
  `futex_wait` on an internal counter. `notify_one` — bump the counter,
  `futex_wake(1)`. `notify_all` — `futex_wake(INT_MAX)` (or futex requeue).
- The unlock-and-wait is atomic w.r.t. notifications: a `notify` that races is
  either seen (thread already registered) or handled by the predicate re-check.
- Spurious wakeups come from the futex layer (signal interruption, requeue races)
  — the standard explicitly permits them so implementations can be simpler/faster.
- Notifying **without** holding the lock is allowed and often preferred (the woken
  thread doesn't immediately contend on a lock the notifier still holds) — but the
  *state change* must be under the lock.

---

## > **HFT relevance**
> - **CVs are for the control plane / helper threads, not the hot path** — a CV
>   wait is a futex sleep (a syscall + context switch to wake). The logger,
>   telemetry drain, timer, and admin threads use CVs to sleep when idle; the hot
>   thread **busy-polls** a lock-free queue instead (folder 28) — it never sleeps,
>   so latency is deterministic.
> - **Worker/pool threads** (`examples/07`) use a CV to wait for tasks — fine,
>   they're not latency-critical per-task at the ns level.
> - **Always the predicate form + state-under-lock** — a lost-wakeup bug in a
>   shutdown path means a thread that never exits (a hang on deploy).
> - **`notify_one` unless the event genuinely concerns all waiters** — avoid
>   thundering herds on the helper threads.
> - Hybrid: a hot consumer that spin-polls for N µs, then falls back to a CV wait
>   if still empty (bounded busy-wait) — low latency when busy, no CPU burn when
>   idle.

---

## Hands-on

```bash
./build.ps1 26-CONCURRENCY/examples/05_condition_variable.cpp
```

Bounded queue, predicate waits, two CVs, graceful `close()`. Then:
- Replace `cv.wait(lk, pred)` with bare `cv.wait(lk)` and run under load a few
  times — occasional hangs / wrong behaviour (spurious/lost).
- Move the `ready = true` (state change) to *outside* the lock → a lost-wakeup
  hang.
- Swap `notify_one` for `notify_all` in the producer and observe extra wakeups
  (add a counter in the consumer's wait predicate).

---

## ⚠️ Traps

### Trap 1 — bare `cv.wait(lk)` without a predicate
Spurious wakeups + lost wakeups. Always `cv.wait(lk, pred)`.

### Trap 2 — changing the shared state without the lock, then notifying
```cpp
ready = true;              // ⚠️ not under the lock
cv.notify_one();           // waiter may miss it (checked `ready` and is about to wait)
```
Change state under the lock; then notify (lock optional).

### Trap 3 — `std::lock_guard` for the waiter
`cv.wait` needs `std::unique_lock` (it unlocks/relocks). `lock_guard` won't compile
with `wait`.

### Trap 4 — `notify_all` where `notify_one` suffices
Thundering herd — all wake, contend, all-but-one sleep again. Wasted CPU.

### Trap 5 — waiting on the wrong / stale predicate
`cv.wait(lk, [&]{ return q.size() > 0; })` but on shutdown you also need
`|| closed_`, else the thread waits forever after `close()`.

### Trap 6 — CV on the hot path
A `wait` is a futex sleep = a syscall + context switch to wake = µs, jitter.
Hot consumers busy-poll a lock-free queue.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`cv.wait(lk)` waits until notified" | It can return spuriously; use the predicate form |
| "notify wakes a thread that isn't waiting yet" | `notify_one/all` only wakes **current** waiters — earlier notifies are lost |
| "notify must be called while holding the lock" | The **state change** must be locked; the notify itself needn't be |
| "`notify_all` is safer than `notify_one`" | It's a thundering herd if only one waiter can progress |
| "a CV works with any lock" | `std::condition_variable` needs `unique_lock<mutex>`; `condition_variable_any` for others |
| "CVs are cheap enough for the hot path" | A wait = futex sleep + wake context switch — control plane only |

---

## Exercises

1. **Fix the pattern:**
   ```cpp
   // waiter
   std::unique_lock lk(m);
   cv.wait(lk);
   process(data_);
   // notifier
   data_ = make();
   cv.notify_one();
   ```

   <details><summary>Answer</summary>

   Waiter: `cv.wait(lk, [&]{ return has_data_; });`. Notifier: `{ std::lock_guard
   lk(m); data_ = make(); has_data_ = true; } cv.notify_one();`. Add a `bool
   has_data_` flag, set it under the lock, wait on the predicate.
   </details>

2. **Lost wakeup:** with bare `cv.wait(lk)`, sketch the interleaving where the
   waiter sleeps forever.

   <details><summary>Answer</summary>

   Waiter: checks `if (!ready)` → true → *before* it reaches `cv.wait(lk)`,
   notifier runs `ready = true; cv.notify_one();` (no one waiting → lost). Waiter
   then enters `cv.wait(lk)` and sleeps forever. The predicate form re-checks
   `ready` after re-locking inside `wait`, so it sees `true` and doesn't sleep.
   </details>

3. **one vs all:** a bounded queue, 1 producer pushes 1 item, 4 consumers wait.
   `notify_one` or `notify_all`? What if the producer pushes 4 items in a loop?

   <details><summary>Answer</summary>

   1 item → `notify_one` (only one consumer can take it). 4 items pushed in a
   loop → `notify_one` **per push** (4 notifies, 4 consumers wake, each takes
   one). A single `notify_all` after the loop also works but risks waking
   consumers before all items are in; per-item `notify_one` is cleaner.
   </details>

4. **Shutdown:** add graceful shutdown to a consumer `while (true) { unique_lock
   lk(m); cv.wait(lk, [&]{ return !q.empty(); }); auto x = pop(); lk.unlock();
   handle(x); }`.

   <details><summary>Answer</summary>

   Add `bool closed_`. Predicate: `[&]{ return !q.empty() || closed_; }`. After
   the wait: `if (q.empty()) return;` (closed and drained). `close()`: `{
   lock_guard lk(m); closed_ = true; } cv.notify_all();`.
   </details>

5. **Hybrid:** design a consumer that has low latency when busy but doesn't burn a
   core when idle.

   <details><summary>Answer</summary>

   Busy-poll the lock-free queue for up to N iterations / T microseconds. If still
   empty, fall back to a CV/futex wait (or `std::this_thread::yield()` then a
   short `wait_for`). When an item arrives, the producer notifies. You pay the
   CV sleep only during genuine idle periods; under load you're always in the
   spin path with ~ns latency.
   </details>

---

## Interview questions

1. Condition variable ka safe usage pattern — teen rules.
2. `cv.wait(lk, pred)` internally kya karta (unlock/wait/relock/re-check)?
3. Lost wakeup — kaise hota, predicate form se kaise fix?
4. Spurious wakeup — kya, kyun allowed?
5. `notify_one` vs `notify_all` — thundering herd kya?
6. `cv.wait` ko `unique_lock` kyun chahiye (`lock_guard` nahi)?
7. HFT hot path pe CV kyun nahi (futex sleep cost)? Hybrid approach?

---

## Next
→ [`12-futures-and-promises.md`](12-futures-and-promises.md)
