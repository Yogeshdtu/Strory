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
⏳ **Yeh folder abhi syllabus stage pe hai.** Upar ki file list poori plan hai —
content agle batch mein aayega.

## Next
→ [`../27-ATOMICS-MEMORY-MODEL/00-README.md`](../27-ATOMICS-MEMORY-MODEL/00-README.md)
