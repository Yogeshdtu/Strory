# 15 — `thread_local` storage

## Prerequisites
- `25-OBJECT-MODEL` file 03/06 (`thread_local` storage/init), `05-race-conditions.md`
- `24-COMPILATION-LINKING` file 06

## Yeh topic abhi kyun
`thread_local` = har thread ka apna instance of a variable. Yeh sharing ko
**khatam** kar deta hai — no lock, no atomic, no race, kyunki koi doosra thread us
object ko dekh hi nahi sakta. HFT ka favourite pattern: per-thread RNG, per-thread
arena, per-thread scratch buffer. Par uski **access cost** aur **destruction
timing** ke sharp edges hain.

---

## Basics

```cpp
thread_local std::mt19937 t_rng{seed()};        // har thread ka apna, seeded per thread
thread_local int t_depth = 0;
thread_local std::vector<char> t_scratch;        // reused across calls on this thread

void handle() {
    ++t_depth;                                    // only THIS thread's t_depth
    t_scratch.clear();
    // ...
    --t_depth;
}
```

- One instance **per thread**, per variable. Lifetime = thread lifetime (or first
  use → thread exit).
- **No synchronization needed** — no other thread can access it. Zero race risk.
- `static thread_local` at namespace scope = internal linkage + per-thread.
  Function-local `thread_local` = one per thread, first-use init (like a
  function-local `static` but per-thread).

`errno` is `thread_local` (per-thread `int`). RNG state, "current transaction"
pointers, allocation arenas, recursion guards, per-thread stats — classic uses.

---

## The access cost — cache it in hot loops

`thread_local` access is **not** a plain memory load:
- **initial-exec model** (the common case for a variable in the main executable):
  `mov rax, fs:[offset]` on x86-64 Linux — a segment-relative load. Cheap-ish but
  not free (an extra addressing step).
- **general-dynamic model** (variable in a `.so` loaded at runtime): a call to
  `__tls_get_addr` — a **function call** per access. Much more expensive.
