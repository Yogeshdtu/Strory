# 02 — Atomicity

## Prerequisites
- `01-data-race-definition.md`, `25-OBJECT-MODEL` file 07 (object representation, tearing)
- `05-OPERATORS` (how `++` compiles)

## Yeh topic abhi kyun
"Atomic" = **indivisible** — ek operation jo ya poori hoti hai ya bilkul nahi,
beech mein koi doosra thread usse aadha-hua nahi dekh sakta. Yeh samajhna zaroori
hai ki **kaunsi cheezein khud atomic nahi hain** (surprising list) aur atomicity
sirf ek hissa hai — ordering alag cheez (file 06 onwards).

---

## Atomic ka matlab

Ek operation **atomic** hai agar koi bhi doosra thread usse sirf do states mein
dekh sakta: **"hua nahi"** ya **"poora ho gaya"** — kabhi "aadha".

```cpp
std::atomic<long> a{0};
a.fetch_add(1);        // ATOMIC: doosra thread ya 0 dekhega ya 1, kabhi "beech mein" nahi
```

**Non-atomic RMW** (`++`, `+=`, `x = x + 1`) **teen** operations hai:
```
1. load  x -> reg
2. add   reg, 1
3. store reg -> x
```
Do threads inhe interleave karein → lost update (folder 26 file 05). `fetch_add`
in teenon ko **ek** indivisible hardware operation banata hai (x86: `lock xadd`).

---

## Non-atomic operations jo toot sakti hain

### 1. Read-modify-write — `++`, `--`, `+=`, `|=`, ...
Load-modify-store — beech mein koi aur likh de → lost update.

### 2. **Torn reads / writes** — bade ya misaligned objects
```cpp
struct Pair { int lo; int hi; };   // 8 bytes
Pair p;
// Thread A: p = {1, 1};
// Thread B: Pair q = p;            // ⚠️ q could be {1, 0} or {0, 1} — TORN
```
Ek non-atomic 8-byte (or bigger) write compiler/hardware do 4-byte stores mein tod
sakta. Reader beech mein padh le → half-old-half-new. **Misaligned** access aur
bhi (do cache lines cross kare → definitely two operations).

### 3. Pointer publication
```cpp
Config* g = nullptr;
// Thread A: g = new Config(...);   // the POINTER write may be atomic on x86...
// Thread B: if (g) use(*g);        // ...but B may see g != null and stale FIELDS
```
Pointer ki value ka store x86 pe practically atomic hai, par **jo woh point karta
hai** uski visibility ordering ka sawaal hai (file 08).

### 4. `bool` — not guaranteed atomic
Practically atomic har platform pe, par standard guarantee nahi. `std::atomic<bool>`.

### 5. `double` / `long double` — not guaranteed atomic
80-bit `long double` definitely non-atomic. `double` (8 bytes) usually OK on x86
but not guaranteed by the language.

---

## Kya "usually atomic" hai (par language guarantee NAHI karta)

On x86-64, a **naturally-aligned** load or store of ≤ 8 bytes is atomic **at the
hardware level** (Intel SDM). So `int x; x = 5;` from one thread and `int y = x;`
from another won't *tear*.

**But it's still a data race (UB)** if `x` is non-atomic — because:
- The **compiler** can still reorder it, cache it in a register, coalesce it,
  eliminate it (it assumes race-free code).
- Cross-line / misaligned → not even hardware-atomic.
- Not portable (ARM, older archs, larger types).

**So: "aligned int/pointer load-store doesn't tear on x86" is a hardware fact, not
a license to skip `std::atomic`.** Use `std::atomic<T>` and you get atomicity
*and* a well-defined memory model *and* portability.

---

## `std::atomic<T>` — how it delivers atomicity

| `sizeof(T)` | x86-64 mechanism |
|---|---|
| 1, 2, 4, 8 bytes | native atomic instructions (`mov`, `lock xadd`, `lock cmpxchg`) — **lock-free** |
| 16 bytes | `cmpxchg16b` if `-mcx16` (else a libcall / internal lock) — often not lock-free |
| > 16 bytes, or non-trivially-copyable | `std::atomic<T>` wraps a **hidden mutex/spinlock** — `is_lock_free()` = false |

