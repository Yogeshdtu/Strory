# 12 — Futures & promises: `future`, `promise`, `async`, `packaged_task`

## Prerequisites
- `03-std-thread.md`, `11-condition-variables.md`, `23-ERROR-HANDLING` (`exception_ptr`)
- [`examples/06_futures.cpp`](examples/06_futures.cpp)

## Yeh topic abhi kyun
`std::thread` deta hai execution, par **result wapas laane** ka koi built-in
rasta nahi (aur exception → `terminate`). `std::future` = "ek value jo baad mein
aayegi." Iske teen producers hain (`async`, `promise`, `packaged_task`), aur ek
consumer (`future::get`) jo blocks aur exception ko re-throw karta hai — clean
cross-thread results *and* errors.

---

## The pieces

| Type | Kya | Kaun set karta |
|---|---|---|
| **`std::future<T>`** | consumer end — `get()` once, blocks until ready | — |
| **`std::promise<T>`** | producer end — `set_value(v)` / `set_exception(ep)` | your code, manually |
| **`std::async`** | run a callable, get its `future` | the runtime |
| **`std::packaged_task<Sig>`** | wrap a callable so calling it fills a `future` | you call it (or a pool does) |
| **`std::shared_future<T>`** | copyable future — many consumers, `get()` many times | — |

A `promise`/`future` share a heap-allocated **shared state** (value slot + ready
flag + a CV). `promise` writes it, `future` reads it.

---

## `std::async`

```cpp
std::future<long> f = std::async(std::launch::async, heavy, arg);   // runs on a new thread
// ... do other work ...
long r = f.get();   // blocks until heavy() returns; moves the result out
```

- **`std::launch::async`** — run **now**, on a new thread.
- **`std::launch::deferred`** — run **lazily**, inside `get()`/`wait()`, on the
  **calling** thread (no new thread). If nothing calls `get()`, it never runs.
- **Default (`std::launch::async | std::launch::deferred`)** — implementation
  chooses. **Avoid the default** — you don't know if it's parallel; always pass
  `std::launch::async` explicitly.

### ⚠️ The `std::async` destructor blocks

```cpp
{
    std::async(std::launch::async, task);   // ⚠️ temporary future -> its dtor JOINS
}   // blocks here until task() finishes — often a surprise, kills parallelism
```

The `future` returned by `std::async` (with `async` policy) has a destructor that
**waits** for the task. If you don't keep the `future` alive, each `std::async`
call runs sequentially. Keep the futures in a container and `get()` them.

---

## `std::promise` / `std::future` — manual channel

```cpp
std::promise<std::string> p;
std::future<std::string>  f = p.get_future();

std::thread worker([pr = std::move(p)]() mutable {
    pr.set_value("done on the worker");        // or pr.set_exception(std::current_exception())
});

std::string r = f.get();   // blocks until set_value/set_exception
worker.join();
```

- `promise` is move-only; move it into the worker.
- Exactly one of `set_value` / `set_exception` — a second call throws
  `std::future_error(promise_already_satisfied)`.
- If the `promise` is destroyed without setting → `future::get()` throws
  `std::future_error(broken_promise)`.

Use when the result isn't a simple "call this function" — e.g. a callback-based
API, a value produced deep in some event handler.

---

## `std::packaged_task`

```cpp
std::packaged_task<long(long, long)> task([](long a, long b){ return a * b; });
std::future<long> f = task.get_future();

std::thread runner(std::move(task), 6, 7);   // invoking the task fills the future
long r = f.get();                             // 42
runner.join();
```

- Wraps a callable; **calling** the `packaged_task` runs it and stores the
  result/exception in the associated `future`.
- The building block of a **thread pool** (`examples/07`): `submit()` makes a
  `packaged_task`, returns its `future`, and enqueues "call this task" for a
  worker.

---

## Exception propagation — the killer feature

