# 02 — Lock-free rules and constraints

## Prerequisites
- `01-why-lock-free.md`
- `27-ATOMICS-MEMORY-MODEL` files 05 (CAS), 08 (acquire/release), 14 (definitions)

## Yeh topic abhi kyun
Lock-free code likhne ke kuch **hard constraints** hain — kuch cheezein bilkul
allowed nahi, aur kuch patterns hi kaam karte hain. Inhe pehle jaan lo, phir har
structure (`04`–`08`) inhi rules ka application hoga.

---

## Rule 1 — Shared path pe koi OS lock / blocking call nahi

Ek stalled thread doosron ko block nahi kar sakta — matlab shared operation ke
andar **kuch bhi aisa nahi** jis par preempt hone se sab ruk jayein:

| ❌ Nahi | Kyun |
|---|---|
| `std::mutex` / spinlock / any OS lock | preempted holder → sab ruke |
| `new` / `delete` / `malloc` / `free` | allocator apna lock le sakta hai |
| blocking syscall (I/O, `futex`, `sleep`) | thread block → agar "holding" kuch to sab ruke |
| `std::shared_ptr` copy jo refcount atomically badhaye — theek hai; par `std::shared_ptr` **allocation** nahi |
| exceptions jo unwind karte hue lock/alloc karein | indirect blocking |

✅ Allowed: atomic load/store/RMW, CAS loops, plain arithmetic, reads/writes of
pre-allocated memory, `std::atomic_thread_fence`.

---

## Rule 2 — Memory pehle se allocate karo

`new` shared path pe nahi ho sakta → **pre-allocate**:
- **Fixed-capacity ring buffers** (`04`, `06`) — ek array, indices se access.
- **Node pools / free-lists** (`08`) — startup pe N nodes bana lo, lock-free
  free-list se lo/wapas do.
- Object jo publish karna hai (snapshot) — pehle se bana ke rakho ya ek
  bounded pool se lo.

HFT engines effectively **zero heap allocation after warm-up** — sab kuch
arenas/pools se.

---

## Rule 3 — Har shared access ka memory order deliberate ho

- **Publish data:** `release` store (index/pointer), consumer `acquire` load.
- **Standalone counter:** `relaxed`.
- **CAS loop:** success `release` (ya `acq_rel` agar consume bhi kar rahe),
  failure `relaxed`/`acquire`.
- **Mutual exclusion / Dekker-style:** `seq_cst` (rare — folder 27 file 16).

Default `seq_cst` correct hai par hot path pe `seq_cst` **store** ka `mfence`
(~13 ns is box pe) bachao jahan `release` kaafi ho.

---

## Rule 4 — ABA se bacho jahan value recycle hoti

CAS value-equality check karta, "kuch nahi badla" nahi (folder 27 file 15). Agar
ek pointer/index free hoke wapas allocate ho sakta hai two-read-then-CAS ke beech:
- **Tagged word** — `{index:32, tag:32}` ek `std::atomic<uint64_t>` mein, tag har
  mutation pe badhao (`08`). Lock-free everywhere, koi 128-bit CAS nahi chahiye.
- **Ya reclamation scheme** — node reuse hi mat hone do jab tak koi reference ho
  sakta hai (`09`–`11`).
- **Ya design hi aisa** ki recycle na ho — monotonic 64-bit counters (SPSC/MPSC
  rings, `04`/`06`) → koi ABA nahi, koi tag nahi.

---

## Rule 5 — "Helping" ya bounded retry

Ek thread multi-step update ke beech ruk jaye to structure ko aisa encode karna
padta ki **koi doosra thread woh op poora kar sake** ("helping" — Michael-Scott
queue `tail` ko koi bhi aage swing kar deta, `07`). Ya har step apne aap mein
atomic ho aur retry safe ho (Treiber stack, `08`).

---

## Rule 6 — Trivially-copyable payloads (mostly)

Lock-free slots mein aksar `memcpy`-able POD hi jate hain — non-trivial
constructors/destructors ka lifecycle lock-free slot reuse ke saath manage karna
bahut mushkil. Bade/complex objects ke liye: pointer publish karo (object heap pe
banao *lock ke bahar*, phir pointer atomically swap).

---

## The shapes that actually work

| Shape | Structure | ABA? | Progress |
|---|---|---|---|
| 1 producer, 1 consumer | SPSC ring (`04`, `05`) | No (monotonic idx) | wait-free in practice |
| N producers, M consumers, bounded | Vyukov MPMC (`06`) | No (monotonic idx) | lock-free |
| Unbounded FIFO | Michael-Scott queue (`07`) | Yes → tag / HP | lock-free (+ helping) |
| LIFO | Treiber stack (`08`) | Yes → tag / HP | lock-free |
| 1 writer, N readers, big value | Seqlock (`12`) | n/a | readers wait-free-ish, writer wait-free |
| Read-mostly pointer | RCU / atomic pointer swap (`11`) | reclamation problem | readers wait-free |

**Prefer the top rows.** Har neeche ki row zyada machinery (tags, hazard pointers,
epochs) mangati hai.

---

