# 06 — Why memory ordering exists

## Prerequisites
- `05-compare-exchange.md`, `03-std-atomic.md` (the `std::memory_order` enum)
- [`examples/05_reordering_demo.cpp`](examples/05_reordering_demo.cpp)

## Yeh topic abhi kyun
Ab tak: atomic = indivisible. Par ek atomic operation **doosri memory operations
ke relative kahan baithti hai** — yeh alag sawaal hai. Program ka source order
!= execution order. Do alag "reorderers" hain (compiler + CPU), aur `memory_order`
argument unhe control karta hai. Yeh folder ka asli dil hai.

---

## Source order execution order nahi hai

Aapne likha:
```cpp
data = 42;          // (1)
ready = true;        // (2)
```

Jo actually chalta hai, woh `(2)` pehle bhi ho sakta hai. **Do** cheezein isse
allow karti hain:

### 1. Compiler reordering
Optimizer independent-lagne wali statements ko reorder/merge/eliminate karta hai —
registers, scheduling, common-subexpression elimination. Single-thread mein result
same rehta hai ("as-if" rule), par ek **doosra thread** original order pe depend
kare toh toot jaata hai.

### 2. CPU reordering
Hardware bhi instructions ko out-of-order execute/retire karta hai aur — most
importantly — **store buffer** ke through stores ko delay karta hai:

```
Core writes X ──► [ store buffer ] ──► L1 cache ──► visible to other cores
                       ↑
              the write sits here for a while;
              this core sees it (store-forwarding),
              other cores don't yet
```

So Core 1 ka `X = 1` abhi Core 1 ki registers/loads ko dikhta hai, par Core 2 ko
kuch nanoseconds baad. Agar Core 1 ne `X = 1` phir `Y = 1` kiya, Core 2 `Y == 1`
dekhkar bhi `X == 0` dekh sakta (x86 pe **nahi** is exact case mein — store-store
order preserve hota — par store-load reorder x86 pe bhi hota, aur ARM pe toh sab).

---

## The demo: store buffering (`examples/05`)

```cpp
// x, y both atomic, start 0
// Thread 1:  x.store(1, MO);  r1 = y.load(MO);
// Thread 2:  y.store(1, MO);  r2 = x.load(MO);
```

Intuition: kam se kam ek thread doosre ka store dekh le, so `r1 == 0 && r2 == 0`
**kabhi nahi** hona chahiye. Reality (measured on this x86 box, 200000 rounds —
counts vary run-to-run, the *pattern* is stable):

| `MO` | `r1==0 && r2==0` count |
|---|---|
| `relaxed` | ~70–330 / 200000 |
| `release` / `acquire` | ~1000–4200 / 200000 |
| `seq_cst` | **0 / 200000** (always) |

- `relaxed` aur `release/acquire` dono mein `r1==r2==0` hota hai — **store-load
  reordering** (store buffer ne store ko load ke baad tak rok liya). x86 TSO isse
  allow karta hai. (`release/acquire` mein *zyada* dikhta hai, kam nahi — woh
  store-load ko rokta hi nahi.)
- `release/acquire` **isse nahi rokta** — woh ek *alag* guarantee deta hai
  (publish/subscribe of *other* data, file 08), store-load fence nahi.
- **Sirf `seq_cst`** ise 0 karta hai (store pe `mfence` / `xchg` — file 09).

Yeh CLAUDE.md Rule 2 ka live example hai: "textbook" bolti hai release/acquire
"stronger" hai, par yeh **specific** litmus test sirf seq_cst rokta hai.

---

## `std::memory_order` — the six values

```cpp
enum class memory_order {
    relaxed,        // atomicity only, no ordering
    consume,        // (deprecated / treated as acquire — ignore it)
    acquire,        // for loads: nothing after can move before
    release,        // for stores: nothing before can move after
    acq_rel,        // for RMW: both
    seq_cst         // acquire/release + a single global total order (DEFAULT)
};
```

Har atomic op ka default `seq_cst` hai:
```cpp
a.store(1);                              // == a.store(1, std::memory_order_seq_cst)
a.load();                               // == a.load(std::memory_order_seq_cst)
a.fetch_add(1);                         // == ...seq_cst
```

