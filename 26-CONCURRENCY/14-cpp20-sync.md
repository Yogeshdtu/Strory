# 14 — C++20 sync: `jthread`, `stop_token`, `latch`, `barrier`, `counting_semaphore`

## Prerequisites
- `03-std-thread.md`, `11-condition-variables.md`
- [`examples/09_jthread_cpp20.cpp`](examples/09_jthread_cpp20.cpp)

## Yeh topic abhi kyun
C++20 ne kai rough edges fix kiye aur naye building blocks diye. `std::jthread`
`std::thread` ke do problems (auto-join, cancellation) fix karta hai. `latch` /
`barrier` / `counting_semaphore` woh coordination patterns dete hain jo pehle
`mutex` + `cv` se haath se banane padte the.

---

## `std::jthread` — RAII join + cooperative stop

```cpp
{
    std::jthread jt([] { work(); });
}   // <-- destructor: request_stop() THEN join(). No std::terminate. No manual join.
```

Vs `std::thread`:
| | `std::thread` | `std::jthread` |
|---|---|---|
| Forgot join/detach | `std::terminate` | auto-joins in dtor |
| Cancellation | none (roll your own flag) | built-in `stop_token` |
| Dtor | `terminate` if joinable | `request_stop()` + `join()` |

**Just use `std::jthread`** unless you specifically need to `detach` or manage the
lifetime manually.

---

## `std::stop_token` / `stop_source` — cooperative cancellation

```cpp
std::jthread worker([](std::stop_token st) {          // jthread passes a token if the callable takes one
    while (!st.stop_requested()) {
        do_a_chunk();
    }
    cleanup();
});
// ...
worker.request_stop();     // ask it to stop; it finishes the current chunk and exits
// (~jthread also calls request_stop() then join())
```

- **`st.stop_requested()`** — poll it in your loop.
- **`std::stop_callback cb(st, []{ ... });`** — run a callback when stop is
  requested (e.g. to wake a blocked wait).
- `std::condition_variable_any::wait(lk, st, pred)` — a CV wait that also returns
  if `st` is stopped (so a stopped worker blocked in `wait` wakes up).
- **Cooperative** — nothing forcibly kills the thread; it must check the token.
  There's no safe way to force-kill a thread in C++ (or in general).

Manual version pre-C++20: an `std::atomic<bool> stop_{false}` you set and poll —
`stop_token` is that, standardized, with the CV integration.

---

## `std::latch` — one-shot countdown

```cpp
std::latch done{N};                 // counter starts at N

// worker:
do_work();
done.count_down();                  // --counter

// coordinator:
done.wait();                        // blocks until counter hits 0
```

- **Single use** — once it hits 0, it's done (can't reset).
- `count_down(n)` / `arrive_and_wait()` (count down and wait) / `try_wait()`.
- Classic uses: **"wait for all N workers to finish startup"**, **"release all N
  workers at once"** (a `latch{1}` as a start gun — `examples/09`), **"fork-join"**.

---

## `std::barrier` — reusable phase sync

```cpp
std::barrier sync(N, [] { std::puts("phase complete"); });   // optional completion fn

// each of N threads, each phase:
for (int p = 0; p < PHASES; ++p) {
    compute_phase(p);
    sync.arrive_and_wait();     // wait for the other N-1; then completion fn runs once; then all proceed
}
```

- **Reusable** — after all N arrive, the barrier **resets** for the next phase.
- The **completion function** runs exactly once, on one of the arriving threads,
  *after* all arrive and *before* any is released — perfect for "merge partial
  results / advance the clock / print progress" between phases.
- `arrive_and_drop()` — a thread arrives and permanently leaves the barrier
  (N decreases).
- Uses: iterative parallel algorithms (physics steps, PDE solvers, ML epochs),
  anything with "all threads must finish phase K before any starts K+1."

---

## `std::counting_semaphore<Max>` / `std::binary_semaphore`

