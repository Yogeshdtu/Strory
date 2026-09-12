# 08 — Treiber stack & ABA in practice

## Prerequisites
- `07-michael-scott-queue.md`, `27-ATOMICS-MEMORY-MODEL` file 15 (ABA)
- [`examples/04_lock_free_stack.cpp`](examples/04_lock_free_stack.cpp)

## Yeh topic abhi kyun
Treiber stack (1986) sabse chhota lock-free structure hai — ek atomic head, push
aur pop dono ek CAS loop. Yeh ABA problem ka **canonical demo** hai, aur folder ka
sabse important negative result yahan hai: is box pe contended Treiber stack
`std::mutex` + `std::vector` se **~5× slow** nikla. Lock-free ≠ fast — measured.

---

## The algorithm

```
  head_ ──► [n3] ──► [n2] ──► [n1] ──► NIL       (LIFO — push/pop at head)
```

```cpp
void push(Node* n) {
    n->next = head_.load(std::memory_order_relaxed);
    while (!head_.compare_exchange_weak(n->next, n,
               std::memory_order_release, std::memory_order_relaxed))
        ;                    // CAS failure refreshed n->next -> retry
}

Node* pop() {
    Node* head = head_.load(std::memory_order_acquire);
    while (head &&
           !head_.compare_exchange_weak(head, head->next,
               std::memory_order_acquire, std::memory_order_relaxed))
        ;                    // failure refreshed head -> retry
    return head;             // may be nullptr (empty)
}
```

- **push:** point the new node at the current head, CAS head to the new node.
- **pop:** read head, read `head->next`, CAS head to `head->next`.
- No dummy node, no helping — every step is a single self-contained CAS.
- **lock-free, not wait-free** — a thread's CAS can fail-and-retry unboundedly
  under contention.

---

## ABA — the reason pop is dangerous

```
Thread 1 pop():                          Stack: A → B → C
  reads head = A
  reads next = A->next = B
  -- descheduled --
                    Thread 2: pop() -> A     Stack: B → C
                    Thread 2: pop() -> B     Stack: C
                    Thread 2: push(A)        Stack: A → C      (A recycled!)
Thread 1 resumes:
  CAS(head, A, B)   -- head IS A -> SUCCEEDS
  -- but A->next is now C, not B. head is set to B — a node
     already popped and possibly freed. Stack corrupted.
```

Thread 1 ka CAS ne dekha "head abhi bhi A hai" aur maan liya "kuch nahi badla" —
par poori list badal gayi. **CAS value equality check karta, history nahi**
(folder 27 file 15).

---

## Fix: tagged head (`examples/04`)

Ek monotonic **tag** head ke saath pack karo. Har push tag badhata hai, to
`{idx=A, tag=old}` current `{idx=A, tag=new}` se match nahi karega → stale CAS
**fail**.

```cpp
// 64-bit head word: low 32 = node index into a pool, high 32 = version tag
static std::uint64_t pack(std::uint32_t idx, std::uint32_t tag) {
    return (static_cast<std::uint64_t>(tag) << 32) | idx;
}
std::atomic<std::uint64_t> head_;   // lock-free everywhere — no 128-bit CAS needed

void push(std::uint32_t idx) {
    std::uint64_t cur = head_.load(std::memory_order_relaxed);
    for (;;) {
        pool_[idx].next = static_cast<std::uint32_t>(cur);       // link
        std::uint64_t next = pack(idx, static_cast<std::uint32_t>(cur >> 32) + 1u);  // tag++
        if (head_.compare_exchange_weak(cur, next,
                std::memory_order_release, std::memory_order_relaxed))
            return;
    }
}
```

- **Node indices, not raw pointers** — a 32-bit index into a fixed pool fits
  alongside a 32-bit tag in one `uint64_t`. `std::atomic<uint64_t>` is lock-free
  on x86 and ARM. A raw 64-bit pointer + tag would need a **128-bit CAS**
  (`cmpxchg16b`) which **doesn't link on this MinGW** (`__atomic_*_16` undefined).
