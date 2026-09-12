# 01 — Data race: the formal definition

## Prerequisites
- `26-CONCURRENCY` (poora — races, mutexes, `std::thread`)
- `25-OBJECT-MODEL` file 15 (UB catalog — C1)
- [`examples/01_atomic_counter.cpp`](examples/01_atomic_counter.cpp)

## Yeh topic abhi kyun
Folder 26 mein "data race = UB" kaha tha. Ab **exact** definition — kyunki poore
folder ke rules (atomics, memory ordering, lock-free) is ek definition ke *around*
bane hain. Iske teen precise components hain, aur teenon zaroori hain data race
hone ke liye.

---

## The definition

Do memory accesses ek **data race** banate hain agar **sab** yeh true hon:

1. Woh **same memory location** ko access karte hain, aur
2. **Kam se kam ek** access ek **write** hai (store / modify), aur
3. **Neither happens-before the other** — koi synchronization unhe order nahi
   karta, aur
4. Woh dono **atomic nahi** hain (ya kam se kam ek non-atomic hai), aur
5. Woh **alag threads** se hain (ek hi thread ke andar "race" nahi — sequencing
   rules apply, folder 25).

**Agar ek data race hai → poore program ka behaviour undefined hai.** Sirf "us ek
access ka result galat" nahi — *the entire program*.

```cpp
long counter = 0;              // shared, NON-atomic
// Thread A                    // Thread B
++counter;                     ++counter;
// same location ✓  ≥1 write ✓  no sync ✓  non-atomic ✓  different threads ✓
//                                              => DATA RACE => UB
```

`examples/01`: 8 threads × 2M non-atomic `++` → final = **~3M instead of 16M**
(~80% lost), different every run — and formally, undefined.

---

## Kya data race **nahi** hai

Har condition todne se race khatam:

| Fix | Kaunsi condition break hoti |
|---|---|
| Sirf reads (no write) — do threads ek `const` object padhte | (2) — no write |
| `std::atomic<T>` — dono accesses atomic | (4) — atomic accesses conflict allowed (defined) |
| `std::mutex` — critical section serialize karta | (3) — mutex lock/unlock ek happens-before edge banata |
| Per-thread data — koi sharing nahi | (1) — different locations |
| Ek hi thread — dono accesses ek thread se | (5) — sequencing, not race |
| Thread A likhta, `join()`, phir B padhta | (3) — `join()` = happens-before edge |

**Atomic operations conflicting hone pe bhi data race NAHI hote** — woh *defined*
behaviour hai (though the *outcome* may still be order-dependent = a logical race
condition — folder 26 file 05). Yeh distinction poore folder ka basis hai:
**atomics let you have conflicting accesses without UB.**

---

## "Same memory location"

- Ek `int`, ek `double`, ek pointer — ek location.
- Ek `struct`'s do alag members — **do alag locations** (do threads jo alag members
  likhein, koi race nahi — par false sharing ho sakta, folder 26 file 16).
- Ek bit-field aur uska padoswala bit-field jo same "allocation unit" share karein —
  **same location** (surprising! do threads adjacent bit-fields likhein → race).
- `a[0]` aur `a[1]` — alag locations.

```cpp
struct S { int a; int b; };
S s;
// Thread A: s.a = 1;   Thread B: s.b = 2;   // NOT a race (different locations)

struct B { unsigned x : 4; unsigned y : 4; };  // x, y may share one byte
B bf;
// Thread A: bf.x = 1;  Thread B: bf.y = 2;   // ⚠️ IS a race (same allocation unit)
```

---

## "Happens-before" (preview — file 10)

Two evaluations are ordered by **happens-before** if:
- They're in the same thread and one is **sequenced-before** the other (`;`,
  `,`, `&&`, function-call boundaries — folder 25), OR
- One **synchronizes-with** the other (a release operation and the acquire that
  reads its value — file 08; `mutex::unlock` and the next `mutex::lock`;
  `thread` creation; `thread::join`; ...), OR
- Transitively via a chain of the above.

If **neither** of a conflicting pair happens-before the other → data race.