```cpp
std::counting_semaphore<4> slots{3};   // up to Max=4 permits; start with 3 available

slots.acquire();          // take a permit (block if 0)
// ... use the limited resource ...
slots.release();          // give a permit back (release(n) for n)

slots.try_acquire();                    // non-blocking
slots.try_acquire_for(100ms);           // timed
```

- A counter of **permits**. `acquire` decrements (waits at 0), `release`
  increments (wakes a waiter).
- **`std::binary_semaphore`** = `std::counting_semaphore<1>` — a lightweight
  "1 permit" signal.
- Uses: **bound concurrency to K** (`examples/09`: max 3 threads in a section),
  **producer→consumer signalling** (often faster / simpler than a CV for a pure
  "wake one" case), a **fast one-shot handoff** (`binary_semaphore` between two
  threads).
- Unlike a mutex, **`release` can be called by a different thread** than
  `acquire` — semaphores are for signalling, mutexes for ownership.

---

## Andar kya hota hai

- All of these are thin wrappers over a futex (Linux) / `WaitOnAddress` (Windows):
  an atomic counter + `futex_wait`/`futex_wake`. `latch`/`barrier`/`semaphore`
  are often **faster and lower-overhead than a `mutex` + `condition_variable`**
  for their specific pattern (fewer allocations, no separate mutex).
- `jthread` holds a `std::stop_source` (a shared refcounted `{ atomic<bool>
  stop_requested, callback list, mutex }`). `request_stop()` sets the flag and
  runs registered `stop_callback`s. `~jthread` = `ss_.request_stop(); if
  (joinable()) join();`.
- `barrier`'s completion function + reset use a generation counter so threads from
  phase K don't accidentally satisfy phase K+1.

---

## > **HFT relevance**
> - **`std::jthread` for every long-lived thread** — RAII join means no
>   `std::terminate` land-mine on shutdown; `stop_token` gives clean cooperative
>   shutdown (the hot loop polls `st.stop_requested()` once per outer iteration —
>   effectively free).
> - **`std::latch` at startup** — spin up all worker threads, each `count_down`s a
>   "ready" latch; the main thread `wait`s, then `count_down`s a "go" latch to
>   release everyone simultaneously (deterministic start, no warm-up skew in
>   benchmarks — folder 35).
> - **`std::barrier` for phased parallel work** (backtests split by time window,
>   Monte-Carlo batches) — the completion function merges partials.
> - **`std::counting_semaphore`** to cap concurrent expensive operations (e.g.
>   parallel file loads at startup, bounded outstanding async requests).
> - Still: **the hot tick path uses none of these** — it's one pinned thread on a
>   lock-free queue. These are startup / helper / offline tools.

---

## Hands-on

```bash
./build.ps1 26-CONCURRENCY/examples/09_jthread_cpp20.cpp
```

All five: `jthread` auto-join, `stop_token` polling, `latch` (start gun + finish
line), `barrier` (3 phases with a completion fn), `counting_semaphore<3>` (max 3
concurrent). Then:
- Replace a `std::thread` + manual `join()` + `atomic<bool> stop_` with
  `std::jthread` + `stop_token` — count the lines removed.
- Build a "run N threads, time from all-ready to all-done" harness with two
  `std::latch`es.
- Use a `std::binary_semaphore` for a ping-pong between two threads; compare to a
  `mutex` + `cv`.

---

## ⚠️ Traps

### Trap 1 — expecting `stop_token` to force-kill the thread
It's cooperative — the thread must poll `stop_requested()` (or use the
`stop_callback` / CV integration). A thread stuck in a blocking call ignores it.

### Trap 2 — reusing a `std::latch`
One-shot. For repeated phase sync, use `std::barrier`.

### Trap 3 — `latch` counter mismatch
`latch{N}` with fewer/more than N `count_down`s → `wait()` hangs forever, or
`count_down` past 0 is UB. Match exactly.

### Trap 4 — `barrier` with a thread that exits early
If a participating thread returns without `arrive_and_wait()` (and didn't
`arrive_and_drop()`), the others wait forever. Use `arrive_and_drop()` to leave.

