# 07 — The Michael-Scott queue

## Prerequisites
- `06-mpmc-queue.md`, `27-ATOMICS-MEMORY-MODEL` files 05 (CAS), 15 (ABA)

## Yeh topic abhi kyun
Bounded MPMC (`06`) fixed capacity maangta. Jab aapko ek **unbounded** lock-free
FIFO chahiye — linked list of nodes — toh Michael & Scott (1996) ka queue classic
hai. `java.util.concurrent.ConcurrentLinkedQueue` isi pe bana hai. Ismein do naye
concepts aate hain jo har pointer-based lock-free structure mein milenge:
**helping** aur **reclamation ka problem**.

---

## Structure

```
  head_ ──► [dummy] ──► [n1] ──► [n2] ──► [n3] ──► nullptr
                                            ▲
  tail_ ─────────────────────────────────── ┘  (may lag by one)

  struct Node { T value; std::atomic<Node*> next; };
  std::atomic<Node*> head_;   // dequeue from here (points at a DUMMY node)
  std::atomic<Node*> tail_;   // enqueue near here
```

- Ek permanent **dummy node** — `head_` hamesha dummy ko point karta; asli first
  element `head_->next` hai. Isse empty-queue edge cases khatam ho jate.
- `tail_` **ek node peeche** ho sakta hai — enqueue do steps hai (link the node,
  then swing `tail_`), aur beech mein koi thread dekh sakta hai.

---

## Enqueue (with helping)

```cpp
void enqueue(const T& v) {
    Node* n = pool_.alloc();            // pre-allocated — NO `new` on the hot path
    n->value = v;
    n->next.store(nullptr, std::memory_order_relaxed);
    for (;;) {
        Node* tail = tail_.load(std::memory_order_acquire);
        Node* next = tail->next.load(std::memory_order_acquire);
        if (tail == tail_.load(std::memory_order_acquire)) {   // tail still consistent?
            if (next == nullptr) {
                // tail really is last -> try to link our node
                if (tail->next.compare_exchange_weak(next, n,
                        std::memory_order_release, std::memory_order_relaxed)) {
                    // linked. try to swing tail_ forward (ok if this fails —
                    // someone will help)
                    tail_.compare_exchange_strong(tail, n,
                        std::memory_order_release, std::memory_order_relaxed);
                    return;
                }
            } else {
                // tail_ is lagging -> HELP: swing it forward, then retry
                tail_.compare_exchange_strong(tail, next,
                    std::memory_order_release, std::memory_order_relaxed);
            }
        }
    }
}
```

**Helping:** agar ek enqueuer node link karke `tail_` swing karne se pehle
descheduled ho jaye, `tail_` "lagging" reh jata. Koi bhi doosra thread jo yeh
dekhta hai woh `tail_` ko `next` pe swing kar deta — original thread ka adhoora
kaam **complete** karke. Isliye system hamesha progress karta (lock-free), bhale
koi individual thread ruka ho.

---

## Dequeue

```cpp
bool dequeue(T& out) {
    for (;;) {
        Node* head = head_.load(std::memory_order_acquire);
        Node* tail = tail_.load(std::memory_order_acquire);
        Node* next = head->next.load(std::memory_order_acquire);
        if (head == head_.load(std::memory_order_acquire)) {
            if (head == tail) {
                if (next == nullptr) return false;          // empty
                // tail lagging -> help
                tail_.compare_exchange_strong(tail, next,
                    std::memory_order_release, std::memory_order_relaxed);
            } else {
                out = next->value;                          // read BEFORE the CAS
                if (head_.compare_exchange_weak(head, next,
                        std::memory_order_release, std::memory_order_relaxed)) {
                    pool_.retire(head);                      // <-- RECLAMATION problem
                    return true;
                }
            }
        }
    }
}
```

`out = next->value` **CAS se pehle** — kyunki CAS ke baad `head` (purana dummy)
koi aur thread reclaim/reuse kar sakta hai, aur tab `next` bhi shayad... — yahi
reclamation ka problem hai, neeche.

