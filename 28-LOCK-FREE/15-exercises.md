# 15 — Exercises: lock-free programming

## Prerequisites
- Poora folder 28 (files 01–14)

## Yeh file kya hai
Practice — behaviour prediction, "find the bug", design, aur ek build challenge.
Answers `<details>` mein. Compile: `./build.ps1 file.cpp`. Benchmarks:
`./build.ps1 fast file.cpp` (`-O2` — reordering/contention demos `-O0` pe
meaningless). Races (Linux): `-fsanitize=thread`.

---

## Part A — Behaviour prediction

### A1
```cpp
// SPSC ring, 1 producer + 1 consumer.
// Producer:  buf[h] = v;  head_.store(h+1, std::memory_order_relaxed);
// Consumer:  while (tail_ == head_.load(std::memory_order_acquire)) {}
//            x = buf[tail_];  tail_.store(tail_+1, release);
```
Kya galat hai?

<details><summary>Answer</summary>

The producer's publishing store is `relaxed`, not `release`. No synchronizes-with
edge → the consumer can observe the new `head_` before `buf[h] = v` is visible →
reads stale/garbage data (a data race on `buf[h]` → UB). Fix:
`head_.store(h+1, std::memory_order_release)`.
</details>

### A2
```cpp
std::atomic<std::uint64_t> head_;   // {idx:32, tag:32} tagged Treiber head
// push does: next = pack(new_idx, tag);          // tag NOT incremented
```
Untagged-style bug: what breaks and when?

<details><summary>Answer</summary>

If the tag never changes, the packed word is just the index again → ABA is back. A
stale `CAS(head, {A, t}, ...)` after node A was popped, reused, and pushed again
succeeds on a stale premise → the stack corrupts (lost nodes / cycles). The tag
must increment on **every** push (`examples/04`).
</details>

### A3
```cpp
// Vyukov MPMC dequeue:
std::size_t seq = cell->seq.load(std::memory_order_acquire);
std::size_t dif = seq - (pos + 1);           // <-- unsigned
if (dif == 0) { ... } else if (dif < 0) { return false; }   // empty
```
Bug?

<details><summary>Answer</summary>

`dif` is unsigned, so `dif < 0` is **always false** → the "empty" branch never
fires, and when `seq` is behind `pos+1` the code spins forever (or misbehaves) instead
of returning `false`. Compute the difference as **signed**: `std::ptrdiff_t dif =
(std::ptrdiff_t)seq - (std::ptrdiff_t)(pos + 1);`.
</details>

### A4
```cpp
// Seqlock reader:
s1 = seq_.load(acquire);
if (s1 & 1) continue;
snap = data_;                 // copy
if (seq_.load(relaxed) == s1) return snap;   // no fence before this load
continue;
```
Kya missing hai?

<details><summary>Answer</summary>

An `acquire` fence between the `data_` copy and the second `seq_` load. Without it,
the compiler/CPU can move the second `seq_` load *before* the copy finishes, so a
write that overlapped the tail of the copy isn't detected → a torn snapshot is
accepted. Add `std::atomic_thread_fence(std::memory_order_acquire);` before the
recheck.
</details>

### A5
```cpp
// examples/04 result on this box:
//   lock-free (tagged Treiber) : ~7 M op/s
//   mutex + std::vector        : ~40 M op/s
```
Yeh "bug" hai?

<details><summary>Answer</summary>

No — it's a correct, expected result and a key lesson. A contended multi-writer
LIFO has one hot head; 4 threads CAS-looping it (plus a free-list head) →
retry storm. `mutex + vector` has a 1-CAS lock, a 2-instruction critical section,
and one hot cache line → barely contends. Lock-free ≠ fast; benchmark against the
mutex baseline (files 08, 14).
</details>

### A6
```cpp
// Lock-free MS queue dequeue:
Node* head = head_.load(acquire);
Node* next = head->next.load(acquire);
if (head_.compare_exchange_weak(head, next, release, relaxed)) {
    T value = next->value;      // <-- read AFTER the CAS
    delete head;
    return value;
}
```
Do bugs.

<details><summary>Answer</summary>