- Plus a **first-use guard** for dynamically-initialized `thread_local`s ("is it
  constructed on this thread yet?") on every access — unless `constinit`.

```cpp
for (long i = 0; i < N; ++i)
    t_accumulator += data[i];          // ⚠️ TLS address computed every iteration

long acc = t_accumulator;              // ✅ hoist: one TLS access
for (long i = 0; i < N; ++i) acc += data[i];
t_accumulator = acc;
```

The compiler *sometimes* hoists it, but not always (especially across function
calls or with `-O1`). **In a hot loop, bind a reference/local once.**

`constinit thread_local T x = ...;` (C++20) — removes the per-access "initialized
yet?" guard (the value is baked, no runtime init).

---

## Destruction timing

- `thread_local` objects are destroyed when their thread **exits** (in reverse
  order of construction), *before* namespace-scope `static` objects are destroyed.
- The **main thread's** `thread_local`s are destroyed at program exit, around the
  same time as `static`s — ordering vs other `static`s is a fiasco risk (folder 25
  file 04).
- A `thread_local` with a **heavy constructor** → each thread pays that cost on
  first use. A pool of 64 threads each constructing a big `thread_local` = 64×
  that cost.
- A `thread_local` destructor that touches a global that's already been destroyed
  → UB (destruction-order fiasco).

---

## `thread_local` vs the alternatives

| Approach | Sharing | Cost | Use |
|---|---|---|---|
| **`thread_local`** | none (per-thread) | TLS access (+ first-use guard) | per-thread RNG / arena / scratch / stats |
| Pass a `Context&` parameter | none (explicit) | a register / pointer | when the "current context" is a real object you already have |
| `std::atomic` shared | shared, lock-free | atomic RMW + cache bounce | a single shared scalar |
| `mutex` + shared | shared | lock (contended = syscall) | compound shared state |
| Per-thread array indexed by thread id | none | one indexed load (no TLS machinery) | when you control the thread set and want explicit control |

For the hot path, **passing a `Context&`** (an arena, buffers, RNG bundled in one
object handed to the thread at startup) is often cleaner and faster than
`thread_local` — no TLS access, no init guard, explicit lifetime.

---

## Andar kya hota hai

- The linker reserves a TLS template (`.tdata` for initialized, `.tbss` for
  zero-init). Each thread, on creation, gets a copy (the "TLS block"), pointed to
  by a register (`fs` base on x86-64 Linux, `tpidr_el0` on AArch64).
- **initial-exec**: the offset into the block is known at link time → `fs:[K]`.
  Only works for TLS in the main executable / libraries loaded at startup.
- **general-dynamic**: for TLS in a `dlopen`'d `.so`, the offset isn't known → a
  `__tls_get_addr(module_id, offset)` call, which does a table lookup.
- Dynamically-initialized `thread_local` → a guard variable per (thread, object);
  first access on a thread runs the ctor and registers the dtor with the
  thread-exit cleanup list.

---

## > **HFT relevance**
> - **Per-thread RNG** — `thread_local std::mt19937_64` seeded distinctly per
>   thread. Never a shared RNG behind a lock (contention + a serialization point),
>   never `rand()` (global state, not thread-safe, modulo bias).
> - **Per-thread arena / bump allocator** — each thread allocates from its own
>   region → no lock, no false sharing on allocator metadata (folder 36).
> - **Per-thread scratch buffers** — decode/encode temporaries reused across
>   messages without re-allocating.
> - **But on the very hottest inner loops, prefer an explicit `Context&`** handed
>   to the thread at startup — no TLS addressing, no first-use guard, lifetime is
>   obvious. `thread_local` for "convenient per-thread state accessed occasionally."
> - **`constinit thread_local`** where the type allows — drops the per-access init
>   check.
> - **Cache the TLS address in hot loops** (`auto& s = t_scratch;`) — measured a
>   real difference in tight loops.

---

## Hands-on

```bash
./build.ps1 26-CONCURRENCY/examples/01_first_thread.cpp   # (uses thread ids)
```

- `thread_local int t_n;` incremented in a loop of 1e8 vs `int local = t_n; for
  (...) ++local; t_n = local;` — time both at `-O2`. See the hoist difference (or
  lack of, at `-O1`).
- `g++ -O2 -S` a function using a `thread_local` — find `%fs:` (initial-exec) or a
  `__tls_get_addr` call.
- A `thread_local std::vector<int>` with a non-trivial ctor across 16 pool threads
  — measure first-use cost per thread.

---

## ⚠️ Traps

### Trap 1 — `thread_local` access in a hot loop
Each access computes a TLS address (+ maybe an init guard). Hoist: bind a local /
reference once outside the loop.

### Trap 2 — heavy `thread_local` constructor
Every thread pays it on first use. Keep `thread_local` objects cheap, or
`constinit`, or lazy-init only what's needed.

### Trap 3 — `thread_local` in a `dlopen`'d plugin
`__tls_get_addr` per access (general-dynamic model) — a function call. Much slower
than a variable in the main binary.

### Trap 4 — `thread_local` destructor touching a global
Destruction order (thread exit, then statics) can leave the global already
destroyed → fiasco / UB. Keep `thread_local` dtors dependency-free.

### Trap 5 — expecting `thread_local` to be shared "within a thread group"
It's strictly per-thread. Two threads = two instances, period.

### Trap 6 — `thread_local` on a huge object × many threads
`thread_local std::array<std::byte, 1<<20>` × 64 threads = 64 MB, all reserved.
Budget it.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`thread_local` access ≈ a normal variable load" | It's a TLS addressing step (+ init guard); can be a function call in a `.so` |
| "`thread_local` needs a lock if two threads use the name" | No — each thread has its own; zero sharing, zero race |
| "`thread_local` is initialized at program start" | Per-thread, first-use (or thread start); `constinit` for compile-time init |
| "the compiler always hoists TLS access out of loops" | Not reliably — hoist it yourself in hot code |
| "`thread_local` is always the fastest per-thread option" | A `Context&` handed in at startup avoids TLS machinery entirely |
| "`thread_local` objects are cheap to have" | Big ones × many threads = big reserved memory; heavy ctors × threads |

---

## Exercises

1. **Race-free?** `thread_local long t_sum = 0;` incremented by many threads in
   parallel, then a main thread reads each thread's `t_sum`. Is the increment a
   race? Is the read?

   <details><summary>Answer</summary>

   The increments are **not** a race — each thread has its own `t_sum`. The read
   *is* a race if it happens while the threads are still running (main reads
   thread A's `t_sum` while A writes it). To read them, join the threads first, or
   have each thread publish its final value to a shared slot with proper
   synchronization.
   </details>

2. **Hoist:** rewrite for the hot loop:
   ```cpp
   thread_local Arena t_arena;
   for (const auto& msg : batch) t_arena.alloc(msg.size);
   ```

   <details><summary>Answer</summary>

   `Arena& a = t_arena; for (const auto& msg : batch) a.alloc(msg.size);` — one
   TLS access, then a plain reference in the loop. (Even better: pass the arena in
   as a parameter so there's no TLS at all.)
   </details>

3. **RNG:** why `thread_local std::mt19937_64` per thread instead of one shared
   RNG behind a mutex?

   <details><summary>Answer</summary>

   A shared RNG serializes every `operator()` on the mutex (contention +
   cache-line bouncing on the state) — it becomes a scaling bottleneck and a
   latency spike. Per-thread RNGs have zero contention; just seed them distinctly
   (e.g. `seed_seq` from a base seed + thread index) so the streams don't overlap.
   </details>

4. **Plugin cost:** a `thread_local Config t_cfg;` lives in a `.so` loaded via
   `dlopen`. Why might accessing `t_cfg` be surprisingly slow?

   <details><summary>Answer</summary>

   TLS in a dynamically-loaded module uses the general-dynamic model → each access
   is a `__tls_get_addr(module, offset)` call (a table lookup), not a simple
   `%fs:[K]`. In a hot path that's a real cost. Options: move the TLS into the
   main binary, pass the config in explicitly, or use initial-exec TLS (`-ftls-
   model=initial-exec`) if you can guarantee the module is loaded at startup.
   </details>

5. **`Context&` vs `thread_local`:** design the per-thread state for a decode
   worker (arena, 2 scratch buffers, an RNG). Which approach and why?

   <details><summary>Answer</summary>

   Bundle them into a `struct WorkerCtx { Arena arena; std::vector<std::byte> buf1,
   buf2; std::mt19937_64 rng; };`, construct one per worker at startup, and pass
   `WorkerCtx&` into the decode function. No TLS addressing, no first-use guards,
   explicit lifetime, easy to test (just pass a `WorkerCtx`). `thread_local` is
   fine if the state is accessed from many scattered functions where threading a
   parameter is awkward — but for a hot, well-structured decode path, the explicit
   context wins.
   </details>

---

## Interview questions

1. `thread_local` — kya, lifetime, race ke saath kya rishta (none)?
2. `thread_local` access ki cost — initial-exec vs general-dynamic model.
3. Hot loop mein `thread_local` — kya karo (hoist)?
4. `constinit thread_local` kya bachata (per-access init guard)?
5. `thread_local` destruction timing — fiasco risk?
6. Per-thread RNG kyun (vs shared + mutex, vs `rand()`)?
7. `thread_local` vs passing a `Context&` — hot path pe kaunsa, kyun?

---

## Next
→ [`16-false-sharing-intro.md`](16-false-sharing-intro.md)
