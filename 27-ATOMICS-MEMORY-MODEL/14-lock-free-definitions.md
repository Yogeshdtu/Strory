# 14 — Lock-free, wait-free, obstruction-free

## Prerequisites
- `05-compare-exchange.md`, `08-acquire-release.md`
- `26-CONCURRENCY` file 06 (mutex), file 11 (deadlock)

## Yeh topic abhi kyun
"Lock-free" ek precise **progress guarantee** hai — "mutex nahi use kiya" nahi.
Folder 28 poora lock-free structures banata hai; usse pehle exact definitions
chahiye: lock-free, wait-free, obstruction-free kya matlab, aur ek algorithm
"lock-free hai ya nahi" kaise decide hota hai.

---

## Progress guarantees — strongest to weakest

### Wait-free
**Every** thread completes its operation in a **bounded** number of its own steps,
regardless of what other threads do (including being stalled, descheduled, or
running full-speed). No thread ever waits for another. Strongest, hardest, often
slowest in the common case (extra bookkeeping for the worst case).

Examples: `fetch_add` on a counter (one `lock xadd` — bounded). A single-word
atomic read/write. Wait-free queues exist (Kogan–Petrank) but are complex.

### Lock-free
**At least one** thread makes progress in a bounded number of steps of the whole
system. Individual threads can starve (retry forever), but the system as a whole
always advances — no deadlock, no livelock of *everyone*.

Examples: Treiber stack, Michael–Scott queue — a CAS loop where a failure means
*someone else succeeded*.

### Obstruction-free
A thread makes progress in bounded steps **if it runs alone** (all other threads
suspended) for long enough. With contention it may livelock (mutual aborts).
Weakest. Basis for STM; rare in practice — usually paired with a back-off
"contention manager".

### (Blocking / lock-based)
A thread can be blocked indefinitely by another (holding a mutex, then getting
descheduled / crashing). Everything with `std::mutex` is here.

```
wait-free   ⊂   lock-free   ⊂   obstruction-free   ⊂   (everything)
per-thread      system-wide     solo progress
bounded         bounded
```

---

## Key consequence: no thread can block another via the OS

A lock-free algorithm must never do anything where **being descheduled while
"holding" something stalls everyone else**:

- ❌ `std::mutex` / any OS lock on the shared path.
- ❌ blocking syscalls, `new`/`delete` (may lock the allocator), `malloc`.
- ❌ a "logical lock" — a flag that says "I'm modifying this, everyone wait" —
  because if the flag-holder is descheduled, everyone spins forever. (That's just
  a spinlock = still blocking, not lock-free.)
- ✅ only atomic reads / writes / RMW / CAS loops, where a stalled thread's
  incomplete work is either invisible or fixable by another thread ("helping").

The formal test: **"if any one thread is paused at an arbitrary point, can the
others still complete?"** Yes → (at least) lock-free. No → blocking.

---

## Lock-free ≠ fast

- A hot CAS loop under heavy contention can spend most cycles **retrying** — still
  lock-free (someone progresses) but throughput can be *worse* than a good mutex.
- `std::mutex` uncontended is ~15–20 ns (a CAS + maybe nothing else). A contended
  lock-free structure with a 10-deep retry storm can beat that badly.
- Lock-free's real wins: **no priority inversion**, **no convoying**, **no
  deadlock**, **progress if a holder is preempted or crashes**, **bounded worst
  case** (for wait-free). Latency *tail*, not always the mean.

Pick lock-free for **predictability under adversarial scheduling**, not for a
headline throughput number. Measure both (folder 28 file 06 does exactly this).

---

## `is_lock_free()` vs the algorithm being lock-free

Two different things:

| | meaning |
|---|---|
| `std::atomic<T>::is_lock_free()` | does *this atomic type* use a hardware atomic instruction (vs a hidden mutex)? Property of `T` + platform. |
| "my queue is a lock-free algorithm" | does the *algorithm's progress guarantee* hold — no thread can block another? Property of the design. |

You need `is_lock_free()` true for the atomics you build on **and** a lock-free
*algorithm* on top. A lock-free algorithm built on a `std::atomic<Big>` that
secretly locks is **not** lock-free.

---

## "Helping" — how lock-free algorithms avoid blocking

If thread A starts a multi-step update and is descheduled mid-way, thread B —
seeing the half-done state — **completes A's operation** before doing its own,
then retries. The shared structure always encodes enough state for any thread to
finish any other thread's in-progress op. (Michael–Scott queue: a lagging `tail`
pointer is "swung" forward by whoever notices it — folder 28 file 8.)

Wait-free algorithms take helping further: every thread announces its operation so
others *always* help it within a bound.

---

## > **HFT relevance**
> - **The hot path wants bounded worst-case latency** — a `std::mutex` that gets
>   preempted while held adds an unbounded scheduler delay to every waiter. A
>   lock-free (ideally single-producer/single-consumer, so effectively wait-free)
>   queue removes that tail.
> - **SPSC ring buffer is effectively wait-free** — one producer, one consumer,
>   each does a bounded number of steps (no CAS loop, just `release` store /
>   `acquire` load of an index). That's the workhorse (folder 28 files 3–5).
> - **Avoid CAS-loop MPMC on the hottest path** — retry storms make latency
>   *less* predictable, the opposite of the goal. Shard to per-thread SPSC queues
>   instead.
> - **No `new`/`delete`/syscalls in a lock-free op** — pre-allocate (memory pool,
>   folder 28 file 10). A `malloc` that grabs its arena lock breaks the guarantee.
> - **`is_lock_free()` assert at startup** for every atomic type the engine relies
>   on — catches a platform/type combo that silently fell back to a lock.

