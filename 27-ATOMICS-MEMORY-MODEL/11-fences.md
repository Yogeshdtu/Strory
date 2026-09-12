# 11 — Fences (`std::atomic_thread_fence`)

## Prerequisites
- `08-acquire-release.md`, `09-seq-cst.md`, `10-happens-before.md`

## Yeh topic abhi kyun
Ab tak ordering **operation ke saath attached** thi (`load(acquire)`,
`store(release)`). Ek **fence** standalone barrier hai — ordering ko kisi
particular atomic op se decouple karta hai. Yeh do kaam ke liye: (1) ek relaxed
atomic ke ordering ko alag jagah rakhna, (2) ek hi fence se kai relaxed ops ko
cover karna. Aur ek **compiler-only** fence bhi hai jo aksar log galti se atomic
fence samajhte hain.

---

## Do tarah ke "fence"

### 1. `std::atomic_signal_fence(order)` — compiler-only
- Sirf **compiler** ko reorder karne se rokta hai (across the fence).
- **Koi CPU instruction nahi** emit hota.
- Use: ek signal handler aur main code ke beech, ya lock-free code jahan aap ko
  pata hai hardware already ordered hai (single core, or same-core signal).
- HFT mein rare — mostly signal-safety.

### 2. `std::atomic_thread_fence(order)` — real hardware fence
- Compiler **aur** CPU dono ko rokta hai.
- x86: `atomic_thread_fence(seq_cst)` → `mfence` (or `lock`ed op). `acquire` /
  `release` fences → usually **no instruction** on x86 (TSO), just a compiler
  barrier. ARM: real `dmb` variants.
- Yeh woh fence hai jo memory model mein happens-before edges banata hai.

---

## Fence semantics (the two useful shapes)

### Release fence
```cpp
// writes ...
std::atomic_thread_fence(std::memory_order_release);   // (F)
flag.store(1, std::memory_order_relaxed);              // (X) — plain relaxed store
```
Rule: a **release fence (F)** + a later **relaxed store (X)** in the same thread
behaves like a release store at X, *for synchronization purposes* — if some thread
does a relaxed load that reads X's value and then an acquire fence, an edge forms.
More precisely: F synchronizes-with an acquire operation A if there's an atomic
store X (any order) sequenced-after F, and an atomic load sequenced-before A that
reads X (or its release sequence).

### Acquire fence
```cpp
if (flag.load(std::memory_order_relaxed) == 1) {       // (Y) — plain relaxed load
    std::atomic_thread_fence(std::memory_order_acquire);// (G)
    // reads ...  — now ordered after whatever was released before the matching store
}
```
Rule: a **relaxed load (Y)** that reads a released value, followed by an
**acquire fence (G)**, gives the same visibility as a `load(acquire)` at Y.

### Combined (the canonical pattern)
```cpp
// Producer
data = build();
std::atomic_thread_fence(std::memory_order_release);
ready.store(1, std::memory_order_relaxed);

// Consumer
while (ready.load(std::memory_order_relaxed) == 0) {}
std::atomic_thread_fence(std::memory_order_acquire);
use(data);   // sees build()'s writes
```
Same guarantee as `store(release)` / `load(acquire)` on `ready` — the fence just
moves the barrier off the atomic op.

---

## Why bother? (vs just `load(acquire)` / `store(release)`)

1. **One fence, many relaxed ops.** If a thread does 10 relaxed stores then wants
   to publish all of them, one `release` fence before the publishing relaxed
   store covers the batch — instead of thinking about each. (An op-attached
   `release` store already does this for everything sequenced-before it, so the
   real win is on the **load** side.)

2. **Acquire fence after a batch of relaxed loads** — load a bunch of slots
   `relaxed` in a tight loop (no per-load barrier — though on x86 there's no
   barrier anyway), then one acquire fence before you *use* the data. Can help the
   compiler schedule the loads freely.