- **Tag width:** 32 bits wraps after ~4 G modifications. At 50 M ops/s that's
  ~86 s of continuous pushes before a wrap could theoretically enable an ABA in
  the exact window — safe in practice. 16-bit tags are *not* (folder 27 file 15
  exercise 2).
- Tag must bump on **every** mutation that can recycle a node — here, every push.

`examples/04` reproduces the ABA deterministically (broken path) and shows the
tagged version rejecting the stale CAS.

---

## Measured (`examples/04`, this box, `-O2`) — the negative result

```
4 threads x (1.5 M push + 1.5 M pop), pool 16384 nodes
  lock-free (tagged) : ~7 M op/s
  mutex + vector     : ~40 M op/s
  ratio              : ~0.18x   <-- lock-free is ~5x SLOWER
  correctness        : OK (multiset of popped == pushed), no ABA
```

**Kyun lock-free haara** (CLAUDE.md Rule 2 — teach it, don't hide it):
- Har iteration = 4 CAS-loops (freelist pop + data push + data pop + freelist
  push), 4 threads, **2 hot atomic words** → most CASes fail → **retry storm**.
- `mutex + std::vector`: lock ek CAS, critical section ~2 instructions,
  `push_back`/`pop_back` sab ek hot vector-tail cache line pe → very cache-
  friendly, low hold time → barely contends.
- LIFO = **single head = maximum contention shape**.

Lock-free ka faayda yahan bhi hai — **no deadlock, no priority inversion, progress
if a holder is preempted** — bas throughput nahi.

---

## When a stack shape is OK

- **Low contention** — a free-list touched occasionally, not hammered.
- **SPSC-ish** — one pusher, one popper (then it's nearly wait-free, no retry
  storm).
- **Sharded** — per-thread stacks, steal only when empty (work-stealing deques do
  this — Chase-Lev).

For a **hot, multi-writer** LIFO: measure a mutex first. It often wins.

---

## > **HFT relevance**
> - **Lock-free free-list for a memory pool** is the common legit use — nodes are
>   allocated/freed off the *steady-state* hot path (during (de)serialization
>   setup, not per tick), so contention is low and the tagged Treiber stack is
>   fine.
> - **Don't put a hot, contended LIFO on the critical path.** If profiling shows a
>   shared stack with a high CAS-retry rate, switch shape: shard it, or use an
>   SPSC/MPSC ring.
> - **32-bit index + 32-bit tag in a `uint64_t`** — lock-free on every target, no
>   `cmpxchg16b` dependency. Size the pool ≤ 4 G nodes (trivially true) and the
>   tag is wide enough.
> - **`examples/04` is the cautionary tale** — a lock-free structure that "works"
>   (correct, ABA-safe) and is still 5× slower than a mutex. Always benchmark
>   against the boring baseline.

---

## Hands-on

```bash
./build.ps1 fast 28-LOCK-FREE/examples/04_lock_free_stack.cpp
```

Then:
- Change the tag increment to `+ 0u` (freeze the tag) → the ABA comes back;
  the broken path's stale CAS starts succeeding and correctness breaks.
- Drop to 1 thread → the lock-free version's throughput jumps (no contention, no
  retries) and may beat the mutex.
- Add a retry counter to `push`/`pop`; print retries/op under 4 threads vs 1.

---

## ⚠️ Traps

### Trap 1 — untagged CAS on a recycled node/index
ABA → the stale CAS succeeds → lost nodes, cycles, use-after-free. Tag it.

### Trap 2 — tag frozen / bumped only on push, not on every recycling op
Every mutation that can hand a node back out must bump the tag.

### Trap 3 — 16-bit (or narrower) tag on a hot stack
Wraps inside the vulnerable window under load → ABA reappears. 32 bits minimum.

### Trap 4 — raw pointer + tag assuming a 128-bit CAS exists
`cmpxchg16b` isn't available everywhere (and not on this MinGW). Use a 32-bit
index + 32-bit tag in a `uint64_t`.

