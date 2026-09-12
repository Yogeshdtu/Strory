# 10 — Data-oriented design (DOD)

## Prerequisites
- `09-aos-vs-soa-deep.md`
- `16-OOP/` (jise DOD challenge karta hai)
- `31-CPU-ARCHITECTURE/07-branch-prediction.md` (virtual call = indirect branch)

## Yeh topic abhi kyun
AoS/SoA ek specific technique thi. DOD woh **soch** hai jismein se woh
technique nikalti hai: *pehle socho data kya hai, memory mein kaise pada hai,
aur usse kis order mein transform karna hai — phir code likho.* Yeh OOP ke
"pehle objects aur unke behaviours model karo" se ulta hai. Game engine aur
HFT dono industries yahi karti hain, kyunki dono ko predictable low latency
chahiye.

---

## Ek line mein

> **"The purpose of the program is to transform data. If you don't understand
> the data, you don't understand the problem."** — Mike Acton (CppCon 2014)

Program input bytes ko output bytes mein badalta hai. Sab kuch — cache misses,
branch mispredicts, SIMD utilization — is transformation ke shape se decide
hota. DOD kehta hai: us shape ko **design** karo, use OOP abstraction ke
neeche chhupne mat do.

---

## OOP layout jo hurt karta hai

```cpp
struct Entity {                 // "an entity has a position, health, AI..."
    virtual void update() = 0;
    Vec3 pos; float health;
    AIController* ai;           // pointer to more scattered state
    Mesh* mesh; ...
};
std::vector<Entity*> entities;  // pointers to heap-scattered objects

for (Entity* e : entities) e->update();   // <-- yeh loop
```

