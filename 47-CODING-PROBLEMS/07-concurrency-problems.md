# 07 — Concurrency problems

## Prerequisites
- `26-CONCURRENCY/` (threads, mutex, condition_variable, futures)
- `27-ATOMICS-MEMORY-MODEL/` (atomics, memory_order, happens-before)
- Data races / debugging: `45-DEBUGGING/08-debugging-multithreaded.md`

## Yeh file kya hai
25 problems — threads, mutexes, condition variables, futures, thread pools,
false sharing, deadlock. Lock-free stuff file 08 mein.

Har problem: `<details>` mein approach + the concurrency gotcha. Poora code →
[`11-solutions/07-concurrency-solutions.md`](11-solutions/07-concurrency-solutions.md).

**Test:** har multi-thread solution ko loop mein 100+ baar chalao; TSan (Linux/
WSL) ke neeche verify karo: `g++ -std=c++20 -fsanitize=thread -g file.cpp`.
MinGW pe TSan nahi — folder 45 dekho.

Compile: `g++ -std=c++20 -Wall -Wextra -pthread file.cpp -o t && ./t`

---

## Part A — Easy (~15 min)

### A1. Parallel sum
`std::vector<int64_t>` (10M elements) ko `N` threads mein baant ke sum karo,
partial sums join karke total.
`Pattern:` chunk the range, one accumulator per thread, no shared write.
<details><summary>Approach</summary>

Har thread apne `[lo, hi)` ka local sum banaye, `results[tid]` (alag cache line
ideally) mein likhe. Join ke baad main sab jodta. **Never** ek shared `total +=`
bina sync ke. Speedup memory-bandwidth-bound ho jaata bade arrays pe.
</details>

### A2. Data race demo + fix
Do threads ek `int counter` ko 1e6 baar `++` karte. Galat total dikhaao, phir
`std::mutex` se fix.
<details><summary>Approach</summary>

`++counter` = load-modify-store, non-atomic → lost updates (total < 2e6).
Fix: `std::lock_guard<std::mutex> lk(m); ++counter;`. Ya `std::atomic<int>`
(next problem). Race `-O0` pe zyada dikhta, `-O2` pe register promotion se chhup
sakta (folder 45).
</details>

### A3. atomic counter vs plain
Same as A2 par `std::atomic<int>` se — aur `fetch_add` ke memory orders.
<details><summary>Approach</summary>

`counter.fetch_add(1, std::memory_order_relaxed)` — sirf counting ke liye
relaxed kaafi (koi doosre data ko "publish" nahi kar rahe). Result exact.
`++counter` on atomic = `seq_cst` fetch_add (default) — correct par extra fences.
</details>

### A4. lock_guard / scoped_lock / unique_lock
Teeno kab. Do mutex ek saath lock karne ka sahi tareeka?
<details><summary>Answer</summary>

`lock_guard` — simplest RAII, ek mutex, no unlock/relock. `scoped_lock` — **ek
ya zyada** mutex, deadlock-free ordering (`std::lock` internally). `unique_lock`
— jab `unlock()`/`lock()` beech mein chahiye, `condition_variable` ke saath, ya
deferred/timed lock. Do mutex: `std::scoped_lock lk(m1, m2);` — never `lock_guard
lk1(m1); lock_guard lk2(m2);` (AB/BA deadlock).
</details>

### A5. Producer / consumer with condition_variable
Ek bounded buffer, ek producer ek consumer, `condition_variable` se.
`Pattern:` predicate loop, notify under/after lock.
<details><summary>Approach</summary>

`cv.wait(lk, [&]{ return !queue.empty() || done; });` — **predicate form**
(spurious wakeup + lost wakeup dono handle). Producer: push, `cv.notify_one()`.
Consumer loop exits jab `done && empty`. `if (…) cv.wait(lk);` ❌ (spurious
wakeup).
</details>

### A6. call_once
Ek expensive resource sirf ek baar init ho, chahe 10 threads race karein.
<details><summary>Approach</summary>

`std::once_flag flag; std::call_once(flag, []{ /* init */ });` — ek thread
chalaata, baaki wait. Init throw kare → flag reset, agla thread retry. Better
than double-checked locking (jo pre-C++11 buggy tha). Function-local `static`
bhi thread-safe init deta (C++11).
</details>

### A7. thread_local counter
Har thread apna `operations` count rakhe, `thread_local`. Sum kaise nikaaloge?
<details><summary>Approach</summary>

`thread_local int ops = 0;` — har thread ki apni copy, zero contention. Global
sum: har thread exit se pehle apna count ek shared atomic mein `fetch_add` kare,
ya registry mein pointer register kare. `thread_local` init lazy per thread.
</details>

### A8. jthread + stop_token
`std::jthread` (C++20) — auto-join, cooperative cancellation.
<details><summary>Approach</summary>