### Trap 5 — semaphore `release` count drift
`release()` without a matching prior `acquire()` inflates the permit count → more
than `Max` concurrent later. Keep acquire/release balanced (RAII wrapper).

### Trap 6 — `jthread` callable taking `stop_token` by value vs not
`std::jthread([](std::stop_token st){...})` — the token is auto-injected as the
**first** argument. `std::jthread([]{...})` — no token; you must get it from the
`jthread` object or a `stop_source`.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`stop_token` cancels the thread" | Cooperative — the thread polls it; nothing forcible |
| "`latch` is reusable" | One-shot; `barrier` is the reusable one |
| "`barrier` and `latch` are the same" | `latch`: count to 0 once. `barrier`: reset each phase, + completion fn |
| "a semaphore is a mutex with a count" | Related, but `release` can be a different thread; it's for signalling |
| "`jthread` is just `thread` with a longer name" | + RAII join (no `terminate`) + built-in `stop_token` |
| "these are lower-level so slower" | Often **faster** than `mutex`+`cv` for their specific pattern (single futex) |

---

## Exercises

1. **Replace:** convert `std::thread t([&]{ while(!stop.load()){ tick(); } });
   ... stop.store(true); t.join();` to C++20.

   <details><summary>Answer</summary>

   ```cpp
   std::jthread t([](std::stop_token st){ while (!st.stop_requested()) tick(); });
   // ... t.request_stop();  (and ~jthread joins) — or just let it go out of scope
   ```
   Drops the `atomic<bool>`, the explicit `join()`, and the `terminate` risk.
   </details>

2. **latch or barrier:** (a) 8 workers must all finish loading before any starts
   processing; (b) a physics sim where all 8 threads finish step K before step
   K+1, for 1000 steps.

   <details><summary>Answer</summary>

   (a) `std::latch{8}` — each worker `count_down`s after loading; processing waits
   on it once. (b) `std::barrier{8}` — `arrive_and_wait()` after each step; it
   resets for the next step (a latch would need 1000 latches).
   </details>

3. **Semaphore use:** cap parallel HTTP requests to 10 at startup while loading
   many resources. Sketch with a `counting_semaphore` + a RAII guard.

   <details><summary>Answer</summary>

   ```cpp
   std::counting_semaphore<10> gate{10};
   struct Permit { std::counting_semaphore<10>& s; Permit(auto& g):s(g){ s.acquire(); }
                   ~Permit(){ s.release(); } };
   // per resource, on a pool thread:
   Permit p{gate};
   fetch(resource);        // at most 10 of these run concurrently
   ```
   </details>

4. **Start gun:** two `std::latch`es for a benchmark harness — describe the flow.

   <details><summary>Answer</summary>

   `latch ready{N}`, `latch go{1}`. Each worker: `ready.count_down(); go.wait();
   /* timed work */`. Main: `ready.wait();` (all threads spawned and parked),
   start the clock, `go.count_down();` (release all simultaneously), then `join`
   all and stop the clock. Removes thread-creation skew from the measurement.
   </details>

5. **Barrier completion fn:** in an 8-thread reduction, what goes in the
   completion function and why is it the right place?

   <details><summary>Answer</summary>

   Merge the 8 partial results into the global accumulator (and maybe check a
   convergence criterion / advance a phase counter). It runs exactly once, after
   all 8 have written their partials and before any resumes — so there's no race
   on the merge and no need for a separate lock or a "last thread does it" hack.
   </details>

---

## Interview questions

1. `std::jthread` `std::thread` ke kaunse do problems fix karta?
2. `stop_token` — cooperative kya matlab, blocked thread ka kya?
3. `std::latch` vs `std::barrier` — farq, ek-ek use case.
4. `std::barrier` ka completion function — kab chalta, kis liye ideal?
5. `std::counting_semaphore` vs `std::mutex` — semantic farq (`release` kaun call kar sakta)?
6. Ye primitives `mutex`+`cv` se faster kyun ho sakte (single futex)?
7. HFT hot path pe inme se kaunsa use hota (none — kyun)?

---

## Next
→ [`15-thread-local.md`](15-thread-local.md)
