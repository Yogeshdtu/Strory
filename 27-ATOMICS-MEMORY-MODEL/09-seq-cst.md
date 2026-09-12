# 09 — `memory_order_seq_cst`

## Prerequisites
- `08-acquire-release.md`
- [`examples/05_reordering_demo.cpp`](examples/05_reordering_demo.cpp),
  [`examples/06_memory_order_bench.cpp`](examples/06_memory_order_bench.cpp)

## Yeh topic abhi kyun
`seq_cst` sabse strong order hai aur **har atomic op ka default**. Yeh acquire +
release deta hai **plus** ek extra: saare `seq_cst` operations ka **ek single
global total order** jispe har thread agree karta hai. Yeh extra guarantee hi woh
cheez hai jo store-buffering / Dekker jaise problems solve karti hai — aur uski
ek measurable cost hai.

---

## Kya `seq_cst` deta hai

1. **Acquire semantics** on every `seq_cst` load / the load half of an RMW.
2. **Release semantics** on every `seq_cst` store / the store half of an RMW.
3. **A single total order `S`** over *all* `seq_cst` operations in the whole
   program, consistent with:
   - each thread's program order (sequenced-before), and
   - the synchronizes-with edges,
   - and every `seq_cst` load sees the last `seq_cst` store to that variable in
     `S` (or a non-seq_cst store not ordered after it).

Point (3) is what acquire/release **lacks**. With acq/rel, two threads can
disagree about the relative order of two independent stores. With `seq_cst`, there
is one order and everybody sees it.

```cpp
// Store buffering — all seq_cst
// T1: x.store(1);  r1 = y.load();
// T2: y.store(1);  r2 = x.load();
// r1 == 0 && r2 == 0  is IMPOSSIBLE — in the total order S, one store is first,
//                                     and the later thread's load must see it.
```
`examples/05`: `seq_cst` → **0 / 200000** double-zero outcomes, every run (relaxed
~70–330, rel/acq ~1000–4200 — counts vary, but seq_cst is always exactly 0).

---

## How x86 implements it

| Operation | `relaxed`/`acq`/`rel` | `seq_cst` |
|---|---|---|
| load | `mov` | `mov` (same — x86 loads are already acquire) |
| store | `mov` | **`xchg`** (implicitly `lock`ed) **or `mov; mfence`** |
| RMW (`fetch_add`, CAS) | `lock xadd` / `lock cmpxchg` | same (already full barrier) |

So on x86 the **only** thing `seq_cst` adds over `release` is on the **store**:
the `xchg`/`mfence` drains the store buffer, preventing store-load reordering.
Loads and RMWs are already "free" `seq_cst` on x86.

On ARM: `seq_cst` load = `ldar`, `seq_cst` store = `stlr`, and pre-ARMv8.3 a full
`dmb ish` is often needed — costly on **both** sides.

---

## Measured cost (`examples/06`, this x86 box, N = 100M, `-O2`)

| Op | relaxed | acquire/release | seq_cst |
|---|---|---|---|
| store | ~0.72 ns | ~0.72 ns | **~12.9 ns** |
| load | ~0.74 ns | ~0.74 ns | ~0.74 ns |
| `fetch_add` (uncontended) | ~13.1 ns | ~12.9 ns (`acq_rel`) | ~13.0 ns |
| `fetch_add` (contended, 8 thr, 1 atomic) | ~26.6 ns/op | — | ~27.1 ns/op |

