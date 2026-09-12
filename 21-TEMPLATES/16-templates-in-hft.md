# 16 — Templates in HFT: zero-cost abstraction in practice

## Prerequisites
- All of folder 21
- Folder 16 (virtual dispatch cost), folder 19 file 26 (STL in HFT), folder 20 file 17 (cache-aware DSA)

## Yeh topic abhi kyun
Folder ka synthesis. Har lesson mein "HFT relevance" tha; yahan ek picture:
**templates hot path pe `virtual` ki jagah lete**, **policy classes se behaviour
compile karte**, aur **fixed-capacity containers monomorphize karte** — sab zero
runtime cost. `examples/07`, `08`: virtual ~2.4–2.5 ns/call vs template/CRTP/
variant ~0.5–1.1 ns/call.

---

## The core idea: move the decision to compile time

A trading hot path has a fixed structure known at build time: *this* feed
handler, *this* order type, *this* strategy, *this* venue. Runtime polymorphism
(`virtual`) pays a per-call price for flexibility you don't use on that path.
Templates encode the choice in the **type**, so:

- the call **inlines** — no vtable load, no indirect call
- the surrounding loop can **vectorize** (an indirect call is an optimization
  barrier)
- the object has **no vptr** → tighter cache packing (folder 16, 20 file 17)
- constants (capacities, tick sizes) **fold** and loops **unroll**

Measured cost of the dispatch itself (`examples/07`, `08`, this box):

| mechanism | ns/call | notes |
|---|---|---|
| template (concrete type) | ~1.1 | inlined; loop often vectorizes |
| CRTP | ~0.56 | inlined; `sizeof` unchanged (no vptr) |
| `std::variant` + `std::visit` | ~1.1 | jump table, one predictable indirect call |
| `virtual` (real `Base*` boundary) | ~2.4–2.5 | vtable load + indirect call, **not** inlined, blocks vectorization |

(If the concrete type is visible, the compiler **devirtualizes** and `virtual` ==
template — the gap is only at a genuine polymorphic boundary.)

---

## Pattern 1 — templated hot-path functions

```cpp
template <class Handler>
void run_feed(std::span<const std::byte> buf, Handler& h) {
    while (!buf.empty()) {
        auto [msg, n] = decode(buf);
        h.on_message(msg);            // concrete Handler -> inlined; the whole loop can vectorize/pipeline
        buf = buf.subspan(n);
    }
}
```

`Handler` is a concrete class; `on_message` inlines. Compare a `virtual
IHandler&` — an indirect call per message, no inlining, at market-data rates
that's real time.

`std::sort`, `std::for_each`, `std::transform` are this pattern — the comparator/
callback is a template parameter, so a no-capture lambda inlines to nothing
(folder 19 file 16).

---

## Pattern 2 — policy-based components

Configure a component by **type parameters**, each policy's code inlined, no
runtime branch:

```cpp
template <class T, std::size_t Cap,
          class OverflowPolicy = AssertOnOverflow,     // or DropOldest, or Block
          class StatsPolicy    = NoStats>              // or CountStats
class SpscQueue : private StatsPolicy {
    alignas(64) std::atomic<std::size_t> head_{0};     // cache-line aligned -> no false sharing (folder 28)
    alignas(64) std::atomic<std::size_t> tail_{0};
    std::array<T, Cap> buf_;                            // Cap is an NTTP -> inline storage, no heap
public:
    bool push(const T& x) {
        // ... on full: OverflowPolicy::on_full(*this);  -> inlined, zero cost if it's a no-op
        // ... StatsPolicy::on_push();
    }
};

SpscQueue<Order, 4096, DropOldest, CountStats> md_queue;   // exact behaviour baked into the type
```

`examples/07_crtp_policy.cpp` shows the shape. `NoStats::on_push()` is an empty
inline function → compiles away entirely.

---

## Pattern 3 — CRTP families

A set of strategies / parsers / risk checks sharing a template-method base,
each call resolved at compile time (file 11):

```cpp
template <class Derived>
struct Strategy {
    void on_tick(const Tick& t) { static_cast<Derived*>(this)->on_tick_impl(t); }   // inlined
};
struct MomentumStrat : Strategy<MomentumStrat> { void on_tick_impl(const Tick&); };
```

No vptr → `sizeof(MomentumStrat)` is just its data → more strategy objects per
cache line if you hold an array of them.

---

## Pattern 4 — `std::variant` for a small runtime-chosen set

