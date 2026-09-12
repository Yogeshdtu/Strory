# 07 — `std::mutex`

## Prerequisites
- `06-critical-sections.md`, `17-RAII`
- [`examples/03_mutex_fix.cpp`](examples/03_mutex_fix.cpp)

## Yeh topic abhi kyun
`std::mutex` — "mutual exclusion" ka basic tool. Ek thread `lock()` karta, critical
section chalata, `unlock()` karta; baaki `lock()` pe wait karte. Iska raw API
(`lock`/`unlock` manually) **bura** hai (exception → unlock miss → deadlock);
isliye RAII wrappers (file 08). Yahan mutex khud, uske variants, aur uski cost.

---

## Basic use (raw — mat karo, sirf samajhne ke liye)

```cpp
#include <mutex>

std::mutex m;
long shared = 0;

void inc() {
    m.lock();          // acquire; block if another thread holds it
    ++shared;          // critical section
    m.unlock();        // release
}
```

**Problems with raw lock/unlock:**
```cpp
void bad() {
    m.lock();
    if (some_check()) return;       // ⚠️ early return -> unlock MISSED -> deadlock
    do_work();                       // ⚠️ if this throws -> unlock MISSED
    m.unlock();
}
```
→ **always** use a RAII lock guard (file 08). Raw `lock()`/`unlock()` only in the
rare hand-rolled case.

---

## Semantics

- **`lock()`** — if free, acquire and return. If held by another thread, **block**
  until it's free.
- **`unlock()`** — release. Must be called by the **same thread** that locked, and
  only if currently held. Otherwise **UB**.
- **`try_lock()`** — acquire if free (return `true`), else return `false`
  immediately (no block).
- **Not recursive** — the same thread calling `lock()` twice → **deadlock** (or UB).
  Need re-entrancy? `std::recursive_mutex` (usually a design smell).
- **Not copyable, not movable** — owns a kernel/futex resource.
- A `std::mutex` must be **unlocked** when destroyed (else UB).

---

## The mutex family

| Type | Extra capability | Cost / use |
|---|---|---|
| **`std::mutex`** | basic lock/unlock | the default |
| **`std::recursive_mutex`** | same thread can lock N times (unlock N times) | design smell; refactor instead |
| **`std::timed_mutex`** | `try_lock_for(dur)` / `try_lock_until(tp)` | bounded wait (`examples/04` uses it) |
| **`std::recursive_timed_mutex`** | both | rare |
| **`std::shared_mutex`** (C++17) | shared (read) + exclusive (write) locks | many readers / rare writer (file 10) |
| **`std::shared_timed_mutex`** | + timed | rare |

---

## What `lock()` actually does

1. **Fast path (uncontended):** an atomic compare-and-swap on a word in the mutex
   (0 → locked). If it succeeds, done — **no syscall**, ~10–25 ns.
2. **Slow path (contended):** the CAS fails → spin a few times (adaptive) → if
   still held, call into the kernel (`futex(FUTEX_WAIT)` on Linux) to **sleep**
   until woken. A context switch each way.
3. **`unlock()`:** store 0; if there were waiters, `futex(FUTEX_WAKE)` one.

So an **uncontended** mutex is cheap (an atomic op). A **contended** one costs a
syscall + context switch + the cache-line bouncing between cores — µs, and
non-deterministic. `examples/03`: 16M **contended** locked increments = 1248 ms
(≈78 ns/op amortized, mostly contention).

---

## `std::lock` / `std::scoped_lock` — multiple mutexes without deadlock

```cpp
std::mutex a, b;
// ❌ different threads lock in different orders -> deadlock (file 09)
// ✅ lock BOTH atomically with a deadlock-avoidance algorithm:
std::scoped_lock lk(a, b);        // C++17 — locks a and b, order-independent, no deadlock
// (pre-C++17: std::lock(a, b); std::lock_guard la(a, std::adopt_lock), lb(b, std::adopt_lock);)
```

`std::scoped_lock` with 2+ mutexes uses a try-and-back-off algorithm so a
consistent global order isn't required (file 08, 09).

---

## `std::call_once` — thread-safe one-time init

```cpp
std::once_flag flag;
Config* g_cfg = nullptr;

Config& config() {
    std::call_once(flag, [] { g_cfg = load_config(); });   // runs the lambda exactly once
    return *g_cfg;
}
```

For lazy init, prefer a **function-local `static`** (the compiler adds the same
guard — folder 25 file 04) unless you need `call_once`'s explicit flag.

---

## > **HFT relevance**
> - **No mutex on the hot path.** An uncontended lock is ~15 ns, but a *contended*
>   one is a syscall + context switch = a µs-scale tail-latency spike, and you
>   can't guarantee it stays uncontended under load. Hot path = lock-free (folder
>   28) or single-threaded.
> - **Mutexes are fine on the control plane** — config reload, admin commands,
>   metrics aggregation — where latency doesn't matter and the code is simpler
>   locked.
> - **`std::timed_mutex` + `try_lock_for` + a fallback** where a hot-ish path might
>   occasionally need a shared resource but must never block indefinitely
>   (`examples/04`).
> - **Never a `std::recursive_mutex`** on anything performance-relevant — it's
>   usually hiding a design where the same lock is taken on two call paths that
>   should be restructured.
> - **Measure lock hold time and contention** (`perf lock`, `mutrace`) — if a hot
>   lock is held > tens of ns or contended, redesign.

---

## Hands-on

```bash
./build.ps1 fast 26-CONCURRENCY/examples/03_mutex_fix.cpp
```