---

## The two hard problems

### 1. ABA (folder 27 file 15)
`head_.compare_exchange_weak(head, next)` — agar `head` node free hoke wapas
allocate ho jaye (dummy ke roop mein) two-read aur CAS ke beech, toh stale CAS
succeed kar sakta hai galat premise pe. **Fix:** tagged pointers
(`{Node*, uint64 counter}` — 128-bit CAS, jo is MinGW pe link nahi hota) **ya** ek
reclamation scheme jo node ko reuse hone hi na de jab tak koi reference ho.

### 2. Reclamation (`09`)
`dequeue` ne `head` ko list se hata diya — par **koi doosra thread abhi bhi**
`head` ko `head_.load()` karke `head->next` padh raha ho sakta hai (usne CAS se
pehle load kiya tha). Agar aap `head` ko abhi `delete`/reuse karein → **use-after-
free**. Kab safe hai delete karna? Yeh `09`–`11` ka poora topic hai (hazard
pointers, epochs, RCU).

**Isliye HFT hot path linked lists se bachta hai** — bounded ring buffers (`04`,
`06`) mein na ABA hai na reclamation problem (monotonic indices, fixed array).
Michael-Scott queue tab use karo jab genuinely unbounded FIFO chahiye aur aap
reclamation ki cost pay karne ko tayyar ho.

---

## Memory orders

| Op | Order | Why |
|---|---|---|
| `tail_.load` / `head_.load` | `acquire` | see the node's fields the publisher released |
| `tail->next.compare_exchange` (link) success | `release` | publish the new node + its `value` |
| `head_.compare_exchange` (unlink) success | `release` | (and `acquire` if you also consume via it) |
| CAS failure orders | `relaxed` | just a re-read |

---

## > **HFT relevance**
> - **Usually the wrong tool for the hot path.** It needs a reclamation scheme
>   (hazard pointers / epochs — real per-op cost and complexity) *and* ABA
>   protection. A bounded MPMC ring (`06`) or sharded SPSC (`04`) avoids both.
> - **Where it fits:** a genuinely unbounded, low-rate control-plane queue
>   (config updates, admin commands) where simplicity of "never full" matters more
>   than ns.
> - **Helping is the pattern to recognize** — any lock-free structure with a
>   multi-step update encodes enough state for another thread to finish a stalled
>   thread's operation. That's what makes it lock-free rather than blocking.
> - **If you must build one:** pre-allocate the node pool (no `new`), pick epoch
>   reclamation (`11`) for throughput or hazard pointers (`10`) for bounded
>   memory, and test with a model checker (`13`) — casual stress testing won't
>   surface the ABA/reclamation races.

---

## Hands-on

```bash
# no dedicated example (reclamation makes a safe standalone demo large);
# study examples/03 (bounded MPMC, no reclamation) and examples/04 (Treiber
# stack — same helping-free CAS-loop + tag shape) instead.
./build.ps1 fast 28-LOCK-FREE/examples/03_mpmc_queue.cpp
./build.ps1 fast 28-LOCK-FREE/examples/04_lock_free_stack.cpp
```

On paper: draw the queue with `tail_` lagging by one node, and trace two
enqueuers — one that linked its node then stalled, one that arrives, sees
`tail->next != nullptr`, helps swing `tail_`, then links its own.

---

## ⚠️ Traps

### Trap 1 — no dummy node
Empty/single-element edge cases multiply. The permanent dummy makes `head_` always
dereferenceable and `enqueue`/`dequeue` uniform.

### Trap 2 — reading `next->value` after the unlink CAS
After `head_` moves, the old `head` (and possibly `next`) can be reclaimed. Read
the value **before** the CAS.

### Trap 3 — no reclamation scheme
`delete head;` in `dequeue` → another thread mid-`dequeue` still holds `head` →
use-after-free. Hazard pointers / epochs (`10`, `11`).

