# 23 — Compile-time dispatch: removing runtime branches with templates

## Prerequisites
- **`21-TEMPLATES`** (`if constexpr`, non-type params, CRTP, policy design),
  **`22-MODERN-CPP`** (`constexpr`/`consteval`), **`33-COMPILER-OPTIMIZATION/06`**
  (constant folding)
- `13-virtual-dispatch-elimination.md`, `12-branch-free-programming.md`

## Yeh topic abhi kyun
A branch that checks a value which is **fixed for the whole run** (a config
flag, a venue id, a "am I in sim mode" bool) still costs a compare + a
(predicted) branch every hot-path iteration, and — worse — it keeps *both
code paths in the binary*, bloating I-cache (lesson 21). If the value is
known at compile time (or you can afford one instantiation per value), the
branch **disappears**.

---

## `if constexpr` — the branch that isn't there

```cpp
template <bool WithRiskCheck>
void process(const Order& o) {
    decide(o);
    if constexpr (WithRiskCheck) {          // resolved at compile time
        if (!risk_ok(o)) return;            // this whole block: gone when WithRiskCheck==false
    }
    send(o);
}
process<true>(o);   // risk check compiled in
process<false>(o);  // risk check ABSENT from the generated code — not just skipped
```
Unlike a runtime `if (with_risk_check)`, the `false` instantiation has **no
risk-check instructions at all** — smaller code, no branch, no
possibility of the predictor being wrong. The cost: **two instantiations**
(code bloat if you do this a lot — measure vs the I-cache win).

---

## Non-type template parameters — specialize on a value

```cpp
template <Venue V>
struct Encoder {
    void encode(const Order& o, std::byte* out) {
        if constexpr (V == Venue::NYSE)  encode_ouch(o, out);
        else if constexpr (V == Venue::CME) encode_mdp(o, out);
        // ... only the matching branch is compiled per V
    }
};
Encoder<Venue::NYSE> nyse_enc;   // knows its venue at compile time — no per-call dispatch
```
When you have **one encoder per venue** (a common HFT shape), template on
the venue → each encoder is monomorphic, fully inlined, no dispatch.

---

## `constexpr` / `consteval` — compute at compile time

```cpp
consteval uint32_t crc_table_entry(uint32_t i) { /* ... */ }
constexpr auto CRC_TABLE = [] { std::array<uint32_t, 256> t{};
    for (uint32_t i = 0; i < 256; ++i) t[i] = crc_table_entry(i); return t; }();
// CRC_TABLE is baked into .rodata — no init code, no runtime cost
```
Lookup tables, bit masks, protocol constants, `switch`-replacement arrays —
build them at compile time so the binary ships with the answer.

---

## `template` vs `variant` vs runtime — the spectrum

| Approach | Dispatch cost | Code size | Set |
|---|---|---|---|
| **template / `if constexpr`** | zero (resolved at compile) | one instantiation **per value** | closed, value known at compile time (or a bounded set you instantiate) |
| **`std::variant` + `visit`** | a jump table, body inlined | one shared dispatch + N bodies | closed, value known at **runtime** |
| **tag `enum` + `switch`** | a jump table / if-chain, bodies inlined | shared | closed, runtime |
| **`virtual`** | indirect call, not inlined | shared | **open** (plugins) |

Measured (`07_dispatch_comparison.cpp`): CRTP/template ~0.6 ns (the floor),
variant/switch ~0.7–0.8 ns homogeneous, `virtual` ~2.5–7 ns. Template wins
on speed; you pay in instantiations.

**Rule:** if the deciding value is **fixed for the run** (config) → template
it (or set a function pointer once at startup). If it varies per event but
the set is **closed** → `variant`/switch. If the set is **open** → `virtual`.

---

## Startup-time dispatch (the "instantiate once, pick once" pattern)

You can't template on a runtime config value directly, but you can **pick
the instantiation once** at startup:
```cpp
using Handler = void(*)(const Order&);
Handler g_handler = config.sim_mode
    ? &process<false>                      // no real send, no risk check
    : config.strict
        ? &process<true, /*strict=*/true>
        : &process<true, /*strict=*/false>;
// hot path: g_handler(o);   -- one indirect call, but ZERO per-event config branches
```
A tiny startup `switch` over config → a function pointer → the hot path has
no config branches at all, and each `process<...>` instantiation is
monomorphic and dense. (A single well-predicted indirect call is cheap; the
win is removing the *body* branches and their code.)

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `if constexpr` used as a runtime `if`
`if constexpr (some_runtime_bool)` doesn't compile (the condition must be a
constant expression). And a plain `if` on a run-fixed value still ships both
paths + costs a branch — use a template + startup pick.

### Trap 2 — template bloat
`process<A, B, C, D>` with 4 bool params = **16 instantiations**, each a
full copy of the hot path → I-cache disaster (lesson 21). Template only the
1–2 params that actually matter; keep the rest runtime.

