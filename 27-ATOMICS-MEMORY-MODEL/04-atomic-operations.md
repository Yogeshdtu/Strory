# 04 — Atomic operations: load, store, exchange, fetch_*

## Prerequisites
- `03-std-atomic.md`
- [`examples/01_atomic_counter.cpp`](examples/01_atomic_counter.cpp), [`examples/02_cas_loop.cpp`](examples/02_cas_loop.cpp)

## Yeh topic abhi kyun
`std::atomic<T>` ke operations chhoti list hai — par har ek ka ek clear use hai,
aur unke **hardware mapping** samajhna (x86: `mov`, `xchg`, `lock xadd`,
`lock cmpxchg`) batata hai kya sasta hai kya mehnga. `compare_exchange` (CAS)
alag file (05) — woh lock-free ka core hai.

---

## The operations

| Operation | Kya karta | x86-64 mapping | Cost (uncontended, measured) |
|---|---|---|---|
| **`load(mo)`** | atomic read | `mov` (even for seq_cst) | ~0.74 ns |
| **`store(v, mo)`** | atomic write | `mov` (relaxed/release); `xchg`/`mfence` (seq_cst) | ~0.72 ns / ~12.9 ns (seq_cst!) |
| **`exchange(v, mo)`** | set to `v`, return old | `xchg` (implicitly `lock`ed) | ~RMW cost |
| **`fetch_add(n, mo)` / `fetch_sub`** | add/sub, return old | `lock xadd` | ~13 ns |
| **`fetch_and` / `fetch_or` / `fetch_xor`** (integral) | bitwise RMW, return old | `lock cmpxchg` loop (no direct instr for return-old) | ~RMW cost |
| **`compare_exchange_weak/strong`** | CAS (file 05) | `lock cmpxchg` | ~13 ns |
| **`++` `--` `+= -= &= |= ^=`** | shorthand for `fetch_*` | same as `fetch_*` | (all `seq_cst`!) |

(Measured single-threaded on x86-64, GCC `-O2`, `examples/06` — absolute ns vary
with machine state; the *ratios* are the point.)

**Key numbers (x86):**
- **Loads are free** — relaxed / acquire / seq_cst all compile to a plain `mov`
  (~0.74 ns, identical).
- **Relaxed / release stores are free** — a plain `mov` (~0.72 ns).
- **A seq_cst *store* costs ~18× a relaxed store** (~12.9 ns vs ~0.72 ns) — it
  needs an `mfence` (or is implemented as `xchg`). This is the one place ordering
  has a visible price on x86.
- **All RMWs (`fetch_add`, CAS, ...) already carry `lock`** — the memory order
  barely changes their cost on x86 (~13 ns regardless).

---

## `load` / `store`

```cpp
std::atomic<long> a{0};
long v = a.load(std::memory_order_acquire);
a.store(42, std::memory_order_release);
```

- Return / take a plain `T`.
- The `std::memory_order` argument controls **ordering** (files 06–11), not
  atomicity.
- `load` accepts only `relaxed`, `consume`, `acquire`, `seq_cst`.
- `store` accepts only `relaxed`, `release`, `seq_cst`. (Passing `acquire` to
  `store` → UB / compile-assert.)

## `exchange`

```cpp
long old = a.exchange(9, std::memory_order_acq_rel);   // a = 9; return previous a
```

Atomic swap. Uses: a spinlock (`exchange` a flag to `true`, see if it was `false`),
"take the current value and reset it" (drain a counter), lock-free hand-off.

## `fetch_add` / `fetch_sub`

```cpp
long prev = a.fetch_add(1, std::memory_order_relaxed);   // a += 1; return OLD value
```

- **Returns the value *before* the operation** (like `x++`, not `++x`).
- The single most common atomic op — counters, ring-buffer index bumps, allocating
  a slot ("`fetch_add(1)` → my index").
- Pointer specialization: `p.fetch_add(n)` advances by `n * sizeof(T)`.

## `fetch_and` / `fetch_or` / `fetch_xor` (integral only)

```cpp
uint32_t prev = flags.fetch_or(MASK, std::memory_order_relaxed);   // flags |= MASK
bool was_set  = flags.fetch_and(~MASK, std::memory_order_relaxed) & MASK;
```

