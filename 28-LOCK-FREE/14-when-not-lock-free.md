# 14 — When NOT to go lock-free

## Prerequisites
- Poora folder 28 tak (`01`–`13`)

## Yeh topic abhi kyun
CLAUDE.md ka spec: "no micro-optimization tricks without context — har technique
ke saath trade-off." Lock-free ka **honest** trade-off yeh hai: complexity,
subtle bugs, aur — jaisa `examples/04` ne dikhaya — **kabhi-kabhi seedha slower**.
Yeh lesson lock-free ke *against* argument hai, taaki aap jaano kab mutex (ya kuch
aur) sahi choice hai.

---

## The costs of going lock-free

| Cost | Detail |
|---|---|
| **Correctness risk** | ABA, missing acquire/release, reclamation UAF, torn reads — bugs that pass x86 stress tests and corrupt under load / on ARM (`13`) |
| **Reclamation burden** | linked structures need hazard pointers / epochs — real per-op cost + a whole subsystem (`09`–`11`) |
| **Review/maintenance** | every shared byte needs a happens-before argument; new team members can't safely touch it |
| **Retry storms** | contended CAS loops can be *slower* than a mutex (`examples/04`: ~5×) |
| **Portability** | memory orders tuned to x86 TSO break on ARM; needs model checking + ARM CI |
| **Harder to compose** | two lock-free ops don't make a lock-free transaction |
| **Debugging** | non-deterministic, timing-dependent; a heisenbug that vanishes under a debugger |

---

## When a `std::mutex` is the right answer

### 1. Low or bursty contention
Uncontended `mutex::lock()` ≈ one CAS (~13 ns here). If threads rarely collide,
that's the whole cost — and you get simple, obviously-correct code. Most
application-level shared state is here.

### 2. Short critical section, cache-friendly data
`examples/04`: `mutex + std::vector` beat a lock-free stack ~5× because the
critical section was ~2 instructions on a hot cache line. A tiny, hot critical
section barely contends.

### 3. The operation is inherently multi-step / needs a real transaction
"Move an item from map A to map B atomically", "update three fields consistently".
A mutex makes this trivial; lock-free makes it a research problem.

### 4. Correctness matters more than the last microsecond
Control plane, config, admin, anything off the hot path. A mutex here is *more*
reliable and everyone can maintain it.

### 5. You can't model-check / ARM-test it
If your CI can't prove the memory orders (`13`), a hand-rolled lock-free structure
is a liability. Use a mutex, or a **vetted library** (folly, boost, TBB, liburcu).

---

## Better-than-lock-free options

| Instead of a hand-rolled lock-free structure | Consider |
|---|---|
| Contended shared counter with a CAS loop | `fetch_add` (one instruction, no retry) or per-thread counters + combine (folder 26 `03`) |
| Contended lock-free stack/queue | **Shard** it — per-thread structure, steal only when empty |
| N→M lock-free queue | **N sharded SPSC rings** (wait-free per link) into one consumer |
| Lock-free hash map | A sharded map: `K` shards, each a `mutex` + plain map. Simple, scales with `K` |
| `atomic<shared_ptr>` traversal | Seqlock (`12`) for one object, RCU pointer-swap for a structure |
| Elaborate lock-free anything | A **vetted library** — don't hand-roll what folly/TBB/liburcu already got right |

**Sharding is the most underused answer.** It turns a contention problem into `K`
independent low-contention problems, each of which a plain mutex handles fine.

---

## The decision checklist

```
Is this on the latency-critical hot path?            no  -> mutex (or sharded mutex)
  yes ↓
Is the contention shape SPSC or shardable?           no  -> reconsider the design;
  yes ↓                                                     a single hot contended point
                                                            rarely wants lock-free
Can you use a vetted library instead of hand-rolling? yes -> use it
  no ↓
Can CI model-check the protocol + stress on ARM?     no  -> mutex / library; hand-rolled
  yes ↓                                                     lock-free is a liability
Have you benchmarked it vs a mutex baseline and won?  no  -> ship the mutex until you have
  yes ↓
Ship the lock-free version. Keep the mutex baseline as a fallback + comparison.
```

---

## `examples/04` recap — the cautionary result

A **correct**, ABA-safe, tagged lock-free Treiber stack: **~5× slower** than
`mutex + std::vector` under 4-thread contention on this box. It's still lock-free
(progress under preemption, no deadlock) — but if the goal was throughput, the
mutex won. Always benchmark against the boring baseline; "lock-free" is not a
performance claim.

---

## > **HFT relevance**
> - **Lock-free is reserved for the actual hot path** — the feed→decode→strategy→
>   gateway pipeline, where SPSC/MPSC rings and seqlocks earn their keep (`06`,
>   `12`). Everything else — order state maps, config, risk checks off the tick
>   path, logging — is a mutex or a sharded mutex.
> - **Shard before you hand-roll.** K per-symbol / per-strategy SPSC rings beat one
>   contended MPMC queue in both latency and simplicity.
> - **Prefer vetted libraries** for anything non-trivial — a hand-rolled
>   hazard-pointer MS queue is a multi-week correctness project; `folly` /
>   `liburcu` / `TBB` already did it.
> - **The mutex baseline stays in the codebase** — as a correctness oracle in
>   tests and a fallback if the lock-free path ever shows a bug in production.
> - **"We made it lock-free" is not a win report.** "We measured a 3× p99
>   improvement on the gateway hand-off, model-checked the protocol, and it passes
>   on Graviton" is.