The whole point of mutexes and release/acquire atomics is to **create
happens-before edges** so your conflicting accesses aren't a race.

---

## Why "the whole program is UB", not just that access

The compiler and CPU assume **race-free** code. Under that assumption they:
- Cache a shared value in a register across a loop (`while (!done) {}` with a
  plain `bool done` → reads `done` once → infinite loop even after another thread
  sets it — folder 26 file 05).
- Reorder, coalesce, or eliminate memory operations. At `-O2` a plain
  `for (k...) ++counter;` on a non-atomic shared `long` gets coalesced into
  `counter += N;` per thread — the race all but "disappears" and you get a
  plausible-but-wrong answer. (`examples/01` marks `counter` `volatile` *only* to
  defeat that coalescing so the lost updates stay visible — `volatile` is not
  atomic and fixes nothing; drop it and `-O2` hides the bug.)
- Assume a load and a later store to "unrelated" memory don't interfere.

A data race breaks these assumptions **globally** — the standard doesn't try to
bound the damage, so the behaviour of the entire program is undefined.

---

## Detecting data races

| Tool | How |
|---|---|
| **ThreadSanitizer** (`-fsanitize=thread`) | instruments every memory access + every sync op; reports the two racing accesses + both stacks + the sync (or lack of). **The tool.** (Linux/macOS; not MinGW) |
| **Helgrind / DRD** (Valgrind) | dynamic, slower, no recompile |
| **Stress + invariant asserts** | run the concurrent path millions of times; non-deterministic failures = a race (crude — `examples/01`, folder 26 `02`) |
| **Model checkers** (CDSChecker, GenMC, herd7) | explore all interleavings of a small kernel exhaustively |
| **Code review** | "what shared mutable state, and what happens-before edge protects each access?" |

`-O0` **hides** most race effects (fewer optimizations built on the race-free
assumption). Test at `-O2`+ under TSan.

---

## > **HFT relevance**
> - **The hot path has zero data races by construction** — it shares no mutable
>   state with other threads; data flows over lock-free queues built from
>   `std::atomic` with explicit ordering (folder 28), which are *conflicting but
>   defined*.
> - **Every cross-thread flag/index/pointer is `std::atomic`** with a deliberate
>   memory order — never a plain `bool`/`int`/`T*`. A plain flag is a data race
>   *and* the compiler will cache it.
> - **TSan on the full concurrency suite in CI** — a data race that "passes" at
>   `-O0` on your laptop will miscompile under `-O2 -flto -march=native` on the
>   trading box, at high volume, when you can least afford it.
> - **Publication discipline** — an object built by one thread and handed to
>   another must be published through a release store the reader acquires
>   (file 08); otherwise the reader races on the object's fields.

---

## Hands-on

```bash
./build.ps1 fast 27-ATOMICS-MEMORY-MODEL/examples/01_atomic_counter.cpp
# Linux:
g++ -std=c++20 -O1 -g -fsanitize=thread .../01_atomic_counter.cpp -o ac && ./ac
```

Non-atomic (~3M/16M, different each run) vs `std::atomic<long>` (exact). TSan
names the two `counter = counter + 1` accesses. Then:
- `while (!done) {}` with plain `bool done` vs `std::atomic<bool>` at `-O2` — the
  plain version can hang (cached load).
- Two threads writing `s.a` and `s.b` of a `struct` — TSan: no race. Two threads
  writing adjacent bit-fields — TSan: race.

---

## ⚠️ Traps

### Trap 1 — "a data race just gives a wrong number"
It's **UB for the whole program** — torn reads, cached flags, coalesced loops,
compiler assumptions built on race-freedom.

### Trap 2 — plain `bool`/`int` as a cross-thread flag
```cpp
bool ready = false;              // ⚠️ data race + compiler caches the load
std::atomic<bool> ready{false};  // ✅
```

### Trap 3 — "reads can't race"
A read racing a **write** is a data race. Only reads of *immutable* data are safe
without synchronization.

### Trap 4 — bit-fields treated as independent locations
Adjacent bit-fields in one allocation unit are the *same* location for the race
definition.

