# 17 — Exercises: atomics & the memory model

## Prerequisites
- Poora folder 27 (files 01–16)

## Yeh file kya hai
Practice — outcome/behaviour prediction, "find the bug", memory-order reasoning,
design, aur ek build challenge. Answers `<details>` mein. Compile:
`./build.ps1 file.cpp`. Benchmarks: `./build.ps1 fast file.cpp`. Reordering demos
`-O2` pe hi meaningful (`./build.ps1 fast ...`). Races (Linux): `-fsanitize=thread`.

---

## Part A — Outcome / behaviour prediction

### A1
```cpp
std::atomic<int> x{0}, y{0};
int r1, r2;
// Thread 1                       // Thread 2
x.store(1, std::memory_order_relaxed);   y.store(1, std::memory_order_relaxed);
r1 = y.load(std::memory_order_relaxed);  r2 = x.load(std::memory_order_relaxed);
```
Can `r1 == 0 && r2 == 0`? On x86? Which memory order forbids it?

<details><summary>Answer</summary>

Yes — this is **store buffering (SB)**. Allowed with `relaxed` *and* observed on
x86 (`examples/05`: ~70–330/200000, varies per run). Also allowed with
`release`/`acquire` (~1000–4200/200000 — *more*, not fewer; acq/rel doesn't stop
store→load). Only `seq_cst` on all four ops forbids it (0/200000, always).
</details>

### A2
```cpp
std::atomic<long> c{0};
// 8 threads, each: for (int k = 0; k < 2'000'000; ++k) c.fetch_add(1, relaxed);
// main after join:
std::printf("%ld\n", c.load());
```
<details><summary>Answer</summary>

Exactly `16000000`, every run. `fetch_add` is a full atomic RMW — no lost
updates. `relaxed` drops *ordering*, not atomicity. (`examples/03` demo 1.)
</details>

### A3
```cpp
int data = 0;
std::atomic<bool> ready{false};
// Producer:  data = 42;  ready.store(true, std::memory_order_release);
// Consumer:  while (!ready.load(std::memory_order_acquire)) {}  int r = data;
```
What is `r`? What if both are `relaxed`?

<details><summary>Answer</summary>

With `release`/`acquire`: `r == 42`, guaranteed — the release store
synchronizes-with the acquire load that reads `true`, so `data = 42`
happens-before `r = data`. With both `relaxed`: no synchronizes-with edge → `r`
can be `0`, and the read of `data` is a **data race → UB** (the whole program).
</details>

### A4
```cpp
std::atomic<int> a{7};
int expected = 5;
bool ok = a.compare_exchange_strong(expected, 100);
std::printf("ok=%d a=%d expected=%d\n", ok, a.load(), expected);
```
<details><summary>Answer</summary>

`ok=0 a=7 expected=7`. `a` was 7 ≠ 5 → CAS fails, `a` unchanged, and **`expected`
is overwritten with the current value (7)**. A follow-up `compare_exchange_strong(expected, 100)`
would now succeed.
</details>

### A5
```cpp
// x86-64, -O2
std::atomic<int> f{0};
f.store(1);                                  // (A) default order
f.store(1, std::memory_order_release);       // (B)
```
Which line emits a barrier instruction, and which doesn't?

<details><summary>Answer</summary>

(A) is `seq_cst` (the default) → `xchg`/`mov;mfence` — drains the store buffer.
(B) `release` on x86 → plain `mov`, no barrier (TSO gives store-release for free;
it only constrains the compiler). Measured cost gap ~18× (`examples/06`: ~12.9 ns
vs ~0.72 ns).
</details>

### A6
```cpp
std::atomic<uint32_t> head{A};   // A is a free-list node index
// T1: reads head==A, computes next = node[A].next == B, then is descheduled
// T2: pops A, pops B, pushes A back (head==A again)
// T1: resumes, does head.compare_exchange_strong(A, B)
```
<details><summary>Answer</summary>