---

## Hands-on

```bash
./build.ps1 fast 28-LOCK-FREE/examples/04_lock_free_stack.cpp    # lock-free LOSES here
./build.ps1 fast 28-LOCK-FREE/examples/06_mutex_vs_lockfree.cpp  # lock-free WINS here (SPSC)
```

Run both. `04` and `06` differ only in **contention shape** — one contended head
(stack) vs a clean producer/consumer split (SPSC). That shape difference decides
whether lock-free is worth it. Then:
- Take `04` and shard it: one stack per thread, no cross-thread contention. Does
  it now beat the mutex? (Yes — you removed the contended point.)

---

## ⚠️ Traps

### Trap 1 — "lock-free because it sounds fast"
It's a progress guarantee. Benchmark vs a mutex; on a contended single point the
mutex often wins (`04`).

### Trap 2 — hand-rolling instead of using a library
folly/TBB/liburcu spent years on the corner cases. Your first version has bugs you
won't find for months.

### Trap 3 — lock-free without an ARM/model-check gate
Memory orders tuned to x86 are a latent bug. If you can't verify them, don't ship
hand-rolled lock-free.

### Trap 4 — ignoring sharding
`K` low-contention mutexed shards beats one clever lock-free structure for most
N→M problems, with a fraction of the risk.

### Trap 5 — lock-free for a multi-step transaction
"Atomically move between two containers" — a mutex does it in 3 lines; lock-free
is a paper.

### Trap 6 — deleting the mutex baseline once the lock-free version "works"
Keep it — as a test oracle and a production fallback.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "lock-free is the fast option" | It's a progress guarantee; can be slower under contention |
| "if I removed the mutex it must be faster" | Measure — a CAS retry storm can beat a mutex to *lose* |
| "hand-rolled is fine, it's a small structure" | Small structures have the subtlest ABA/reclamation bugs |
| "sharding is a hack" | Sharding is usually the *better* answer than a clever lock-free structure |
| "one contended lock-free queue scales" | A single hot point rarely does — shard it |
| "we made it lock-free" is the deliverable | The measured p99 improvement + verification is |

---

## Exercises

1. **Mutex or lock-free:** (a) a config map read on every request, written once a
   minute; (b) a per-request metrics counter; (c) the feed→strategy hand-off;
   (d) "move an order from `pending` to `live`".

   <details><summary>Answer</summary>

   (a) `atomic<shared_ptr>` / RCU pointer swap, or even a `shared_mutex` — writes
   are rare. (b) `fetch_add(relaxed)` or per-thread + combine — not a "structure".
   (c) lock-free SPSC ring — the one clear lock-free win. (d) mutex — it's a
   two-container transaction; lock-free is not worth it.
   </details>

2. **Why `04` lost and `06` won:** name the single structural difference.

   <details><summary>Answer</summary>

   Contention shape. `04` is a LIFO with one head that 4 threads all CAS →
   retry storm. `06` is SPSC — one producer, one consumer, disjoint indices, no
   CAS, no contention. Same "lock-free" label, opposite outcome.
   </details>

3. **Shard a hot map:** 12 threads, a shared `unordered_map`, heavy contention on
   one mutex. Design a fix that doesn't use a single lock-free structure.

   <details><summary>Answer</summary>

   Partition into `K` shards (say 64), `shard = hash(key) % K`, each shard a
   `std::mutex` + its own `unordered_map`. Each operation locks only its shard →
   contention drops ~K-fold, and each shard is trivially correct. This is what
   `folly::ConcurrentHashMap`-style designs and Java's old `ConcurrentHashMap`
   did.
   </details>

4. **Library vs hand-roll:** you need a lock-free MPMC queue with reclamation.
   Argue for using `folly::MPMCQueue` / `moodycamel::ConcurrentQueue` over writing
   your own.

   <details><summary>Answer</summary>

   A correct MPMC queue with reclamation is a multi-week project with ABA,
   hazard-pointer/epoch, and weak-memory corner cases that only surface under load
   or on ARM. These libraries have years of production hardening, fuzzing, and
   model checking. Your version's first bug will cost more than the integration
   effort saved. Hand-roll only the pieces genuinely specific to your system (a
   fixed-layout SPSC ring for a known record type is reasonable; a general MPMC
   queue is not).
   </details>

5. **Keep the baseline:** give two concrete uses for keeping the `std::mutex`
   implementation after the lock-free one ships.

   <details><summary>Answer</summary>

   (1) Test oracle — run both against the same operation stream and assert
   identical results (differential testing catches lock-free bugs). (2) Production
   fallback — a build flag / config to switch back to the mutex version instantly
   if the lock-free path shows corruption or a livelock in the field, while you
   debug.
   </details>

---

## Interview questions

1. Lock-free ke honest costs — 4+ likho.
2. `std::mutex` kab right choice hai — 3 concrete cases.
3. `examples/04` mein lock-free kyun haara — structural reason?
4. Sharding — kaise ek contention problem ko K low-contention problems banata?
5. Hand-roll vs vetted library — kab library?
6. Lock-free ship karne ke pehle decision checklist ka ek pass batao.
7. Mutex baseline ko delete kyun nahi karte lock-free ship karne ke baad?

---

## Next
→ [`15-exercises.md`](15-exercises.md)