When the choice *is* a runtime value (which strategy is live) but the set is
small and closed, `std::variant<A, B, C>` + `std::visit` gives **jump-table
dispatch**: one indirect call through a compiler-built table (predictable,
BTB-friendly), and the callee inlines *inside* the visitor. Faster and more
cache-friendly than `virtual` through a scattered vtable, and allocation-free
(the alternatives live inline). Folder 19 file 15.

```cpp
using Strat = std::variant<MomentumStrat, MeanRevStrat, MarketMakeStrat>;
Strat live = pick_from_config();
for (const auto& tick : feed) std::visit([&](auto& s){ s.on_tick(tick); }, live);
```

---

## Pattern 5 — compile-time tables and validation

- `constexpr` functions build CRC tables, tick-size maps, FSM transition tables
  into `std::array` → `.rodata`, zero startup init (file 13).
- `static_assert(sizeof(WireMsg) == 48, "layout drift")`,
  `static_assert(std::is_trivially_copyable_v<Order>)`,
  `static_assert(alignof(Slot) == 64)` — protocol/ABI invariants caught at build
  time, not in production.
- Concepts constrain generic infra: `void publish(TriviallyCopyable auto&& m)` —
  a one-line error if someone tries to publish a type with a `std::string`.

---

## The cost, managed

The template tax (file 15): compile time, `.o` size, binary size from distinct
instantiations, header coupling.

Mitigations used in real trading codebases:
- **Lean hot-path templates** — the T-dependent part is small; T-independent bulk
  in a non-template base.
- **`extern template`** the instantiations used in hundreds of TUs.
- **Type erasure at cold boundaries** — `std::function` for config callbacks,
  `std::span` for API params, a `virtual` interface for the admin plane.
- **Small policy sets** — every `<Policy...>` combo is a new instantiation.
- **Profile** with `-ftime-trace`; C++20 modules where supported.

Pay the tax where it buys runtime speed (the hot path); don't pay it everywhere
by reflex.

---

## Andar kya hota hai

- A templated call site with a concrete type: the compiler has the callee's body
  → inlines it → the "call" disappears, replaced by the work. The optimizer then
  treats a loop of such calls as straight-line code it can unroll/vectorize.
- `virtual` through an unresolvable `Base*`: `mov rax, [obj]` (vptr), `call [rax +
  k]` (indirect). The indirect call is a hard inlining barrier and pressures the
  branch-target buffer at a polymorphic call site. `examples/08`: ~2.5 ns vs
  ~1.1 ns for the template.
- `std::visit`: the compiler emits a `switch` / jump table on the variant's
  active index → one indirect jump with a small, predictable target set; each
  case calls a concrete function that can inline. Cheaper and more predictable
  than a vtable whose target is data-dependent and whose code is far away.
- NTTP capacities fold: `for (i < Cap)` with `Cap = 4096` a constant → the
  compiler can unroll, bounds-prove, and pick SIMD widths.
- Policy classes: empty ones hit EBO (0 bytes); their methods are inline → a
  `NoStats::on_push()` no-op leaves *nothing* in the binary.

> **HFT relevance:** this lesson is the HFT template philosophy end to end. The
> hot path uses **compile-time dispatch** (templates / CRTP / `if constexpr` /
> `std::visit`) so every call inlines and every loop can vectorize; **policy
> classes** compile features and instrumentation in or out at zero cost;
> **fixed-capacity NTTP containers** are inline/preallocated with unrolled loops;
> **`constexpr` tables** and **`static_assert`** move work and safety checks to
> build time. `virtual` and `std::function` live in the control plane. The
> compile-time price is paid deliberately — lean hot templates, `extern
> template`, type erasure at cold edges — because the runtime payoff is a call
> that costs ~0.5–1 ns instead of ~2.5 ns and a loop the optimizer can actually
> transform.

---

## Hands-on

```bash
./build.ps1 fast 21-TEMPLATES/examples/07_crtp_policy.cpp
./build.ps1 fast 21-TEMPLATES/examples/08_compile_time_dispatch.cpp
./build.ps1 asm 21-TEMPLATES/examples/08_compile_time_dispatch.cpp   # no `call` in the template loop; `call [rax+..]` in the virtual loop
```

Sketch (types only): an `SpscQueue<T, Cap, OverflowPolicy, StatsPolicy>`; a CRTP
`FeedParser<Derived>` with a template-method `parse()`; a
`std::variant<Strat...>` tick loop. Confirm `sizeof` of the CRTP objects has no
vptr and the policy no-ops leave nothing in the assembly.

---

## ⚠️ Traps

### Trap 1 — `virtual` on the tick path "for flexibility"
```cpp
struct IStrategy { virtual void on_tick(const Tick&) = 0; };
// per-tick indirect non-inlined call + no vectorization. Template it, or std::variant + visit.
```

