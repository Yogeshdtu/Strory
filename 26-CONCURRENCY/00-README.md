# 26 — CONCURRENCY (PHASE 16)

## Prerequisites
`25-OBJECT-MODEL`, `17-RAII`

## Yeh folder kyun
Ab tak ek hi thread tha. Ab kai honge — aur sab kuch mushkil ho jaayega.

Prerequisites ka dhyaan: functions → threads → race conditions → shared data → mutex.
Yeh order strictly follow hoga.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-concurrency-vs-parallelism.md` | Do alag cheezein, kab kaunsa chahiye |
| 02 | `02-process-vs-thread.md` | Recap + detail, memory sharing, creation cost |
| 03 | `03-std-thread.md` | `std::thread`, join, detach, thread ID, hardware_concurrency |
| 04 | `04-passing-data-to-threads.md` | Arguments, references (`std::ref`), dangling risks |
| 05 | `05-race-conditions.md` | **Race condition kya hai** — demo, non-determinism |
| 06 | `06-critical-sections.md` | Critical section, mutual exclusion ka idea |
| 07 | `07-mutex.md` | `std::mutex`, `lock`/`unlock`, kyun manual lock bura hai |
| 08 | `08-lock-guards.md` | `lock_guard`, `unique_lock`, `scoped_lock` — **RAII for locks** |
| 09 | `09-deadlock.md` | **Deadlock** — 4 conditions, demo, lock ordering, `std::scoped_lock` |
| 10 | `10-shared-mutex.md` | `shared_mutex`, reader-writer locks, kab faayda hai |
| 11 | `11-condition-variables.md` | `condition_variable`, wait/notify, **spurious wakeups**, predicate form |
| 12 | `12-futures-and-promises.md` | `future`, `promise`, `async`, `packaged_task` |
| 13 | `13-thread-pool.md` | **Apna thread pool banao** — queue, workers, shutdown |
| 14 | `14-cpp20-sync.md` | `jthread`, `stop_token`, `latch`, `barrier`, `counting_semaphore` |
| 15 | `15-thread-local.md` | `thread_local` storage, use cases, cost |
| 16 | `16-false-sharing-intro.md` | **False sharing** ka pehla parichay — measured demo |
| 17 | `17-concurrency-bugs.md` | Races, deadlocks, livelock, starvation, TSan se pakadna |
| 18 | `18-exercises.md` | Practice + concurrent programs |

## Examples

| File | Kya |
|---|---|
| `examples/01_first_thread.cpp` | Pehla thread |
| `examples/02_race_condition.cpp` | ⚠️ Race demo (TSan) |
| `examples/03_mutex_fix.cpp` | Mutex se fix |
| `examples/04_deadlock.cpp` | ⚠️ Deadlock reproduce |
| `examples/05_condition_variable.cpp` | Producer-consumer |
| `examples/06_futures.cpp` | async/future patterns |
| `examples/07_thread_pool.cpp` | Poora thread pool |
| `examples/08_false_sharing.cpp` | False sharing — measured (padding se fix) |
| `examples/09_jthread_cpp20.cpp` | C++20 primitives |

## Time
3 hafte

## Status
✅ **COMPLETE (Batch 8 — PHASE 16).** 17 lessons (`01`–`17`) + `18-exercises.md`
+ 9 examples. Sab `.cpp` `-Wall -Wextra -Wpedantic -Wshadow -Wconversion
-Wsign-conversion -Wcast-align -Wunused -Wnull-dereference -Wdouble-promotion` pe
clean (`./build.ps1 folder 26-CONCURRENCY`). `-pthread` a no-op on this MinGW
(winpthreads auto-links); in every compile command for Linux.

- `02_race_condition` — 8 threads × 200k un-synced `++counter` → **~70–75% lost
  updates, different every run** (data race = UB).
- `03_mutex_fix` (`-O2`, 16M increments): `std::mutex` **~1248 ms** / `std::atomic`
  **~430 ms** / per-thread local + combine **~1.9 ms**.
- `04_deadlock` — opposite lock order shown safely via `std::timed_mutex` +
  `try_lock_for` (~10 near-deadlocks/​run detected + backed off); FIX A
  (consistent order) + FIX B (`std::scoped_lock`).
- `07_thread_pool` (`-O2`, fib(30)×64, 8 cores): serial **~271 ms** / async/task
  **~52 ms** / `ThreadPool(8)` **~48 ms**.
- `08_false_sharing` — **measured 10×**: packed counters (~1 line) **~11258 ms**
  vs `alignas(64)` padded **~1130 ms**. Same logic, layout only.
- `09_jthread_cpp20` — `jthread` auto-join, `stop_token`, `latch`, `barrier`
  (3 phases), `counting_semaphore<3>` (max 3 concurrent).
- ⚠️ TSan/ASan not on this MinGW — race/deadlock examples show the *effect*; the
  READMEs give the Linux `-fsanitize=thread` commands.

**Coverage:** concurrency vs parallelism (+ Amdahl) · process vs thread (what's
shared, creation cost, context switch, isolation) · `std::thread` (join-or-
`terminate`, move-only, `native_handle`) · **passing data** (decay-copy,
`std::ref`, dangling captures, `this` lifetime) · **race conditions & data races**
(the `++counter` interleaving, UB, cached flags, torn reads) · critical sections &
mutual exclusion (keep them tiny) · `std::mutex` (uncontended CAS vs contended
futex, the family, `std::call_once`) · **lock guards** (`lock_guard` /
`unique_lock` / `scoped_lock`, the temporary-guard trap) · **deadlock** (Coffman
4, consistent order / `scoped_lock` / hierarchy / try+backoff) · `std::shared_mutex`
(when it helps, when the snapshot pattern is better) · **condition variables**
(the predicate pattern, lost/spurious wakeup, `notify_one` vs `all`) · futures &
promises (`async` policies, the blocking dtor, `packaged_task`, exception
propagation) · **build a thread pool** · **C++20 sync** (`jthread`/`stop_token`/
`latch`/`barrier`/`counting_semaphore`) · `thread_local` (TLS access cost, hoist
in hot loops, `Context&` vs TLS) · **false sharing** (measured 10×, MESI, the
SPSC head/tail case) · concurrency bug catalog + detection (TSan, Helgrind,
stress+asserts, `perf c2c`).

## Next
→ [`../27-ATOMICS-MEMORY-MODEL/00-README.md`](../27-ATOMICS-MEMORY-MODEL/00-README.md)
