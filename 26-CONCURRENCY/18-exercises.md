# 18 — Exercises: concurrency

## Prerequisites
- Poora folder 26 (files 01–17)

## Yeh file kya hai
Practice — output / behaviour prediction, "find the bug", design, aur ek build
challenge. Answers `<details>` mein. Compile: `./build.ps1 file.cpp` (or add
`-pthread`). Benchmarks: `./build.ps1 fast file.cpp`. Races (Linux):
`-fsanitize=thread`.

---

## Part A — Output / behaviour prediction

### A1
```cpp
{ std::thread t([]{ std::puts("run"); }); }
std::puts("after");
```
<details><summary>Answer</summary>

The program **terminates** — `~std::thread` runs with `joinable() == true` →
`std::terminate` (`terminate called without an active exception`, abort). "run"
may or may not print first. Fix: `t.join();` or use `std::jthread`.
</details>

### A2
```cpp
long c = 0;
std::vector<std::thread> ts;
for (int i = 0; i < 4; ++i) ts.emplace_back([&]{ for (int k=0;k<100000;++k) ++c; });
for (auto& t : ts) t.join();
std::printf("%ld\n", c);
```
<details><summary>Answer</summary>

Some value **< 400000**, different each run — a data race on `c` (load-add-store
interleaving → lost updates). It's also UB. Fix: `std::atomic<long> c`, or per-
thread partials + combine.
</details>

### A3
```cpp
std::async(std::launch::async, []{ std::this_thread::sleep_for(50ms); std::puts("a"); });
std::async(std::launch::async, []{ std::this_thread::sleep_for(50ms); std::puts("b"); });
```
<details><summary>Answer</summary>

Takes ~100 ms, not ~50 — each `std::async` returns a temporary `future` whose
destructor (async policy) **blocks** until the task finishes, so they run
sequentially. Keep the futures in variables to get parallelism.
</details>

### A4
```cpp
std::mutex m; std::condition_variable cv; bool ready = false;
std::thread w([&]{ std::unique_lock lk(m); cv.wait(lk); std::puts("woke"); });
ready = true;
cv.notify_one();
w.join();
```
<details><summary>Answer</summary>

Likely **hangs** (`w.join()` never returns). Two bugs: bare `cv.wait(lk)` (no
predicate), and `ready = true` set outside the lock. The `notify_one` can fire
before `w` is inside `wait` → lost wakeup → `w` sleeps forever. Fix: set `ready`
under `m`, and `cv.wait(lk, [&]{ return ready; });`.
</details>

### A5
```cpp
struct alignas(64) C { std::atomic<long> v{0}; char pad[64 - sizeof(std::atomic<long>)]; };
C counters[8];
// vs
struct P { std::atomic<long> v[8]; } packed;
// 8 threads, each hammers its own counter 50M times
```
<details><summary>Answer</summary>

The `alignas(64)` array is ~**10× faster** (`examples/08` measured 9.96×). Each
padded counter is on its own cache line → no cross-core invalidation. In `packed`,
all 8 share ~1 line → every `fetch_add` bounces the line between cores.
</details>

### A6
```cpp
std::jthread j([](std::stop_token st){
    int n = 0;
    while (!st.stop_requested()) { ++n; std::this_thread::sleep_for(1ms); }
    std::printf("n=%d\n", n);
});
std::this_thread::sleep_for(25ms);
// no explicit request_stop / join
```
<details><summary>Answer</summary>

At scope exit, `~jthread` calls `request_stop()` then `join()` → the loop sees
`stop_requested()` and exits, printing `n≈25`. No `terminate`, no manual
cleanup. That's the `jthread` + `stop_token` win over `std::thread`.
</details>

---

## Part B — Find the bug

### B1
```cpp
class Counter {
    long n_ = 0;
    std::mutex m_;
public:
    void inc() { std::lock_guard lk(m_); ++n_; }
    long get() const { return n_; }              // <-- ?
};
```
<details><summary>Answer</summary>

`get()` reads `n_` without the lock → it races with `inc()`'s write (data race /
UB — torn or stale read). Lock in `get()` too (needs `m_` mutable, and drop
`const` or make `m_` `mutable`), or make `n_` `std::atomic<long>`.
</details>

### B2
```cpp
void transfer(Account& a, Account& b, long amt) {
    std::lock_guard la(a.m);
    std::lock_guard lb(b.m);
    a.bal -= amt; b.bal += amt;
}
```
Called as `transfer(x, y, 100)` on one thread and `transfer(y, x, 50)` on another.
<details><summary>Answer</summary>

