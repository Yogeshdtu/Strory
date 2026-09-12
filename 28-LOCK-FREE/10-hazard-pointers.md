# 10 — Hazard pointers

## Prerequisites
- `09-memory-reclamation.md`
- `27-ATOMICS-MEMORY-MODEL` files 08 (acquire/release), 11 (fences)

## Yeh topic abhi kyun
Hazard pointers (Maged Michael, 2004) reclamation ka woh scheme hai jo **bounded
garbage** deta hai — ek stalled thread bhi memory ko unbounded grow nahi kar
sakta. Idea: har thread jo node use karne wala hai, use **publicly announce**
karta hai; reclaimer kisi announced node ko free nahi karta. C++26 mein
`std::hazard_pointer` aa raha hai; concept har lock-free library mein hai.

---

## The mechanism

```
  Global: an array of per-thread HAZARD SLOTS (single-writer each)

      hp[0]  hp[1]  hp[2]  ...  hp[T*K-1]      (T threads, K slots each)
       │      │
   thread 0's two slots

  Reader (before touching a node it might race on):
    1. load the pointer:        p = node_ptr.load(acquire)
    2. publish it:              my_hp.store(p, release)         // "I'm using p"
    3. re-validate:             if (node_ptr.load(acquire) != p) goto 1   // p changed -> retry
    4. now safe to deref p — a reclaimer will see my_hp == p and skip it

  Reader (done with p):
    my_hp.store(nullptr, release)

  Retiring thread (unlinked node p, wants to free it):
    1. push p onto this thread's private "retired" list
    2. when the retired list grows past a threshold R:
       - collect all non-null hazard slot values into a set H
       - for each retired q: if q in H -> keep; else -> free(q)
```

Step 3 (**re-validate after publishing**) is the crux: if the node was unlinked
between load and publish, the reclaimer might have already passed its scan without
seeing your slot → you must notice `p` changed and retry, because `p` might now be
free.

---

## Why garbage is bounded

At any instant, at most `T × K` nodes are hazard-protected (one per slot). Each
thread's retired list is scanned when it hits threshold `R`. So total un-freed
nodes ≤ `T × R` (+ the `T × K` protected). Pick `R ≈ 2 × T × K` → amortized O(1)
frees per retire, and retained memory is a fixed multiple of thread count. **A
stalled thread pins at most `K` nodes** — not the whole world (unlike epochs).

---

## Costs

| Op | Cost |
|---|---|
| Protect a pointer | 1 release store + 1 acquire re-load (+ retry on the rare race) |
| Unprotect | 1 relaxed/release store (nullptr) |
| Retire a node | push to a private list (~free) |
| Scan (every `R` retires) | read `T×K` slots, build a set, partition the retired list — O(T·K + R) amortized to O(1) per retire |

Per-traversal-step overhead: ~a store + a load per node you hold a hazard on
(often just 1–2 live hazards at a time, not one per node visited). Cheaper than
`atomic<shared_ptr>` (which is an RMW per node), pricier than epochs (which are
~nothing on the read side).

---

## Hazard pointers vs epochs (`11`)

| | Hazard pointers | Epoch / RCU |
|---|---|---|
| Read-side cost | store + reload per protected ptr | ~2 relaxed ops per read section |
| Retained garbage | **bounded** (`T·R`) | **unbounded** if a reader stalls |
| Stalled reader | pins ≤ K nodes | pins **every** retired node |
| Complexity | per-pointer discipline, re-validation | per-section enter/exit, epoch bump |
| Best for | adversarial / preemptible readers, bounded memory required | pinned threads, short read sections, max throughput |

---

## In practice

- **libcds**, **folly** (`folly::hazptr`), **boost** all ship hazard-pointer
  implementations. C++26 adds `std::hazard_pointer` / `std::hazard_pointer_obj_base`.
- Number of slots per thread `K` is small — 1 or 2 for a stack/queue, more for
  structures that hold several node refs at once (a skiplist).
- Combine with a **fixed node pool**: freed nodes go back to the pool's free-list
  rather than to `operator delete`; hazard pointers just gate *when* a node may
  return to the pool.

---

## > **HFT relevance**
> - **Use hazard pointers when a stall must not grow memory** — a strategy thread
>   that can block on a slow calculation, a plugin you don't fully trust. Bounded
>   garbage is the safety property epochs lack.
> - **Keep `K` tiny** — 1–2 hazards per thread for a queue/stack. The scan cost is
>   `T·K`; small `K` and pinned thread count keep it trivial.
> - **Still prefer "don't reclaim"** — if the structure has a bounded capacity,
>   a fixed pool + tagged CAS (`08`) beats any reclamation scheme for both speed
>   and simplicity. Hazard pointers are for genuinely unbounded structures.
> - **The re-validation step is not optional** — skipping the post-publish re-load
>   reopens the exact UAF window hazard pointers exist to close.

---

## Hands-on

