# 17 — Concurrency bugs: races, deadlock, livelock, starvation — and TSan

## Prerequisites
- Poora folder 26 so far (files 01–16)
- `25-OBJECT-MODEL` file 15 (UB — C1)

## Yeh topic abhi kyun
Concurrent code ke bugs ek **alag category** hain: non-deterministic, timing-
dependent, "passed in dev, hangs/corrupts under load." Yeh file un bugs ka
catalog + detection strategy hai — kyunki inhe **tooling** se pakadna chahiye,
"soch ke" nahi.

---

## The bug catalog

| Bug | Kya | Symptom | Fix direction |
|---|---|---|---|
| **Data race** | 2 threads, non-atomic object, ≥1 write, no sync (file 05) | wrong values, torn reads, cached flags, crashes; **UB** | atomic / mutex / don't-share / immutable snapshot |
| **Deadlock** | circular wait on locks (file 09) | hang; `gdb bt` shows threads blocked in `lll_lock_wait` | consistent lock order / `scoped_lock` / one lock / lock-free |
| **Livelock** | threads keep changing state to "yield" to each other, no progress | 100% CPU, no forward progress, no hang | randomized backoff; don't spin-retry politely in lock-step |
| **Starvation** | a thread never gets a resource (unfair lock, reader-preference, low priority) | one thread stalls indefinitely while others run | fair locks, priority discipline, batch writers |
| **Lost / missed wakeup** | notify fires before the waiter waits; bare `cv.wait` (file 11) | a thread waits forever after the event happened | predicate `cv.wait(lk, pred)`, state-change under lock |
| **Spurious wakeup** (mishandled) | `cv.wait` returns with no notify; code assumes event | acts on a non-event | predicate form re-checks |
| **ABA problem** | a lock-free CAS sees the "same" value that was A→B→A; assumes nothing changed | corrupted lock-free structure | tagged pointers / hazard pointers / epochs (folder 28) |
| **False sharing** | independent vars on one cache line (file 16) | correct but 2–10× slow | `alignas(64)` the hot fields; accumulate locally |
| **Torn read/write** | non-atomic access to a >word-size (or misaligned) object | half-old/half-new value | `std::atomic` (lock-free path) or a lock |
| **Publication bug** | object constructed by one thread, pointer published without a release; reader sees the pointer but not the fields | reader dereferences partially-constructed data | `std::atomic` release/acquire on the pointer (folder 27) |
| **Use-after-free across threads** | one thread frees an object another still references | crash / corruption, timing-dependent | `shared_ptr`, hazard pointers, "no reader after unpublish + grace period" |
| **Iterator/handle invalidation across threads** | one thread mutates a container another iterates | crash / skipped/duplicated elements | lock the container, or per-thread copies, or a concurrent structure |

---

## Livelock vs deadlock

```cpp
// livelock: both threads "politely" back off in lock-step
for (;;) {
    lock(A);
    if (!try_lock(B)) { unlock(A); yield(); continue; }   // ⚠️ both do this, forever
    break;
}
```

Deadlock = **stuck** (no CPU). Livelock = **busy but no progress** (100% CPU).
Fix: **randomized** backoff (`sleep_for(rand() % k)`), or an asymmetry (one thread
has priority), or don't use try-lock-retry — acquire both with `std::scoped_lock`.

---

## Starvation

- **Unfair mutex** — the OS may keep handing the lock back to the thread that just
  released it (better cache locality) → another waiter starves. `std::mutex` gives
  no fairness guarantee.
- **`std::shared_mutex` reader-preference** — a steady reader stream starves the
  writer (file 10).
- **Priority inversion** — a low-prio thread holds a lock a high-prio thread
  needs; a medium-prio thread preempts the low-prio one → the high-prio thread is
  effectively starved. Fix: priority inheritance mutexes (`PTHREAD_PRIO_INHERIT`),
  or don't share locks across priority levels.

---

## Why "just think harder" doesn't work

- These bugs depend on **interleavings** that occur in 1 run in 10,000 — or only
  under production load, on a different core count, with a different scheduler.
- The compiler + memory model make "reading the code" insufficient — a plain
  `bool` flag *looks* fine and is a bug (file 05).