---

## Hands-on

```bash
./build.ps1 fast 27-ATOMICS-MEMORY-MODEL/examples/06_memory_order_bench.cpp   # RMW costs
./build.ps1 fast 27-ATOMICS-MEMORY-MODEL/examples/02_cas_loop.cpp             # CAS-loop = lock-free
```
Then reason about each: SPSC index hand-off (`examples/04` Demo 2) — pause the
producer after the payload write, before the `release` store: consumer just sees
"nothing new", keeps working → lock-free (wait-free even). Pause a CAS-loop thread
in `02` mid-loop: others still succeed → lock-free but that thread may starve.

- Classify: `counter.fetch_add(1)`; a Treiber stack push; a `std::mutex`-guarded
  list; an obstruction-free STM word swap.

---

## ⚠️ Traps

### Trap 1 — "no `std::mutex` ⇒ lock-free"
A spinlock (`while (flag.exchange(true)) {}`) uses no `std::mutex` and is **not**
lock-free — a preempted holder stalls everyone.

### Trap 2 — "lock-free ⇒ faster"
Under contention a CAS-loop can be slower than a mutex. Lock-free buys
predictability / no deadlock / preemption-tolerance, not guaranteed throughput.

### Trap 3 — `is_lock_free()` true ⇒ my structure is lock-free
That's only the *atomic type*. The algorithm on top must also guarantee progress.

### Trap 4 — `new`/`delete`/`malloc` inside a lock-free op
The allocator may lock. Pre-allocate; use a lock-free pool.

### Trap 5 — confusing wait-free and lock-free
Wait-free: *every* thread bounded. Lock-free: *some* thread progresses (others may
starve).

### Trap 6 — assuming lock-free ⇒ no starvation
Lock-free explicitly allows individual-thread starvation. Only wait-free rules it
out.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "lock-free = doesn't use a mutex" | = progress guarantee: a paused thread can't block the others |
| "spinlock is lock-free" | No — preempted holder stalls everyone; it's blocking |
| "lock-free is always faster than a mutex" | Often not under contention; it buys predictability, not throughput |
| "wait-free and lock-free are the same" | Wait-free bounds *every* thread; lock-free bounds the *system* |
| "`atomic::is_lock_free()` ⇒ lock-free data structure" | Only the atomic type; the algorithm is separate |
| "lock-free ⇒ no thread starves" | Individual starvation is allowed; wait-free forbids it |

---

## Exercises

1. **Classify:** (a) `x.fetch_add(1)`; (b) `while (lock.exchange(true, acquire)) {}`
   critical section; (c) Treiber stack `push` (CAS loop); (d) a queue where a
   descheduled enqueuer leaves `tail` stale and any dequeuer fixes it.

   <details><summary>Answer</summary>

   (a) wait-free (one bounded RMW). (b) blocking (spinlock — preempted holder
   stalls all). (c) lock-free (a failure means someone else progressed; that
   thread may starve). (d) lock-free with helping (system always progresses).
   </details>

2. **Progress test:** state the one-sentence check for "is this at least
   lock-free?"

   <details><summary>Answer</summary>

   Pause any single thread at any point in its operation — can every other thread
   still complete its operation in a bounded number of steps? Yes ⇒ lock-free (or
   better).
   </details>

3. **Why can lock-free be slower:** describe the scenario.

   <details><summary>Answer</summary>

   High contention on one CAS target: each success invalidates every other
   thread's read, so N−1 threads fail and retry each round — a retry storm. Total
   useful work per unit time drops; a mutex (which parks losers instead of
   spinning) can do better. Still lock-free — someone always wins.
   </details>

4. **Allocator trap:** why is `node* n = new node;` inside a lock-free push a
   problem?

   <details><summary>Answer</summary>

   `new` may take the allocator's internal lock (or a syscall to grow the heap).
   If the thread is preempted holding that lock, other threads calling `new` in
   their push block → the "lock-free" push is actually blocking. Pre-allocate from
   a lock-free pool.
   </details>

5. **SPSC = wait-free?** argue why a single-producer/single-consumer ring buffer
   is effectively wait-free.

   <details><summary>Answer</summary>

   Each side does a fixed, bounded sequence: read own index (relaxed), check other
   index (acquire), read/write the slot, publish own index (release). No CAS loop,
   no retry, no waiting on the other side to *act* (only "is there space / data?",
   and if not you return / spin by choice). Bounded steps per operation for both
   threads ⇒ wait-free in practice.
   </details>

---

## Interview questions

1. Wait-free, lock-free, obstruction-free — teenon ki exact progress guarantee.
2. "Lock-free = no mutex" galat kyun — spinlock example.
3. Ek algorithm lock-free hai — kaise test karein (pause-any-thread)?
4. Lock-free contention pe mutex se slow kaise ho sakta?
5. `is_lock_free()` aur "lock-free data structure" — do alag cheezein kyun?
6. "Helping" kya hai — descheduled thread ka kaam kaun poora karta?
7. SPSC ring buffer practically wait-free kyun?

---

## Next
→ [`15-aba-problem.md`](15-aba-problem.md)