Ordering strength: `relaxed < acquire/release < acq_rel < seq_cst`.

| Order | Legal on | Guarantee |
|---|---|---|
| `relaxed` | load, store, RMW | just atomic — no happens-before with anything |
| `acquire` | load, RMW | this load + everything after it stays after; pairs with a release |
| `release` | store, RMW | this store + everything before it stays before; pairs with an acquire |
| `acq_rel` | RMW only | acquire on the read half, release on the write half |
| `seq_cst` | all | acq/rel **plus** all seq_cst ops share one global order all threads agree on |

---

## What ordering buys you: the message-passing pattern (file 08 in full)

```cpp
int data = 0;                          // plain
std::atomic<bool> ready{false};

// Producer
data = compute();                      // (A)
ready.store(true, std::memory_order_release);   // (B) — everything before B is "published"

// Consumer
while (!ready.load(std::memory_order_acquire)) {}  // (C) — once C sees true...
use(data);                             // (D) — ...D is guaranteed to see (A)'s write
```

The `release` store (B) and the `acquire` load (C) that reads its value form a
**synchronizes-with** edge → (A) *happens-before* (D) → no data race on `data`,
and `data`'s value is visible. With `relaxed` on B/C, no edge — (D) can read stale
`data` (or it's a data race — file 01 exercise 5).

---

## Why not always `seq_cst`?

It's the default and it's the easiest to reason about (one global order). Cost:

- **`seq_cst` store on x86** = `xchg` or `mov`+`mfence` — ~**12.9 ns** here vs
  ~**0.72 ns** for a `relaxed`/`release` store (`examples/06`). ~18×.
- `seq_cst` load on x86 is cheap (~same as relaxed, ~0.74 ns) — the expense is on
  the store side.
- On ARM, `seq_cst` needs full barriers (`dmb ish`) on **both** sides — much more.

So: **start with `seq_cst` (correct, simple). Profile. Downgrade the hot atomics to
`acquire`/`release`/`relaxed` where you can prove the weaker order is enough**
(files 07–09). Never downgrade by guessing.

---

## > **HFT relevance**
> - **The hot path uses `relaxed` / `acquire` / `release` deliberately** — a
>   `seq_cst` store's `mfence` on every publish is ~13 ns you don't spend if
>   `release` suffices (SPSC queue: `release` push index, `acquire` pop index —
>   folder 28).
> - **`seq_cst` loads are cheap on x86** — don't contort code to avoid a `seq_cst`
>   *load*; the store side is where the barrier lives.
> - **Reason from the litmus test, not from "stronger is safer"** — `examples/05`
>   shows `release/acquire` does *not* stop store-load reordering; if your
>   algorithm needs that (rare — Dekker-style mutual exclusion), you need
>   `seq_cst` or an explicit `seq_cst` fence.
> - **Cross-platform HFT**: code tuned to x86 TSO (where store-store and load-load
>   "just work") breaks on an ARM trading box. Write the orders the *model*
>   requires and let x86 make them free.

---

## Hands-on

```bash
./build.ps1 fast 27-ATOMICS-MEMORY-MODEL/examples/05_reordering_demo.cpp
./build.ps1 fast 27-ATOMICS-MEMORY-MODEL/examples/06_memory_order_bench.cpp
```

`05`: store-buffering counts for relaxed / rel-acq / seq_cst. `06`: per-op ns for
each order. Then:
- `g++ -O2 -S` a `x.store(1, relaxed)` vs `x.store(1, seq_cst)` — see the `mfence`
  (or `xchg`) appear only for seq_cst.
- Swap the loads in `05` to `seq_cst` but keep stores `relaxed` — still non-zero
  (you need seq_cst on *both* sides / the stores).

---

## ⚠️ Traps

### Trap 1 — "atomic means ordered"
Atomic = indivisible. Ordering is the separate `memory_order` argument. `relaxed`
is fully atomic and fully unordered.

### Trap 2 — "release/acquire is a full barrier"
It's a **one-way** barrier (release: no upward moves past it… wait — release keeps
prior ops *before* it; acquire keeps later ops *after* it) and it does **not**
prevent store-load reordering across the pair (`examples/05`).