3. **Conditional acquire.** You only need the acquire barrier on the path where
   the flag was set:
   ```cpp
   if (ready.load(relaxed)) { atomic_thread_fence(acquire); use(data); }
   ```
   The fast "not ready" path pays nothing (on x86 a `load(acquire)` is also free,
   so this matters more on ARM).

4. **`seq_cst` fence for store-load ordering** without making every op `seq_cst`
   (file 09) — keep flags `relaxed`, drop one `atomic_thread_fence(seq_cst)`
   between each thread's store and load to kill store-buffering.

On **x86 all of this is mostly compiler-scheduling nuance** — `acquire`/`release`
fences emit nothing. The payoff is real on ARM/POWER and for guiding the
optimizer. **Prefer op-attached orders unless you have a specific reason.**

---

## `std::atomic_thread_fence` on x86 — what actually emits

| Fence | x86-64 |
|---|---|
| `atomic_thread_fence(relaxed)` | nothing (not even a compiler barrier — basically a no-op) |
| `atomic_thread_fence(acquire)` | compiler barrier only, **no instruction** |
| `atomic_thread_fence(release)` | compiler barrier only, **no instruction** |
| `atomic_thread_fence(acq_rel)` | compiler barrier only, **no instruction** |
| `atomic_thread_fence(seq_cst)` | **`mfence`** (or `lock or [rsp], 0`) |

So on x86 only the `seq_cst` fence costs a cycle. On ARMv8: `acquire`→`dmb ishld`,
`release`→`dmb ish`, `seq_cst`→`dmb ish`.

---

## > **HFT relevance**
> - **Mostly you use op-attached `acquire`/`release`** — clearer, and on x86
>   identical cost. Reach for a fence only when profiling/asm shows the compiler
>   is over-constrained, or on ARM where a conditional acquire fence saves a `dmb`
>   on the fast path.
> - **`seq_cst` fence** is the tool to keep a hand-rolled mutual-exclusion
>   protocol's flags `relaxed` while still getting store-load ordering — one
>   `mfence` instead of a `seq_cst` store's `mfence` on every write.
> - **`atomic_signal_fence`** — only for signal-handler / same-core scenarios;
>   **never** as a substitute for a real fence between two cores (it emits no
>   instruction — the store buffer will still bite you).
> - **Seqlock reader** (folder 28) often uses `atomic_thread_fence(acquire)`
>   between reading the sequence and reading the payload, with the payload loads
>   `relaxed` — a textbook fence use.

---

## Hands-on

```bash
# no dedicated example — inspect codegen:
printf '#include <atomic>\nvoid f(){ std::atomic_thread_fence(std::memory_order_seq_cst); }\n' > /tmp/fence.cpp
g++ -std=c++20 -O2 -S -o- /tmp/fence.cpp | c++filt
# -> mfence

printf '#include <atomic>\nvoid g(){ std::atomic_thread_fence(std::memory_order_acquire); }\n' > /tmp/facq.cpp
g++ -std=c++20 -O2 -S -o- /tmp/facq.cpp | c++filt
# -> nothing (empty function body)
```
Then rewrite `examples/04` Demo 1 with `relaxed` stores/loads + release/acquire
**fences** and confirm it still reports 0 mismatches — same guarantee, barrier
relocated.

---

## ⚠️ Traps

### Trap 1 — `atomic_signal_fence` used between two threads/cores
It's compiler-only, **no instruction**. The store buffer still reorders across
cores. Use `atomic_thread_fence` (or op-attached orders).

### Trap 2 — a lone fence with no atomic op
A release fence needs a following atomic store (and a reader with a load + acquire
fence) to actually form an edge. A fence in isolation orders nothing across
threads.

### Trap 3 — `atomic_thread_fence(relaxed)`
Does nothing. Not even a compiler barrier. Pointless.

### Trap 4 — assuming an `acquire`/`release` fence costs an instruction on x86
It doesn't — compiler barrier only. Don't "optimize it away" thinking it's a
`dmb`; and don't assume it fixed a hardware reorder that only `mfence` fixes.

