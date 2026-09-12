# 08 — Lock-free problems

## Prerequisites
- `27-ATOMICS-MEMORY-MODEL/` (poora — atomics, memory_order, happens-before,
  fences)
- `28-LOCK-FREE/` (poora — CAS, ABA, Treiber stack, SPSC/MPSC, hazard pointers)
- `07-concurrency-problems.md` (mutex baseline)

## Yeh file kya hai
15 problems — atomics, CAS loops, memory ordering, the ABA problem, SPSC/MPSC
rings, seqlock, safe reclamation, false sharing. Yeh HFT interview ka **deep
end** hai.

Har problem: `<details>` mein approach + ordering justification + the trap.
Poora code → [`11-solutions/08-lock-free-solutions.md`](11-solutions/08-lock-free-solutions.md).

**Rule:** har atomic op ke saath likho *why this memory_order and not weaker/
stronger*. "Bas `seq_cst` laga do" interview mein fail hai.

Compile: `g++ -std=c++20 -Wall -Wextra -pthread -O2 file.cpp -o t && ./t`
(lock-free bugs `-O2` pe zyada dikhte — reordering.)

---

## 1. atomic_flag spinlock
`SpinLock` — `lock()` / `unlock()` sirf `std::atomic_flag` se.
<details><summary>Approach</summary>

`lock`: `while (flag.test_and_set(std::memory_order_acquire)) { while
(flag.test(std::memory_order_relaxed)) _mm_pause(); }` — TTAS (test-test-and-set)
+ `pause` to reduce bus traffic. `unlock`: `flag.clear(std::memory_order_release)`.
Acquire on lock / release on unlock = critical-section writes properly published.
No fairness — starvation possible.
</details>

## 2. Relaxed atomic counter — when is it safe?
Ek shared statistics counter kai threads badhaate. Kab `relaxed` OK, kab nahi?
<details><summary>Answer</summary>

`relaxed` OK jab counter ki value kisi doosre data ko "guard" nahi karti — bas
final total chahiye (stats, metrics). Atomicity (no lost updates) `relaxed` bhi
deta; sirf **ordering** guarantee nahi. **Not OK** jab tum counter dekh ke koi
aur memory access karte ho (e.g. "count reached N → array is full") — wahan
acquire/release chahiye.
</details>

## 3. Treiber stack + ABA
Lock-free stack (`push`/`pop`) CAS se. ABA problem demonstrate karo.
<details><summary>Approach</summary>

`push`: `new_node->next = head.load(relaxed); while
(!head.compare_exchange_weak(new_node->next, new_node, release, relaxed));`.
`pop`: load head, `cas(head, head->next)`. **ABA:** thread reads head=A; stalls;
another pops A, pops B, pushes A back (A reused); first thread's `cas(A, B)`
succeeds but B is now freed/wrong. Fix → #4.
</details>

## 4. ABA fix — tagged pointer / generation counter
#3 ka ABA fix.
<details><summary>Approach</summary>