The CAS **succeeds** (`head` is `A` again) and sets `head = B` — but `B` was
already popped and possibly reused. **ABA**: structure corrupted (lost/dangling
node). Fix: pack a version tag with the index (`examples/07` — `{idx:32, tag:32}`
in a `uint64_t`, tag bumped every push → stale CAS fails).
</details>

### A7
```cpp
std::atomic_thread_fence(std::memory_order_acquire);   // (P)
std::atomic_thread_fence(std::memory_order_seq_cst);   // (Q)
```
On x86-64 `-O2`, what instruction does each emit?

<details><summary>Answer</summary>

(P) — nothing (a compiler-scheduling barrier only). (Q) — `mfence` (or a `lock`ed
dummy). On x86 only the `seq_cst` fence costs a cycle; `acquire`/`release`/`acq_rel`
fences are compiler barriers with no instruction.
</details>

---

## Part B — Find the bug

### B1
```cpp
std::atomic<bool> done{false};
long result = 0;
// worker:  result = heavy_compute();  done.store(true, std::memory_order_relaxed);
// main:    while (!done.load(std::memory_order_relaxed)) {}  print(result);
```
<details><summary>Answer</summary>

`relaxed` on both ends → no synchronizes-with edge → `main` can see `done == true`
while `result` is still `0` (or torn). And `result` is a plain non-atomic read
racing the write → **UB**. Fix: `done.store(true, release)` /
`done.load(acquire)`. (x86 will often mask this — it's still wrong.)
</details>

### B2
```cpp
std::atomic<int> refcount{1};
void release_ref() {
    if (refcount.fetch_sub(1, std::memory_order_relaxed) == 1)
        delete resource;
}
```
<details><summary>Answer</summary>

The final decrementer `delete`s the resource without an **acquire** on the other
threads' prior `fetch_sub`s → their last uses of the resource aren't ordered
before the `delete` → use-after-free. Fix: `fetch_sub(1, std::memory_order_acq_rel)`
(or `release` on the decrement + an `acquire` fence before the `delete`).
</details>

### B3
```cpp
// Dekker-style entry:
flag[me].store(true, std::memory_order_release);
if (!flag[other].load(std::memory_order_acquire)) {
    // enter critical section
}
```
<details><summary>Answer</summary>

Store buffering — `flag[me].store` can sit in the store buffer while
`flag[other].load` executes, so **both** threads see the other's flag as `false`
and **both enter** the critical section. `release`/`acquire` does not stop
store→load. Fix: `seq_cst` on the store and the load (or an
`atomic_thread_fence(seq_cst)` between them). (`examples/05` shows rel/acq still
~1000–4200/200000 both-zero; seq_cst 0.)
</details>

### B4
```cpp
std::atomic<uint32_t> head_tagged;  // {index:16, tag:16} packed
// push bumps tag by 1 each time
```
Pool has 4096 slots; ~50M push/pop per second; a thread can stall up to 2 ms.
<details><summary>Answer</summary>

The **16-bit tag** (65,536 values) can wrap within a 2 ms stall (~100,000
modifications at 50M/s) → a stale CAS can see the tag return to its old value →
ABA reappears. Widen: 32-bit index + 32-bit tag in a `uint64_t` (still lock-free,
tag needs ~86 s to wrap).
</details>

### B5
```cpp
struct __attribute__((packed)) Slot { uint8_t kind; uint64_t seq; char payload[48]; };
Slot ring[1024];
// publisher:
std::atomic_ref<uint64_t>{ring[i].seq}.store(n, std::memory_order_release);
```
<details><summary>Answer</summary>

`ring[i].seq` is at offset 1 (packed) → misaligned. `std::atomic_ref<uint64_t>::required_alignment`
is 8; constructing the ref over a misaligned object is **UB**, and on x86 a
split-lock `mfence`/`lock` across a cache line is a huge penalty. Fix: don't pack,
or lay `seq` out 8-byte aligned (put it first, or add explicit padding).
</details>

### B6
```cpp
std::atomic<Node*> head;
// push:
Node* n = new Node{v};
n->next = head.load(std::memory_order_relaxed);
while (!head.compare_exchange_weak(n->next, n, std::memory_order_relaxed)) {}
// pop elsewhere reads head->next and CASes
```
<details><summary>Answer</summary>

Two bugs. (1) Success order `relaxed` on push → the consumer that `acquire`s
`head` and reads `n->next` / `n->v` has no happens-before with the `new Node{v}`
writes → it can see a half-constructed node. Use `release` on the successful CAS.
(2) The pop side dereferences and (eventually) frees nodes → ABA + use-after-free
with no reclamation scheme. Needs tagged head + hazard pointers/epochs (folder
28).
</details>

---

## Part C — Memory-order reasoning

### C1
For each shared access, pick the **weakest correct** memory order: (a) a metrics
counter incremented by many threads, read at shutdown; (b) the write index of an
SPSC ring the producer bumps after filling a slot; (c) the read of that index by
the consumer; (d) two flags in a hand-rolled Peterson lock.

<details><summary>Answer</summary>

(a) `relaxed` — standalone count, no ordering needed. (b) `release` — publishes
the slot payload. (c) `acquire` — pairs with (b). (d) `seq_cst` — Peterson is
store-buffering-shaped; needs store→load ordering.
</details>

### C2
A builder thread constructs an immutable `RiskLimits` object, then publishes it:
`g_limits.store(p, ??)`. Readers: `auto* L = g_limits.load(??); check(*L);`.
Orders? And what still needs solving after that?

<details><summary>Answer</summary>

`release` on the store, `acquire` on the load — so a reader seeing the new pointer
also sees the fully-built object. Still to solve: **reclamation** of the old
`RiskLimits` — a reader may still hold the old pointer. Use `std::shared_ptr`
(atomic load/store), hazard pointers, or an epoch/RCU grace period (folder 28).
</details>

### C3
Explain, in happens-before terms, why this reads `v == 1` always:
```cpp
int v = 0; std::atomic<int> a{0}, b{0};
// T1: v = 1; a.store(1, release);
// T2: while (a.load(acquire) == 0) {}  b.store(1, release);
// T3: while (b.load(acquire) == 0) {}  r = v;
```
<details><summary>Answer</summary>

`v = 1` sequenced-before `a.store(release)`; that synchronizes-with T2's
`a.load(acquire)` reading 1; sequenced-before T2's `b.store(release)`; which
synchronizes-with T3's `b.load(acquire)` reading 1; sequenced-before `r = v`.
Happens-before is transitive across both edges → `v = 1` happens-before `r = v`
→ `r == 1`. (Release/acquire chains compose.)
</details>

### C4
On x86 you profile and find a hot `seq_cst` atomic. Which operation *kind* on it
is worth downgrading, and which kinds won't move the needle on x86 (but would on
ARM)?