```cpp
std::future<int> f = std::async(std::launch::async, []() -> int {
    throw std::runtime_error("worker blew up");
});
try {
    int x = f.get();          // <-- re-throws the worker's exception HERE
} catch (const std::exception& e) {
    // "worker blew up" — caught on the calling thread
}
```

The worker's exception is captured (`std::exception_ptr`) into the shared state
and **re-thrown by `get()`** on the consumer's thread. Contrast `std::thread`,
where an uncaught exception = instant `std::terminate`. For "do work on another
thread and I need the result or the error," `async`/`packaged_task` is the clean
tool.

---

## `std::shared_future` — many consumers

```cpp
std::shared_future<Config> sf = std::async(std::launch::async, load_config).share();
// pass sf (by copy) to many threads; each can .get() (returns a const ref)
```

`future` is move-only, `get()` once. `.share()` converts to `shared_future`
(copyable, `get()` returns `const T&`, callable by many threads). Use for a
"compute once, many readers" result.

---

## Andar kya hota hai

- Shared state: heap block with `{ value-or-exception slot, ready atomic/flag,
  mutex + CV }`. `set_value` writes the slot, sets ready, `notify_all`.
  `future::get` — `wait()` on the CV until ready, then move the value out (or
  re-throw the stored `exception_ptr`).
- `std::async(async)` — creates a `std::thread` internally (or draws from an
  implementation pool), running a `packaged_task`-like wrapper; the returned
  `future` holds a handle whose destructor `join`s.
- `std::async(deferred)` — stores the callable in the shared state; `get()`/`wait`
  invokes it inline on the calling thread the first time.
- `packaged_task` — its `operator()` does `try { state.set_value(f(args...)); }
  catch (...) { state.set_exception(std::current_exception()); }`.

---

## > **HFT relevance**
> - **Futures are a control-plane / setup tool, not a hot-path tool.** `future::get`
>   is a CV wait (futex sleep) and the shared state is a heap allocation with a
>   mutex — µs-scale, allocating, blocking. Fine for "kick off symbology download
>   and config load in parallel at startup, then `get()` both."
> - **`packaged_task` powers the thread pool** (`examples/07`) used for cold
>   parallel work (backtests, analytics, snapshot generation).
> - **On the hot path, results flow over lock-free queues**, not futures — no
>   allocation, no blocking, no CV.
> - **Always `std::launch::async` explicitly**; never rely on the default policy.
> - **Beware the `std::async` future destructor** — a stray temporary serializes
>   your "parallel" launches. Keep the futures.
> - **`exception_ptr` for moving errors across threads** even without a future —
>   the hot path pushes an error *value* to a queue; a worker that must surface a
>   C++ exception to another thread uses `std::current_exception` /
>   `std::rethrow_exception` (folder 23 file 03).

---

## Hands-on

```bash
./build.ps1 26-CONCURRENCY/examples/06_futures.cpp
./build.ps1 fast 26-CONCURRENCY/examples/07_thread_pool.cpp   # packaged_task-based pool
```