(Absolute ns drift with machine state — an earlier run on this box measured the
`seq_cst` store at ~6.3 ns and the RMW at ~6 ns. The **ratios and the shape** are
what's stable and what matters.)

Takeaways:
- **`seq_cst` store ≈ 18× a relaxed/release store** — the `mfence`/`xchg`.
- **`seq_cst` load is free on x86** — identical to relaxed (~0.74 ns).
- **RMWs don't care** about the order on x86 — `lock`-prefixed ops are already a
  full barrier; relaxed vs seq_cst `fetch_add` is within noise (~13 ns).
- Contended `fetch_add`: the cache-line ping-pong (~27 ns) dwarfs any
  order difference — again, order barely matters for a contended RMW here.

(CLAUDE.md Rule 2: the "textbook" figure for a contended atomic is often quoted as
40–100+ ns; on this machine it's ~27 ns/op. Measure your box.)

---

## When you actually need `seq_cst`

- **Store-buffering / Dekker / Peterson** mutual exclusion done by hand — a
  thread stores its flag then loads the other's; needs store-load ordering.
- **IRIW** (independent reads of independent writes) — two reader threads must
  agree on the order two writer threads' independent stores happened (file 16).
- **A "sequentially consistent" mental model** — when you want to reason about the
  program as a plain interleaving of thread steps and not track edges. It's the
  **safe default**; keep it until profiling says a specific atomic is hot.

If your pattern is publish→subscribe, producer→consumer, refcount, lock-free
queue/stack — **acquire/release is enough** and cheaper.

---

## `seq_cst` fence vs `seq_cst` operations

`std::atomic_thread_fence(std::memory_order_seq_cst)` is a standalone full barrier
(file 11). A `seq_cst` fence between a relaxed store and a relaxed load recovers
store-load ordering **for that thread** without making every op `seq_cst`. Used to
optimize: keep ops `relaxed`, drop one fence where the global order actually
matters. Subtle — the fence's participation in `S` has its own rules; prefer
`seq_cst` *operations* unless profiling forces the fence.

---

## > **HFT relevance**
> - **Default to `seq_cst`; downgrade the measured-hot atomics.** Correctness
>   first. A `seq_cst` store's `mfence` is ~13 ns here — invisible off the hot
>   path, material *on* it (per-message queue index → use `release`).
> - **`seq_cst` loads are free on x86** — never contort code to avoid a `seq_cst`
>   *read*. The cost is entirely on the store side.
> - **Hand-rolled mutual exclusion (rare)** — a `seq_cst` fence or `seq_cst` flags
>   are mandatory; `release`/`acquire` flags silently allow both threads into the
>   critical section (store-buffering).
> - **Cross-arch**: on ARM `seq_cst` is expensive on both sides — an HFT codebase
>   targeting ARM audits every `seq_cst` and replaces it with acq/rel where the
>   pattern allows.
> - **`fetch_add` order is nearly free** on x86 (already `lock`ed) — a `relaxed`
>   vs `seq_cst` counter increment is within noise; pick `relaxed` for clarity of
>   intent, not for measurable speed.

---

## Hands-on

```bash
./build.ps1 fast 27-ATOMICS-MEMORY-MODEL/examples/05_reordering_demo.cpp
./build.ps1 fast 27-ATOMICS-MEMORY-MODEL/examples/06_memory_order_bench.cpp
```

Then:
- `g++ -O2 -S` for `x.store(1)` (default seq_cst) vs `x.store(1,
  std::memory_order_release)` — see `xchg`/`mfence` vs plain `mov`.
- In `05`, make **only the stores** `seq_cst` and leave loads `relaxed` — still 0
  double-zeros (the store-side barrier is what matters here).
- In `06`, add a `seq_cst` fence variant of the relaxed store loop — cost should
  land near the `seq_cst` store.

---

## ⚠️ Traps

### Trap 1 — "seq_cst everywhere = free safety"
The `seq_cst` **store** costs ~18× a release store on x86 (`mfence`). Fine off the
hot path; measure on it.

### Trap 2 — avoiding `seq_cst` *loads* for speed
On x86 a `seq_cst` load is identical to a relaxed load. No reason to avoid it.

### Trap 3 — "acquire/release gives a global order"
It doesn't. Only `seq_cst` ops share one total order all threads agree on. Dekker
/ IRIW need `seq_cst`.

### Trap 4 — mixing `seq_cst` and weaker ops on the same variable and expecting
### the total order to still cover everything
The total order `S` covers only the `seq_cst` ops. A `relaxed` store to the same
variable isn't in `S`; reasoning gets subtle. Keep a synchronization variable's
ops one consistent order.

### Trap 5 — `seq_cst` fence assumed identical to a `seq_cst` op
Related but distinct rules for how the fence joins `S`. Use `seq_cst` *operations*
unless you've measured a need for the fence.

### Trap 6 — assuming `seq_cst` fixes a data race on *non-atomic* data
It orders the atomics; the non-atomic data still needs to be published *through*
an atomic (release/acquire or seq_cst) — a `seq_cst` op on `x` doesn't make a
plain `y` race-free.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "seq_cst = acquire + release" | That **plus** one global total order over all seq_cst ops |
| "seq_cst is always too slow" | Only the *store* is costly on x86 (~13 ns); loads ~free, RMW order ~free — it's the right default |
| "release/acquire also gives a global order" | No — pairwise only; store-buffering/IRIW need seq_cst |
| "seq_cst load needs a fence on x86" | No — plain `mov`; x86 loads are already acquire and seq_cst-compatible |
| "downgrade all seq_cst to relaxed for HFT" | Only where you can prove a weaker order suffices; a wrong downgrade is UB that passes on x86 |
| "seq_cst makes my non-atomic struct safe" | It orders atomics; publish the struct through an atomic edge |

---

## Exercises

1. **Impossible outcome:** all-`seq_cst` store buffering. Why is `r1==0 && r2==0`
   impossible, in terms of the total order `S`?

   <details><summary>Answer</summary>

   In `S`, `x.store(1)` and `y.store(1)` have some order — say `x.store` first.
   T2's `y.store` is after it in T2's program order, and T2's `x.load` is after
   `y.store`, so `x.load` is after `x.store(1)` in `S` and must read `1`. So
   `r2 == 1`. Symmetric if `y.store` is first. Both-zero can't occur.
   </details>

2. **x86 cost:** which of `seq_cst` load / store / `fetch_add` costs more than its
   `relaxed` form on x86, and why?

   <details><summary>Answer</summary>

   Only the **store** — `xchg`/`mfence` to drain the store buffer (store-load
   ordering). Loads are already acquire on x86; RMWs already carry a full `lock`
   barrier. Measured: store ~12.9 ns vs ~0.72 ns; load & `fetch_add` unchanged.
   </details>

3. **Right tool:** for each, `seq_cst` or `acquire/release`? (a) SPSC queue index;
   (b) hand-rolled Peterson lock; (c) publish a config pointer; (d) two flags for
   a barrier where both threads set-then-check.

   <details><summary>Answer</summary>

   (a) acq/rel. (b) seq_cst (store-load). (c) acq/rel. (d) seq_cst — set-then-check
   on two variables is store-buffering-shaped.
   </details>

4. **Downgrade safe?** a `seq_cst` counter `hits.fetch_add(1)` read only at
   shutdown → change to `relaxed`?

   <details><summary>Answer</summary>

   Safe — a standalone counter needs only atomicity; no ordering w.r.t. other
   memory is required. On x86 it won't even be measurably faster (RMW order is
   ~free), but `relaxed` documents the intent.
   </details>

5. **Fence trick:** you keep two flags `relaxed` but put
   `atomic_thread_fence(seq_cst)` between each thread's store and load. What does
   that recover, and what's the risk?

   <details><summary>Answer</summary>

   It recovers store-load ordering for those threads (drains the store buffer at
   the fence), so the store-buffering both-zero outcome is prevented — without
   making every access `seq_cst`. Risk: the fence's interaction with the global
   `seq_cst` order has subtle rules; easy to get wrong. Prefer `seq_cst`
   operations unless profiling demands the fence.
   </details>

---

## Interview questions

1. `seq_cst` acquire/release ke upar kya **extra** deta hai (total order `S`)?
2. Store-buffering `seq_cst` pe impossible kyun — `S` ke terms mein.
3. x86 pe `seq_cst` ka machine cost — load / store / RMW alag-alag.
4. `seq_cst` default hai — accept karein ya har jagah downgrade karein? Kaunsa rule?
5. IRIW / Dekker ke liye `acquire/release` kyun kaafi nahi?
6. `seq_cst` fence vs `seq_cst` operation — kab, kya farq?
7. `seq_cst` op non-atomic data ko race-free banata hai? (Nahi — kyun.)

---

## Next
→ [`10-happens-before.md`](10-happens-before.md)
