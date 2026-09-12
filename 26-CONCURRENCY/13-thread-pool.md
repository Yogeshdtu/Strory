# 13 — Build a thread pool

## Prerequisites
- `11-condition-variables.md`, `12-futures-and-promises.md`, `08-lock-guards.md`
- [`examples/07_thread_pool.cpp`](examples/07_thread_pool.cpp)

## Yeh topic abhi kyun
Thread aur process creation mehnga hai (file 02). Ek server / engine ko **per
task** thread nahi banana chahiye — banao **N long-lived threads once**, unke saamne
ek **task queue** rakho, kaam aaye to enqueue karo. Yeh "thread pool" pattern har
serious concurrent system mein hai. Isse **apne haath se** banana concurrency ke
saare pieces ek jagah use karta hai.

---

## The design

```
submit(task) ──► [ mutex-protected task queue ] ◄── worker 1 (loop: wait cv, pop, run)
                        │  cv.notify_one()      ◄── worker 2
                        │                        ◄── worker 3
                     shutdown flag              ◄── worker N
```

Pieces:
- A **`std::queue<std::function<void()>>`** of type-erased jobs.
- A **`std::mutex`** protecting the queue + a **`std::condition_variable`** so idle
  workers sleep.
- A **`bool stop_`** flag for shutdown.
- **`submit(f, args...)`** — wraps `f(args...)` in a `std::packaged_task`, returns
  its `std::future`, enqueues "call the task", `notify_one`.
- Workers loop: `cv.wait(lk, [&]{ return stop_ || !tasks_.empty(); })`, pop, run.
- **Destructor**: set `stop_`, `notify_all`, `join` all workers.

`examples/07_thread_pool.cpp` is exactly this in ~60 lines.

---

## `submit` — the interesting part

```cpp
template <class F, class... Args>
auto submit(F&& f, Args&&... args)
    -> std::future<std::invoke_result_t<F, Args...>> {
    using R = std::invoke_result_t<F, Args...>;

    auto task = std::make_shared<std::packaged_task<R()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    std::future<R> fut = task->get_future();
    {
        std::lock_guard lk(m_);
        if (stop_) throw std::runtime_error("submit on stopped pool");
        tasks_.emplace([task] { (*task)(); });   // type-erase into std::function<void()>
    }
    cv_.notify_one();
    return fut;
}
```

- **`std::invoke_result_t<F, Args...>`** — deduce the return type `R`.
- **`std::packaged_task<R()>`** in a **`shared_ptr`** — `std::function` requires a
  copyable target; `packaged_task` is move-only, so wrap it in `shared_ptr` and
  capture that (copyable) in the `std::function<void()>` lambda.
- The caller gets a `std::future<R>` — exception propagation for free (file 12).

C++23's `std::move_only_function` removes the `shared_ptr` dance (a move-only
`std::function`).

---

## The worker loop

```cpp
void worker_loop() {
    for (;;) {
        std::function<void()> job;
        {
            std::unique_lock lk(m_);
            cv_.wait(lk, [this] { return stop_ || !tasks_.empty(); });
            if (stop_ && tasks_.empty()) return;   // drained -> exit
            job = std::move(tasks_.front());
            tasks_.pop();
        }
        job();                                     // run OUTSIDE the lock
    }
}
```

- Run the job **outside** the lock (file 06) — else all workers serialize.
- On `stop_`: workers finish the remaining queued tasks, then exit (drain
  semantics). A "cancel pending" variant just returns immediately.
