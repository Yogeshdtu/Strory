# 09 — Memory ordering cheatsheet

Deep: folders `27-ATOMICS-MEMORY-MODEL`, `28-LOCK-FREE`, `47/08`.
`#include <atomic>`.

---

## `std::atomic` vs `volatile`

| | `atomic<T>` | `volatile T` |
|---|---|---|
| indivisible ops (no torn r/w) | ✅ | ❌ |
| cross-thread ordering / fences | ✅ | ❌ |
| RMW (`fetch_add`, `compare_exchange`) | ✅ | ❌ |
| "don't optimize away / reorder w.r.t. other volatiles" | (also) | ✅ |
| for | threading | MMIO, signal handlers, `setjmp` |

**Never use `volatile` for threading.**

---

## The six orders (weak → strong)

| Order | Guarantee | Typical use |
|---|---|---|
| `relaxed` | atomicity only; **no** ordering, no visibility promise | counters/stats that guard nothing |
| `consume` | data-dependent loads ordered (in theory) | **don't** — every compiler promotes it to `acquire` |
| `acquire` | on a **load**: no reads/writes after it move before it; sees the paired release's writes | reader side of a handoff |
| `release` | on a **store**: no reads/writes before it move after it; publishes prior writes | writer side of a handoff |
| `acq_rel` | both, on a RMW | `fetch_sub` on a refcount, lock-free stack CAS |
| `seq_cst` | acq_rel **+** one single total order all threads agree on | default; when you need a global order (rare) |

Default (no arg) = `seq_cst`.

---

## The one pattern you must know: release/acquire handoff

```cpp
// producer
data.x = 1; data.y = 2;                       // plain writes
ready.store(true, std::memory_order_release); // publish

// consumer
if (ready.load(std::memory_order_acquire))    // acquire
    use(data.x, data.y);                      // guaranteed to see 1, 2
```

The `release` store **happens-before** the `acquire` load *that reads its value*
→ everything the producer did before the store is visible to the consumer after
the load. A plain (`relaxed`) store here is a bug: the CPU/compiler may make
`ready` visible before `data` (breaks on ARM/POWER; often "works" on x86-TSO).

---

## Fences (standalone)

```cpp
std::atomic_thread_fence(std::memory_order_acquire);   // no load can move up past it... roughly
std::atomic_thread_fence(std::memory_order_release);
std::atomic_thread_fence(std::memory_order_seq_cst);
std::atomic_signal_fence(...);   // compiler-only barrier (signal handlers, same thread)
```
Prefer ordering *on the atomic op*; standalone fences are for seqlock-style code.

---

## `compare_exchange_weak` vs `_strong`

```cpp
T expected = x.load(relaxed);
while (!x.compare_exchange_weak(expected, f(expected), acq_rel, relaxed)) { /* expected updated */ }
```

- `weak` can fail spuriously (even when equal) — cheaper on LL/SC (ARM/POWER); use **in a loop**.
- `strong` never spurious — use for a one-shot (lazy init).
- On failure, `expected` is loaded with the current value.
- Two orders: success order, then failure order (must be ≤ success, no `release`).
- x86: both compile to `LOCK CMPXCHG`, same cost.

---

## ABA problem

Thread reads `head == A`, stalls; others pop A, pop B, push A back (address
reused). CAS `(A → B)` succeeds against a **stale** `next`. Fixes:
- **tagged pointer** / generation counter (128-bit CAS: `cmpxchg16b`), `tag++` per success
- pack a counter in the pointer's unused bits (48-bit VA / 8-byte alignment)
- **hazard pointers** / **RCU** / epoch-based reclamation (node never freed while referenced)

---

## Lock-free vs wait-free

| | means |
|---|---|
| obstruction-free | a thread makes progress if it runs alone |
| lock-free | **some** thread always makes progress (system-wide) |
| wait-free | **every** thread makes progress in a bounded number of steps |

SPSC ring = wait-free (no CAS, one writer per index). Treiber stack = lock-free
(CAS retry loop). MPSC Vyukov = wait-free for producers (`XCHG`).

---

## `is_lock_free()`

True → the type's atomics are HW instructions (no hidden mutex). `atomic<int>`,
`atomic<void*>` almost always. `atomic<BigStruct>`, 128-bit without `cmpxchg16b`
→ may use a lock → not usable in a signal handler / true wait-free path.
Check `std::atomic<T>::is_always_lock_free` (compile-time).

---

## x86 (TSO) vs ARM/POWER (weak) — why your tests lie

x86 = Total Store Order: only store→load reordering is visible; a missing
`acquire`/`release` often "works" on x86 and **breaks on ARM**. Write to the
*intent* (the C++ memory model), not the platform. Test with TSan.

## Next
→ [`10-latency-numbers.md`](10-latency-numbers.md)
