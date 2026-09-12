# Examples — Folder 26 (Concurrency)

| File | Lesson(s) | Kya dikhata hai |
|---|---|---|
| `01_first_thread.cpp` | 03, 04 | `std::thread` construct=start / `join` / `detach`, `hardware_concurrency`, thread ids, lambda threads, a `vector<std::thread>`, **`std::ref`** for pass-by-reference (args are decay-copied) |
| `02_race_condition.cpp` | 05 | ⚠️ **Data race.** 8 threads × 200k un-synchronized `++counter` → **~70–75% lost updates, different every run**. Formally UB. (TSan on Linux points at the two `++counter` accesses) |
| `03_mutex_fix.cpp` | 06, 07, 08 | Same work fixed 3 ways + **measured** (`-O2`): `std::mutex`+`lock_guard` **~1248 ms** / `std::atomic<long>`+`fetch_add` **~430 ms** / per-thread local + combine **~1.9 ms**. The critical section *was* the program |
| `04_deadlock.cpp` | 09 | ⚠️ Opposite lock order (would hang) — demonstrated safely with `std::timed_mutex` + `try_lock_for` (**~10 near-deadlocks detected & backed off** per run). Then FIX A (consistent order) and FIX B (`std::scoped_lock`) |
| `05_condition_variable.cpp` | 11 | Bounded producer-consumer queue: **predicate `cv.wait(lk, pred)`**, two CVs (`not_empty`/`not_full`), state-change-under-lock, graceful `close()` → `pop()` returns `nullopt` when drained. Sum verified exact |
| `06_futures.cpp` | 12 | `std::async(async)` vs `(deferred)`, `promise`/`future` hand-off, **exception through `future::get()`**, `std::packaged_task`. Note: the `std::async` future destructor **blocks** |
| `07_thread_pool.cpp` | 13 | A full ~60-line pool: mutex+cv task queue, `submit()` → `std::future` (via `shared_ptr<packaged_task>`), drain-on-shutdown. **Measured** (`-O2`, fib×64, 8 cores): serial **~271 ms** / `std::async`/task **~52 ms** / `ThreadPool(8)` **~48 ms** |
| `08_false_sharing.cpp` | 16, 08 | ⚠️ **Measured 10×.** 8 threads, each hammers its own `std::atomic<long>`. Counters **packed** (~1 cache line) → **~11258 ms**; `alignas(64)` **padded** (1 per line) → **~1130 ms**. Same logic, layout only |
| `09_jthread_cpp20.cpp` | 14 | `std::jthread` (auto-join), `std::stop_token` / `request_stop`, `std::latch` (start gun + finish line), `std::barrier` (3 phases + completion fn), `std::counting_semaphore<3>` (max 3 concurrent) |

## Compile / run

```bash
./build.ps1 26-CONCURRENCY/examples/01_first_thread.cpp        # Windows (MinGW: -pthread optional)
make FILE=26-CONCURRENCY/examples/01_first_thread.cpp          # Linux/Mac/Git-Bash (-pthread)
```

Benchmarks at **`-O2`** (required — `-O0` makes mutex/atomic/false-sharing all
equally slow):
```bash
./build.ps1 fast 26-CONCURRENCY/examples/03_mutex_fix.cpp
./build.ps1 fast 26-CONCURRENCY/examples/07_thread_pool.cpp
./build.ps1 fast 26-CONCURRENCY/examples/08_false_sharing.cpp
```

**Race / deadlock detection** (Linux — MinGW has no libtsan/libasan):
```bash
g++ -std=c++20 -O1 -g -fsanitize=thread 26-CONCURRENCY/examples/02_race_condition.cpp -o rc && ./rc
```

## Measured (GCC 15.1.0, `-O2`, 8 logical cores, this box) — sample runs

### `02_race_condition.cpp`
```
expected 1,600,000 (8 threads x 200k)
run 1: 471,995   (70.5% lost)
run 2: 398,534   (75.1% lost)
run 3: 455,661   (71.5% lost)     <- different every run
```

### `03_mutex_fix.cpp` (16M increments, 8 threads)
```
(a) std::mutex        : 1248.1 ms
(b) std::atomic       :  430.1 ms
(c) local + combine   :    1.9 ms      <- no shared write in the hot loop
```

### `07_thread_pool.cpp` (fib(30) x 64 tasks)
```
serial          : 271.3 ms
std::async/task :  51.8 ms
ThreadPool(8)   :  47.6 ms
```

### `08_false_sharing.cpp` (8 threads x 50M atomic increments)
```
PACKED (false sharing): 11258.5 ms
PADDED (alignas 64)   :  1129.9 ms
speedup: 9.96x
```

## Notes / jaan-boojh kar cheezein

- **No `broken_on_purpose` file.** `02` (race) and `08` (false sharing) are
  *correct* programs that demonstrate a bug/cost you can measure; `04` (deadlock)
  uses timeouts so it never actually hangs.
- **TSan/ASan/UBSan are not available on this MinGW build** — the race/deadlock
  examples run and show the *effect* (lost updates, near-deadlocks). On Linux,
  `-fsanitize=thread` names the exact racing accesses; the READMEs give the
  commands.
- `04_deadlock.cpp` uses `std::timed_mutex` + `try_lock_for(50ms)` to demonstrate
  the opposite-order pattern **without hanging** the run — it detects the impasse
  and backs off (~10× per run).
- All 9 compile clean under `-Wall -Wextra -Wpedantic -Wshadow -Wconversion
  -Wsign-conversion -Wcast-align -Wunused -Wnull-dereference -Wdouble-promotion`
  (`./build.ps1 folder 26-CONCURRENCY`). `-pthread` is a no-op on this MinGW
  (winpthreads links automatically) but is in every compile command for Linux.
