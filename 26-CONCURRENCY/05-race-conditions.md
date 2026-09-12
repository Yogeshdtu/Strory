# 05 — Race conditions & data races

## Prerequisites
- `03-std-thread.md`, `25-OBJECT-MODEL` file 15 (UB catalog — C1)
- [`examples/02_race_condition.cpp`](examples/02_race_condition.cpp)

## Yeh topic abhi kyun
Yeh woh problem hai jiske liye baaki poora folder (mutex, atomics, lock-free)
exist karta hai. **Data race = undefined behaviour** — sirf "kabhi-kabhi galat
answer" nahi, poora program undefined. Isse pehchanna aur `examples/02` mein 70%
lost updates dekhna yeh sink-in karta hai.

---

## Do alag (related) terms

| | **Race condition** | **Data race** |
|---|---|---|
| Kya | Program ka result **timing/interleaving pe depend** karta hai (logical bug) | Do threads ek **non-atomic** object ko access karein, ≥1 write, **no synchronization** |
| Verdict | Bug (usually), not necessarily UB | **Undefined behaviour** (C++ standard) |
| Example | "check then act": `if (!exists(f)) create(f)` — someone else creates it in between | `++counter` from 2 threads without a lock |

Har data race ek race condition hai. Har race condition data race nahi (e.g. two
threads racing on a properly-`std::atomic` variable — defined, but the *outcome*
may still be order-dependent = a logical race condition).

Is folder ka focus: **data races**, kyunki woh UB hain.

---

## `++counter` — teen steps

```cpp
long counter = 0;   // shared, non-atomic
// Thread A and B both do:
++counter;
```

Compiler ke liye `++counter` ≈:
```
    mov  rax, [counter]    ; (1) load
    add  rax, 1            ; (2) increment
    mov  [counter], rax    ; (3) store
```

Interleaving:
```
Thread A            Thread B
(1) load 100
                    (1) load 100
(2) 100 + 1 = 101
                    (2) 100 + 1 = 101
(3) store 101
                    (3) store 101       <- overwrites A's store

Result: counter = 101. TWO increments happened, ONE counted. LOST UPDATE.
```

`examples/02`: 8 threads × 200k increments = expected 1,600,000. Actual: **~400k–470k
per run** (70–75% lost), **different every run**.

---

## Why it's UB, not just "wrong number"

The C++ memory model (folder 27) says: if two evaluations conflict (access the
same memory, at least one a write), and neither happens-before the other, and
they're not both atomic → **the behaviour is undefined**.

Practical consequences beyond lost updates:
- **Torn reads/writes** — a non-atomic 64-bit or larger object can be read
  half-updated (part old, part new). A pointer read as garbage → wild deref.
- **Compiler assumes no race** — it can hoist a load out of a loop, cache a value
  in a register, reorder — because "no other thread touches this" is a valid
  assumption for non-atomic data. Your `while (!done) {}` can loop forever if
  `done` is a plain `bool` (the compiler reads it once).
- **`-O2` makes it worse**, not better — more aggressive assumptions.

```cpp
bool done = false;   // NON-atomic
// Thread 1: while (!done) { spin(); }   // ⚠️ compiler may read `done` ONCE -> infinite loop
// Thread 2: done = true;
```

---

## Where races hide

- A shared counter / statistic / "last value" without a lock or atomic.
- A `std::vector` / `std::map` / `std::string` accessed from two threads (STL
  containers are **not** thread-safe for concurrent mutation; concurrent
  *reads* of a const container are fine).
- A shared pointer being reassigned while another thread dereferences it.
- Lazy init: `if (!ptr) ptr = make();` from two threads.
- A flag set by one thread, polled by another, both plain (`bool`/`int`).
- Iterator invalidation across threads (one `push_back`s, another iterates).
- "It's just a `bool`, surely that's atomic" — **not guaranteed** for a plain
  `bool` (use `std::atomic<bool>`).

---

## Detecting races