(1) `next->value` is read **after** the CAS — by then another dequeuer could have
CASed past `next` and freed it → use-after-free. Read the value **before** the CAS.
(2) `delete head` with no reclamation scheme — another thread may still hold
`head` from its own `head_.load()` → UAF. Defer via hazard pointers / epochs
(files 09–11), or use a fixed pool.
</details>

---

## Part B — Find the bug

### B1
```cpp
template <class T, std::size_t N>
struct SpscRing {
    T buf[N];
    std::atomic<std::size_t> head{0}, tail{0};   // adjacent
    bool push(const T& v) {
        auto h = head.load(std::memory_order_relaxed);
        if (((h + 1) % N) == tail.load(std::memory_order_acquire)) return false;
        buf[h % N] = v;
        head.store(h + 1, std::memory_order_release);
        return true;
    }
};
```
Two performance bugs (not correctness).

<details><summary>Answer</summary>

(1) `% N` on the hot path — a division. Require `N` a power of two and use `&
(N-1)`. (2) `head` and `tail` are adjacent → same cache line → the producer's
`head` store and the consumer's `tail` store ping-pong the line. `alignas(64)`
each + a trailing pad, and cache the opposite index (files 03, 05).
</details>

### B2
```cpp
class Pool {
    std::atomic<Node*> free_;
public:
    Node* alloc() {
        Node* n = free_.load(std::memory_order_acquire);
        while (n && !free_.compare_exchange_weak(n, n->next)) {}
        return n;
    }
    void release(Node* n) {
        n->next = free_.load(std::memory_order_relaxed);
        while (!free_.compare_exchange_weak(n->next, n)) {}
    }
};
```
<details><summary>Answer</summary>

ABA on `alloc`: thread 1 reads `free_ = A`, `n->next` target `A->next = B`, stalls.
Thread 2 allocs A, allocs B, releases A → `free_ = A` again with `A->next = C`.
Thread 1's `CAS(free_, A, B)` succeeds → `free_` points at B which is already
handed out → the same node gets allocated twice. Fix: tag the `free_` word
(`{idx, tag}` in a `uint64_t`, tag bumped on `release`), as in `examples/04`.
</details>

### B3
```cpp
// "lock-free" queue
bool enqueue(const T& v) {
    Node* n = new Node{v};                        // <-- ?
    Node* t = tail_.load(std::memory_order_acquire);
    while (!tail_->next.compare_exchange_weak(/*expected null*/ nullNode, n, release)) {
        t = tail_.load(std::memory_order_acquire);
    }
    tail_.compare_exchange_strong(t, n, release);
    return true;
}
```
<details><summary>Answer</summary>

`new Node{v}` on the shared path — `operator new` can take the allocator's lock
(or a syscall to grow the heap). If the thread is preempted holding that lock,
other enqueuers calling `new` block → the "lock-free" enqueue is actually
blocking, with an unbounded tail. Pre-allocate a fixed `Node` pool and take from a
lock-free (tagged) free-list (file 02).
</details>

### B4
```cpp
// Seqlock with two writer threads, no serialization:
void write(const Quote& q) {
    auto s = seq_.load(std::memory_order_relaxed);
    seq_.store(s + 1, std::memory_order_relaxed);   // odd
    std::atomic_thread_fence(std::memory_order_release);
    data_ = q;
    std::atomic_thread_fence(std::memory_order_release);
    seq_.store(s + 2, std::memory_order_release);   // even
}
```
<details><summary>Answer</summary>

Seqlock assumes a **single** writer. With two writers, both can read the same `s`,
both `store(s+1)` (still odd), both write `data_` interleaved, both `store(s+2)` —
`seq_` ends up even while `data_` is a mix of the two writes → readers accept a
corrupt snapshot with no torn detection. Fix: a small `std::mutex` among the
writers only (readers stay lock-free), or shard one seqlock per single-writer
symbol (file 12).
</details>

### B5
```cpp
// Hazard pointer protect:
Node* p = node_ptr.load(std::memory_order_acquire);
my_hazard.store(p, std::memory_order_release);
// ... immediately use p ...
T v = p->value;
```
<details><summary>Answer</summary>

Missing the **re-validation** after publishing the hazard. Between the `load` and
the `my_hazard.store`, a retiring thread could have unlinked `p`, scanned hazard
slots (not seeing it), and freed it. After `my_hazard.store(p)` you must re-load
`node_ptr` and check it still equals `p` (still linked); if not, restart. Without
that check, hazard pointers don't close the UAF window they exist for (file 10).
</details>