```bash
# No standalone example (a correct HP implementation is ~150 lines and its
# failure mode needs a mid-traversal stall to show). Study the shape:
./build.ps1 fast 28-LOCK-FREE/examples/04_lock_free_stack.cpp   # pool + tag = the "don't reclaim" alternative
```

On paper: take the Treiber `pop` (`08`), insert the 4-step protect sequence before
`head->next` is read, and show that a concurrent `retire(head)` scan now sees your
hazard slot and defers the free.

---

## ⚠️ Traps

### Trap 1 — no re-validation after publishing the hazard
`hp.store(p)` then deref `p` without re-checking `p` is still linked → the
reclaimer may have scanned before your store → UAF. Re-load and compare.

### Trap 2 — one hazard slot when you hold two node refs
E.g. `head` and `head->next` both need protection during a pop retry. Size `K` to
the max simultaneous live references.

### Trap 3 — forgetting to clear the slot
A stale hazard slot pins a node forever → that node (and its successors, if you
retire in order) never freed. Clear on every exit path, including error paths.

### Trap 4 — scanning on every retire
O(T·K) per retire kills throughput. Batch: scan only when the private retired list
hits threshold `R`.

### Trap 5 — hazard pointers without ABA protection where nodes recycle
HP stops *use-after-free*, not ABA on a CAS. If a node can be reused while your
CAS is pending you still need a tag (`08`) or an HP-based "is still the same node"
check.

### Trap 6 — using epochs' mental model ("a stall is fine")
With hazard pointers a stall pins ≤ K nodes; with epochs it pins everything. Don't
carry the epoch intuition over — but *do* still keep `K` small.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "publish the pointer, then deref" | Publish, **re-validate it's still linked**, then deref |
| "one hazard slot per thread is always enough" | Size `K` to the max node refs held at once |
| "scan on every retire" | Batch — scan at a retired-list threshold `R` |
| "hazard pointers also fix ABA" | They stop UAF; ABA on a CAS still needs a tag |
| "same failure mode as epochs" | HP retains bounded garbage; a stalled thread pins ≤ K nodes |
| "always better than not reclaiming" | A bounded fixed pool + tag is simpler and faster where capacity is bounded |

---

## Exercises

1. **The race HP closes:** describe the window between loading a node pointer and
   publishing the hazard, and how re-validation handles it.

   <details><summary>Answer</summary>

   You load `p = node_ptr` but haven't stored it to your hazard slot yet. A
   retiring thread unlinks `p`, scans hazard slots (doesn't see `p`), frees it.
   You then publish `p` and deref → UAF. Re-validation: after publishing, re-load
   `node_ptr`; if it != `p`, `p` was unlinked (and maybe freed) — discard and
   retry from the fresh value.
   </details>

2. **Bounded garbage math:** 8 threads, 2 hazard slots each, retire threshold R =
   64. Max un-freed nodes?

   <details><summary>Answer</summary>

   Protected: 8 × 2 = 16. Pending on private retired lists: up to 8 × 64 = 512. So
   ≤ ~528 nodes un-freed at any instant — a fixed bound independent of runtime
   length, and unaffected by a stalled thread (it pins only its 2 slots).
   </details>

3. **HP vs epoch:** a thread `SIGSTOP`s for 10 seconds mid-traversal. What happens
   to retained memory under each?

   <details><summary>Answer</summary>

   Hazard pointers: the stopped thread pins at most `K` nodes (whatever was in its
   slots); everything else keeps getting freed → memory stays bounded. Epochs: the
   stopped thread never leaves its read section → no grace period completes → every
   retired node across all threads is retained for 10 seconds → memory can blow up.
   </details>

4. **How many slots** for a lock-free singly-linked list `find` that holds `prev`
   and `curr` while scanning?

   <details><summary>Answer</summary>

   At least 2 (`prev` and `curr`) — and often 3 if you also protect `curr->next`
   before advancing. Size `K` to the peak count of live node references the
   traversal holds simultaneously.
   </details>

5. **Still need a tag?** you protect the Treiber `head` with a hazard pointer.
   Does that remove the need for the version tag?

   <details><summary>Answer</summary>

   It removes the *use-after-free* (the node won't be freed while your hazard is
   set), but not ABA: the node could still be popped, its value changed, and
   pushed back with the same address while your `CAS(head, old, old->next)` is
   pending — value matches, premise stale. You still need a tag, or an HP-based
   "head unchanged since I protected it" re-check.
   </details>

---

## Interview questions

1. Hazard pointer ka 4-step protect sequence — re-validation kyun zaroori?
2. Garbage bounded kaise rehta — max un-freed nodes ka formula?
3. Stalled thread ka asar — HP vs epoch?
4. Kitne slots per thread — kis pe depend karta?
5. Retire pe har baar scan kyun nahi — threshold R ka role?
6. HP ABA fix karta hai? (Nahi — kya karta.)
7. Kab HP, kab "don't reclaim" (fixed pool)?

---

## Next
→ [`11-epoch-based-reclamation.md`](11-epoch-based-reclamation.md)
