# 10 — Happens-before, synchronizes-with, sequenced-before

## Prerequisites
- `08-acquire-release.md`, `09-seq-cst.md`
- `25-OBJECT-MODEL` file 03 (sequencing within an expression)

## Yeh topic abhi kyun
Yeh woh **formal vocabulary** hai jisme C++ memory model define hota hai. Data
race ki definition (file 01) "neither happens-before the other" pe tiki hai.
Ab exactly samajho ki happens-before edges kahaan se aate hain — taaki aap kisi
bhi concurrent code ke baare mein *prove* kar sako ki race-free hai.

---

## Teen relations

### 1. Sequenced-before — *ek thread ke andar*
Ek single thread mein evaluations ka partial order:
- `a; b;` → `a` sequenced-before `b`.
- Full-expression boundaries, `,` operator, `&&` `||` `?:` short-circuit, function
  call: arguments sequenced-before the call body (folder 25 file 03).
- `i++ + i++` → **unsequenced** → UB (not a *race*, a sequencing violation).

Yeh purely intra-thread hai. Koi atomics nahi chahiye.

### 2. Synchronizes-with — *do threads ke beech*
Ek inter-thread edge, banta hai jab:
- A **release** operation (store/fence) **synchronizes-with** an **acquire**
  operation (load/fence) that **reads the value** that release wrote (or a value
  later in its release sequence — file 08).
- `std::thread` constructor (parent) synchronizes-with the start of the new
  thread's function.
- `std::thread::join()` — the thread's completion synchronizes-with the `join()`
  return.
- `std::mutex::unlock()` synchronizes-with the next `lock()` that obtains it.
- `std::promise::set_value()` synchronizes-with the `std::future::get()` that
  retrieves it.
- `std::atomic_flag` / `atomic::notify` ↔ `wait` (C++20), `latch::count_down` ↔
  `wait`, `barrier` arrive ↔ release phase.

### 3. Happens-before — the master relation
`A` **happens-before** `B` if any of:
- `A` is sequenced-before `B` (same thread), OR
- `A` synchronizes-with `B` (cross-thread), OR
- `A` happens-before `X` and `X` happens-before `B` (**transitive** — chains
  through both kinds of edge).

(Slight simplification — the standard also has "inter-thread happens-before" and
handles `consume`/dependency-ordered-before, which we ignore since `consume` is
dead.)

---

## The two things happens-before decides

### 1. Is there a data race?
Two conflicting accesses (same location, ≥1 write, ≥1 non-atomic, different
threads) with **neither happening-before the other** → data race → UB (file 01).
Add an edge (mutex, release/acquire, join) → ordered → no race.

### 2. What value does a read see?
A non-atomic read must have exactly one "visible side effect" — a write `W` that
happens-before it, with no other write happening-before the read but after `W`.
If happens-before doesn't pin it down → race. For atomics, the rules are looser
(modification order + the ordering constraints of files 07–09).

---

## Worked example — the publication chain

```cpp
int data = 0;                                   // plain
std::atomic<int> flag{0};

// Thread A
data = 99;                                      // A1
flag.store(1, std::memory_order_release);       // A2

// Thread B
while (flag.load(std::memory_order_acquire) == 0) {}   // B1
int r = data;                                   // B2
```

Edges:
- `A1` sequenced-before `A2` (same thread, `;`).
- `A2` synchronizes-with `B1` (release store read by acquire load — the iteration
  that sees `1`).
- `B1` sequenced-before `B2` (same thread).
- **Transitively: `A1` happens-before `B2`.**

⇒ `data = 99` (A1) happens-before `r = data` (B2). No data race. `r == 99`
guaranteed. Break any single link (make A2 or B1 `relaxed`) → no chain → race.

---

## Transitivity carries *all* prior writes