`std::jthread t([](std::stop_token st){ while (!st.stop_requested()) { … } });`
— `~jthread` calls `request_stop()` + `join()` (vs `std::thread` jo `~` pe
`std::terminate` agar joined/detached nahi). Cancellation cooperative — thread ko
`st` khud check karna.
</details>

---

## Part B — Medium (~30 min)

### B1. Bounded blocking queue
`BlockingQueue<T>` — `push` (full pe block), `pop` (empty pe block), `close()`.
`Pattern:` mutex + 2 condition variables.
<details><summary>Approach</summary>

`mutex m; condition_variable notFull, notEmpty; std::queue<T> q; size_t cap;
bool closed;`. `push`: `notFull.wait(lk, [&]{ return q.size() < cap || closed;
})`; push; `notEmpty.notify_one()`. `pop`: wait for `!q.empty() || closed`; pop;
`notFull.notify_one()`. `close()`: set flag, `notify_all()` both. Full worked in
solutions file.
</details>

### B2. Thread pool
`ThreadPool` — `submit(fn) -> future<R>`, fixed worker count, clean shutdown.
`Pattern:` task queue (B1) + worker loop + `packaged_task`.
<details><summary>Approach</summary>

Workers: loop `pop` a `std::function<void()>` from the queue, run it. `submit`:
`std::packaged_task<R()>` banao, `future` nikaalo, task ko `void()` lambda mein
wrap karke queue push. Shutdown: `close()` queue, `join` all workers. Trap:
`~ThreadPool` mein pending tasks ka policy (drain vs drop) decide karo.
</details>

### B3. Reader-writer lock
`std::shared_mutex` — kai readers ya ek writer. Writer starvation kab hota?
<details><summary>Approach</summary>

`std::shared_lock` (read) / `std::unique_lock` (write) on `std::shared_mutex`.
Readers ki continuous stream writer ko bhookha maar sakti (implementation-
dependent; `std::shared_mutex` fairness guarantee nahi deta). Fix: writer-priority
custom lock, ya read-copy-update (RCU) for read-heavy. Read-mostly + short
critical section → often a plain `mutex` faster (shared_mutex ka overhead zyada).
</details>

### B4. Deadlock — reproduce and fix
Do threads, `mutex a` aur `b`, ek `a→b` order mein leta doosra `b→a`. Hang.
Teen fixes.
<details><summary>Answer</summary>

Fixes: (1) **lock ordering** — sab jagah `a` phir `b` (address se sort). (2)
`std::scoped_lock lk(a, b);` — deadlock-free algorithm. (3) `std::lock(a, b)`
phir adopt. (4) `try_lock` + backoff (livelock risk). Detection: `gdb` `thread
apply all bt` → dono `__lll_lock_wait` mein (folder 45).
</details>

### B5. future / promise / async
Ek value ek thread compute kare, doosra `get()` kare — teen APIs.
<details><summary>Approach</summary>

`std::async(std::launch::async, fn)` → `future` (auto thread). `std::promise<T>
p; auto f = p.get_future();` — producer `p.set_value(x)` / `set_exception`.
`std::packaged_task` — callable + future. `future::get()` blocks; exception
producer se consumer ko propagate hoti. `std::async` ka future dtor **blocks**
(gotcha).
</details>

### B6. Parallel map
`parallel_transform(vec, fn)` — chunked across `hardware_concurrency()` threads.
<details><summary>Approach</summary>

`n / T` size ke chunks, har thread `std::transform` apne chunk pe (disjoint
output ranges → no sync). Small `n` → single thread (spawn cost). Uneven work →
smaller chunks + work queue. C++17: `std::transform(std::execution::par, …)`.
</details>

### B7. Spurious wakeup
Dikhaao `if (!ready) cv.wait(lk);` kyun buggy hai, `while` se fix.
<details><summary>Answer</summary>

`cv.wait` bina notify ke bhi return kar sakta (spurious) — POSIX/Windows allow.
`if` → thread aage badh jaata jab condition abhi false hai. `while (!ready)
cv.wait(lk);` (ya predicate overload) re-checks. Lost wakeup bhi: notify jo
`wait` se pehle aayi — predicate loop use pakadta kyunki pehle check hota.
</details>

### B8. barrier / latch
`std::latch` (one-shot) aur `std::barrier` (reusable) — phased computation.
<details><summary>Approach</summary>

`std::latch done{N};` — workers `done.count_down()`, main `done.wait()`.
`std::barrier b{N, completionFn};` — har phase ke end pe sab `b.arrive_and_wait()`,
last arriver `completionFn` chalata, phir sab release. Iterative solvers
(phase = one sweep).
</details>

### B9. False sharing — measure and fix
Do threads, do adjacent `std::atomic<long>` counters (same struct, same cache
line). Measure. `alignas(64)` se fix, re-measure.
<details><summary>Approach</summary>