`06` covers all five: `async(async)`, `async(deferred)`, `promise`/`future`,
exception through `get()`, `packaged_task`. Then:
- `{ std::async(std::launch::async, slow); std::async(std::launch::async, slow); }`
  — time it; the two run **sequentially** (each temporary future's dtor blocks).
  Fix by keeping the futures.
- Make a worker `promise.set_exception(std::make_exception_ptr(std::runtime_error
  ("x")))` and catch it on `get()`.

---

## ⚠️ Traps

### Trap 1 — the default `std::async` policy
```cpp
auto f = std::async(task);   // ⚠️ might be deferred (runs on your thread, at get())
auto f = std::async(std::launch::async, task);   // ✅ explicit
```

### Trap 2 — not keeping the `std::async` future
```cpp
std::async(std::launch::async, a);   // ⚠️ temporary dtor blocks -> not parallel with the next
std::async(std::launch::async, b);
```
Store both in variables / a `vector`, then `get()`.

### Trap 3 — `future::get()` twice
`get()` moves the value out; a second `get()` throws `std::future_error`. Use
`shared_future` for multiple reads.

### Trap 4 — `promise` destroyed without setting
`future::get()` throws `broken_promise`. Ensure every path sets the value or an
exception.

### Trap 5 — futures on the hot path
`get()` is a blocking CV wait; the shared state allocates. Lock-free queue instead.

### Trap 6 — `packaged_task` invoked twice
Second call throws. It's one-shot; make a new one per task.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`std::async(task)` runs it on another thread" | Default policy may **defer** — runs on your thread at `get()` |
| "the `std::async` future is fire-and-forget" | Its destructor **blocks** (with `async` policy) — keep it |
| "`future::get()` can be called repeatedly" | Once (move-out); `shared_future` for many |
| "an exception in an `async` task terminates" | It's stored and re-thrown by `get()` — clean cross-thread error |
| "futures are lightweight enough for per-tick use" | Allocating shared state + a CV wait — control plane only |
| "`packaged_task` is reusable" | One-shot per invocation |

---

## Exercises

1. **Sequential surprise:** `for (auto& u : urls) std::async(std::launch::async,
   fetch, u);` — why is this not parallel, and the fix?

   <details><summary>Answer</summary>

   Each `std::async` returns a temporary `future` whose destructor (async policy)
   blocks until `fetch` finishes — so the loop runs `fetch` calls one at a time.
   Fix: `std::vector<std::future<...>> fs; for (auto& u : urls)
   fs.push_back(std::async(std::launch::async, fetch, u)); for (auto& f : fs)
   use(f.get());`.
   </details>

2. **promise/future:** a legacy API `void register_cb(std::function<void(Result)>)`.
   Adapt it to `std::future<Result>`.

   <details><summary>Answer</summary>

   ```cpp
   auto p = std::make_shared<std::promise<Result>>();
   std::future<Result> f = p->get_future();
   register_cb([p](Result r){ p->set_value(std::move(r)); });
   return f;
   ```
   (`shared_ptr` so the promise outlives this function until the callback fires;
   handle the error path with `set_exception`.)
   </details>

3. **Exception:** worker computes a value but may throw. Show both the `promise`
   and the consumer side.

   <details><summary>Answer</summary>

   ```cpp
   // worker
   try { pr.set_value(compute()); }
   catch (...) { pr.set_exception(std::current_exception()); }
   // consumer
   try { auto v = fut.get(); use(v); }
   catch (const std::exception& e) { handle(e); }
   ```
   </details>

4. **Which tool:** (a) run 3 independent startup tasks in parallel and wait for
   all; (b) a thread pool's `submit`; (c) one result computed by a background
   thread, read by 10 threads; (d) per-tick order results back to the strategy.

   <details><summary>Answer</summary>

   (a) `std::async(std::launch::async, ...)` × 3, keep the futures, `get()` each.
   (b) `std::packaged_task` + a queue. (c) `std::shared_future` (`.share()`).
   (d) **not** futures — a lock-free result queue the strategy polls.
   </details>

5. **`shared_future`:** why does `future::get()` return `T` (by move) but
   `shared_future::get()` return `const T&`?

   <details><summary>Answer</summary>

   `future` is single-consumer, single `get()` — it can move the value out of the
   shared state. `shared_future` allows many `get()`s from many threads — the
   value must stay in the shared state, so `get()` hands out a `const T&` (all
   callers see the same object).
   </details>

---

## Interview questions

1. `std::future` / `std::promise` — do ends of kya (shared state)?
2. `std::async` launch policies — `async` / `deferred` / default; default kyun avoid?
3. `std::async` ka future destructor — kya karta, kya surprise?
4. Worker ka exception `future::get()` pe kaise aata (`std::thread` se farq)?
5. `std::packaged_task` — kya, thread pool mein role?
6. `future` vs `shared_future` — `get()` semantics.
7. Futures hot path pe kyun nahi (allocation + CV wait)?

---

## Next
→ [`13-thread-pool.md`](13-thread-pool.md)