### Trap 4 — no ABA protection
`head_.compare_exchange(head, next)` with a recycled `head` → stale success →
corruption. Tagged pointer (128-bit CAS) or a reclamation scheme that prevents
reuse.

### Trap 5 — forgetting to help
If `enqueue` doesn't swing a lagging `tail_` forward, a stalled enqueuer blocks
all others → not lock-free.

### Trap 6 — `new` per node
Allocator lock → not lock-free. Pre-allocate a pool.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "linked lock-free queue is the default" | Bounded ring (`06`) is — no ABA, no reclamation; MS queue only for true unbounded |
| "`tail_` always points at the last node" | It can lag by one; readers must handle and help |
| "just `delete` the dequeued node" | Another thread may still hold it — need hazard pointers / epochs |
| "ABA can't happen, CAS is atomic" | Recycled node → A→B→A defeats the value check |
| "helping is an optimization" | It's what makes it lock-free — without it a stalled thread blocks all |
| "MS queue is wait-free" | Lock-free (CAS retries + helping); not wait-free |

---

## Exercises

1. **Why the dummy node?** what breaks in `dequeue` on an empty queue without it?

   <details><summary>Answer</summary>

   Without a dummy, `head_` is `nullptr` when empty, so `head_->next` faults, and
   the first `enqueue`/`dequeue` must special-case setting both `head_` and
   `tail_`. The permanent dummy makes `head_` always non-null and dereferenceable;
   empty is simply `head_ == tail_ && head_->next == nullptr`.
   </details>

2. **Helping trace:** enqueuer A links its node N (CAS on `tail->next` succeeds)
   then is descheduled before swinging `tail_`. Enqueuer B arrives. What does B
   see and do?

   <details><summary>Answer</summary>

   B loads `tail` (still the old node), loads `tail->next` → sees N (not null). So
   B knows `tail_` is lagging: B does `tail_.compare_exchange(tail, N)` — swinging
   it forward on A's behalf — then loops and links its own node after N. A's
   operation was completed by B.
   </details>

3. **Use-after-free:** exactly which line can touch freed memory if `dequeue`
   does `delete head` immediately, and another thread is also in `dequeue`?

   <details><summary>Answer</summary>

   Thread 2 did `Node* head = head_.load()` then `Node* next =
   head->next.load()`. Thread 1 CASes `head_` past that node and `delete head`s it.
   Thread 2's `head->next.load()` (or its retry's re-read of `head->next`) now
   dereferences freed memory. Value must be read before the CAS *and* the node
   must not be freed until no thread can hold it (`09`–`11`).
   </details>

4. **Bounded vs MS:** you need a queue between 6 strategy threads and 1 order
   router, ~200k msg/s, fixed max backlog acceptable. Which, and why?

   <details><summary>Answer</summary>

   Bounded MPMC (`06`) or 6 sharded SPSC rings. Fixed backlog is acceptable → no
   need for unbounded. Bounded gives you no allocation, no ABA, no reclamation
   scheme — much less to get wrong. The MS queue's unboundedness would only add a
   reclamation burden for no benefit here.
   </details>

5. **Memory order:** the CAS that links a new node onto `tail->next` — success
   order and why.

   <details><summary>Answer</summary>

   `release` — it publishes the new node and its `value` (written before the CAS)
   to any consumer that later `acquire`-loads its way to that node. Failure order
   `relaxed` (just re-read and retry).
   </details>

---

## Interview questions

1. Michael-Scott queue: dummy node ka role kya?
2. `tail_` lag kyun karta, "helping" kaise fix karta?
3. Dequeue mein `value` CAS se pehle kyun padhte ho?
4. Is queue ke do hard problems — ABA aur reclamation — kahan aate?
5. Bounded MPMC ring vs MS queue — hot path pe kaunsa, kyun?
6. Kaunse memory orders — link CAS, unlink CAS?
7. Helping ke bina structure lock-free rehta hai? (Nahi — kyun.)

---

## Next
→ [`08-lock-free-stack.md`](08-lock-free-stack.md)