### Trap 5 — assuming aligned 64-bit load/store is "atomic enough"
Often true on x86 in practice, but it's still a **data race** (UB) if non-atomic —
the compiler can reorder/cache it. Use `std::atomic`.

### Trap 6 — `-O0` as a correctness test for races
`-O0` suppresses most of the optimizations that expose the UB.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "data race = incorrect result" | Data race = **undefined behaviour** (whole program) |
| "atomic accesses that conflict are a race" | No — conflicting atomic accesses are **defined** (that's the point) |
| "concurrent reads need synchronization" | Only reads of immutable data are fine; read-vs-write is a race |
| "different struct members can't race" | Correct — but adjacent **bit-fields** in one unit can |
| "`volatile` fixes a race" | No — `volatile` is not atomic; each access still races |
| "it didn't crash at `-O0`, so no race" | `-O0` hides it; test `-O2` + TSan |

---

## Exercises

1. **Race or not:** (a) two threads `x.fetch_add(1)` on `std::atomic<int>`;
   (b) two threads `++x` on a plain `int`; (c) one thread writes `g`, then
   `t.join()`, then main reads `g`; (d) two threads read a `const std::string`;
   (e) two threads write `s.a` and `s.b` of `struct S { int a, b; };`.

   <details><summary>Answer</summary>

   (a) not a race (both atomic — defined). (b) race (non-atomic, conflicting).
   (c) not a race (`join()` is a happens-before edge). (d) not a race (no write).
   (e) not a race (different memory locations).
   </details>

2. **Which condition is broken** by each fix: mutex, `std::atomic`, per-thread
   copies, `join()` before read.

   <details><summary>Answer</summary>

   Mutex → "neither happens-before" (lock/unlock creates the edge). `std::atomic`
   → "both non-atomic" (atomic conflicts are defined). Per-thread copies → "same
   memory location". `join()` → "neither happens-before" (join is an edge).
   </details>

3. **Why the whole program:** `while (!done) {}` with plain `bool done`, another
   thread does `done = true;`. Explain the possible infinite loop.

   <details><summary>Answer</summary>

   The compiler assumes race-free code → nothing in the loop modifies `done` →
   it may hoist the load out, reading `done` once into a register before the
   loop. The loop then spins on a stale `false` forever. `std::atomic<bool>`
   (with at least acquire) forces a real reload each iteration.
   </details>

4. **`-O2` coalescing:** if `examples/01` dropped the `volatile` on `counter`,
   running the non-atomic loop at `-O2` would often give exactly 16000000. Why is
   that *worse*, pedagogically?

   <details><summary>Answer</summary>

   At `-O2` GCC coalesces `for (k...) ++counter;` into `counter += kIters;` per
   thread — 8 racing adds instead of 16M, which often lands on the right total.
   It's "worse" because the bug is now invisible: the code is still UB, but it
   *looks* correct. `volatile` (which the example keeps — so its non-atomic run
   prints ~3M), or `-O1`, or TSan, makes the race visible again.
   </details>

5. **Formal check:** `std::atomic<int> flag{0}; int data = 0;` — producer:
   `data = 42; flag.store(1, relaxed);` consumer: `while (!flag.load(relaxed)); r
   = data;`. Is there a data race on `data`?

   <details><summary>Answer</summary>

   Yes. `flag` is atomic (no race on `flag`), but `relaxed` creates **no**
   happens-before edge, so the producer's write to `data` and the consumer's read
   of `data` are conflicting, non-atomic, and unordered → data race → UB. Fix:
   `flag.store(1, release)` / `flag.load(acquire)` (file 08).
   </details>

---

## Interview questions

1. Data race ki formal definition — teen (paanch) required conditions.
2. Data race UB hai "poore program ka" — kyun, sirf us access ka nahi?
3. Conflicting atomic accesses — race hain? Kyun / kyun nahi?
4. "Same memory location" — struct members, bit-fields, array elements.
5. Happens-before — kaunse edges (sequenced-before, synchronizes-with, thread ops)?
6. `while (!done)` with plain `bool` — infinite loop kaise?
7. `-O0` race ke liye valid test kyun nahi?

---

## Next
→ [`02-atomicity.md`](02-atomicity.md)