- A job that throws → the exception is caught by the `packaged_task` and stored in
  the future; the worker keeps going. (If you use raw lambdas, wrap `job()` in a
  `try/catch` so one bad job doesn't kill a worker.)

---

## Shutdown

```cpp
~ThreadPool() {
    { std::lock_guard lk(m_); stop_ = true; }
    cv_.notify_all();                 // wake every idle worker
    for (auto& w : workers_) w.join();
}
```

- Non-copyable, non-movable (owns threads + a mutex + a CV).
- `join` in the destructor → the pool's lifetime bounds the workers'.

---

## Sizing

| Workload | Pool size |
|---|---|
| CPU-bound | `hardware_concurrency()` (or `-1` to leave a core for the main thread) |
| I/O-bound (threads mostly wait) | more than cores — tune to the I/O concurrency |
| Mixed | separate pools, or a work-stealing scheduler |

`examples/07` (fib × 64, CPU-bound, 8 cores): serial 271 ms → pool(8) **47.6 ms**
(~5.7×). `std::async`-per-task: 51.8 ms — the pool edges it (no per-task thread
create/destroy).

---

## Beyond the basics (production pools)

- **Multiple queues + work stealing** (one deque per worker; idle workers steal
  from others' tails) — much less contention on a single queue. `tbb`,
  `folly::CPUThreadPoolExecutor`, `taskflow`.
- **Bounded queue + backpressure** — reject / block submitters when the queue is
  full (don't let it grow unbounded).
- **Priority queues** — latency-sensitive tasks first.
- **Affinity** — pin workers to cores.
- **Batching** — pop N tasks under one lock acquisition.

---

## > **HFT relevance**
> - **The hot path is NOT a thread pool** — it's one pinned thread polling a
>   lock-free queue. A pool's mutex + CV + `std::function` allocation are all
>   jitter sources.
> - **Pools are for cold parallel work** — backtests, EOD analytics, order-book
>   snapshot generation for a dashboard, log compression. Size to cores, pin the
>   workers away from the hot cores.
> - **If you build a pool for anything latency-ish**, replace the single
>   `std::mutex` queue with per-worker lock-free deques + work stealing, and
>   `std::move_only_function` (C++23) or a hand-rolled small-task type instead of
>   `std::function` (no heap alloc for small callables).
> - **Bounded queue always** — an unbounded task queue is a latent OOM / latency
>   blow-up under a burst.

---

## Hands-on

```bash
./build.ps1 fast 26-CONCURRENCY/examples/07_thread_pool.cpp
```

Serial vs `std::async`-per-task vs `ThreadPool(hw)` on fib × 64. Then:
- Add a `try/catch` around `job()` and submit a task that throws — the worker
  survives, the future re-throws.
- Change the destructor to **not** drain (return immediately on `stop_`) — observe
  some futures never complete (`broken_promise` on `get()`).
- Replace the single queue with two queues (odd/even worker index) and split
  submissions — measure the contention drop with more workers.

---

## ⚠️ Traps

### Trap 1 — running the job while holding the lock
All workers serialize on the lock for the job's whole duration. Pop under the
lock, run outside.

### Trap 2 — `std::function` can't hold a `packaged_task`
`packaged_task` is move-only; `std::function` needs copyable. Wrap in `shared_ptr`
(or use `std::move_only_function`, C++23).

### Trap 3 — a throwing job kills the worker
With raw lambdas, an uncaught exception in `job()` propagates out of the worker
thread → `std::terminate`. `try/catch` around `job()` (or use `packaged_task`,
which stores it).

### Trap 4 — unbounded task queue
A burst of submissions grows the queue without limit → memory blow-up, unbounded
latency. Bound it + apply backpressure.

### Trap 5 — `submit` after shutdown
Enqueuing onto a stopped pool → the task never runs → `future::get()` hangs or
throws `broken_promise`. Check `stop_` in `submit` and throw / return an error.

### Trap 6 — one giant queue with many workers
The single mutex is the bottleneck at high task rates. Per-worker deques + work
stealing.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "spawn a thread per task" | Create N threads once; queue tasks to them |
| "run the job under the queue lock" | Pop under the lock, execute outside |
| "`std::function` holds any callable" | Copyable only — `packaged_task` needs `shared_ptr` / `move_only_function` |
| "the queue can grow as needed" | Bound it + backpressure or you get OOM / latency spikes |
| "one worker crashing is contained" | Uncaught exception → `std::terminate` for the process; `try/catch` the job |
| "a pool is fine for the hot path" | Mutex + CV + alloc = jitter; hot path = one pinned thread + lock-free queue |

---

## Exercises

1. **Return type:** why does `submit` use `std::invoke_result_t<F, Args...>` and
   `std::packaged_task<R()>` (nullary), not `std::packaged_task<R(Args...)>`?

   <details><summary>Answer</summary>

   The arguments are bound *now* (via `std::bind` / a lambda capture) so the task
   the worker calls takes no arguments — it's `R()`. The worker doesn't know the
   argument types; it just calls `(*task)()`. `std::invoke_result_t` computes `R`
   from `F` and the arg types at the call site.
   </details>

2. **`shared_ptr` dance:** what breaks if you `tasks_.emplace([task = std::move(pt)]
   { pt(); })` with `pt` a bare `std::packaged_task`?

   <details><summary>Answer</summary>

   The lambda captures a move-only `packaged_task`, making the lambda move-only,
   which `std::function<void()>` can't store (it requires a copyable target) →
   compile error. Wrapping in `std::shared_ptr` makes the capture copyable. (C++23
   `std::move_only_function` accepts the move-only lambda directly.)
   </details>

3. **Drain vs cancel:** the destructor currently finishes queued tasks. Write the
   "cancel pending" variant and note the consequence for outstanding futures.

   <details><summary>Answer</summary>

   Worker loop: `if (stop_) return;` (don't check `tasks_.empty()`). Destructor:
   `stop_ = true; notify_all(); join();` — pending tasks are dropped. Their
   `packaged_task`s are destroyed unset → `future::get()` on those throws
   `std::future_error(broken_promise)`. Document it, or drain by default.
   </details>

4. **Sizing:** you have 16 logical cores. Your tasks each do ~1 ms of CPU then
   ~5 ms waiting on a network call. Pool size?

   <details><summary>Answer</summary>

   More than 16 — while a thread waits on the network, a core is idle. Roughly
   `cores × (1 + wait/compute) = 16 × (1 + 5/1) = ~96` to keep the cores busy
   (tune empirically). For a pure CPU pool it'd be ~15–16. Mixed workloads often
   want separate pools.
   </details>

5. **Contention:** at 5M tasks/sec with 8 workers, the single-queue mutex is the
   bottleneck. Sketch the fix.

   <details><summary>Answer</summary>

   Per-worker lock-free (Chase-Lev style) deques: a worker pushes/pops its own
   deque's bottom (no contention); when empty, it **steals** from another
   worker's top (a single CAS). Submissions round-robin or go to the least-loaded
   deque. Contention drops from "all 8 on one lock" to "occasional CAS on a steal."
   (Or shard: K queues, each with its own mutex, K > workers.)
   </details>

---

## Interview questions

1. Thread pool kyun (per-task thread creation cost)?
2. `submit` ka return type kaise deduce hota, `packaged_task` `shared_ptr` mein kyun?
3. Worker loop — job ko lock ke andar ya bahar chalana, kyun?
4. Shutdown — drain vs cancel; outstanding futures ka kya?
5. Pool sizing — CPU-bound vs I/O-bound.
6. Single queue ka bottleneck at high task rates — work stealing kaise help karta?
7. HFT hot path pool kyun nahi use karta?

---

## Next
→ [`14-cpp20-sync.md`](14-cpp20-sync.md)