### Trap 5 — fence on the wrong side of the atomic op
Release fence goes **before** the publishing store; acquire fence goes **after**
the subscribing load. Reversed → no edge.

### Trap 6 — replacing a `seq_cst` op with an `acquire`/`release` fence + relaxed
That gives acq/rel semantics, **not** `seq_cst` — store-load / IRIW still broken.
You need a `seq_cst` fence for that.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`atomic_signal_fence` is a lightweight thread fence" | Compiler-only, no instruction — useless across cores |
| "a fence by itself creates happens-before" | Needs a paired atomic store/load to form the edge |
| "acquire/release fences emit `mfence` on x86" | No instruction on x86 — compiler barrier only; only `seq_cst` fence → `mfence` |
| "fence is always better than op-attached order" | Usually the same on x86 and less clear; use op-attached by default |
| "release fence goes after the store" | Before the store (acquire fence after the load) |
| "seq_cst fence == acq/rel fence" | seq_cst fence also gives store-load / participates in the total order |

---

## Exercises

1. **Which fence:** (a) order a signal handler's writes vs main on one core;
   (b) publish data with a relaxed flag across two cores; (c) kill store-buffering
   while keeping flags relaxed.

   <details><summary>Answer</summary>

   (a) `atomic_signal_fence` (compiler-only is enough — same core). (b)
   `atomic_thread_fence(release)` before the relaxed store + `atomic_thread_fence(acquire)`
   after the relaxed load. (c) `atomic_thread_fence(seq_cst)` between each
   thread's store and load.
   </details>

2. **x86 codegen:** what instruction does each emit — `atomic_thread_fence`
   with `acquire`, `release`, `seq_cst`?

   <details><summary>Answer</summary>

   `acquire` → none (compiler barrier). `release` → none (compiler barrier).
   `seq_cst` → `mfence` (or a `lock`ed dummy). Only `seq_cst` costs a cycle on
   x86.
   </details>

3. **Broken:** `atomic_thread_fence(release); flag.store(1, relaxed);` in the
   producer, but the consumer does `flag.load(acquire); use(data);` (op-attached
   acquire, no fence). Edge?

   <details><summary>Answer</summary>

   Yes — this is fine. A release fence + relaxed store pairs with an op-attached
   `load(acquire)` just as well as with a relaxed-load + acquire-fence. The fence
   forms are interoperable with the op-attached forms.
   </details>

4. **Isolated fence:** does `void f() { std::atomic_thread_fence(std::memory_order_seq_cst); }`
   called by one thread, with no atomics anywhere, order anything cross-thread?

   <details><summary>Answer</summary>

   No useful cross-thread ordering — with no atomic stores/loads to pair with,
   there's nothing to synchronize-with. It does drain this core's store buffer
   (`mfence`), but without a paired atomic op no happens-before edge forms.
   </details>

5. **ARM motivation:** why is a conditional acquire fence (`if (flag.load(relaxed))
   { fence(acquire); ... }`) more valuable on ARM than on x86?

   <details><summary>Answer</summary>

   On x86 `load(acquire)` is a plain `mov` — free — so guarding it buys nothing.
   On ARM `load(acquire)` (`ldar`) or an acquire fence (`dmb ishld`) is a real
   barrier; skipping it on the common "flag not set" path avoids that cost every
   spin iteration.
   </details>

---

## Interview questions

1. `atomic_signal_fence` vs `atomic_thread_fence` — kya farq (compiler vs CPU)?
2. Release fence + relaxed store — kaise ek release store jaisa kaam karta hai?
3. x86 pe kaunse fences instruction emit karte hain (sirf seq_cst)?
4. Fence op-attached order ke over kab prefer karein (ARM, batch loads, conditional)?
5. Ek akela fence bina kisi atomic op ke — cross-thread order banata hai? (Nahi.)
6. `seq_cst` fence acq/rel fence se kya extra deta hai?
7. Seqlock reader mein acquire fence kahan aur kyun?

---

## Next
→ [`12-hardware-memory-models.md`](12-hardware-memory-models.md)