### Trap 2 — `std::function` for a hot callback
```cpp
std::function<void(const Tick&)> cb = ...;   // indirect + no inline + maybe a heap alloc on assign. Template parameter instead
```

### Trap 3 — assuming virtual is always ~2× slower
```cpp
// If the concrete type is visible, the compiler devirtualizes -> same as a template. The gap is at a real Base* boundary.
```

### Trap 4 — exploding policy combinations
```cpp
Queue<T, Cap, P1, P2, P3, P4>   // every combination is a distinct instantiation -> bloat. Keep the set tiny / default hard
```

### Trap 5 — templating a cold path and paying the compile-time tax for nothing
```cpp
// A config-time registry templated on 30 handler types = 30 instantiations, zero runtime benefit. Erase the type there.
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Templates and `virtual` cost the same at `-O2`" | Only when the concrete type is visible (devirtualization); at a real boundary, ~2× on the call + lost vectorization |
| "`std::function` is a fine hot-path callback" | Indirect, non-inlined, possible heap alloc — template the parameter |
| "Policy classes add overhead" | Empty policy → EBO → 0 bytes; methods inline; no-ops vanish |
| "Use templates everywhere for speed" | Only where they buy runtime speed; the build tax is real — erase types at cold edges |
| "`std::variant`+`visit` is as slow as `virtual`" | Jump-table dispatch, predictable target, callee inlines — faster and more cache-friendly |

---

## Exercises

1. **Dispatch choice:** for each, pick template / CRTP / `variant`+`visit` /
   `virtual`: (a) `std::sort` comparator, (b) 3 strategies, one chosen at
   startup, called per tick, (c) a plugin system loading `.so` files at runtime,
   (d) a family of feed parsers, each call site knows its feed.

   <details><summary>Answer</summary>

   (a) template parameter (no-capture lambda inlines). (b) `std::variant<S1,S2,
   S3>` + `std::visit` (runtime choice, small closed set). (c) `virtual`
   interface (open set, loaded at runtime). (d) CRTP (types known at compile
   time, call site knows which).
   </details>

2. **Zero-cost policy:** show why `SpscQueue<T, N, AssertOnOverflow, NoStats>`
   has no metrics code in the binary.

   <details><summary>Answer</summary>

   `NoStats::on_push()` is an empty inline function; the calls to it inline to
   nothing. `NoStats` is an empty base → EBO → 0 bytes. The compiler emits no
   counter, no increment, no member — as if the metrics code weren't written.
   </details>

3. **Devirtualization:** `examples/07` measured virtual ≈ CRTP with one concrete
   object but 2.4 ns vs 0.56 ns with a heterogeneous `vector<unique_ptr<Base>>`.
   Explain.

   <details><summary>Answer</summary>

   One concrete object → GCC proves the dynamic type → replaces the vtable call
   with a direct inlined call (devirtualization) → same as CRTP. A vector of
   mixed derived types hides the target → a real indirect vtable call per element
   → the gap appears.
   </details>

4. **NTTP unroll:** why does `SpscQueue<int, 8>` produce different (better) code
   for a "copy all elements" loop than a `std::vector<int>` of size 8?

   <details><summary>Answer</summary>

   `8` is a compile-time constant (NTTP) → the loop bound is known → the compiler
   fully unrolls (8 loads/stores, or one SIMD move) with no counter, no branch.
   `std::vector::size()` is a runtime value → a real loop with a counter and a
   branch.
   </details>

5. **Build tax:** you have a lean `RingBuffer<T, N>` used with 6 types in the hot
   path and a fat `Engine<T>` used with 40 types across 200 TUs. Different
   treatment?

   <details><summary>Answer</summary>

   `RingBuffer` — leave it; 6 lean instantiations, real runtime benefit.
   `Engine<T>` — factor the T-independent bulk into a non-template base,
   `extern template` the common instantiations, and consider erasing the type at
   any part of `Engine`'s interface that isn't latency-critical.
   </details>

---

## Interview questions

1. Hot path pe `virtual` ki jagah template kyun — inlining, vectorization, vptr?
2. Devirtualization kab hota — virtual template jitna fast kab?
3. Policy-based design — empty policy ka cost (zero) kaise?
4. `std::variant` + `visit` dispatch vs `virtual` — kya fark (jump table vs vtable)?
5. NTTP capacity se optimizer ko kya milta (unroll, bounds)?
6. Template build-tax hot path pe kyun accept, cold path pe kya karte (type erasure)?

---

## Next
→ [`17-exercises.md`](17-exercises.md)