Version (a) is `std::mutex` + `lock_guard`. Compare its time (~1248 ms) to (b)
atomic (~430 ms) and (c) no-sharing (~1.9 ms). Then:
- Add a `try_lock()` loop version and compare.
- Time a **single-threaded** loop with the same lock/unlock per iteration
  (uncontended) — see how cheap an uncontended mutex is.
- `perf stat -e context-switches ./mf` — the contended mutex generates them.

---

## ⚠️ Traps

### Trap 1 — raw `lock()`/`unlock()` with early return or exceptions
Unlock gets skipped → the mutex stays locked forever → every future `lock()`
deadlocks. Use `std::lock_guard` / `std::scoped_lock` (file 08).

### Trap 2 — locking the same non-recursive mutex twice on one thread
```cpp
void outer() { std::lock_guard lk(m_); inner(); }
void inner() { std::lock_guard lk(m_); ... }   // ⚠️ deadlock — m_ already held by this thread
```
Refactor so the lock is taken once, or split the shared state.

### Trap 3 — unlocking from a different thread
`m.unlock()` from a thread that didn't lock it → UB. Locks are thread-scoped.

### Trap 4 — a `std::recursive_mutex` "fix"
It makes trap 2 "work" but hides a bad design (shared state touched by two call
paths that should be one). Restructure.

### Trap 5 — mutex on the hot path assuming "it's usually uncontended"
Under load it *will* contend → syscall + context switch = a spike. Design for the
contended case (lock-free / single-threaded).

### Trap 6 — destroying a locked mutex
UB. Ensure every lock is released (RAII does this) before the mutex's lifetime
ends.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`std::mutex` is recursive" | Not — same-thread re-lock deadlocks; `std::recursive_mutex` if you truly need it |
| "a mutex always involves a syscall" | Uncontended: just an atomic CAS (~15 ns). Contended: futex + context switch |
| "any thread can `unlock()`" | Only the locking thread; else UB |
| "raw `lock`/`unlock` is fine if I'm careful" | Early returns / exceptions skip the unlock — use RAII |
| "`recursive_mutex` is a clean fix for re-entrancy" | Usually a design smell — split the lock or the code path |
| "a small critical section can't spike latency" | Contention → futex sleep + context switch, µs-scale, non-deterministic |

---

## Exercises

1. **Spot the deadlock:** `void log(const std::string& s){ std::lock_guard lk(m_);
   sink_ += s; } void flush(){ std::lock_guard lk(m_); for (...) log(line); }` —
   what happens?

   <details><summary>Answer</summary>

   `flush()` holds `m_`, then calls `log()` which tries to lock `m_` again on the
   same thread → deadlock (`std::mutex` is not recursive). Fix: have `flush()`
   build the output without calling `log()`, or factor the actual append into an
   unlocked helper that both call while holding the lock once.
   </details>

2. **Cost:** a single thread does 10M iterations of `{ std::lock_guard lk(m);
   ++x; }` with no other thread. Rough time, and why so cheap?

   <details><summary>Answer</summary>

   ~100–250 ms (≈10–25 ns/iter) — every lock is uncontended, so it's just an
   atomic CAS to acquire and a store to release, no syscall, no context switch.
   The cost of a mutex is almost entirely in *contention*.
   </details>

3. **`try_lock`:** write a "skip if busy" update — if the lock is held, drop the
   update instead of waiting.

   <details><summary>Answer</summary>

   ```cpp
   if (m_.try_lock()) {
       std::lock_guard lk(m_, std::adopt_lock);   // RAII owns it now
       shared_ = value;
   } else {
       ++dropped_;   // or push to a fallback queue
   }
   ```
   Useful for best-effort stats / sampling where staleness beats blocking.
   </details>

4. **Two mutexes:** you must update `book_` (under `bm_`) and `risk_` (under `rm_`)
   together. Deadlock-safe way in one line.

   <details><summary>Answer</summary>

   `std::scoped_lock lk(bm_, rm_);` — locks both with a deadlock-avoidance
   algorithm, so it's safe regardless of the order other code locks them in.
   (Pre-C++17: `std::lock(bm_, rm_);` then two `std::lock_guard(..., std::adopt_lock)`.)
   </details>

5. **Hot path:** a per-tick handler takes a `std::mutex` to update a shared "last
   trade" struct read by a UI thread. It's fine in testing. Why might it spike in
   production, and what's the fix?

   <details><summary>Answer</summary>

   Under real load the UI thread (or another) can be holding / contending the lock
   when a tick arrives → the tick handler blocks on a futex sleep + context switch
   = a µs-scale P99 spike, non-deterministic. Fix: make "last trade" an immutable
   snapshot the handler publishes via an atomic pointer swap (no lock, no wait),
   or a lock-free single-writer/many-reader structure. The UI reads a possibly
   slightly-stale snapshot — fine.
   </details>

---

## Interview questions

1. `std::mutex` — `lock`/`unlock`/`try_lock` semantics; recursive?
2. Raw `lock`/`unlock` ke problems (early return, exceptions).
3. Uncontended vs contended mutex ki cost — kya farq (CAS vs futex + context switch)?
4. `std::scoped_lock` with 2 mutexes — deadlock se kaise bachata?
5. `std::recursive_mutex` kab, aur kyun aksar design smell?
6. `std::call_once` vs function-local `static` for lazy init.
7. HFT hot path pe mutex kyun nahi (contended-case cost)?

---

## Next
→ [`08-lock-guards.md`](08-lock-guards.md)
