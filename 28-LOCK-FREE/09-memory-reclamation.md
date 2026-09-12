# 09 — The memory reclamation problem

## Prerequisites
- `07-michael-scott-queue.md`, `08-lock-free-stack.md`
- `17-RAII`, `18-COPY-MOVE` (ownership), `27-ATOMICS-MEMORY-MODEL` file 15 (ABA)

## Yeh topic abhi kyun
Lock-free **linked** structures (MS queue, Treiber stack over real nodes) mein ek
node ko list se unlink karna aasan hai. Usse **`delete` kab karein** — yeh sabse
mushkil part hai. Ek thread ne node unlink kiya, par doosra thread abhi bhi usi
node ka pointer haath mein liye baitha ho sakta hai. Jaldi delete → **use-after-
free**. Yeh folder ke `10` (hazard pointers) aur `11` (epochs/RCU) ka setup hai.

---

## The problem, precisely

```cpp
// Thread 1 — dequeue
Node* head = head_.load(acquire);        // (A) grab a reference
Node* next = head->next.load(acquire);   // (B) use it
// -- descheduled here --

// Thread 2 — dequeue
Node* h = head_.load(acquire);           // also sees `head`
... CAS head_ past it ...
delete h;                                // (C) frees the node Thread 1 holds

// Thread 1 — resumes
head_.compare_exchange_weak(head, next); // (D) head is a dangling pointer;
                                         //     re-reading head->next = UAF
```

Refcounting the *container* doesn't help — the danger is a **raw pointer to an
internal node** that a thread loaded before the node was unlinked. GC languages
don't have this (the GC keeps the node alive while any reference exists). C++ has
no GC → you build one of these:

| Scheme | Idea | Cost | Bounded memory? |
|---|---|---|---|
| **Fixed pool, never free** | nodes cycle through a free-list, memory never returned to the OS | ~0 | yes (capacity-bounded) |
| **Reference counting (per node, atomic)** | free at count 0 | an atomic RMW per access + the count field | yes |
| **Hazard pointers** (`10`) | each thread *publishes* the node it's about to touch; reclaimer skips published nodes | a store + a scan per retire | yes (bounded garbage) |
| **Epoch-based / RCU** (`11`) | free a node only after every thread has passed a "quiescent" point | ~0 on the read side; deferred frees | no (unbounded if a reader stalls) |
| **QSBR** (quiescent-state) | app tells the system "I hold no references now" | ~0 read side | no |

---

## Why "just use `shared_ptr<Node>`" doesn't work (well)

- `std::atomic<std::shared_ptr<Node>>` (C++20) exists, but on most
  implementations it's **not lock-free** (`__cpp_lib_atomic_shared_ptr`
  undefined here) — it uses an internal lock table. So your "lock-free" structure
  isn't.
- Even where lock-free, every traversal step is an atomic refcount increment +
  decrement — a `lock`-prefixed RMW per node (~13 ns each here) → traversals
  crawl.
- It *does* solve correctness (no UAF, no ABA). It's a fine choice for a
  **read-rarely** structure; wrong for a hot one.

---

## The fixed-pool escape hatch (what HFT often does)

Don't reclaim at all. Pre-allocate `N` nodes; unlinked nodes go back to a
lock-free free-list; `alloc` takes from it. Memory is **capacity-bounded** and
never returned to the OS.

- **No `delete`** on the hot path → no UAF from reclamation timing.
- **ABA still applies** to the free-list and the structure (nodes *are* reused) →
  tag the CAS words (`08`).
- Works because HFT structures have a known max size (max in-flight orders, max
  book depth). "Never free" is acceptable when capacity is bounded by design.

This is why `examples/03` (bounded MPMC) and `examples/04` (pool + tagged Treiber)
sidestep this whole chapter — and why the hot path prefers them.

---

## Grace periods (the RCU idea — `11`)

"It's safe to free node X once **every thread that could have had a reference to X
has moved on**." Between X's unlink and that moment is a **grace period**. RCU /
epoch schemes track when all threads have passed a quiescent point since the
unlink, then free the batch of retired nodes.

- Read side: nearly free (enter/exit a "read section" = a couple of relaxed ops).
- Reclaim side: deferred, batched.
- **Weakness:** one stalled reader (stuck in a read section) delays *all* frees →
  unbounded memory. HFT mitigates with pinned threads + bounded read sections.

---

## Hazard pointers (the per-reference idea — `10`)

Each thread has a small set of **hazard pointer** slots. Before dereferencing a
node it might race on, it stores that node's address in a slot (and re-checks the
node is still linked). To retire a node, a thread scans all threads' hazard slots;
if the node is listed, it defers; else it frees.

- Read side: a store + a validating re-load per protected pointer.
- **Bounded garbage:** at most `threads × slots` nodes pending.
- More per-op cost than epochs, but no unbounded-memory failure mode.

---

