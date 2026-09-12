# 15 — The ABA problem

## Prerequisites
- `05-compare-exchange.md`, `14-lock-free-definitions.md`
- [`examples/07_aba_problem.cpp`](examples/07_aba_problem.cpp)

## Yeh topic abhi kyun
CAS **value equality** check karta hai — "abhi bhi wahi value hai jo maine padhi
thi?" Par "wahi value" ≠ "kuch nahi badla". Agar value `A → B → A` gayi, CAS ko
lagega kuch nahi hua, aur woh ek **stale premise** pe succeed kar jayega. Yeh
lock-free stacks/queues ka #1 correctness bug hai. Folder 28 ke har pointer-CAS
structure ko isse bachna padta hai.

---

## The classic scenario: Treiber stack pop

```cpp
// pop():
Node* head = h.load(acquire);
do {
    if (!head) return nullptr;
    Node* next = head->next;              // (READ head->next)
} while (!h.compare_exchange_weak(head, next, acq_rel, acquire));
return head;   // caller now owns this node
```

Interleaving:

| Time | Thread 1 | Thread 2 | Stack |
|---|---|---|---|
| t0 | reads `head = A`, `next = A->next = B` | | `A → B → C` |
| t1 | *(descheduled)* | pop → returns `A` | `B → C` |
| t2 | | pop → returns `B` | `C` |
| t3 | | push(A) (reuses the freed `A` node!) | `A → C` |
| t4 | resumes: `CAS(h, A, B)` — **`h` is `A`, so it SUCCEEDS** | | ❌ |
| t5 | stack head set to `B` | | **`B` was already popped — freed / reused. Corrupt.** |

Thread 1's CAS saw `head == A` and concluded "nothing changed" — but the whole
list under `A` changed. `B` is now a dangling / reused node. The stack is
corrupted (lost nodes, use-after-free, cycles).

`examples/07` reproduces this deterministically with an index-based free list and
a `phase` handshake that forces exactly this interleaving: the stale
`CAS(head, 0, 1)` succeeds and `head` ends up pointing at an already-popped node.

---

## Why ABA needs *reuse*

ABA bites when the value that comes back (`A`) refers to something whose **meaning
changed** — a pointer/index that was freed and handed back out. If nodes were
never reused (unbounded fresh allocation, or a GC), `A` returning would genuinely
mean "still the same node, still on top" and the CAS would be correct.

So the two ingredients:
1. A CAS on a value that identifies a resource (pointer, array index, slot id).
2. That resource getting **recycled** (freed and re-allocated) between the read
   and the CAS.

---

## Fix 1 — tagged pointer / versioned word (the standard trick)

Pack a **monotonic counter** next to the value in one atomic word. Every
modification bumps the counter, so `A` with tag 5 ≠ `A` with tag 7 — the CAS
compares the *whole word* and fails on stale.

```cpp
// 64-bit word: low 32 bits = node index, high 32 bits = version tag
static uint64_t pack(uint32_t idx, uint32_t tag) { return (uint64_t(tag) << 32) | idx; }
std::atomic<uint64_t> head_tagged;

// push: bump tag every time
uint64_t cur = head_tagged.load(acquire);
uint64_t next;
do {
    uint32_t cur_idx = uint32_t(cur);
    uint32_t cur_tag = uint32_t(cur >> 32);
    pool[new_idx].next = cur_idx;
    next = pack(new_idx, cur_tag + 1);       // <-- tag++
} while (!head_tagged.compare_exchange_weak(cur, next, acq_rel, acquire));
```

Now the t4 stale CAS in the table above compares `(A, tag=0)` against the live
`(A, tag=3)` → **fails**, Thread 1 re-reads, no corruption. `examples/07`'s fix
does exactly this: tag goes 0→3 across the interleaving, stale CAS fails.

**Width matters:**
- 64-bit word = 32-bit index + 32-bit tag → works with a plain
  `std::atomic<uint64_t>` (lock-free everywhere). Index space ≤ 4B nodes; tag
  wraps after 4B modifications (practically safe, not theoretically).