`head` ko `{Node* ptr; uintptr_t tag;}` banao (128-bit CAS: `cmpxchg16b` /
`std::atomic<struct>` if lock-free). Har successful CAS `tag`++ karta. A→B→A mein
tag badal gaya → stale CAS fail. Alternative: pack a 16-bit counter in the
pointer's unused high/low bits (48-bit VA / 8-byte alignment). Hazard pointers
(#11) bhi ABA solve karte (freed nahi hoga jab tak hazard).
</details>

## 5. SPSC ring buffer — the real one
Single producer, single consumer, bounded, lock-free.
<details><summary>Approach</summary>

Power-of-two capacity, `head_`/`tail_` `std::atomic<size_t>`, `& (CAP-1)` wrap.
Producer: `t = tail_.load(relaxed); if (t - head_.load(acquire) == CAP) full;
buf[t & mask] = x; tail_.store(t + 1, release);`. Consumer symmetric with roles
swapped. **No CAS** — har index ka ek hi writer. Acquire/release pair = data
write happens-before the read. `alignas(64)` on `head_`, `tail_` + cache the
opposite index to cut coherence traffic. Folder 28/41; example in
`46-INTERVIEW-PREP/examples/03_spsc_ring_buffer.cpp`.
</details>

## 6. MPSC queue (Vyukov intrusive)
Many producers, one consumer, unbounded, lock-free-ish.
<details><summary>Approach</summary>

Intrusive nodes (`next` atomic). Producer: `prev = tail.exchange(node,
acq_rel); prev->next.store(node, release);` — `XCHG`, no CAS loop, wait-free for
producers. Consumer: follow `head->next`; ek transient state jahan `prev->next`
abhi set nahi hua (producer preempted between exchange and store) → consumer
"empty" return karta us pal. Stub node se head/tail kabhi null nahi.
</details>

## 7. seqlock
Ek writer, many readers, ek small struct (e.g. `{price, qty}`). Readers never
block the writer.
<details><summary>Approach</summary>

`std::atomic<uint64_t> seq{0};`. Writer: `seq.store(s+1, relaxed); /* release
fence */ data = …; seq.store(s+2, release);` (odd = write in progress). Reader:
`s1 = seq.load(acquire); read data; s2 = seq.load(relaxed); retry if (s1 & 1) ||
s1 != s2`. Great for hot market-data snapshots — reader retries on the rare
collision. Data race technically on `data` — use atomics per field or
`memcpy` + fences carefully (folder 28).
</details>

## 8. compare_exchange_weak vs _strong
Loop shape, spurious failure — kab kaunsa.
<details><summary>Answer</summary>

`_weak` spuriously fail kar sakta (even if `expected` matches) — cheaper on
LL/SC architectures (ARM/POWER). Use in a **loop** (you're retrying anyway):
`while (!x.compare_exchange_weak(exp, des));`. `_strong` jab loop nahi hai (single
attempt, e.g. lazy init) — spurious fail ka extra branch nahi chahiye. x86 pe
dono same (`LOCK CMPXCHG`). On fail, `expected` gets the current value.
</details>

## 9. Publish a pointer safely
Thread A ek object banata + publish karta; thread B use padhta. Plain store kyun
bug hai?
<details><summary>Answer</summary>

A: `p->x = 1; p->y = 2; gPtr.store(p, release);` B: `T* q = gPtr.load(acquire);
use q->x;`. Release store ⇒ `x=1,y=2` writes B ko dikhte **before** it sees the
pointer (acquire pairs with release → happens-before). Plain (relaxed) store:
compiler/CPU `gPtr` write ko `x`/`y` writes se pehle kar sakta → B ko pointer
dikhta par fields garbage. x86 pe often "works" (TSO), ARM pe breaks.
</details>

## 10. Lock-free freelist
Fixed slab, `alloc`/`free` CAS-ing a `head` index/pointer. ABA yahan bhi.
<details><summary>Approach</summary>

`alloc`: `h = head.load(acquire); while (h && !head.compare_exchange_weak(h,
h->next, acquire));` return `h`. `free`: `n->next = head.load(relaxed); while
(!head.compare_exchange_weak(n->next, n, release));`. ABA: same slot alloc/free/
alloc between load and CAS → tag the head (#4). SPSC pool → no CAS needed at all.
</details>

## 11. Hazard pointers
#3 ke Treiber stack ka safe `delete` — hazard pointers se.
<details><summary>Approach</summary>

Reader `pop`: candidate node ko `hp[tid].store(node, release)` (hazard), phir
**re-validate** `head` still == node, phir use. Retiring thread: node ko
`retired[]` list mein daale; jab list badi ho, saare `hp[]` slots scan kare;
jo retired node kisi hazard mein nahi → `delete`. `O(1)` read, deferred + bounded
reclamation. ABA bhi solved (node freed hi nahi hota while hazarded).
</details>

## 12. RCU concept
Read-Copy-Update — reader side kaise "free" hota, writer kaise reclaim karta?
<details><summary>Answer</summary>

Readers: `rcu_read_lock()` = bas preemption disable / mark epoch — **no atomics,
no waiting**. Writer: naya version banao, pointer ko atomically swap
(`rcu_assign_pointer` = release store), phir `synchronize_rcu()` — wait until
every reader that could've seen the old pointer has finished (grace period) —
phir purana free. Ideal for read:write = 1000:1 (routing tables, config). Linux
kernel mein everywhere.
</details>

## 13. memory_order_consume — why avoid
`consume` ka intent kya tha, aur kyun log `acquire` use karte hain?
<details><summary>Answer</summary>

`consume` = "sirf data-dependent loads ko order karo" (cheaper than acquire on
weakly-ordered CPUs — no fence, just rely on the dependency chain). **Par** koi
compiler ise implement nahi karta as-specified — sab `consume` ko `acquire` mein
promote kar dete (dependency tracking through arbitrary code too hard). Standard
committee ne effectively deprecate kar diya. Use `acquire`.
</details>

## 14. Contended atomic vs sharded counters — benchmark
`N` threads ek `atomic<long>` `fetch_add` karte vs `N` alag counters (baad mein
sum). Measure the cache-line ping-pong.
<details><summary>Approach</summary>

Single atomic: har `fetch_add` ko line ka exclusive ownership chahiye → cores ke
beech line bounce → throughput `N` ke saath **girta**. Sharded: per-thread
`alignas(64)` counter, zero contention, linear scaling; read = sum all shards
(rare). Expect 5–50× on 4+ threads. Folder 32; `09-optimization-problems.md` #19.
</details>

## 15. False sharing in a lock-free queue
SPSC ring ka `head_` aur `tail_` same cache line pe → producer aur consumer ek
doosre ko slow karte bina koi logical sharing ke. Fix.
<details><summary>Approach</summary>

`head_` sirf consumer likhta, `tail_` sirf producer — logically independent. Same
64B line pe → producer ka `tail_` store consumer ke `head_` line ko invalidate
karta (aur ulta). `alignas(64) std::atomic<size_t> head_;` … padding … `alignas
(64) std::atomic<size_t> tail_;` — alag lines. Plus opposite index ko locally
cache karo (har baar atomic load na karo). Measurable ~2× on the ring throughput.
</details>

---

## Next
→ [`09-optimization-problems.md`](09-optimization-problems.md)