## > **HFT relevance**
> - **First choice: don't reclaim.** Fixed-capacity pools / ring buffers, sized to
>   the domain max. `examples/03`/`04` do this. No grace periods, no hazard
>   scans — just tag the ABA-prone CASes.
> - **If you must reclaim:** epochs/RCU (`11`) for throughput with **pinned
>   threads and bounded read sections** (so a stalled reader can't blow memory);
>   hazard pointers (`10`) when a rogue stall must not grow memory (bounded
>   garbage) at the cost of per-op work.
> - **Never `std::atomic<shared_ptr>` on the hot path** — not lock-free here, and
>   an atomic RMW per node kills traversal.
> - **Test reclamation under stalls** — a model checker or a stress test that
>   `SIGSTOP`s a reader mid-traversal. The UAF window is small and timing-
>   dependent; casual testing misses it.

---

## Hands-on

```bash
# The folder's examples deliberately avoid reclamation (fixed pools):
./build.ps1 fast 28-LOCK-FREE/examples/03_mpmc_queue.cpp   # bounded, monotonic idx
./build.ps1 fast 28-LOCK-FREE/examples/04_lock_free_stack.cpp   # pool + tagged head
```

On paper: take the MS queue `dequeue` (`07`) and mark every point where thread 2
could `delete` the node thread 1 is holding. Then place a hazard-pointer store /
an epoch enter-section and show the window closes.

---

## ⚠️ Traps

### Trap 1 — `delete` the unlinked node immediately
Another thread loaded that pointer before the unlink and is about to deref it →
use-after-free. Defer via a reclamation scheme.

### Trap 2 — refcounting the container instead of the node
The race is on a raw internal-node pointer, not the container handle. Per-node
protection (hazard pointer / epoch / per-node atomic count) is what's needed.

### Trap 3 — `std::atomic<std::shared_ptr>` on a hot structure
Usually not lock-free; a `lock`-prefixed RMW per node. Fine for read-rarely, wrong
for hot.

### Trap 4 — epochs with an unpinned / preemptible reader
A reader stuck in a read section defers every free → unbounded memory. Pin
threads, bound read sections, or use hazard pointers.

### Trap 5 — "fixed pool, never free" without ABA tags
Nodes *are* reused → ABA on the structure's and free-list's CASes. Tag them (`08`).

### Trap 6 — assuming GC-language intuition
"I still have a reference so it won't be freed" is true with a GC, false with a
raw pointer in C++.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "unlink then `delete`" | Another thread may hold the pointer — defer the free |
| "`shared_ptr` makes it safe and fast" | Safe yes; fast no (not lock-free here; RMW per node) |
| "epochs are free with no downside" | Free read side, but a stalled reader → unbounded retained memory |
| "hazard pointers are just slower epochs" | They bound the garbage; epochs don't |
| "fixed pool means no ABA" | Reused nodes → ABA on the CAS words; tag them |
| "refcount the queue object" | Doesn't protect a raw internal-node pointer |

---

## Exercises

1. **Name the window:** in MS-queue `dequeue`, between which two lines can another
   thread free the node you're about to CAS on?

   <details><summary>Answer</summary>

   Between `Node* head = head_.load()` / `Node* next = head->next.load()` and your
   successful `head_.compare_exchange(head, next)`. Another dequeuer can CAS
   `head_` past that node and free it in that window; your re-read of `head->next`
   (on a CAS retry) or a late deref then touches freed memory.
   </details>

2. **Scheme choice:** (a) a read-mostly config list traversed rarely; (b) a hot
   MS queue at 5 M ops/s with pinned threads; (c) a structure where a buggy
   plugin thread might hang mid-traversal.

   <details><summary>Answer</summary>

   (a) `atomic<shared_ptr>` or a big RCU grace period — simplicity wins, cost
   doesn't matter. (b) epoch/RCU — cheap read side, pinned threads keep grace
   periods short. (c) hazard pointers — bounded garbage even if a thread hangs;
   epochs would leak unboundedly.
   </details>

3. **Fixed pool:** why does a bounded ring buffer (`examples/03`) have no
   reclamation problem at all?

   <details><summary>Answer</summary>

   There are no per-node pointers handed around — access is by monotonic index
   into a fixed array. A slot is "freed" simply by the consumer's index passing
   it; nothing is deallocated, and the producer can't reuse the slot until the
   consumer's `seq`/index says so. No dangling pointer is possible.
   </details>

4. **`shared_ptr` cost:** a lock-free list traversal visits 20 nodes per lookup.
   With `atomic<shared_ptr<Node>>` links, what's the per-lookup atomic overhead
   roughly, on this box?

   <details><summary>Answer</summary>

   ~2 atomic RMWs per node (refcount ++ on acquire, -- on release), ~13 ns each →
   ~40 ns/node × 20 ≈ 800 ns of pure refcount traffic per lookup, before any
   actual comparison work — and worse under contention. That's why hot structures
   don't use it.
   </details>

5. **Grace period:** define it, and explain the unbounded-memory failure mode.

   <details><summary>Answer</summary>

   A grace period is the interval from a node's unlink until every thread that
   could have held a reference to it has passed a quiescent point (left all read
   sections). Only then is the node safe to free. Failure mode: if one thread
   stalls inside a read section, no grace period ever completes, so every retired
   node stays un-freed → retained memory grows without bound.
   </details>

---

## Interview questions

1. Reclamation problem exactly kya — kaunsa pointer, kaunsi race?
2. Container refcount se kyun fix nahi hota?
3. `std::atomic<shared_ptr>` — correct? fast? kyun/kyun nahi?
4. Fixed pool "never free" — kab acceptable, ABA ka kya?
5. Grace period kya hai — epochs/RCU ka core idea?
6. Hazard pointers vs epochs — bounded garbage kaun deta?
7. Epoch scheme ka unbounded-memory failure mode?

---

## Next
→ [`10-hazard-pointers.md`](10-hazard-pointers.md)