Same 64B line pe do atomics → har `fetch_add` doosre core ki line invalidate
karta (cache-line ping-pong) → 3–10× slower. Fix: `struct alignas(64) Counter {
std::atomic<long> v; char pad[64 - sizeof(v)]; };`. Real numbers → folder 32,
`09-optimization-problems.md` #19.
</details>

### B10. try_lock_for
Ek thread lock 100ms tak try kare, na mile to kuch aur kaam kare.
<details><summary>Approach</summary>

`std::timed_mutex tm; std::unique_lock lk(tm, std::defer_lock); if
(lk.try_lock_for(100ms)) { … } else { /* fallback */ }`. Latency-sensitive code
mein "block forever" ki jagah bounded wait + degrade. `try_lock_until` for
absolute deadline.
</details>

### B11. Adaptive mutex (spin then block)
Sketch: pehle N iterations spin (`pause`), phir OS mutex pe fall back.
<details><summary>Approach</summary>

`for (int i = 0; i < 40; ++i) { if (flag.exchange(true, acquire) == false)
return; _mm_pause(); }` — phir `futex`/`std::mutex` wait. Short critical sections
pe spin syscall bachaata; long pe block CPU bachaata. glibc `pthread_mutex`
(`PTHREAD_MUTEX_ADAPTIVE_NP`) yehi. Measure — HFT often pure spinlock on a pinned
core.
</details>

---

## Part C — Hard (~45+ min)

### C1. SPSC queue (mutex version)
Ek producer, ek consumer, bounded. Mutex + cv se. (Lock-free version → file 08.)
<details><summary>Approach</summary>

Ring buffer + `mutex` + `notFull`/`notEmpty`. Correct but the mutex is the
bottleneck under load. Lock-free SPSC (file 08 #5) hot path pe ~20–40 ns vs
mutex ka ~50–200 ns + contention. Yeh baseline hai jise beat karna hai.
</details>

### C2. MPSC queue design
Many producers, one consumer. Design (mutex-based + note the lock-free option).
<details><summary>Answer</summary>

Mutex: ek `mutex` producers ke beech, consumer batch mein drain kare
(swap the whole queue under lock, process outside). Lock-free: **Vyukov MPSC** —
producers `exchange` the tail atomically (`XCHG`, no CAS loop), link the node;
consumer walks `head`. Intrusive nodes (payload owns `next`). Bounded version:
per-slot sequence numbers.
</details>

### C3. Correct double-checked locking
Singleton init with `std::atomic` + memory_order — bina C++11 ke classic bug.
<details><summary>Approach</summary>

`std::atomic<T*> inst; T* p = inst.load(std::memory_order_acquire); if (!p) {
lock; p = inst.load(relaxed); if (!p) { p = new T; inst.store(p,
std::memory_order_release); } }`. Acquire/release pair = constructor ke writes
doosre thread ko visible before pointer. Pre-C++11 (no ordering guarantees) →
readers ko half-constructed object dikhta. Bas `static T inst;` use karo (C++11
thread-safe).
</details>

### C4. Work-stealing deque
Design (Chase-Lev). Owner ek end se push/pop, thieves doosre end se steal.
<details><summary>Answer</summary>

Circular array + `top` (thieves, CAS) + `bottom` (owner, mostly plain).
Owner `push`/`pop` bottom pe — usually no atomics, sirf jab `top` ke paas
pahunche. `steal` — `top` ko CAS se badhao. Empty/single-element race ko CAS
handle karta. Growable array on overflow. Tokio / TBB / Cilk ka core. `top`/
`bottom` alag cache lines.
</details>

### C5. Pipeline shutdown
Stages A→B→C, har ek apne thread mein, queues se juda. Graceful stop.
<details><summary>Answer</summary>

**Poison pill**: A ke input queue `close()`; A apna kaam khatam karke ek sentinel
B ko bhejta, phir exit; B sentinel dekh ke drain+forward+exit; chain propagate.
Alternative: shared `atomic<bool> stop` + har stage bounded-wait poll — simpler
par in-flight items lost. Poison pill = no data loss, clean.
</details>

### C6. The ARM-only memory-ordering bug
Ek snippet jo x86 pe hamesha sahi chalta par ARM/POWER pe fail — kyun?
<details><summary>Answer</summary>

x86 = TSO (total store order): stores dusre threads ko program order mein dikhte,
sirf store→load reorder hota. `relaxed` atomics ya missing fences ka bug x86 pe
**chhup jaata**. ARM/POWER weakly ordered — store→store aur load→load bhi reorder.
Classic: publish `data` phir `flag`, reader `flag` dekh ke `data` padhe — bina
release/acquire ke ARM pe reader ko stale `data` milta. Fix: `flag.store(true,
release)` / `flag.load(acquire)`. Test intent pe likho, platform pe nahi. (Folder
27.)
</details>

---

## Next
→ [`08-lock-free-problems.md`](08-lock-free-problems.md)