| Tool | How |
|---|---|
| **ThreadSanitizer** (`-fsanitize=thread`) | Instruments every memory access; reports the two racing accesses + stacks. **The tool.** (Linux/macOS; not MinGW.) |
| **Helgrind / DRD** (Valgrind) | Dynamic race detection, slower, no recompile |
| **Stress + assertions** | Run the concurrent path millions of times with invariant checks; non-deterministic failures = a race (weak, but catches gross ones — `examples/02`) |
| **Code review** | "What shared mutable state does this touch, and what protects it?" for every thread body |

`examples/02` runs the loop 3× and shows a different (wrong) answer each time — a
crude but visible race signature.

---

## Fixes (rest of the folder)

| Fix | When | File |
|---|---|---|
| **Don't share** — per-thread data + combine | the work partitions | 03 example (c), 15 |
| **`std::atomic<T>`** | a single scalar (counter, flag, pointer) | folder 27 |
| **`std::mutex` + lock guard** | a compound invariant, a container, multiple fields | 07, 08 |
| **`std::shared_mutex`** | many readers, rare writers | 10 |
| **Message passing** — SPSC/MPSC lock-free queue | producer/consumer, hot path | folder 28 |
| **Immutability** — publish a new const snapshot, swap a pointer atomically | read-mostly config/book | 27, folder 28 |

---

## > **HFT relevance**
> - **The hot path shares nothing mutable with other threads** — data comes in on
>   a lock-free SPSC queue, results go out on another. No locks, no atomics on the
>   shared-counter pattern, no races by construction (folder 28, 41).
> - **Where sharing is unavoidable** (a config, a symbology table, a risk limit):
>   **immutable snapshot + atomic pointer swap** — readers grab the current
>   `std::shared_ptr` / an `std::atomic<const T*>`, the writer builds a new one and
>   swaps. Readers never block, never see a torn state.
> - **`std::atomic<bool>` for every "stop"/"ready"/"published" flag** — never a
>   plain `bool` (the compiler will cache it).
> - **TSan in CI** on the full concurrency test suite — every race is a bug that
>   *will* eventually miscompile under `-O2 -flto`.
> - A data race in a trading engine = wrong prices / wrong quantities / crashes
>   under load — the worst kind of "passed in dev."

---

## Hands-on

```bash
./build.ps1 26-CONCURRENCY/examples/02_race_condition.cpp        # ~70% lost, different each run
# Linux:
g++ -std=c++20 -O1 -g -fsanitize=thread 26-CONCURRENCY/examples/02_race_condition.cpp -o rc && ./rc
```

TSan (Linux) will point at the exact two `++counter` accesses and their stacks.
Then:
- Change `long counter` to `std::atomic<long> counter` and `++counter` to
  `counter.fetch_add(1)` → correct, every run. (`examples/03` (b).)
- The `while (!done)` spin bug: plain `bool` vs `std::atomic<bool>` at `-O2`.

---

## ⚠️ Traps

### Trap 1 — "a data race just gives a wrong number"
It's **UB** — torn reads, infinite loops from cached flags, compiler assumptions.
Not merely "occasionally off by a bit."

### Trap 2 — plain `bool`/`int` as a cross-thread flag
```cpp
bool ready = false;              // ⚠️ compiler may read once -> spin forever
std::atomic<bool> ready{false};  // ✅
```

### Trap 3 — "reads don't need synchronization"
Concurrent reads of *immutable* data are fine. A read racing with a **write** is a
data race.

### Trap 4 — STL container from two threads
`std::map`, `std::vector`, `std::string` — not safe for concurrent mutation.
Lock it, or give each thread its own.

### Trap 5 — assuming 64-bit aligned load/store is atomic "in practice"
Often true on x86, **not guaranteed** by the language, and the compiler can still
reorder/cache. Use `std::atomic`.