<details><summary>Answer</summary>

Worth downgrading: a `seq_cst` **store** → `release` or `relaxed` (removes the
`mfence`/`xchg`, ~12.9 ns → ~0.72 ns here). Won't help on x86: `seq_cst` **loads**
(already a plain `mov`) and `seq_cst` **RMWs** (`lock`-prefixed = full barrier
regardless).
All three *do* cost extra on ARM (`ldar`/`stlr`/`dmb`), so annotate correctly for
portability anyway.
</details>

### C5
`compare_exchange_weak` vs `strong`: give a case where using `strong` in a loop is
the right call, and a case where `weak` outside a loop is a bug.

<details><summary>Answer</summary>

`strong` in a loop when recomputing the `desired` value after a failure is very
expensive (a syscall, a big allocation) — you don't want to redo it for a
*spurious* failure, so pay for strong's internal retry. `weak` outside a loop
(single "try once") is a bug on ARM: a spurious failure makes your one attempt
"fail" even though the value matched — you'd wrongly take the failure branch.
</details>

---

## Part D — Challenge

### D1 — Measure the seq_cst store barrier
Write a micro-benchmark (N = 100M, single thread, `-O2`) that times, per
operation:
1. `x.store(i, std::memory_order_relaxed)`
2. `x.store(i, std::memory_order_release)`
3. `x.store(i, std::memory_order_seq_cst)`
4. `x.load(std::memory_order_relaxed)` vs `x.load(std::memory_order_seq_cst)`
5. `x.fetch_add(1, relaxed)` vs `fetch_add(1, seq_cst)`