ABBA deadlock — thread 1 locks `x.m` then waits for `y.m`; thread 2 locks `y.m`
then waits for `x.m`. Fix: `std::scoped_lock lk(a.m, b.m);` (deadlock-free), or
lock by address order (`&a.m < &b.m ? ...`).
</details>

### B3
```cpp
std::vector<int> shared;
void producer() { for (int i=0;i<1000;++i) shared.push_back(i); }
void consumer() { for (int x : shared) process(x); }
// run producer and consumer on two threads
```
<details><summary>Answer</summary>

Multiple bugs: `push_back` mutates `shared` (may reallocate) while `consumer`
iterates it → data race + iterator invalidation → crash / garbage. `std::vector`
isn't thread-safe for concurrent mutation. Fix: a mutex around all access, or a
bounded lock-free / cv-based queue (`examples/05`), or produce fully then consume.
</details>

### B4
```cpp
std::atomic<bool> ready{false};
Data* g_data = nullptr;
// producer:
g_data = new Data(compute());
ready.store(true, std::memory_order_relaxed);
// consumer:
while (!ready.load(std::memory_order_relaxed)) {}
use(*g_data);
```
<details><summary>Answer</summary>

`g_data` is a plain pointer (data race) *and* `relaxed` gives no ordering — the
consumer can observe `ready == true` while `g_data` is still `nullptr` or points
at a partially-constructed `Data`. Fix: `std::atomic<Data*> g_data;`, producer
`g_data.store(p, std::memory_order_release)`, consumer `g_data.load(std::memory_
order_acquire)` (folder 27).
</details>

### B5
```cpp
ThreadPool pool(8);
for (auto& job : jobs) pool.submit(job);
// pool destructor runs here
```
`ThreadPool::submit` wraps `job` in a lambda and enqueues it; workers run jobs
outside the lock. One `job` throws.
<details><summary>Answer</summary>

If `submit` uses `std::packaged_task`, the exception is stored in the (discarded)
future and the worker survives — bug is only that nobody sees the error. If
`submit` enqueues a raw lambda and the worker does `job();` with no `try/catch`,
the uncaught exception propagates out of the worker thread → `std::terminate` for
the whole process. Fix: `try { job(); } catch (...) { log/store; }` in the worker
loop (or use `packaged_task` and actually check the futures).
</details>

### B6
```cpp
std::shared_mutex sm;
Config cfg;
Config read() { std::lock_guard lk(sm); return cfg; }   // <-- ?
void write(Config c) { std::unique_lock lk(sm); cfg = std::move(c); }
```
<details><summary>Answer</summary>

`read()` uses `std::lock_guard` on the `shared_mutex` → that takes the
**exclusive** lock, so readers serialize with each other (defeating the point of
`shared_mutex`). Use `std::shared_lock lk(sm);` in `read()`. (And consider whether
a `shared_mutex` beats a snapshot + atomic pointer here anyway — file 10.)
</details>

---

## Part C — Design

### C1
A market-data handler decodes 2M msgs/sec on one pinned thread and must feed
(a) a strategy engine and (b) an audit logger. Neither consumer can slow the
decoder. Design the data flow.

<details><summary>Answer</summary>

Decoder thread: pure, `noexcept`, single-threaded, no locks. For each message it
writes a compact record into **two** lock-free SPSC ring buffers (one per
consumer) — a bounded `memcpy` into a slot + an atomic publish of the write index
(release store). Strategy thread and logger thread each busy-poll their own ring
(acquire load of the write index), process, advance their read index. Rings are
`alignas(64)` on head/tail to avoid false sharing. If a ring fills (consumer too
slow), the decoder either drops + counts (audit) or applies backpressure
(strategy) — a policy decision, but the decoder never blocks on a lock or a CV.
</details>

### C2
You need a per-thread scratch arena (256 KB) for a decode path run by a pool of
16 threads. `thread_local` or a passed-in `Context&`? Justify, and note the
memory cost.

<details><summary>Answer</summary>

For a hot, well-structured decode path: **`Context&`** — bundle the arena +
buffers into a struct, construct one per worker at startup, pass `Context&` into
`decode()`. No TLS addressing, no first-use guard, explicit lifetime, trivially
testable. `thread_local` is acceptable if the arena is accessed from many
scattered helpers where threading a parameter is ugly. Either way: 16 × 256 KB =
4 MB reserved — fine, but budget it, and if it were `thread_local` on a large
pool you'd pay 4 MB whether or not every thread decodes.
</details>

### C3
Explain why the HFT hot path takes **no locks**, in terms of the specific costs a
`std::mutex` can impose, and what replaces it.

<details><summary>Answer</summary>