### Trap 3 — templating on a value that isn't actually fixed
"Venue is fixed per connection" — but you have one code path handling
multiple connections → the venue varies → a template forces a dispatch back
in. Template only where the instantiation genuinely sees one value.

### Trap 4 — `consteval` table that's huge
A 64 KiB compile-time table is 64 KiB of `.rodata` — fine if it's hot
(better than computing it), a waste if it's rarely touched. Size it.

### Trap 5 — `constexpr` that silently becomes runtime
If a `constexpr` function is called with non-constant args, it runs at
runtime. `consteval` forces compile-time (or a compile error) — use it when
you *require* the computation to be baked in.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`if (config_flag)` in the hot loop is cheap (predicted)" | still a branch + both paths in the binary; template + startup pick |
| "`if constexpr` = a faster `if`" | it's compile-time; the untaken branch isn't generated at all |
| "template on every config param" | 2^N instantiations → I-cache bloat; template the 1-2 that matter |
| "`constexpr` guarantees compile-time" | only with constant args; `consteval` forces it |
| "compile-time dispatch always wins" | it costs instantiations; measure vs the branch you removed |

---

## Exercises

1. Hot path mein: `if (venue_ == NYSE) encode_ouch(o); else if (venue_ ==
   CME) encode_mdp(o); else if (venue_ == LSE) encode_native(o);`. `venue_`
   ek member, ek engine instance ek hi venue ke saath deal karta (per-venue
   engines). Redesign to remove the per-event branches.

   <details><summary>Answer</summary>

   Since each engine instance handles exactly one venue, `venue_` is
   **fixed for that instance's lifetime** — but the code still checks it
   every event, and every instance's binary contains all three encoders
   inline in the hot path.
   Options:
   (1) **Template the engine on the venue**: `template <Venue V> class
   Engine`, and inside, `if constexpr (V == Venue::NYSE) encode_ouch(o);`
   etc. Each `Engine<V>` compiles with **only its encoder** — no branch, no
   other venues' code. Instantiate `Engine<Venue::NYSE>` / `<CME>` / `<LSE>`
   at startup based on config. Cost: 3 instantiations of the engine, but
   each is smaller (one encoder) and branch-free. Since you run one venue
   per process (or per engine), the bloat is bounded and the hot path is
   dense.
   (2) **Startup function-pointer pick**: `using EncFn = void(*)(const
   Order&, std::byte*); EncFn enc_ = pick(config.venue);` where `pick`
   returns `&encode_ouch` / `&encode_mdp` / `&encode_native`. The hot path
   is `enc_(o, out)` — one well-predicted indirect call (same target every
   time for this instance → the BTB nails it), and the *other* encoders
   aren't inlined into the hot function. Simpler than templating the whole
   engine; slightly less optimal (no cross-inlining of the encoder into the
   caller) but usually fine.
   Either way: zero per-event `venue_` branches, and the hot function only
   contains the code it uses.
   </details>

2. Ek engineer `process<bool RealSend, bool RiskCheck, bool Logging, bool
   Metrics>` template banata hai aur startup pe config se sahi
   instantiation pick karta. Binary size 3× badh gaya aur p99 **bura** ho
   gaya. Kya galat?

   <details><summary>Answer</summary>

   4 bool template params = **2⁴ = 16 instantiations** of the entire
   `process` hot path, each a full copy. Only one is used at runtime, but
   **all 16 are in the binary** (unless the linker's dead-code elimination
   removes the unused ones — and even the one that's used is now a distinct,
   large function). This bloats `.text`, hurts the instruction-cache /
   iTLB working set (lesson 21), and can push the *actually-used*
   instantiation's hot loop past the µop cache → **Frontend Bound** → worse
   p99. The engineer optimized away a few cheap, perfectly-predicted config
   branches and paid for it in code size.
   Fix: keep **at most 1–2** template params — the ones where the compiled-
   out code is *substantial* and the branch is genuinely in the hot inner
   loop (e.g. `RealSend` if the send path is big and sim mode should exclude
   it entirely). The rest (`Logging`, `Metrics`) are already off the hot
   path (they push to rings — lesson 17), so gate them with a plain runtime
   `if (logging_enabled) [[unlikely]] ...` — a single predicted branch,
   negligible, and only *one* copy of the code. Or set a function pointer
   for the send stage and leave everything else runtime. Measure binary
   size and `perf` top-down before and after — the goal is a *small* hot
   function, and 16 instantiations is the opposite.
   </details>

---

## Interview questions

1. `if constexpr` vs a runtime `if` on a run-fixed value — code generation difference.
2. The template / variant / virtual spectrum — dispatch cost vs code size vs open/closed.
3. "Instantiate once, pick once at startup" — the function-pointer pattern.
4. Template bloat — 2^N instantiations; how to decide which params to template.
5. `constexpr` vs `consteval` — guaranteeing compile-time evaluation.
6. When compile-time dispatch loses (instantiation bloat → Frontend Bound).

---

## Next
→ [`24-tradeoffs-and-when-not-to.md`](24-tradeoffs-and-when-not-to.md)