Atomic bit set/clear/toggle. Return the old value → you can tell whether a bit was
already set. On x86 these become a `lock cmpxchg` loop (there's `lock or` but it
doesn't return the old value).

## No `fetch_max` / `fetch_min` / `fetch_mul` — CAS-loop them

```cpp
// atomic max
long cur = a.load(std::memory_order_relaxed);
while (candidate > cur &&
       !a.compare_exchange_weak(cur, candidate, std::memory_order_relaxed)) { }
```

Anything not in the fixed list → a `compare_exchange` loop (file 05, `examples/02`).
(C++26 adds `fetch_max`/`fetch_min`.)

---

## `x++` vs `++x` vs `fetch_add`

```cpp
a++;                 // fetch_add(1) — returns OLD value  (as a plain T)
++a;                 // fetch_add(1) — returns NEW value
a.fetch_add(1);      // returns OLD value
a += 1;              // fetch_add(1) — returns NEW value
```

All are `fetch_add(1, seq_cst)`. The shorthands differ only in what they return.
**In real code use `a.fetch_add(1, memory_order_relaxed)`** (or the order you
actually need) — explicit and not `seq_cst`.

---

## The memory-order default is `seq_cst`

```cpp
a.load();                              // == a.load(std::memory_order_seq_cst)
a.store(1);                            // == seq_cst
a.fetch_add(1);                        // == seq_cst
++a;  a = 5;  a += 3;                  // == seq_cst
```

`seq_cst` is the **safe** default (easiest to reason about — file 09) but the
**most expensive** (a seq_cst store fences on x86). Files 07–09 are about
choosing a weaker order deliberately where it's provably enough.

---

## `compare_exchange` — the odd one out (file 05)

CAS takes **two** memory orders (success / failure) and, on failure, **writes the
current value into your `expected`**:

```cpp
T expected = a.load(std::memory_order_relaxed);
T desired  = f(expected);
while (!a.compare_exchange_weak(expected, desired,
                               std::memory_order_release,   // on success
                               std::memory_order_relaxed))  // on failure
    desired = f(expected);   // expected now holds the current value — recompute
```

Full treatment in file 05.

---

## > **HFT relevance**
> - **`fetch_add(1, relaxed)`** for lock-free ring-buffer slot allocation, stats,
>   sequence numbers — the workhorse.
> - **`exchange`** for "grab and reset" (drain a per-interval counter) and
>   spinlocks.
> - **`store(v, release)` + `load(acquire)`** for publishing (the SPSC pattern,
>   folder 28) — and note the store is a plain `mov` on x86, so it's cheap.
> - **Avoid `seq_cst` stores in the hot loop** — the `mfence` is ~13 ns and it's
>   almost never needed for a single-producer publish (release is enough).
> - **`fetch_or`/`fetch_and`** for atomic flag sets in a bitmask (e.g. "which
>   venues have quoted"), reading the old value to detect first-set.
> - The uncontended numbers (~0.7 ns load/store, ~13 ns RMW) are the *floor* — the
>   moment two cores touch the same atomic, cache-coherence traffic (~25–30 ns
>   here, more with core count) dwarfs the instruction cost (folder 26 file 16).

---

## Hands-on

```bash
./build.ps1 fast 27-ATOMICS-MEMORY-MODEL/examples/06_memory_order_bench.cpp
```

The per-op numbers: load (~0.74 ns all orders), store relaxed/release (~0.72 ns)
vs seq_cst (~12.9 ns), fetch_add (~13 ns all orders). Then:
- `g++ -O2 -S` and find `mov` for a relaxed store, `xchg`/`mfence` for a seq_cst
  store, `lock xadd` for `fetch_add`, `lock cmpxchg` for `fetch_or`.
- `examples/02` — atomic max via a CAS loop (no `fetch_max`).
- `a.exchange(0)` to drain a counter: N threads `fetch_add`, one thread
  periodically `exchange(0)` and sums the drained values.

---

## ⚠️ Traps

### Trap 1 — `fetch_add` returns the OLD value
`long slot = ring.widx.fetch_add(1);` — `slot` is the index you got (pre-bump).
`++a` returns the new value.

### Trap 2 — the shorthands are `seq_cst`
`++counter` on a `std::atomic` = `fetch_add(1, seq_cst)`. Use explicit
`fetch_add(1, relaxed)` on the hot path.

### Trap 3 — a seq_cst store where release would do
`flag.store(1)` (default seq_cst) → `mfence` on x86. `flag.store(1, release)` →
plain `mov`. For a publish, release is correct and ~18× cheaper (~0.72 vs ~12.9 ns).

### Trap 4 — `a += 1` expecting a single instruction
It's `fetch_add(1, seq_cst)` — a `lock`-prefixed RMW, and `seq_cst`.

### Trap 5 — passing `acquire` to `store` / `release` to `load`
UB (and `static_assert`s in libstdc++). Loads: `relaxed`/`acquire`/`seq_cst`.
Stores: `relaxed`/`release`/`seq_cst`.

### Trap 6 — expecting `fetch_max`/`fetch_mul`
Not in C++20. CAS-loop them (`examples/02`).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`fetch_add` returns the new value" | Returns the **old** value (like `x++`) |
| "atomic loads/stores cost the same at every order on x86" | Loads yes; a **seq_cst store** needs `mfence` (~18× a relaxed store here) |
| "the default memory order is relaxed" | It's `seq_cst` (strongest, priciest) |
| "`++a` on an atomic is a single instruction" | `lock`-prefixed RMW, and `seq_cst` |
| "there's `fetch_max`/`fetch_min`" | Not in C++20 — CAS-loop; C++26 adds them |
| "the RMW cost depends heavily on memory order (x86)" | Barely — it's `lock`-prefixed regardless (~13 ns); contention dominates |

---

## Exercises

1. **Old or new:** `std::atomic<long> a{5}; long r = a.fetch_add(3); long s = ++a;`
   — values of `r`, `s`, `a`?

   <details><summary>Answer</summary>

   `r == 5` (old value before adding 3), `a` becomes 8, then `++a` → `a == 9`,
   `s == 9` (new value).
   </details>

2. **Cheapest publish:** you publish a "ready" flag once, read by one consumer.
   `flag.store(1)` vs `flag.store(1, std::memory_order_release)` on x86 — codegen
   and cost?

   <details><summary>Answer</summary>

   `flag.store(1)` = seq_cst → `mov` + `mfence` (or `xchg`), ~12.9 ns.
   `flag.store(1, release)` = plain `mov`, ~0.72 ns. Release is sufficient for
   publish (pairs with the consumer's acquire load — file 08) and ~18× cheaper.
   </details>

3. **Atomic min:** write an atomic-min update for `std::atomic<int> lo`.

   <details><summary>Answer</summary>

   ```cpp
   int cur = lo.load(std::memory_order_relaxed);
   while (candidate < cur &&
          !lo.compare_exchange_weak(cur, candidate, std::memory_order_relaxed)) {}
   ```
   (`compare_exchange_weak` refreshes `cur` on failure.)
   </details>

4. **Drain:** N threads `fetch_add` into `std::atomic<long> pending`. A reporter
   thread wants the count-so-far and to reset it. One op?

   <details><summary>Answer</summary>

   `long got = pending.exchange(0, std::memory_order_relaxed);` — atomically reads
   the current value and stores 0, returning the old. Sum the `got`s over time for
   the running total. (Beware: increments between the reader's read and the
   producers' next add are just counted in the next drain — no loss.)
   </details>

5. **Bit set:** `std::atomic<uint32_t> venues{0};` — atomically set bit `v` and
   find out if it was already set.

   <details><summary>Answer</summary>

   `uint32_t prev = venues.fetch_or(1u << v, std::memory_order_relaxed);`
   `bool was_already_set = prev & (1u << v);` — `fetch_or` returns the value
   *before* setting the bit.
   </details>

---

## Interview questions

1. `std::atomic` ke core ops — `load`/`store`/`exchange`/`fetch_add`/`fetch_or`.
2. `fetch_add` return value — old ya new?
3. x86 pe kaunsa atomic op mehnga (seq_cst store — `mfence`), kaunse free (loads)?
4. `++a` on `std::atomic` — kya expand hota, default order?
5. `fetch_max` kyun nahi — kaise implement (CAS loop)?
6. `store` ko `acquire` pass karna — kya hota?
7. RMW ka cost memory order pe kitna depend karta (x86)?

---

## Next
→ [`05-compare-exchange.md`](05-compare-exchange.md)
