# 12 — Virtual dispatch cost (measured)

## Prerequisites
- [`04-vtable-deep-dive.md`](04-vtable-deep-dive.md), [`11-rtti-and-dynamic-cast.md`](11-rtti-and-dynamic-cast.md)
- Folder 06 file 08 (branch prediction), folder 07 (benchmark hygiene), folder 14 file 08 (tail latency)

## Yeh topic abhi kyun
Ab tak "virtual call slow hai" bola. **Kitna slow?** Aur kyun — sirf indirect
call nahi, balki lost inlining aur branch prediction bhi. Yeh lesson `examples/
07_dispatch_benchmark.cpp` ke **measured numbers** ke saath, aur alternatives
(direct, CRTP, variant, function table) ka comparison — HFT dispatch design ka
core.

---

## Ek virtual call ka anatomy (recap file 04)

```asm
    mov  rax, [rdi]          ; load 1: object -> vtable pointer   (vptr)
    mov  rax, [rax + slot]   ; load 2: vtable -> function pointer (dependent on load 1)
    call rax                 ; indirect call
```

Teen alag costs:
1. **2 dependent loads** — serialized (~4-5 cycles each if L1 hit; a miss = 10s-100s).
2. **Indirect call** — target address ek register mein → CPU BTB (branch target
   buffer) se predict karta. **Data-dependent target** (alag-alag objects, alag
   types) → mispredicts → ~15-20 cycle bubble per miss.
3. **No inlining** — compiler ko target pata nahi → us call ke aar-paar constant
   propagation, CSE, vectorization, loop transforms — sab **ruk** jaate. Yeh
   aksar sabse bada cost hai (call khud sasta ho sakta, par uske kaaran khoi hui
   optimizations mehngi).

---

## Measured — `examples/07_dispatch_benchmark.cpp` (`-O2`, GCC 15.1, x86-64)

Har approach: `sum += shape.area()` over 100,000 mixed shapes, 500 reps,
per-call = total / (N × reps).

```
  virtual  (unique_ptr)         ~23.0 ns/call
  direct   (monomorphic)         ~2.2 ns/call
  CRTP     (static poly)         ~2.2 ns/call
  std::variant + visit          ~16.0 ns/call

  virtual / direct  ratio : ~10x
  variant / direct  ratio : ~7x
  CRTP    / direct  ratio : ~1x
```

Padhna:
- **direct / CRTP** — compiler `area()` ko inline karta → 1-2 FLOPs per call.
  CRTP static dispatch = zero cost (`static_cast<Derived*>(this)->impl()`
  resolves at compile time).
- **`std::variant` + `visit`** — tagged union; `visit` ek jump-table (function
  pointers per alternative) generate karta, ya chhote closed sets mein ek
  `switch` — yahan ~7x direct (arms fully inline nahi hue, par virtual se behtar,
  aur data contiguous — no pointer chase).
- **virtual** — indirect call + no inline + `unique_ptr` = pointer chase (each
  object on the heap, poor locality) → ~10x direct. Aur **data-dependent** — mixed
  types → BTB mispredicts → real tail (not shown in mean; profile `perf stat -e
  branch-misses`).

> Numbers machine/compiler pe vary karenge (~5-15x typical for virtual). Shape
> (direct ≈ CRTP ≪ variant < virtual) reproduce hoga.

---

## Alternatives to virtual dispatch

### 1. CRTP — compile-time polymorphism (file 13)

```cpp
template <class D> struct Shape { double area() const { return static_cast<const D*>(this)->areaImpl(); } };
struct Circle : Shape<Circle> { double r_; double areaImpl() const { return 3.14159 * r_ * r_; } };
```
- **Zero cost** — resolves at compile time, fully inlinable, no vptr.
- **Limit:** no heterogeneous container (`Shape<Circle>` ≠ `Shape<Square>`). Use
  when the type is known at each call site (templated algorithm, strategy).

### 2. `std::variant` + `std::visit` — closed-set runtime polymorphism