## > **HFT relevance**
> - **"No allocation on the hot path" is a hard rule**, not a guideline — one
>   `malloc` that grabs its arena lock turns a "lock-free" queue into a blocking
>   one, and adds an unbounded tail. Pools/rings only.
> - **Design to avoid ABA entirely** — monotonic 64-bit index counters (SPSC/MPSC)
>   never recycle a value, so no tags, no hazard pointers. That's a big reason the
>   hot path is ring buffers, not linked lists.
> - **`seq_cst` audit** — grep the hot path for default-order atomics; each
>   `seq_cst` store is an `mfence` you probably don't need.
> - **One reviewer question per shared byte:** "which happens-before edge orders
>   this, and can this value be recycled between the read and the CAS?" No answer
>   → not ready.

---

## Hands-on

```bash
./build.ps1 fast 28-LOCK-FREE/examples/03_mpmc_queue.cpp
```

Vyukov MPMC: no `new` in `enqueue`/`dequeue`, monotonic positions (no ABA), per-
cell `seq` as the turnstile, CAS loop on the shared position with `relaxed`
success (ordering is on the cell `seq`). Every rule above is visible in ~30 lines.

---

## ⚠️ Traps

### Trap 1 — `new`/`make_shared` inside a lock-free op
Allocator may lock. Pre-allocate a pool.

### Trap 2 — a "logical lock" flag
`while (busy.exchange(true)) {}` then "everyone waits" — that's a spinlock, not
lock-free. A preempted flag-holder stalls all.

### Trap 3 — forgetting ABA on a recycled pointer/index
`compare_exchange` on `head->next` without a tag or reclamation → corruption
under load (folder 27 file 15).

### Trap 4 — default `seq_cst` everywhere on the hot path
Correct but the `seq_cst` store `mfence` is ~13 ns each. Downgrade to `release`
where the pattern is publish/subscribe.

### Trap 5 — non-trivial types in ring slots
Constructors/destructors + slot reuse = lifetime hell. Use POD, or publish a
pointer to a heap object built off the hot path.

### Trap 6 — assuming "lock-free" composes
Two lock-free operations done together are **not** automatically atomic or
lock-free as a unit. Composition needs its own design.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "spinlock is lock-free" | No — preempted holder blocks everyone |
| "I can `new` a node, it's fast" | `new` may take the allocator lock → not lock-free; pre-allocate |
| "monotonic counters still need ABA tags" | No — a never-reused value can't go A→B→A |
| "default memory order is fine, just slower" | On the hot path the `seq_cst` store `mfence` is a real ~13 ns tax |
| "lock-free ops compose into lock-free transactions" | Composition is a separate, harder problem |
| "any type works in a lock-free slot" | Trivially-copyable POD, or publish a pointer |

---

## Exercises

1. **Allowed or not** inside a lock-free `push`: (a) `node* n = pool.alloc();`
   (pool is a lock-free free-list); (b) `n = new node;`; (c)
   `head.compare_exchange_weak(...)`; (d) `log_mutex.lock(); log(...);
   log_mutex.unlock();`.

   <details><summary>Answer</summary>

   (a) OK — lock-free pool, no OS lock. (b) not OK — `new` may lock the allocator.
   (c) OK — the core primitive. (d) not OK — an OS lock on the shared path; a
   preempted holder stalls other pushers.
   </details>

2. **ABA needed?** (a) SPSC ring with `uint64_t` head/tail; (b) Treiber stack over
   a recycled node pool; (c) Vyukov MPMC with monotonic positions.

   <details><summary>Answer</summary>

   (a) No — counters only increase, never reused. (b) Yes — a node is freed and
   handed back out; tag the head or use hazard pointers. (c) No — positions are
   monotonic; the per-cell `seq` handles the lap.
   </details>

3. **Fix the rule violation:** a lock-free queue whose `enqueue` does
   `auto n = std::make_shared<Node>(v);`.

   <details><summary>Answer</summary>

   `make_shared` allocates (and the control block adds an alloc) — the allocator
   can lock. Pre-allocate a fixed pool of `Node`s (or use an intrusive free-list)
   and hand out indices/pointers from it with a lock-free `alloc`/`free`.
   </details>

4. **Which shape** for: a market-data decoder feeding one strategy thread and one
   logger thread, neither allowed to slow the decoder?

   <details><summary>Answer</summary>

   Two independent **SPSC rings** — one decoder→strategy, one decoder→logger. Each
   is single-producer/single-consumer (wait-free in practice), no CAS, no ABA. If
   a consumer falls behind, its ring fills and the decoder drops+counts (logger)
   or applies policy (strategy) — but never blocks.
   </details>

5. **Memory orders** for a Treiber `push` CAS: success and failure?

   <details><summary>Answer</summary>

   Success `release` — publishes the new node and its `next` to any popper that
   `acquire`s the head. Failure `relaxed` — it's just a re-read; you'll retry.
   (`acq_rel`/`acquire` if the push also needs to *observe* data guarded by the
   head, which a plain Treiber push doesn't.)
   </details>

---

## Interview questions

1. Lock-free shared path pe kya-kya banned hai (locks, alloc, blocking syscalls)?
2. Memory kyun pre-allocate karni padti — pool / ring?
3. ABA se bachne ke teen tareeke (tag / reclamation / monotonic design)?
4. "Helping" kya hai — kis structure mein (Michael-Scott `tail`)?
5. Lock-free slots mein kaunse types — kyun trivially-copyable?
6. Kaunsi lock-free shapes sabse aasan (SPSC, bounded MPMC), kaunsi mushkil?
7. Do lock-free ops milke lock-free transaction bante hain? (Nahi — kyun.)

---

## Next
→ [`03-cache-line-padding.md`](03-cache-line-padding.md)