- Real 64-bit *pointers* + a tag need a **128-bit CAS** (`cmpxchg16b` /
  `CASP` on ARM) — `std::atomic<__int128>` / a struct. **On this MinGW that
  doesn't link** (`__atomic_*_16` undefined) — hence the example uses a 32-bit
  index + 32-bit tag in a 64-bit word.
- Or steal alignment bits: a 16-byte-aligned pointer has 4 low bits free for a
  tiny tag (only defers ABA, doesn't kill it).

---

## Fix 2 — don't reuse memory until it's safe (folder 28)

Remove ingredient #2. Defer reclamation of a popped node until **no thread can
still hold a reference** to it:

- **Hazard pointers** — each thread publishes the node it's about to touch;
  reclamation skips any hazard-listed node.
- **Epoch-based reclamation / RCU** — free nodes only after every thread has
  passed a quiescent point.
- **Reference counting** the nodes (atomic) — free at count 0.
- **GC** — languages with it don't have ABA on pointers at all.

These are the folder-28 topic (files 9–11). Tagging is the cheap fix when a
bounded index space works (which, for a fixed-capacity HFT pool, it does).

---

## ABA in non-pointer contexts

- **Array index free-list** (`examples/07`) — same problem with slot indices.
- **Sequence / generation numbers** — a bounded counter that wraps: `seq` goes
  all the way around back to the same value → a stale check passes. Use a wide
  enough counter.
- **`compare_exchange` on a "state" enum** that cycles `IDLE→BUSY→IDLE` — a thread
  that read `IDLE` long ago CAS-es into a *different* `IDLE`. Add a generation.

---

## > **HFT relevance**
> - **Every pointer/index CAS in the engine's lock-free structures carries a
>   version tag** — the fixed-capacity node pool means a 32-bit index + 32-bit tag
>   in one `uint64_t` is lock-free on x86 and ARM, no 128-bit CAS needed.
> - **Prefer designs without pointer-CAS at all** — SPSC/MPSC ring buffers use
>   monotonically-increasing 64-bit indices (never reused, never wrap in any
>   realistic runtime) → **no ABA by construction**. That's a big reason the hot
>   path is ring buffers, not linked lists (folder 28).
> - **If you must reclaim nodes** (Michael–Scott queue), hazard pointers or epochs
>   — budget for their cost and complexity, or accept a bounded pool that never
>   frees.
> - **Test for ABA deliberately** — a forced interleaving (like `examples/07`'s
>   phase handshake) or a model checker; ABA almost never shows up in casual
>   stress testing, then corrupts in production.

---

## Hands-on

```bash
./build.ps1 fast 27-ATOMICS-MEMORY-MODEL/examples/07_aba_problem.cpp
```
BROKEN run: forced interleaving → stale `CAS` succeeds → `head` points at an
already-popped node (detected + printed). FIXED run: 32/32 packed
`{index, tag}` → tag mismatch → stale `CAS` fails → structure stays consistent.
Then:
- Widen the tag from the example and confirm behaviour is unchanged (tag just
  needs to not wrap during the window).
- Sketch why an SPSC ring buffer with `uint64_t` head/tail counters has no ABA.

---

## ⚠️ Traps

### Trap 1 — "CAS is atomic so the state is consistent"
CAS checks the **value**, not history. `A→B→A` defeats it.

### Trap 2 — tagging the pointer but not bumping the tag on every mutation
The tag must increment on *every* push/pop (any op that could recycle the value),
or a stale CAS can still match.

### Trap 3 — assuming you need a 128-bit CAS
If your resource id fits in 32 bits (bounded pool), a 32-bit id + 32-bit tag in a
`uint64_t` is enough and is lock-free everywhere.

### Trap 4 — tag too narrow / counter wraps
An 8-bit or 16-bit tag can wrap within the vulnerable window under load. 32 bits
is the practical minimum for a hot structure.

### Trap 5 — ABA only considered for pointers
Bounded sequence numbers, cycling state enums, index free-lists — all ABA-prone.

### Trap 6 — "stress test passed, no ABA"
ABA needs a specific interleaving that's rare by chance. Force it or model-check
it.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "the value is unchanged, so nothing changed" | The value returned to `A`, but the thing it names was freed/reused |
| "ABA is a theoretical curiosity" | It's the standard corruption mode of lock-free stacks/queues with reuse |
| "tagged pointers need `cmpxchg16b`" | Only for full 64-bit pointer + tag; a 32-bit id + 32-bit tag fits `uint64_t` |
| "bump the tag on push only" | Bump on every mutation that can recycle the value |
| "ring buffers have ABA too" | Monotonic 64-bit index counters are never reused → no ABA |
| "GC languages still have pointer ABA" | No — GC keeps the node alive, so `A` really is the same node |

---

## Exercises

1. **Spot the reuse:** in the Treiber-pop table, exactly which step makes Thread
   1's CAS premise stale, and what does the CAS wrongly do?

   <details><summary>Answer</summary>

   Step t3: Thread 2 pushes the *recycled* node `A` back as head. Now `h == A`
   again, but `A->next` is `C`, not the `B` Thread 1 cached. Thread 1's
   `CAS(h, A, B)` succeeds and sets head to `B` — a node already popped and
   possibly freed → use-after-free / lost `C`.
   </details>

2. **Tag width:** a lock-free pool of 1024 slots, ~50M ops/sec, worst-case thread
   stall 1 ms. Is a 16-bit tag safe? A 32-bit tag?

   <details><summary>Answer</summary>

   In 1 ms at 50M ops/s ≈ 50,000 modifications. A 16-bit tag (65,536 values) can
   *almost* wrap in one stall window — unsafe. A 32-bit tag (~4.3B) needs ~86 s of
   continuous modification to wrap — safe in practice.
   </details>

3. **No ABA by design:** why does an SPSC ring with `std::atomic<uint64_t> head,
   tail` (monotonically increasing, index = `pos % N`) not need tags?

   <details><summary>Answer</summary>

   The CAS/stores are on the *counters*, which only ever increase and won't wrap
   in any realistic runtime (2^64 ops). A counter value is never reused, so there's
   no "back to A" — the slot array is reused, but access to it is gated by the
   monotonic counters, not by a CAS on a recycled value.
   </details>

4. **Non-pointer ABA:** `std::atomic<State> s;` cycling `IDLE→RUNNING→IDLE`. A
   thread read `IDLE`, stalled, wants to CAS `IDLE→RUNNING`. What can go wrong,
   and the fix?

   <details><summary>Answer</summary>

   It CAS-es into a *later* `IDLE` (a different logical state instance) as if no
   one had run in between — it may re-trigger work that already ran, or step on
   the thread that legitimately owns the current `IDLE`. Fix: pack a generation
   counter with the state and bump it on every transition.
   </details>

5. **Which fix:** you need a Michael–Scott queue (nodes are dynamically
   allocated and freed). Tagging enough?

   <details><summary>Answer</summary>

   Tagging stops the CAS from matching stale, but it does **not** make it safe to
   *dereference or free* a node another thread might still be reading — that's a
   use-after-free independent of the CAS. You need a reclamation scheme (hazard
   pointers / epochs / refcount) — folder 28 files 9–11. Tagging + a safe
   reclamation scheme together.
   </details>

---

## Interview questions

1. ABA problem — ek Treiber-stack pop interleaving se samjhao.
2. ABA ke liye kaunse do ingredients zaroori (CAS on resource id + reuse)?
3. Tagged pointer / versioned word fix — kaise kaam karta, tag kab bump?
4. 128-bit CAS kab chahiye, kab 64-bit word kaafi (bounded index)?
5. Tag width — kitna, wrap ka risk kaise estimate karein?
6. Ring buffer mein ABA kyun nahi (monotonic counters)?
7. Tagging use-after-free se bachata hai? (Nahi — reclamation alag problem.)

---

## Next
→ [`16-litmus-tests.md`](16-litmus-tests.md)