Predict the ranking first. Then `g++ -O2 -S` and match each to its instruction.

<details><summary>What you should find (this repo's x86 box — examples/06)</summary>

store: relaxed ~0.72 ns ≈ release ~0.72 ns ≪ seq_cst ~12.9 ns (the `mfence`/`xchg`).
load: relaxed ~0.74 ns ≈ seq_cst ~0.74 ns (both plain `mov` on x86).
fetch_add: ~13 ns regardless of order (`lock xadd` is already a full barrier);
~27 ns/op contended (8 threads, 1 atomic). (Absolute ns drift with machine state
— an earlier run measured ~6 ns for the seq_cst store and RMW; the *ratios* hold.)
So on x86 the *only* order that costs you is a `seq_cst` **store**. asm: relaxed
store `mov`; seq_cst store `xchg` (or `mov`+`mfence`); loads `mov`; `fetch_add`
`lock xadd`.
</details>

### D2 — Observe reordering, then stop it
Start from `examples/05_reordering_demo.cpp` (store buffering with persistent
threads + a `go`/`done` handshake). Without changing the round count:
1. Run it as-is: record the both-zero count for `relaxed`, `release`/`acquire`,
   `seq_cst`.
2. Add a 4th variant: keep the stores/loads `relaxed` but put
   `std::atomic_thread_fence(std::memory_order_seq_cst)` between each thread's
   store and its load. Record its both-zero count.
3. Run the whole thing at `-O0` as well.

<details><summary>What you should find</summary>

1. relaxed ~70–330/200000, rel/acq ~1000–4200/200000, seq_cst **0**/200000 (this
   box — counts swing run-to-run but the *pattern* holds: rel/acq does NOT stop SB
   — it shows *more* both-zeros than relaxed, and seq_cst is always exactly 0).
2. relaxed + `seq_cst` fence between store and load → **0** both-zeros — the fence
   drains the store buffer, same effect as `seq_cst` ops, without upgrading every
   access.
3. `-O0` still shows non-zero both-zeros for relaxed/rel-acq — proof it's the
   **CPU** store buffer, not the compiler. (Fewer than `-O2` because the loop is
   slower / less tight.)
</details>

### D3 — ABA, reproduced and fixed
From `examples/07_aba_problem.cpp`: the BROKEN path uses a forced `phase`
handshake so a stale `CAS(head, A, ...)` succeeds after `A` was popped and pushed
back. The FIXED path packs `{index:32, tag:32}` in a `uint64_t` and bumps the tag
on every push.

1. Run both; confirm BROKEN corrupts (head points at an already-popped node) and
   FIXED does not (tag mismatch → stale CAS fails).
2. Change the FIXED tag increment to `+0` (i.e. never bump). Does ABA come back?
3. Argue why an SPSC ring buffer with `std::atomic<uint64_t>` head/tail counters
   has **no** ABA, so it needs no tag.

<details><summary>What you should find</summary>

1. BROKEN: the forced interleaving makes `head` end up at a popped node — detected
   and printed. FIXED: across the same interleaving the tag goes 0→3, so the
   stale `CAS` compares `(A, 0)` vs `(A, 3)` and **fails** → structure stays
   consistent.
2. Yes — with the tag frozen the packed word is just the index again; the stale
   CAS matches and corrupts. The tag *must* change on every mutation that can
   recycle the value.
3. The head/tail counters only ever increase and won't wrap in any realistic
   runtime (2^64). A counter value is never reused, so there's no "value returns
   to A" — the slot array is reused, but access is gated by the monotonic
   counters, not by a CAS on a recycled value.
</details>

---

## Next
→ [`../28-LOCK-FREE/00-README.md`](../28-LOCK-FREE/00-README.md)