### B6
```cpp
// EBR read section:
rcu_read_lock();
Node* n = list_head.load(std::memory_order_acquire);
while (n) {
    if (match(n)) { auto r = fetch_from_db(n->key); rcu_read_unlock(); return r; }
    n = n->next.load(std::memory_order_acquire);
}
rcu_read_unlock();
```
<details><summary>Answer</summary>

A **blocking call** (`fetch_from_db`) inside the RCU read section. While that call
runs, this thread's epoch is pinned → the global epoch can't advance → *every*
retired node across the process is retained → unbounded memory growth. Read
sections must be tiny and non-blocking: copy what you need out, `rcu_read_unlock()`,
*then* do the slow work (file 11).
</details>

---

## Part C — Design

### C1
A feed handler on one pinned thread must publish per-symbol BBO (a 32-byte struct)
to ~20 strategy threads. Updates ~3 M/s per hot symbol. Design the shared state.

<details><summary>Answer</summary>

One **seqlock per symbol** (file 12): `struct { alignas(64) atomic<uint64_t> seq;
BBO data; char pad[...]; }` in an array indexed by symbol id. The feed thread is
the single writer per seqlock; strategy threads read a consistent snapshot with
two `seq` loads + a copy, no lock. `seq` on its own cache line (every reader
hammers it). Payload fields as `relaxed` atomics (or `atomic_ref`) for a
strictly-correct build. At 3 M/s the write window is ~small vs the ~few-ns read
copy → low retry rate. Beats `shared_mutex` by ~80–100× (`examples/05`).
</details>

### C2
6 strategy threads each generate orders; 1 gateway thread sends them. No strategy
thread may be slowed by another or by the gateway. Design the transport.

<details><summary>Answer</summary>

**6 sharded SPSC rings** (file 04) — one per strategy thread, each single-producer
(that strategy) / single-consumer (the gateway). The gateway round-robins /
priority-drains the 6 rings. No shared-position CAS (unlike one MPMC queue), each
link wait-free, no strategy contends with another. Rings hold POD order records;
if a ring fills the gateway is behind → that's a hard alert (you don't drop
orders), but the strategy thread still never blocks on a lock.
</details>

### C3
You're tempted to replace a contended `std::mutex`-guarded `std::stack` (12
threads, high contention) with a lock-free Treiber stack. Talk yourself out of it
— what do you do instead?

<details><summary>Answer</summary>

`examples/04` shows a contended lock-free stack losing ~5× to `mutex + vector` —
one hot head is a retry storm. Instead **shard**: give each thread its own stack
(or a small pool of stacks), and only touch another's when yours is empty
(work-stealing). Each per-thread stack is uncontended → a plain `mutex` (or even
no lock, if truly thread-local) is fine, and total throughput scales with thread
count. Remove the single contended point rather than making it lock-free.
</details>

### C4
Design a bounded MPMC job queue for a thread pool: 8 submitters, 8 workers, cap
4096, jobs are `std::function<void()>` (not trivially copyable).

<details><summary>Answer</summary>