```cpp
using AnyShape = std::variant<Circle, Square, Triangle>;
std::vector<AnyShape> shapes;
for (auto& s : shapes) total += std::visit([](const auto& x) { return x.area(); }, s);
```
- **Closed set** (compiler knows all types) → exhaustive, no `nullptr`, contiguous
  storage (good locality — no pointer chase).
- Dispatch = index → jump table. Faster than virtual (better locality, sometimes
  inlinable arms), slower than direct.
- **Limit:** all types must be known up-front; adding a type = recompile
  everything using the variant.

### 3. Function-pointer table / tag dispatch

```cpp
enum class Kind : std::uint8_t { Circle, Square, Triangle };
struct Shape { Kind kind; double a, b; };

double area(const Shape& s) {
    switch (s.kind) {                      // branch-predictable if grouped/sorted
        case Kind::Circle:   return 3.14159 * s.a * s.a;
        case Kind::Square:   return s.a * s.a;
        case Kind::Triangle: return 0.5 * s.a * s.b;
    }
    return 0;
}
```
- POD struct + a tag byte. `switch` on the tag — if inputs are grouped by kind
  (or one kind dominates), branch predictor nails it. All inline.
- This is the classic HFT order-book / message pattern — a `MsgType` byte read
  from the wire + a `switch`.

### 4. Just use a virtual — when it's fine

Cold paths (config, startup, admin, error handling, plugin boundaries), or when
the dispatch happens rarely relative to the work it triggers (a virtual call
that then does 10 µs of work — the ~20 ns dispatch is noise).

---

## When virtual's cost actually bites

| Situation | Virtual OK? |
|---|---|
| Called once per market-data tick, millions/sec | ❌ — use CRTP/variant/tag |
| Called once at startup per config entry | ✅ — irrelevant |
| Dispatch → 5 µs of real work | ✅ — 20 ns is 0.4% |
| Tight loop, dispatch is most of the work | ❌ |
| Type varies unpredictably every call (BTB thrash) | ❌❌ — worst case |
| Type is stable across many calls (predictable) | ⚠️ — better, but still no inline |

---

## Andar kya hota hai

- **BTB (branch target buffer):** the CPU caches "last target of this indirect
  call". Monomorphic-in-practice call sites (always the same type) → BTB hits →
  cheap-ish. Polymorphic-in-practice (types interleaved) → BTB misses → each miss
  ~15-20 cycle pipeline flush. This is why "virtual is fine if the type is
  stable" has some truth.
- **Lost optimizations:** across an un-inlined call, the compiler must assume the
  callee can read/write any global memory → kills CSE, keeps values in memory
  not registers, blocks autovectorization of the surrounding loop. Often bigger
  than the call itself.
- **`-flto` + `-fdevirtualize`** + `final` — can turn virtual calls into direct
  calls when the whole program is visible and the type is provable. Helps, not a
  full substitute for design.
- **Speculative devirtualization** (`-fdevirtualize-speculatively`) — compiler
  emits `if (vptr == &Circle_vtable) inline_circle_area(); else virtual_call();`
  — a guarded fast path.

> **HFT relevance:** the hot path's dispatch set is **closed and known** (all
> order types, all message types are defined in your system) — so the
> open-endedness that `virtual` buys you is worthless there, and its cost
> (indirect call + no inline + BTB pressure + pointer-chase from
> `vector<unique_ptr>`) is pure loss. Standard hot-path pattern: **POD structs +
> a type-tag byte + a `switch`** (or a `std::variant`), everything inlined,
> contiguous, branch-predictable. `virtual` lives at the cold boundary (venue
> adapters, strategy plugins) and in tooling. When a profiler shows `operator
> new` (folder 14) or an indirect `call` in the hot loop — that's the bug to
> fix by design, not to micro-tune.

---

## Hands-on — performance engineering loop (CLAUDE.md §12)

```bash
# 1. measure
./build.ps1 fast 16-OOP/examples/07_dispatch_benchmark.cpp
# 2. profile the misses (Linux)
perf stat -e instructions,branch-misses,L1-dcache-load-misses ./db
# 3. try alternatives -- CRTP (file 13), variant, tag switch
# 4. explain: virtual -> indirect + no inline + BTB; CRTP -> compile-time -> direct
```