### Trap 6 — `-O0` "works", ship it
`-O0` hides many race effects. `-O2`/`-flto` + real load exposes them.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "data race = wrong result" | Data race = **undefined behaviour** (torn state, cached flags, ...) |
| "`bool` reads/writes are atomic" | Not guaranteed; use `std::atomic<bool>` |
| "concurrent reads are always safe" | Only of immutable data; read vs write = race |
| "STL containers are thread-safe" | Concurrent mutation is a data race |
| "it printed the right number, so no race" | Non-deterministic; may be wrong 1 run in 1000, and it's still UB |
| "`-O0` is a valid test for races" | It hides most of them; test `-O2` + TSan |

---

## Exercises

1. **Race condition or data race (or both):** (a) two threads `++x` on a plain
   `int`, (b) two threads `x.fetch_add(1)` on `std::atomic<int>`, (c)
   `if (map.count(k)==0) map[k]=v;` from two threads, (d) two threads reading a
   `const std::vector`.

   <details><summary>Answer</summary>

   (a) both — data race (UB) and a race condition. (b) race condition only — the
   final value is defined, but if the logic depends on *who* incremented when,
   that's still an ordering bug; no UB. (c) both — non-atomic map mutation (data
   race / UB) *and* check-then-act (race condition). (d) neither — concurrent
   reads of immutable data are fine.
   </details>

2. **Explain the lost update:** in `examples/02`, 8 threads × 200k = 1.6M
   expected, actual ~450k. Roughly what fraction of increments "collided"?

   <details><summary>Answer</summary>

   ~70% were lost — meaning for ~70% of the 1.6M increments, another thread's
   load-add-store interleaved and overwrote the result. High collision because all
   8 threads are hammering the same variable with no other work between iterations.
   </details>

3. **The spin bug:** why can `while (!done) {}` (plain `bool done`) never exit even
   after another thread sets `done = true`?

   <details><summary>Answer</summary>

   `done` is non-atomic and the loop body has no synchronization, so the compiler
   may prove "nothing in this loop changes `done`" and hoist the load out — reading
   `done` once into a register before the loop. It spins on a stale `false`
   forever. `std::atomic<bool>` (with at least `memory_order_relaxed`, really
   acquire) forces a real reload each iteration.
   </details>

4. **Which fix:** (a) a per-request latency sum across 8 worker threads, (b) a
   "shutdown requested" flag, (c) a shared `std::unordered_map<OrderId, Order>`
   updated by 4 threads, (d) a read-mostly config replaced once a minute.

   <details><summary>Answer</summary>

   (a) per-thread partial sums + combine (no sharing) — or `std::atomic<long>` if
   you must. (b) `std::atomic<bool>` / `std::stop_token`. (c) `std::mutex` around
   the map (or shard it, or a concurrent map). (d) immutable snapshot +
   `std::atomic<std::shared_ptr<const Config>>` swap.
   </details>

5. **TSan:** you run the suite under `-fsanitize=thread` and get a report about
   two accesses to `g_last_price`. What's the minimal information the report gives
   you, and what's the fix pattern?

   <details><summary>Answer</summary>

   The two racing accesses (file:line + which is a read/write), the stack traces
   of both threads, and often the allocation site of the object. Fix: identify the
   invariant `g_last_price` is part of and protect *all* access with the same
   mutex, or make it `std::atomic` if it's a lone scalar, or stop sharing it.
   </details>

---

## Interview questions

1. Race condition vs data race — farq, kaunsa UB?
2. `++counter` se 2 threads mein kya galat — teen steps.
3. Data race UB hone ke practical consequences (torn reads, cached flags, reorder).
4. `while (!done)` with plain `bool` — kyun infinite loop ho sakta?
5. STL containers thread-safe hain? Concurrent read vs write.
6. Race detect karne ke tools — TSan kaise kaam karta?
7. Fixes ka spectrum — don't-share / atomic / mutex / message-passing / immutable.

---

## Next
→ [`06-critical-sections.md`](06-critical-sections.md)