```cpp
// Thread A
x = 1;  y = 2;  z = 3;                       // A1 A2 A3
flag.store(1, release);                      // A4
// Thread B
while (!flag.load(acquire)) {}               // B1
use(x, y, z);                                // B2  — sees 1, 2, 3
```
The single release/acquire edge on `flag` orders **everything** sequenced-before
`A4` (that's `x`, `y`, `z`) before **everything** sequenced-after `B1`. You don't
need an atomic per variable — one publication flag covers the whole payload.

---

## Non-edges (things that do NOT synchronize)

| Not an edge |
|---|
| Two `relaxed` operations (even on the same variable) — coherence only, no happens-before |
| A `release` store and an `acquire` load of a **different** variable |
| An `acquire` load that reads a value written *before* the `release` (didn't read *its* value) |
| Plain (non-atomic) reads/writes across threads |
| `std::this_thread::sleep_for` / wall-clock time ("thread B started later" is not an edge) |
| Creating a thread object but the writes happen *after* the constructor |
| A `detach()`ed thread's later work vs the detaching thread |

---

## > **HFT relevance**
> - **Every cross-thread hand-off is one explicit edge you can name** — "the
>   consumer acquires `write_idx`, which the producer released after filling the
>   slot." If you can't name the edge, it's a race.
> - **One release/acquire publishes a whole struct** — build the `Order` /
>   `MarketSnapshot` fully, then one `release` store of the index/pointer. Readers
>   `acquire` once. No per-field atomics.
> - **`join()` and thread-start are edges** — startup config written before
>   spawning workers is visible without extra atomics; end-of-run stats read after
>   `join()` are visible. Use them; don't add redundant fences.
> - **Code review question**: for each shared mutable byte, "which
>   happens-before edge orders this write against that read?" No answer → fix
>   before it ships.

---

## Hands-on

```bash
./build.ps1 fast 27-ATOMICS-MEMORY-MODEL/examples/04_acquire_release.cpp
```

Trace the edges in Demo 2 (SPSC): producer `buf[w%N] = msg` (seq-before)
`widx.store(w+1, release)` (synchronizes-with) consumer `widx.load(acquire)`
(seq-before) `read buf[r%N]`. Then:
- Draw the happens-before graph for `examples/01`'s **non-atomic** counter: there
  are **no** cross-thread edges → every `++` pair races.
- Add `mtx.lock()/unlock()` around the `++` → each unlock synchronizes-with the
  next lock → a total order of critical sections → no race.

---

## ⚠️ Traps

### Trap 1 — "thread B ran later, so it sees A's writes"
Wall-clock ordering is **not** happens-before. Only edges (release/acquire, mutex,
join, thread-start...) count.

### Trap 2 — expecting an edge from a `relaxed` pair
`relaxed`↔`relaxed` gives coherence on that one variable, **not** happens-before.
The payload it "gates" still races.

### Trap 3 — release and acquire on different variables
No edge. The acquire must read the value the release stored, **same variable**.

### Trap 4 — acquire load that missed the release's value
If your acquire load reads the *initial* value (not the released one), no
synchronizes-with yet — keep spinning until it reads the published value.

### Trap 5 — forgetting transitivity works *through* mutexes too
`unlock` sync-with next `lock` means data written under the lock by thread 1 is
visible to thread 2 under the lock — a chain, same as release/acquire.

### Trap 6 — `i = i++ + 1` "race"
That's an **unsequenced** (intra-thread) UB, not a data race. Different rule,
same "don't".

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "happens-before = happens earlier in time" | It's a formal relation from edges; time order ≠ happens-before |
| "sequenced-before is about threads" | It's *within one thread*; cross-thread is synchronizes-with |
| "any release pairs with any acquire" | Only when the acquire reads that release's value (same variable, its release sequence) |
| "need an atomic per shared variable" | One release/acquire edge orders every write sequenced-before it |
| "mutexes don't create happens-before" | `unlock` synchronizes-with the next `lock` — a full edge |
| "join isn't synchronization" | Thread completion synchronizes-with `join()` return |

---

## Exercises

1. **Name the edge:** (a) main writes `cfg`, spawns worker; (b) worker `fetch_sub`
   on a refcount hits 0, deletes; (c) producer `release`s index, consumer
   `acquire`s it; (d) main `join()`s a worker then reads its result slot.

   <details><summary>Answer</summary>

   (a) `std::thread` constructor synchronizes-with the worker function start.
   (b) each earlier `fetch_sub`'s release synchronizes-with the final `acq_rel`
   decrement's acquire. (c) release store synchronizes-with the acquire load that
   reads it. (d) worker completion synchronizes-with `join()` return.
   </details>

2. **Race?** `std::atomic<int> f{0}; int d = 0;` A: `d = 5; f.store(1, relaxed);`
   B: `while (f.load(relaxed) == 0) {} int r = d;`

   <details><summary>Answer</summary>

   Race on `d`. `relaxed`↔`relaxed` on `f` gives no happens-before, so `d = 5`
   does not happen-before `r = d` → conflicting non-atomic access unordered → UB.
   Need `release`/`acquire` on `f`.
   </details>

3. **Transitive chain:** A writes `p, q, r` then `release`s `flag`; B `acquire`s
   `flag`, then `release`s `flag2`; C `acquire`s `flag2`. Does C see A's `p,q,r`?

   <details><summary>Answer</summary>

   Yes. A's writes seq-before A's release; A's release sync-with B's acquire; B's
   acquire seq-before B's release; B's release sync-with C's acquire. Transitively
   A's `p,q,r` happen-before C's reads.
   </details>

4. **Not an edge:** give two things people wrongly assume synchronize threads.

   <details><summary>Answer</summary>

   `sleep_for` / elapsed wall-clock time ("B started 1ms later"); a `relaxed`
   store/load pair; a `printf` in each thread; a `volatile` variable; creating a
   `std::thread` object whose relevant writes happen after construction.
   </details>

5. **Value rule:** non-atomic `int v`; write `v = 7` in A happens-before read of
   `v` in B, and no other write to `v` happens-before that read. What does B read?

   <details><summary>Answer</summary>

   `7` — that write is the unique visible side effect (happens-before the read,
   with nothing else in between). This is exactly the guarantee an edge buys you.
   </details>

---

## Interview questions

1. Sequenced-before, synchronizes-with, happens-before — teenon define karo.
2. Synchronizes-with edges ki list — release/acquire ke alawa kaunse (thread, mutex, future)?
3. Happens-before transitive kaise — release/acquire aur mutex dono chains?
4. Data race ki definition mein happens-before ka role (file 01 se link).
5. Ek non-atomic read kaunsi value dekhta hai — happens-before ke terms mein.
6. Wall-clock "baad mein chala" — happens-before kyun nahi?
7. Ek publication flag se poora payload publish — kyun kaafi hai (transitivity)?

---

## Next
→ [`11-fences.md`](11-fences.md)