### Trap 5 — assuming lock-free stack beats a mutex
On a contended multi-writer LIFO it often loses to `mutex + vector` (`examples/04`
here: ~5× slower). Measure.

### Trap 6 — `pop()` returning a node another thread will free
Same reclamation problem as the MS queue (`07`, `09`) if nodes are truly
`delete`d rather than returned to a fixed pool.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "CAS on `head` is safe because it's atomic" | ABA: recycled node → value matches, premise stale → corruption |
| "tag on push is enough" | Every op that can recycle a node must bump the tag |
| "need `cmpxchg16b` for tagged pointers" | 32-bit index + 32-bit tag in a `uint64_t` is enough and portable |
| "lock-free stack is faster than a mutex" | Contended multi-writer LIFO often *loses* — measure (`examples/04`) |
| "Treiber stack is wait-free" | Lock-free — CAS retries are unbounded under contention |
| "stacks are a good hot-path structure" | A single head is max contention; prefer rings, or shard |

---

## Exercises

1. **ABA trace:** fill in the stack contents at each step of the pop/pop/push
   interleaving that corrupts an untagged Treiber stack.

   <details><summary>Answer</summary>

   Start `A→B→C`. T1 reads `head=A`, `next=B`, stalls. T2 `pop→A` (`B→C`). T2
   `pop→B` (`C`). T2 `push(A)` (`A→C`). T1 resumes: `CAS(head, A, B)` — `head` is
   `A` so it succeeds, setting `head=B`. But `B` was popped/freed and `A->next` is
   now `C`. The stack is `B→(garbage)`; `C` is leaked/lost.
   </details>

2. **Tag width:** pool of 4096 nodes, 20 M ops/s, worst thread stall 5 ms. Is a
   16-bit tag safe? 32-bit?

   <details><summary>Answer</summary>

   In 5 ms at 20 M ops/s ≈ 100,000 modifications. A 16-bit tag (65,536 values)
   wraps well inside that window → unsafe. A 32-bit tag (~4.3 G) needs ~215 s of
   continuous modification to wrap → safe.
   </details>

3. **Why it lost:** the lock-free stack was 5× slower than `mutex + vector` in
   `examples/04`. Give the two mechanisms and one design change that flips it.

   <details><summary>Answer</summary>

   (1) 4 CAS-loops per iteration on 2 hot atomics shared by 4 threads → retry
   storm. (2) `mutex + vector` has a 1-CAS lock, a 2-instruction critical section,
   and all threads hit one hot cache line — extremely cache-friendly, low hold
   time. Change: shard into per-thread stacks (steal only when empty), or use an
   SPSC/MPSC ring — remove the single contended head.
   </details>

4. **`pop` memory order:** why `acquire` on the successful CAS?

   <details><summary>Answer</summary>

   The popped node's `value` (and `next`) were published by the `push` that linked
   it with a `release` CAS. The popper must `acquire` so those writes
   happen-before its use of the node. `relaxed` would be a data race on the node's
   fields.
   </details>

5. **Single producer/consumer:** why does the lock-free stack's throughput jump
   when you drop from 4 threads to 1?

   <details><summary>Answer</summary>

   With one thread there's no contention on `head_` → every CAS succeeds first try
   → no retries. The CAS-loop degenerates to a straight-line load + CAS + done.
   The retry storm was the entire cost; remove contention and it's nearly
   wait-free.
   </details>

---

## Interview questions

1. Treiber stack push/pop — CAS loop kaise, kya refresh hota fail pe?
2. ABA is stack pe kaise — ek pop/pop/push interleaving.
3. Tagged head fix — kyun kaam karta, tag kab bump?
4. 32-bit index + 32-bit tag kyun (128-bit CAS ki zaroorat nahi)?
5. `examples/04` mein lock-free mutex se slow kyun (retry storm + cache-friendly baseline)?
6. Kaunsi conditions mein stack shape theek (low contention / SPSC / sharded)?
7. `pop` ka successful CAS `acquire` kyun?

---

## Next
→ [`09-memory-reclamation.md`](09-memory-reclamation.md)