Vyukov bounded MPMC (file 06) for the slot arbitration — monotonic positions (no
ABA), fixed array (no alloc), per-cell `seq` turnstile, `relaxed` position CAS +
`release`/`acquire` cell `seq`. The non-trivial `std::function` payload: store it
**by move** into the cell's `data` under the producer's exclusive claim
(`seq == pos`), and move it out on the consumer side before releasing the cell —
the per-cell `seq` protocol already gives each side exclusive access to the slot,
so a non-trivial type is OK here (unlike a raw ring where you'd prefer POD).
Alternatively store a `unique_ptr<Job>` from a pool. Measure vs a
`mutex + deque` — for 8×8 with real job durations the mutex may well be fine
(file 14).
</details>

### C5
A routing table (~10k entries) is read on every packet by 100 threads and updated
~once/second. Design it.

<details><summary>Answer</summary>

**RCU / atomic pointer swap** (file 11). Readers: `auto* t =
table_.load(acquire); lookup(*t, key);` — one acquire load, wait-free, no shared
writes → scales to 100 threads. Writer (once/sec): build a new table (copy +
change, or a persistent structure), `table_.store(new, release)`, `retire(old)`,
free `old` after a grace period (QSBR fits a packet loop — quiescent point at the
top of each iteration). No seqlock (10k entries too big to copy per read); no
per-entry locking.
</details>

---

## Part D — Challenge

### D1 — Build and optimize an SPSC ring
Implement `SpscRing<T, N>` (POD `T`, `N` power of two) with `try_push` / `try_pop`
using only release-store / acquire-load of two `std::atomic<size_t>` indices.
Then apply, one at a time, measuring after each:
1. baseline (indices adjacent, opposite index reloaded every op)
2. `alignas(64)` on each index + a trailing pad
3. a producer-local `cached_tail` / consumer-local `cached_head`
4. (bonus) a batched `try_push_n` / `try_pop_n`

Predict the ranking first. Compare to `examples/02`.

<details><summary>What you should find (this box, -O2)</summary>

Baseline ~25 ns/msg. Step 2 (padding alone): **~noise, sometimes slightly worse** —
the producer still `acquire`-loads the constantly-written `tail` every push, so
padding doesn't remove the cross-core traffic (`examples/02` V0→V1). Step 3
(cached index): **the real win**, ~16–20 ns/msg (~1.3–1.6×) — the producer stops
touching the consumer's line in steady state. Step 4 (batching): amortizes the
per-message release store + cache-line handoff → another solid drop. Lesson: the
cached opposite index, not the padding, is the SPSC optimization that moves the
number here.
</details>

### D2 — Reproduce ABA, then fix it
Build an index-based Treiber stack over a fixed `Node` pool with an **untagged**
`std::atomic<uint32_t>` head. Using a `phase` handshake (à la folder 27
`examples/07`), force this interleaving: thread A reads `head`/`next`, stalls;
thread B does pop, pop, push (recycling A's node); thread A resumes and CASes.
1. Show A's stale CAS **succeeds** and the stack is corrupted (head points at an
   already-popped node).
2. Switch head to a packed `{idx:32, tag:32}` `std::atomic<uint64_t>`, bump the
   tag every push. Show the same interleaving now makes A's CAS **fail**.
3. Freeze the tag increment (`+ 0`). Does the corruption return?

<details><summary>What you should find</summary>

(1) Untagged: `head` is back to A's index, so `CAS(head, A, ...)` matches and
succeeds on a stale `next` → `head` ends up at a popped node; a follow-up pop
returns a dead node / the structure loses entries. (2) Tagged: across the same
interleaving the tag advanced (e.g. 0→3), so A's `{idx=A, tag=0}` ≠ live `{idx=A,
tag=3}` → CAS fails → A re-reads the fresh head → no corruption. (3) Tag frozen →
packed word is just the index again → ABA returns, corruption comes back. This
mirrors `examples/04` / `27/07`.
</details>

### D3 — Mutex vs lock-free, both shapes
Take `examples/06` (SPSC: lock-free wins) and `examples/04` (contended stack:
mutex wins). Without changing the primitives:
1. In `04`, shard the stack — one per thread, steal from a random other only when
   yours is empty. Re-benchmark vs the single `mutex + vector`. Does lock-free
   (sharded) now win?
2. In `06`, add a variant D: `folly::ProducerConsumerQueue`-style with a batched
   consumer (drain up to 64 per acquire load). Measure p50 hand-off + throughput.
3. Write one paragraph: what property of the *contention shape* predicts whether
   lock-free beats a mutex?

<details><summary>What you should find</summary>

(1) Sharding removes the single contended head → each per-thread stack is
uncontended → the lock-free (or even lockless thread-local) sharded version
beats the single mutex, and scales with threads. (2) Batching the consumer
amortizes the acquire load + cache-line handoff → throughput up, p50 roughly
unchanged or slightly better. (3) The predictor: **is there a single memory
location that multiple threads must CAS/modify on every operation?** If yes
(Treiber head, one MPMC position under heavy load) → retry storm → a mutex (which
parks losers instead of spinning) often wins. If the accesses are *disjoint*
(SPSC indices, sharded structures) → no CAS contention → lock-free wins clearly.
Shape, not the "lock-free" label, decides.
</details>

---

## Next
→ [`../29-LINUX-SYSTEMS/00-README.md`](../29-LINUX-SYSTEMS/00-README.md)