```cpp
std::atomic<int>::is_always_lock_free    // true  — 4B
std::atomic<long>::is_always_lock_free   // true  — 8B
std::atomic<Big24>::is_always_lock_free  // false — hidden lock (examples/01)
```

`std::atomic<T>` for a large `T` is still **correct** (no data race, no tearing) —
just not lock-free, so not usable in a truly lock-free algorithm and slower.

---

## Atomicity ≠ ordering (the folder's core split)

`std::atomic` gives you **two separate things**, and you tune them separately:

1. **Atomicity** — the operation is indivisible (always; you can't turn this off).
2. **Ordering** — how this operation is ordered relative to *other* memory
   operations, across threads (the `std::memory_order` argument — files 06–11).

```cpp
a.fetch_add(1, std::memory_order_relaxed);   // atomic ✓   ordering: none
a.store(1, std::memory_order_release);        // atomic ✓   ordering: release
a.load(std::memory_order_seq_cst);            // atomic ✓   ordering: full (seq_cst)
```

A `relaxed` atomic is **fully atomic** — it just imposes no ordering on the
surrounding non-atomic memory. This is the confusion to kill early: **relaxed is
not "less atomic," it's "no ordering."**

---

## > **HFT relevance**
> - **Every shared scalar is `std::atomic<T>`** (a counter, a "stop" flag, a write
>   index, a published pointer) — for atomicity *and* to stop the compiler caching
>   it. `relaxed` where only the count matters, `release`/`acquire` where it gates
>   data.
> - **Design shared state to fit a lock-free atomic** — ≤ 8 bytes (a counter, an
>   index, a tagged 64-bit pointer, a small enum). A 24-byte "status struct" shared
>   across threads means a hidden lock; instead publish an *immutable snapshot*
>   and swap a pointer (folder 26 file 10, folder 28 file 12 seqlock).
> - **Never rely on "x86 doesn't tear aligned 8-byte accesses"** in portable HFT
>   code — even on x86 the compiler will reorder/cache a non-atomic. `std::atomic`.
> - **A `double` price shared across threads** → `std::atomic<double>` (or store
>   the integer tick representation in a `std::atomic<int64_t>` and `bit_cast`).

---

## Hands-on

```bash
./build.ps1 fast 27-ATOMICS-MEMORY-MODEL/examples/01_atomic_counter.cpp
```

Shows `is_always_lock_free` for `int`/`long`/`void*` (true) vs a 24-byte struct
(false), and non-atomic `++` losing updates vs atomic. Then:
- `struct Pair { int a, b; }; std::atomic<Pair> ap;` — check `.is_lock_free()`
  (8 bytes → true). Add a third `int` → 12 bytes → still lock-free (padded to 16?
  check). Add to 24 bytes → false.
- `g++ -O2 -S` a `a.fetch_add(1)` — see `lock xadd`. A non-atomic `++x` at `-O2` —
  see it get coalesced/hoisted.

---

## ⚠️ Traps

### Trap 1 — "aligned int assignment is atomic, so no `std::atomic` needed"
Hardware-atomic ≠ race-free. The compiler still reorders/caches/coalesces a
non-atomic. And it's not portable.

### Trap 2 — "`relaxed` is less atomic"
`relaxed` is **fully atomic**. It imposes no *ordering* on surrounding memory.

### Trap 3 — non-atomic RMW (`x += 1`, `flags |= BIT`) on shared data
Load-modify-store — lost updates. `fetch_add` / `fetch_or`.

### Trap 4 — torn reads of a big shared struct
A non-atomic 16-byte read while another thread writes it → half-old-half-new.
`std::atomic<Struct>` (may not be lock-free) or a seqlock (folder 28).

### Trap 5 — `std::atomic<BigStruct>` assumed lock-free
Check `is_lock_free()` / `is_always_lock_free`. Big → hidden mutex → not usable in
lock-free code, and slower.

### Trap 6 — `std::atomic<double>` for FP RMW
`fetch_add` on `std::atomic<double>` exists since C++20 but may be a CAS loop; and
FP addition isn't associative, so concurrent `fetch_add`s give an order-dependent
sum (folder 19 file 10).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "aligned load/store is atomic → skip `std::atomic`" | Hardware-atomic, but still a data race (compiler reorders/caches); not portable |
| "`relaxed` atomic is only partly atomic" | Fully atomic; it just adds no ordering |
| "`x += 1` on a shared int is fine if int is atomic-sized" | RMW is load-modify-store — lost updates; use `fetch_add` |
| "`std::atomic<T>` is always lock-free" | Only for small trivially-copyable `T`; big → hidden lock |
| "atomic gives me ordering too, by default" | The *default* is `seq_cst` (strong); but atomicity and ordering are separate knobs |
| "`bool`/`double` are atomic on every platform" | Not guaranteed by the language — use `std::atomic<bool>`/`<double>` |

---

## Exercises

1. **Atomic or torn:** on x86-64, non-atomic: (a) `int x = y;` (both aligned),
   (b) `struct P { int a, b; } p = q;`, (c) a `long` that straddles two cache
   lines, (d) `char c = d;`.

   <details><summary>Answer</summary>

   (a) hardware-atomic (aligned 4B) — but still a data race if unsynchronized.
   (b) 8 bytes — a single aligned store on x86 is atomic, but the compiler may
   split it; not guaranteed. (c) **not** atomic — crosses a line, two ops.
   (d) atomic (1 byte). In all cases, use `std::atomic` for a well-defined
   program.
   </details>

2. **Lock-free?** `std::atomic<T>::is_always_lock_free` for `T` = `int`, `void*`,
   `struct{long a,b;}` (16B), `struct{long a,b,c;}` (24B), `std::shared_ptr<int>`.

   <details><summary>Answer</summary>

   `int` — true. `void*` — true. 16B struct — platform-dependent (true with
   `cmpxchg16b` / `-mcx16`, often false without; on this MinGW: false). 24B —
   false (hidden lock). `std::shared_ptr<int>` in a `std::atomic<>` — typically
   **not** lock-free (internal lock table).
   </details>

3. **Fix the RMW:** `std::atomic<uint32_t> flags{0}; ... flags = flags | MASK;`
   from two threads.

   <details><summary>Answer</summary>

   `flags = flags | MASK` is `load; or; store` — three ops, racing (and `flags`'s
   `operator|` isn't a single atomic RMW here). Use `flags.fetch_or(MASK,
   std::memory_order_relaxed);` — one atomic RMW.
   </details>

4. **Big shared state:** you need threads to read a `struct MarketSnapshot { double
   bid, ask; int64_t bid_sz, ask_sz; uint64_t ts; }` (40 bytes) that one thread
   updates. `std::atomic<MarketSnapshot>` — good idea?

   <details><summary>Answer</summary>

   No — 40 bytes → `std::atomic` wraps a hidden mutex (not lock-free, slow, and
   readers block the writer). Better: a **seqlock** (folder 28 file 12 — writer
   bumps a sequence counter, readers retry if it changed mid-read) or publish an
   immutable snapshot via `std::atomic<const MarketSnapshot*>` pointer swap.
   </details>

5. **Atomicity vs ordering:** `a.fetch_add(1, std::memory_order_relaxed)` — is the
   increment atomic? Does anything about *other* memory get ordered?

   <details><summary>Answer</summary>

   The increment is fully atomic (no lost updates, no tearing). `relaxed` adds
   **no** ordering — writes/reads to other variables around it can be reordered
   past it by the compiler/CPU. Perfect for a standalone counter; wrong for
   publishing data.
   </details>

---

## Interview questions

1. "Atomic" ka exact matlab — indivisible; kaunse states dikhte?
2. Non-atomic `++` kyun toot sakta (3 steps)?
3. Torn read/write — kab (big / misaligned / cross-line)?
4. x86 pe aligned 8-byte load-store atomic hai — phir `std::atomic` kyun?
5. `std::atomic<T>` lock-free kab, kab hidden mutex?
6. `relaxed` "kam atomic" hai? (No — samjhao.)
7. Atomicity aur ordering — do alag knobs, kaise?

---

## Next
→ [`03-std-atomic.md`](03-std-atomic.md)