- **You need tooling** + **stress + invariant assertions** + **design discipline**
  (minimize shared mutable state).

---

## The detection toolbox

| Tool | Catches | Notes |
|---|---|---|
| **ThreadSanitizer** (`-fsanitize=thread`) | data races, lock-order inversions (potential deadlocks) | ~5–15× slowdown, ~5–10× memory; **the primary tool**. Linux/macOS; **not MinGW** |
| **Helgrind / DRD** (Valgrind) | races, lock-order issues, misused pthread APIs | slower than TSan, no recompile |
| **gdb** — `thread apply all bt` on a hung process | deadlock (see the blocked mutexes) | after the fact |
| **`std::timed_mutex` + `try_lock_for` + log** | would-be deadlocks, in tests | `examples/04` pattern |
| **Stress harness + invariant checks** | gross races, lost updates, corruption | run the concurrent path millions of times with `assert`s; non-deterministic failure = a bug (`examples/02`) |
| **`perf c2c`** | false sharing (cache-to-cache transfers) | folder 32/35 |
| **Fuzzing / schedule exploration** (`rr` chaos mode, tsan's `history_size`) | rare interleavings | advanced |
| **Code review checklist** | "what shared mutable state, protected how, on every thread body" | the cheapest, do it always |

**CI must run the concurrency suite under TSan** (and, separately, under
ASan/UBSan). Every TSan report is a bug that will eventually miscompile or corrupt
under `-O2 -flto`.

---

## > **HFT relevance**
> - **Design out the bug classes.** The hot path shares no mutable state → no data
>   races, no deadlocks, no false sharing, no starvation *by construction*. Data
>   flows over lock-free SPSC queues (folder 28); shared read-mostly state is an
>   immutable snapshot swapped by an atomic pointer.
> - **Where locks exist (control plane):** one documented global lock order,
>   `std::scoped_lock` for multi-lock, no callbacks/`join`/blocking under a lock,
>   `HierarchicalMutex` in debug builds.
> - **CI: TSan on every concurrency test**, plus a soak test (hours, production
>   load profile) with invariant assertions.
> - **`perf c2c` in the profiling loop** to catch false sharing that slipped
>   through review.
> - **Publication discipline** — never publish a pointer to an object another
>   thread will read without a release store (folder 27); this is the subtle one
>   that TSan catches and review often misses.
> - A concurrency bug in a live trading engine = wrong orders / crashes under the
>   exact conditions (high volume) when you can least afford it.

---

## Hands-on

```bash
./build.ps1 26-CONCURRENCY/examples/02_race_condition.cpp   # data race (visible lost updates)
./build.ps1 26-CONCURRENCY/examples/04_deadlock.cpp         # deadlock pattern (timeout-detected)
./build.ps1 fast 26-CONCURRENCY/examples/08_false_sharing.cpp  # false sharing (10x)

# Linux — the real tool:
g++ -std=c++20 -O1 -g -fsanitize=thread 26-CONCURRENCY/examples/02_race_condition.cpp -o rc && ./rc
```

- TSan on `examples/02` → points at the two `++counter` accesses + stacks.
- TSan on a version of `examples/04` with plain `std::mutex` (lock in opposite
  orders) but *no actual deadlock this run* → still reports the lock-order
  inversion.
- Write a stress test: a shared `std::vector` pushed by 4 threads without a lock,
  with an `assert(v.size() == expected)` at the end — run 1000× → intermittent
  crash / assert.

---

## ⚠️ Traps

### Trap 1 — "it passed the tests, ship it"
Concurrency bugs are non-deterministic. Tests must run under TSan + stress + soak.

### Trap 2 — plain `bool` / `int` cross-thread flag
Compiler caches it → infinite spin, or torn value. `std::atomic`.

### Trap 3 — try-lock-retry backoff in lock-step
Livelock. Randomize the backoff, or don't try-lock-retry.

### Trap 4 — publishing a pointer without a release store
The reader sees the pointer but stale/garbage fields. `std::atomic` release on
publish, acquire on read (folder 27).

### Trap 5 — assuming `std::mutex` is fair
No fairness guarantee → possible starvation. If you need fairness, build it (a
ticket lock) or restructure.

### Trap 6 — freeing an object another thread might still hold
Timing-dependent use-after-free. `shared_ptr`, or an unpublish + grace-period
scheme, or hazard pointers.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "concurrency bugs are found by careful reading" | Non-deterministic + memory-model subtleties → need TSan + stress |
| "deadlock and livelock are the same" | Deadlock: stuck (no CPU). Livelock: 100% CPU, no progress |
| "`std::mutex` is fair" | No fairness guarantee — starvation possible |
| "a data race just gives a slightly wrong number" | UB — torn reads, cached flags, compiler assumptions |
| "TSan is too slow to bother with" | Run it in CI on the concurrency suite; every hit is a real bug |
| "if it doesn't deadlock in testing, the lock order is fine" | TSan flags lock-order inversions even without an actual deadlock |

---

## Exercises

1. **Classify:** (a) two threads `++x` on a plain int; (b) T1 locks A then B, T2
   locks B then A, program hangs; (c) two threads each grab lock A, fail to get B,
   release A, retry — CPU at 100%, no progress; (d) a writer never acquires a
   `shared_mutex` because readers keep coming.

   <details><summary>Answer</summary>

   (a) data race. (b) deadlock. (c) livelock. (d) starvation (writer).
   </details>

2. **Livelock fix:** `for(;;){ lock(A); if(try_lock(B)) break; unlock(A);
   this_thread::yield(); }` — two threads, opposite start. Fix.

   <details><summary>Answer</summary>

   Best: `std::scoped_lock lk(A, B);` — acquires both deadlock-free, no retry
   loop. If you must keep the loop, add randomized backoff:
   `std::this_thread::sleep_for(std::chrono::nanoseconds(rand() % 1000));` before
   retrying, so the two threads desynchronize.
   </details>

3. **Publication bug:** `g_ptr = new Config(...); g_ready = true;` (both plain).
   Reader: `if (g_ready) use(*g_ptr);`. Two things wrong.

   <details><summary>Answer</summary>

   (1) Data race on `g_ready` and `g_ptr` (plain, non-atomic). (2) Even made
   atomic with `relaxed`, the reader could see `g_ready == true` but a stale /
   partially-constructed `*g_ptr` — no release/acquire ordering. Fix: build the
   `Config` fully, then `g_ptr.store(p, std::memory_order_release);` and reader
   `g_ptr.load(std::memory_order_acquire)` (folder 27).
   </details>

4. **TSan report:** you get "data race on `Stats::count` — write at stats.cpp:42
   (thread T3), read at report.cpp:17 (thread T1)". Minimal fix options.

   <details><summary>Answer</summary>

   (1) Make `count` `std::atomic<uint64_t>` if it's a lone counter. (2) Protect
   *both* the write and the read with the same mutex. (3) Have T3 accumulate
   locally and publish a snapshot T1 reads via an atomic pointer. Pick based on
   whether `count` is part of a larger invariant.
   </details>

5. **Soak test:** design a test that would catch a rare (1-in-10^6) race in an
   order-book update path.

   <details><summary>Answer</summary>

   Run the real update path under `-fsanitize=thread` (finds it even without
   hitting the bad interleaving), *and* a schedule-stress harness: N threads
   hammering the path for hours with randomized timing (`sched_yield`, tiny random
   sleeps), plus continuous invariant checks (`assert(bid < ask)`, checksum of the
   book vs a serialized replay). A non-deterministic assert failure or a TSan hit
   = the bug. Also run at the production core count, not just your laptop's.
   </details>

---

## Interview questions

1. Concurrency bugs ka catalog — race / deadlock / livelock / starvation — ek line each.
2. Livelock vs deadlock — CPU usage, symptom.
3. Starvation ke sources (unfair mutex, reader-preference, priority inversion).
4. "Just think harder" kyun kaafi nahi (non-determinism + memory model)?
5. TSan kya pakadta hai, kaise (instrumentation), CI mein kyun?
6. Publication bug — pointer publish without release — reader ko kya dikhta?
7. ABA problem — kya, kahan (lock-free CAS)?
8. Detection toolbox — TSan / Helgrind / gdb bt / stress+asserts / `perf c2c`.

---

## Next
→ [`18-exercises.md`](18-exercises.md)