Is loop ka har iteration:
1. `entities[i]` load → `Entity*` (pointer chase #1, scattered heap).
2. `e->update` → **vtable load** (pointer chase #2) → indirect call.
3. Indirect call = **indirect branch** — BTB predict karta, mixed types →
   mispredict (`31/07`), + I-cache jumps to different `update` bodies.
4. `update()` body `e->ai`, `e->mesh` deref → more scattered misses.
5. Har object ke **saare** fields line mein aate chahe `update` sirf 2 use
   kare.

Result: ~1 cache miss + ~1 branch mispredict + wasted line bandwidth **per
entity**, no vectorization possible. 10k entities → 10k misses + 10k
mispredicts every frame.

---

## DOD layout

```cpp
// Ek array per component. Systems batch-process homogeneous data.
struct World {
    std::vector<Vec3>  positions;   // SoA
    std::vector<Vec3>  velocities;
    std::vector<float> health;
    // ... sorted / grouped by type where behaviour differs
};

// "movement system": ek tight, vectorizable loop, zero indirection
void integrate(World& w, float dt) {
    for (size_t i = 0; i < w.positions.size(); ++i)
        w.positions[i] += w.velocities[i] * dt;
}

// "damage system": another tight loop over just health + incoming damage
```

- **No pointer chase** — contiguous arrays.
- **No virtual call** — behaviour differences handled by **grouping** data
  by type/state and running a different loop per group ("existence-based
  processing").
- **Vectorizable** — dense `Vec3` arrays, compiler SIMDs the integrate loop.
- **Only the needed fields** — movement touches `positions` + `velocities`,
  not `mesh`/`ai`.

---

## Core DOD principles

1. **Data is the interface.** Design the arrays and their layout first. The
   code is just the transform between input arrays and output arrays.
2. **Think in bulk.** You never have "one entity" — you have 10,000. Design
   for the loop, not the individual.
3. **Separate hot from cold.** Fields touched every frame ≠ fields touched
   on spawn/death. Different arrays.
4. **Group by what you do, not what things are.** All the things that need
   physics → one set of arrays. Whether they're "players" or "rocks" is
   irrelevant to the physics loop.
5. **Existence-based processing.** If an entity is in the `has_velocity`
   array, it has velocity — no `if (has_velocity)` check, no null pointer.
   Being in the array *is* the flag.
6. **Prefer indices / handles over pointers.** 4 bytes, stable across
   reallocation, no chase (lesson 08).
7. **Make the common case linear.** The 90% path should be a sequential scan.

---

## Where OOP is still fine

DOD is about **hot loops over many items**. For:
- Cold code (startup, config, once-per-second admin) — clarity wins, use
  objects.
- Genuinely singular, complex state machines (the exchange connection, the
  session) — an object is the right model.
- Code that isn't on any measured critical path — don't contort it.

DOD isn't "no classes ever." It's "the 5% of code that runs 95% of the time
gets designed around its data."

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `std::vector<Base*>` polymorphic loop on the hot path
Pointer chase + vtable load + indirect branch + no SIMD, per element. Replace
with type-grouped arrays + a loop per type.

### Trap 2 — DOD-ifying cold code
Rewriting a once-per-startup config parser as SoA — zero benefit, worse
readability. DOD where the profiler points.

### Trap 3 — "SoA but still one giant struct of vectors touched randomly"
If your access is random-per-record over all fields, SoA hurts (lesson 09
Case C). DOD ≠ blindly SoA — match layout to the actual transform.

### Trap 4 — indices without generation counters
Handle = raw index → element freed, index reused → stale handle points at a
different object (ABA). Use `{index : 24, generation : 8}` handles; validate
generation on deref.

### Trap 5 — losing the data dependencies
Splitting into per-component arrays can hide that system B needs system A's
output. Order the systems explicitly; document the data flow (A writes
`positions`, B reads `positions`).

### Trap 6 — premature DOD (over-engineering)
Full ECS framework for a 200-entity tool. The cost (complexity, indirection
of the ECS itself) can exceed the cache benefit at small scale. Measure first.

---

## > **HFT relevance**

> - **The tick loop is a data transform.** Input: a batch of market-data
>   messages. Output: updated book arrays + a list of signals. Design those
>   arrays; the handler code follows.
> - **No `virtual` on the hot path.** Strategy dispatch: instead of
>   `std::vector<Strategy*>` with `virtual on_tick()`, group instruments by
>   strategy-id and run each strategy's loop over its slice. Or a
>   `switch` on a small enum (jump table, one predictable indirect).
> - **SoA everywhere in the book/signal engine.** `price[]`, `qty[]`,
>   `feature0[]`… column-major, vectorized scans.
> - **Handles, not pointers.** Orders, instruments, levels — `uint32` indices
>   into pools, with generation counters for safety.
> - **Existence-based:** an order is "live" because it's in the live-orders
>   array, not because of a `bool live` you must check.
> - **This is why HFT code looks "un-OOP"** — flat arrays, free functions,
>   `switch` statements. It's shaped around the data path.

---

## Hands-on

```bash
# DOD vs OOP-layout ka core: contiguous SoA vs scattered pointer+vtable
./build.ps1 fast 32-CACHE-MEMORY-PERFORMANCE/examples/05_aos_vs_soa.cpp   # layout
./build.ps1 fast 31-CPU-ARCHITECTURE/examples/03_branch_prediction.cpp    # indirect branch cost

# watch: CppCon 2014 "Data-Oriented Design and C++" (Mike Acton)
#        CppCon 2018 "OOP Is Dead, Long Live Data-oriented Design" (Stoyan Nikolov)
```

Experiment: 10k "shapes" with a `virtual area()` in a `vector<Shape*>` vs
a SoA `{ vector<uint8_t> type; vector<float> a, b; }` + a `switch(type[i])`
loop. Time both. The SoA+switch version: no pointer chase, no vtable, jump
table predicts well when types are grouped, and it can even vectorize per
type.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "DOD = no classes" | DOD = hot loops designed around data; cold code stays OOP |
| "virtual calls are cheap" | pointer chase + vtable load + indirect branch + no SIMD |
| "DOD = always SoA" | match layout to the transform (random-whole-record → AoS) |
| "abstraction first, optimize later" | for the 5% hot code, data layout *is* the design |
| "indices are just slower pointers" | 4 B, realloc-stable, no chase, + generation safety |
| "ECS everywhere" | at small scale the framework overhead can exceed the gain |

---

## Exercises

1. Ek `std::vector<std::unique_ptr<Order>>` hai, har frame aap sabpe
   `o->revalue(market)` (virtual) call karte ho. Cache/branch cost list karo,
   phir DOD redesign.

   <details><summary>Answer</summary>

   Per order: (1) load `unique_ptr` → `Order*` (contiguous, ok), (2) deref
   `Order*` → heap-scattered object, ~1 miss, (3) load vtable ptr, (4)
   indirect call `revalue` → BTB predict; if multiple `Order` subtypes
   interleaved → mispredicts + I-cache thrash between bodies, (5) `revalue`
   touches maybe 3 of ~15 fields but the whole object's lines came in.
   **Redesign**: SoA pools — `struct Orders { vector<double> px, qty, mult;
   vector<uint8_t> kind; ... };`. Group by `kind`. Per kind, a tight loop:
   `for (i in kind_slice) value[i] = px[i]*qty[i]*mult[i]*factor(market);` —
   contiguous, vectorizable, one branchless expression, no vtable. ~1 miss
   per 16 orders instead of ~1 per order, zero mispredicts.
   </details>

2. "Existence-based processing" — ek concrete example do (game ya HFT) aur
   batao yeh kaunsa branch/check khatm karta.

   <details><summary>Answer</summary>

   HFT: instead of `struct Instrument { bool has_open_orders; ... }` and
   `for (i) if (inst[i].has_open_orders) manage(inst[i]);`, keep a separate
   `std::vector<uint32_t> instruments_with_open_orders;` and
   `for (uint32_t idx : instruments_with_open_orders) manage(inst[idx]);`.
   Being **in the array** means "has open orders" — no `bool` field, no
   `if` per instrument (which over all instruments is a data-dependent branch
   → mispredicts + a wasted line load for the ones that don't qualify). The
   loop now visits only the ~50 relevant instruments, contiguously. Adding/
   removing from the array happens on order-place/fill (rare) instead of
   checking every instrument every tick (hot).
   </details>

3. Aapke pas ek handle type `uint32_t` = raw pool index. Ek subtle bug batao
   jo iska generation-counter version fix karta.

   <details><summary>Answer</summary>

   Order 42 lives at pool index 100. You store `handle = 100` in a pending-
   fills list. Order 42 gets cancelled → pool slot 100 freed → new order 77
   allocated, reuses slot 100. Now your stale `handle = 100` in the pending-
   fills list dereferences to order **77** — you act on the wrong order
   (cancel it, apply a fill to it). Classic ABA. Fix: `handle = {index:24,
   gen:8}`; the pool slot stores its current generation; on `deref(handle)`
   check `pool[handle.index].gen == handle.gen`, else return null (handle is
   stale). Slot 100's gen bumped from 5→6 when reallocated → old handle
   (gen 5) now fails validation.
   </details>

---

## Interview questions

1. DOD ki ek-line philosophy aur woh OOP-first se kaise ulta.
2. `vector<Base*>` + `virtual` hot loop — har element ki costs (misses +
   branch + SIMD).
3. "Group by what you do, not what things are" — ek example.
4. Existence-based processing — kya branch/check woh khatm karta.
5. Handles vs pointers — 3 benefits, aur generation counter kyun.
6. DOD kahan **nahi** apply karna (cold code, singular state).
7. HFT hot path "un-OOP" kyun dikhta hai — 3 concrete choices.

---

## Next
→ [`11-tlb-and-huge-pages.md`](11-tlb-and-huge-pages.md)
