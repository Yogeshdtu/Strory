# 11 — Epoch-based reclamation & RCU

## Prerequisites
- `09-memory-reclamation.md`, `10-hazard-pointers.md`

## Yeh topic abhi kyun
Hazard pointers har protected pointer pe ek store + reload maangte hain. **Epoch-
based reclamation (EBR)** aur uska cousin **RCU** read side ko ~free bana dete hain
— traversal karte waqt bas ek "read section" enter/exit karo. Trade-off:
ek stalled reader saari deferred frees rok deta hai. HFT mein (pinned threads,
short read sections) yeh trade acceptable hota, isliye RCU-style publication
market-data pointers ke liye bahut common hai.

---

## The RCU idea (Read-Copy-Update)

```
  Publish a new version:
    1. build the new object entirely (off to the side)
    2. atomic pointer swap:  g_ptr.store(new_obj, release)
    3. RETIRE the old object — but don't free it yet
    4. wait for a GRACE PERIOD, then free old_obj

  Readers:
    rcu_read_lock();                 // enter read section (cheap)
    T* p = g_ptr.load(acquire);      // grab the current version
    ... use *p ...                   // guaranteed alive for this section
    rcu_read_unlock();               // exit
```

"Copy-Update": you never mutate a live object; you build a new copy and swap the
pointer. Readers always see a complete, consistent version — either the old one or
the new one, never a half-updated one. This is **publication via release store**
(folder 27 file 08) + **deferred free**.

---

## Grace period = "everyone has moved on"

Old object safe to free once **every thread that could have been in a read section
at swap time has exited it at least once**. Two ways to detect:

### EBR (epoch-based)
A global epoch counter `e ∈ {0,1,2}`. Each thread, on entering a read section,
records the current global epoch in its per-thread slot. A reclaimer can advance
the global epoch (and free the batch retired *two* epochs ago) only when **all
active threads** are in the current or previous epoch. Three epochs give a safe
"nobody is still looking at gen `e-2`" window.

### QSBR (quiescent-state-based)
No explicit read sections. The application periodically calls `quiescent()` at a
point where it provably holds **no** references into the structure (e.g. top of an
event loop). A grace period completes when every thread has reported quiescent
since the retire. Cheapest read side of all (zero), but needs the app to have
natural quiescent points — which an HFT event loop does.

---

## Costs

| | EBR | QSBR | RCU (Linux kernel style) |
|---|---|---|---|
| `read_lock` / `read_unlock` | ~2 relaxed ops (record epoch, clear) | **nothing** | preempt-disable (kernel) / nothing (userspace variants) |
| Publish | pointer swap + retire | same | same |
| Reclaim | batched, on epoch advance | batched, on grace period | batched |
| Read-side scalability | perfect (no shared writes on read) | perfect | perfect |
| Failure mode | **stalled reader ⇒ epoch can't advance ⇒ unbounded retained memory** | same | same |

The read side doing **no shared writes** is why EBR/RCU scale to hundreds of
reader threads where hazard pointers (a store per protected ptr) and
`atomic<shared_ptr>` (an RMW per node) don't.

---

## The failure mode (say it plainly)

One thread that enters a read section and then **stalls** (preempted, blocked on
I/O, `SIGSTOP`, infinite loop) keeps its epoch pinned forever → the global epoch
can't advance → **every retired object across the whole process is retained**
until that thread moves. Memory grows without bound.

Mitigations HFT uses:
- **Pinned, non-preemptible-ish threads** with **short, bounded read sections** —
  a read section is "load pointer, read a few fields, done", microseconds at most.
- **No blocking calls inside a read section** — ever.
- A **watchdog** that alarms if the epoch hasn't advanced in N ms.
- Fall back to **hazard pointers** for any code path that might stall.

---

## Seqlock is the degenerate single-object case (`12`)

For **one** object published by **one** writer, you don't need EBR at all — a
seqlock (`12`) lets readers get a consistent snapshot with no reclamation (the
object's storage is fixed; the writer overwrites it in place, readers retry if
they catch a write in progress). EBR/RCU is for structures with **many** nodes
being retired over time.

---

## > **HFT relevance**
> - **RCU-style publication is the standard for read-mostly shared state** —
>   `RiskLimits`, `SymbolConfig`, a routing table, an `OrderBook` snapshot: build
>   the new version, `ptr.store(new, release)`, retire the old, free after a grace
>   period. Readers do one `acquire` load and never wait.
> - **QSBR fits an HFT event loop** — the top of each poll iteration is a natural
>   quiescent point (no references held across iterations). Zero read-side cost.
> - **Pinned threads + microsecond read sections** make the unbounded-memory
>   failure mode a non-issue *in practice* — but keep a watchdog on epoch
>   advancement.
> - **Never a blocking call in a read section.** One `mutex::lock` or syscall
>   inside `rcu_read_lock`/`unlock` can stall the epoch and leak memory.
> - **`liburcu`** (userspace RCU) is the production library; folly has
>   `folly::rcu`.

---

## Hands-on