### Trap 3 — reasoning in source order about other threads
Another thread may observe your writes in a different order unless you placed the
right release/acquire (or seq_cst).

### Trap 4 — "`-O0` so no reordering"
The **CPU** still reorders at `-O0`. `examples/05` shows SB violations at `-O0`
too (fewer, but non-zero).

### Trap 5 — downgrading `seq_cst` → `relaxed` by guessing
Every downgrade needs a happens-before argument. A wrong downgrade is a data race
that passes every test on x86 and corrupts on ARM / under load.

### Trap 6 — `memory_order_consume`
Deprecated, every compiler promotes it to `acquire`. Don't use it.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "compiler doesn't reorder atomics" | It reorders *non-atomic* ops around a `relaxed` atomic freely; ordering comes from acquire/release/seq_cst |
| "reordering is only a compiler thing" | The **CPU** reorders too (store buffer, OoO) — visible even at `-O0` |
| "x86 has no reordering" | x86 TSO still allows **store-load** reordering (`examples/05`) |
| "release/acquire stops all reordering" | One-way; doesn't stop store-load across the pair — only `seq_cst` does |
| "`seq_cst` is too slow to ever use" | `seq_cst` *loads* are cheap on x86; it's the sane default — downgrade only hot, proven paths |
| "default order is `relaxed`" | Default is `seq_cst` |

---

## Exercises

1. **Two reorderers:** name them and one concrete thing each does to
   `data = 42; ready = true;`.

   <details><summary>Answer</summary>

   Compiler: may swap the two stores (they look independent), or keep `ready` in a
   register. CPU: the store to `data` sits in the store buffer while `ready`'s
   store reaches cache first, so another core sees `ready == true`, `data == 0`.
   </details>

2. **Predict `examples/05`:** which orders leave `r1==0 && r2==0` possible on x86?

   <details><summary>Answer</summary>

   `relaxed` and `release/acquire` — both allow store-load reordering (store buffer
   holds the store past the later load). Only `seq_cst` forces 0 (store-side
   `mfence`/`xchg`).
   </details>

3. **Default order:** `std::atomic<int> a; a.store(5); int v = a.load();` — what
   memory order?

   <details><summary>Answer</summary>

   `seq_cst` for both — it's the default argument. Explicit: `a.store(5,
   std::memory_order_seq_cst)`.
   </details>

4. **Why not all `seq_cst`:** cost of a `seq_cst` store vs a `relaxed` store on
   this box, and on which side (load/store) is `seq_cst` cheap on x86?

   <details><summary>Answer</summary>

   ~12.9 ns vs ~0.72 ns (`examples/06`) — ~18×, because the `seq_cst` store emits
   an `mfence`/`xchg`. `seq_cst` *loads* are ~as cheap as relaxed on x86 (~0.74 ns
   — no barrier needed for a load under TSO).
   </details>

5. **MP pattern:** in the producer/consumer snippet, replace `release`/`acquire`
   with `relaxed`. What breaks?

   <details><summary>Answer</summary>

   No synchronizes-with edge → the consumer can observe `ready == true` while
   `data` is still 0 (or a partially-written value), and the read of `data` is a
   data race → UB. `release`/`acquire` (or `seq_cst`) is required.
   </details>

---

## Interview questions

1. Source order vs execution order — do reorderers kaunse, ek-ek example.
2. Store buffer kya karta hai — store-load reordering kaise?
3. `std::memory_order` ki six values — strength order, kaun kahan legal.
4. Default memory order kya hai? `seq_cst` kyun default?
5. Store-buffering litmus — kaunsa order `r1==r2==0` rokta, kaunsa nahi?
6. `seq_cst` mehnga kyun (kaunsi side, x86 vs ARM)?
7. `seq_cst` → weaker downgrade karne ke liye kya justify karna padta?

---

## Next
→ [`07-memory-order-relaxed.md`](07-memory-order-relaxed.md)