An uncontended `std::mutex` is ~15 ns (an atomic CAS) — tolerable. But under real
load it *will* contend, and a contended lock is: a futex syscall + a context
switch to sleep + another to wake + the lock-word cache line bouncing between
cores — µs-scale and non-deterministic (a P99/P99.9 spike). You can't guarantee
"uncontended" under production volume. Replacement: the hot thread shares no
mutable state — data arrives on lock-free SPSC queues it polls; read-mostly shared
state (config, limits) is an immutable snapshot swapped via an atomic pointer, so
readers do one atomic load and never wait.
</details>

### C4
A pool's single-mutex task queue is the bottleneck at 5M tasks/sec across 12
workers. Sketch two designs that reduce contention.

<details><summary>Answer</summary>

(1) **Sharded queues**: K > 12 queues, each with its own mutex; submitters
round-robin; workers pull from an assigned subset. Contention drops ~K-fold.
(2) **Per-worker lock-free deques + work stealing** (Chase-Lev): each worker
pushes/pops its own deque's bottom with no atomics-vs-others contention; when
empty it steals from another's top with a single CAS. Submissions go to the
least-loaded deque. This is what TBB / folly / Go's scheduler do.
</details>

### C5
Give the memory-model / publication rule for handing a freshly-built `OrderBook`
snapshot from a builder thread to reader threads, and why `relaxed` isn't enough.

<details><summary>Answer</summary>

Builder: fully construct the `OrderBook`, then `g_book.store(p, std::memory_order_
release)`. Readers: `auto* b = g_book.load(std::memory_order_acquire); use(*b);`.
The release store guarantees all the writes that built `*p` are visible *before*
the pointer becomes visible; the acquire load pairs with it so the reader that
sees the new pointer also sees the fully-built object. With `relaxed` on both,
there's no ordering — a reader could see the new pointer but stale/garbage book
fields (a publication bug). (Reclamation of the old snapshot: `shared_ptr`
refcount, or an epoch/grace-period scheme — folder 28.)
</details>

---

## Part D — Challenge

### D1 — Build a lock-free-ish SPSC queue and measure false sharing

Implement a **single-producer, single-consumer** bounded ring buffer of a
trivially-copyable `T` (e.g. a 32-byte `Msg`), power-of-two capacity:

- `bool try_push(const T&)` — fails if full.
- `bool try_pop(T&)` — fails if empty.
- `head_` written only by the producer, `tail_` written only by the consumer,
  each `std::atomic<size_t>`.
- Producer publishes with a **release** store to `head_`; consumer reads it with
  **acquire**. Symmetrically for `tail_`.

Then benchmark throughput (msgs/sec) in two layouts:
1. `head_` and `tail_` adjacent in the struct (no padding).
2. `alignas(64)` on the `head_` block and the `tail_` block (and pad the ring
   slots if `T` is small).

Report the ratio. Predict first (hint: look at `examples/08`).

<details><summary>Hints</summary>

- Capacity `N` power of two → `idx & (N-1)` wrap.
- `try_push`: `h = head_.load(relaxed); if (h - tail_.load(acquire) == N) return
  false; buf_[h & mask] = v; head_.store(h + 1, release); return true;`
- `try_pop`: mirror with `tail_`.
- Benchmark: producer thread pushes M messages, consumer pops M, time it; use two
  `std::latch`es for a clean start (file 14).
- Expect the unpadded version to be several× slower — `head_`/`tail_` on one line
  means every push invalidates the consumer's `tail_` line and vice versa.
- This is the SPSC queue folder 28/41 builds on; `examples/05` is the
  cv-based (blocking) cousin.
</details>

### D2 — Race hunt

Take `examples/02_race_condition.cpp` and, without changing the thread count or
iteration count, produce **three** correct versions and rank them by speed at
`-O2` (predict, then measure):

1. `std::mutex` + `std::lock_guard`.
2. `std::atomic<long>` + `fetch_add(1, std::memory_order_relaxed)`.
3. Per-thread local sum + a single combine at the end.

Then, on Linux, run version 0 (the original racy one) under
`-fsanitize=thread` and paste what TSan reports (the two racing accesses).

<details><summary>What you should find</summary>

Speed: **local+combine ≫ atomic > mutex** (`examples/03` measured 1.9 ms / 430 ms
/ 1248 ms). Mutex serializes every increment through a contended lock; atomic
avoids the lock but every `fetch_add` is a cache-line ping-pong on the one shared
word; local+combine has no shared write in the hot loop at all. TSan on the racy
version names the two `++counter` sites (a read and a write) with both threads'
stacks and flags it as a data race.
</details>

---

## Next
→ [`../27-ATOMICS-MEMORY-MODEL/00-README.md`](../27-ATOMICS-MEMORY-MODEL/00-README.md)