```bash
# Folder examples use fixed pools (no reclamation) or seqlock (12) — the safe
# HFT defaults. To see RCU-style publication:
./build.ps1 fast 28-LOCK-FREE/examples/05_seqlock.cpp   # single-object version of the same idea
```

On paper: sketch EBR with 3 epochs. Show a reader that recorded epoch 0, a writer
that retired object X in epoch 0 and epoch 1, and why X can only be freed once the
global epoch reaches 2 *and* no thread is still in epoch 0.

---

## ⚠️ Traps

### Trap 1 — a blocking call inside a read section
Stalls the epoch → unbounded retained memory. Read sections must be tiny and
non-blocking.

### Trap 2 — mutating a live object instead of copy-swap
RCU readers must see a complete version. Build a new copy, swap the pointer; never
edit the object a reader might be in.

### Trap 3 — freeing on retire instead of after a grace period
The whole point is the deferral. `retire(old)` ≠ `free(old)`.

### Trap 4 — EBR with only 2 epochs
You need 3 (current, previous, safe-to-free) so a reader that recorded the
previous epoch can't still be looking at what you're about to free.

### Trap 5 — assuming bounded memory like hazard pointers
EBR/RCU retained memory is **unbounded** under a stalled reader. If you can't rule
that out, use hazard pointers.

### Trap 6 — QSBR without real quiescent points
If a thread never truthfully reports "I hold no references", grace periods never
complete. The quiescent call must be at a point where that's provably true.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "RCU updates the object in place" | Copy-Update: new version built aside, pointer swapped |
| "`retire` frees the object" | It defers; free happens after a grace period |
| "EBR needs 2 epochs" | 3 — current / previous / safe-to-reclaim |
| "epochs bound memory like hazard pointers" | No — a stalled reader retains everything |
| "read sections can do a bit of I/O" | Never — any stall pins the epoch |
| "need EBR to publish one snapshot" | One object + one writer → a seqlock (`12`), no reclamation |

---

## Exercises

1. **Why 3 epochs?** what goes wrong with only 2?

   <details><summary>Answer</summary>

   With epochs {0,1}: you retire in epoch 0, advance to 1, and want to free. But a
   reader that entered in epoch 0 and is still running could hold a reference to
   the object you're freeing — epoch 1 doesn't prove they've left. A third epoch
   gives a full "everyone who could have seen it has since passed a quiescent
   point" gap: free gen `e-2` only when all threads are in `e` or `e-1`.
   </details>

2. **Read-side cost:** compare per-node overhead for a 30-node traversal under
   EBR, hazard pointers, and `atomic<shared_ptr>` (this box).

   <details><summary>Answer</summary>

   EBR: ~2 relaxed ops for the whole traversal (enter/exit section) → ~0/node.
   Hazard pointers: a store + reload per protected pointer, ~1–2 live at a time →
   a few tens of ns total. `atomic<shared_ptr>`: ~2 RMWs/node × ~13 ns ≈ ~800 ns
   for 30 nodes. EBR wins the read side decisively.
   </details>

3. **Failure injection:** a reader thread hits an infinite loop inside
   `rcu_read_lock`/`unlock`. Trace what happens to retired-node memory over the
   next minute.

   <details><summary>Answer</summary>

   The looping thread's recorded epoch never changes → the reclaimer can never
   confirm "all threads have left epoch e" → the global epoch stops advancing →
   every `retire()`d node from every thread accumulates unfreed. Over a minute at,
   say, 1 M retires/s that's ~60 M nodes retained → OOM. A watchdog on epoch
   advancement is how you catch it.
   </details>

4. **QSBR fit:** why is an HFT poll loop a good match for QSBR?

   <details><summary>Answer</summary>

   At the top of each poll iteration the thread provably holds no references into
   any RCU-protected structure (it hasn't started processing an event yet). So it
   can call `quiescent()` there for free — no read-section bracketing, zero
   per-access cost — and grace periods complete every time all threads have looped
   once.
   </details>

5. **Copy-swap:** you need to add one entry to a 10,000-entry routing table read
   by 200 threads. RCU approach?

   <details><summary>Answer</summary>

   Don't mutate the live table. Allocate a new table (copy the 10,000 + the new
   entry, or use a structure with structural sharing), `table_ptr.store(new,
   release)`, `retire(old)`, free `old` after a grace period. The 200 readers each
   do one `acquire` load and see either the old or the new table, always complete.
   The copy cost is paid by the (rare) writer, not the (hot) readers.
   </details>

---

## Interview questions

1. RCU ka "copy-update" — object in place mutate kyun nahi?
2. Grace period ki definition — kab old object free safe?
3. EBR mein 3 epochs kyun (2 se kya toota)?
4. QSBR vs EBR — read-side cost ka farq?
5. Epoch/RCU ka unbounded-memory failure mode — kaise trigger, kaise mitigate?
6. Read section mein blocking call kyun banned?
7. Ek single snapshot publish karne ke liye EBR chahiye? (Nahi — seqlock.)

---

## Next
→ [`12-seqlock.md`](12-seqlock.md)