Experiment: sort the shapes by type before the loop → virtual gets faster (BTB
hits). Interleave randomly → slower. That's the BTB effect.

---

## ⚠️ Traps

### Trap 1 — "virtual call is just a pointer deref, ~1 ns"
2 dependent loads + indirect call + **lost inlining** + BTB miss risk. ~5-15x
direct, measured.

### Trap 2 — benchmarking virtual with one concrete type
Monomorphic → compiler devirtualizes / BTB always hits → misleadingly fast. Use
a real mix.

### Trap 3 — `std::vector<std::unique_ptr<Base>>` in the hot loop
Pointer chase (each object elsewhere on the heap) + virtual call. Double cost.
`std::vector<std::variant<...>>` is contiguous.

### Trap 4 — assuming `-flto` fixes it
Helps devirtualize provable cases; doesn't help when the type genuinely varies.

### Trap 5 — replacing virtual with `std::function`
`std::function` has its own indirect call + possible heap allocation for the
callable. Not a speedup over virtual — often worse.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Virtual call cost = 1 indirect call" | + 2 loads + lost inlining + BTB misses |
| "CRTP and virtual are similar cost" | CRTP ≈ direct (compile-time); virtual ~5-15x |
| "`std::variant` is slower than virtual" | Usually faster — contiguous, no pointer chase, inlinable arms |
| "Stable type → virtual is free" | Better (BTB hits) but still no inlining |
| "`std::function` replaces virtual cheaply" | Its own overhead; not a win |

---

## Exercises

1. **Run + read:** `07_dispatch_benchmark.cpp` — note virtual/direct and
   variant/direct ratios on your machine. Which is closest to direct?

   <details><summary>Answer</summary>

   CRTP ≈ direct (~1x — compile-time dispatch, fully inlined). variant ~5-8x,
   virtual ~8-15x (machine-dependent).
   </details>

2. **BTB effect:** modify the benchmark to `std::sort` the `vpoly` vector by
   `typeid(*p).name()` before timing (group same types). Virtual ns/call —
   better or worse? Why?

   <details><summary>Answer</summary>

   Better — grouping makes each stretch of the loop monomorphic → the BTB
   predicts the indirect target correctly → fewer pipeline flushes. Random
   interleave = worst case.
   </details>

3. **Tag switch:** rewrite the shapes as `struct Shape { uint8_t kind; double a,
   b; };` + a `switch`-based `area()`. Add it to the benchmark. ns/call vs
   virtual vs direct?

   <details><summary>Answer</summary>

   Typically ~direct (1-3 ns) — POD, contiguous, `switch` inlines all arms,
   branch-predictable if grouped. This is why the wire-tag pattern wins in HFT.
   </details>

4. **`perf` the misses:** `perf stat -e branch-misses,instructions ./db` — which
   dispatch approach has the most `branch-misses`? Relate to ns/call.

   <details><summary>Answer</summary>

   virtual (mixed types) — high `branch-misses` from indirect-call
   mispredictions. direct/CRTP — near zero. The branch-miss count tracks the
   ns/call gap.
   </details>

5. **When it doesn't matter:** a virtual `onOrderRejected(const Reject&)` called
   ~10 times/sec, each doing logging + a metrics update (~2 µs). Worth
   de-virtualizing?

   <details><summary>Answer</summary>

   No — 10 calls/sec × 20 ns dispatch = 200 ns/sec total, utterly negligible vs
   the 2 µs of work and the low rate. Keep it virtual; spend the effort on the
   million-per-sec path.
   </details>

---

## Interview questions

1. Virtual call ki 3 alag costs (loads, indirect call, inlining)?
2. Measured: virtual vs direct ns/call — rough ratio, kis se depend?
3. BTB kya hai, virtual call ki predictability kaise affect karta?
4. CRTP vs virtual — cost aur limitation?
5. `std::variant` + `visit` — kab virtual se behtar, kab worse?
6. Virtual dispatch kab genuinely fine hai (2-3 cases)?

---

## Next
→ [`13-crtp.md`](13-crtp.md)
