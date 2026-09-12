# 21 — Instruction cache: hot/cold splitting, function ordering

## Prerequisites
- **`31-CPU-ARCHITECTURE/05`** (frontend, µop cache), **`33-COMPILER-OPTIMIZATION/08,10,11`**
  (branch hints, LTO, PGO), **`35-PROFILING-BENCHMARKING/10-11`** (`perf`, top-down)
- `examples/12_hot_cold_split.cpp`

## Yeh topic abhi kyun
The frontend (fetch + decode) can starve the backend if the hot path's
**code** doesn't fit L1i (~32 KB) / the µop cache (~1.5–4K µops). `perf`
top-down calls this **Frontend Bound**. A big C++ hot path — templates,
inlined call chains, rarely-taken error/log code inline — bloats past the
budget → I-cache & iTLB misses per event. Yeh lesson: keep the hot path's
code dense.

---

## Symptoms

- `perf stat -M TopdownL1` → **Frontend Bound** high (>20–30%).
- `perf stat -e L1-icache-load-misses,iTLB-load-misses,frontend_retired.latency_ge_16`
  → high counts.
- Latency that scales with **how much code** ran, not how much data.
- Worse after a quiet period (I-cache cold — lesson 20).

---

## Techniques

### 1. Hot/cold splitting
Rarely-taken code (error handling, logging, slow fallbacks, assertions) does
not belong inline in the hot loop — it pushes hot instructions out of L1i.
```cpp
if (rare_condition) [[unlikely]]
    handle_rare(ctx);                       // <-- a [[gnu::cold]] [[gnu::noinline]] function

[[gnu::cold]] [[gnu::noinline]]
static void handle_rare(Ctx& ctx) { /* bulky */ }
```
`[[gnu::cold]]` → the compiler places it in `.text.unlikely` (a separate
section, far from the hot code); `[[unlikely]]` on the branch → the fall-
through is the hot path, the jump to the cold code is the rarely-taken
forward branch. Result: the hot loop is straight-line and dense.

**Measured (`12_hot_cold_split.cpp`)**: in a *micro-benchmark* where the hot
loop already fits L1i, A (inline) / B (plain fn) / C (`[[gnu::cold]]`+`[[unlikely]]`)
were **all ~1.49 ns/iter — no measurable difference** (Rule 2 — kept, not
hidden). The concept is sound; the payoff needs a hot path that's **genuinely
I-cache-bound** (`perf` says Frontend Bound), where splitting + PGO can be
10–30%.

### 2. `[[likely]]` / `[[unlikely]]` for layout (not prediction)
On a *predictable* branch, the hint tells the compiler which side is the
fall-through and which is out-of-line — it changes **code layout**, not the
hardware predictor (which already gets a predictable branch right). Folder
33/08.

### 3. Function ordering
Pack hot functions **contiguously** so they share cache lines / pages and
don't conflict:
- **PGO** (33/11) — profile-guided, the compiler orders blocks and functions
  by hotness, splits cold code automatically. The single biggest lever for
  Frontend-Bound C++.
- **LTO** (33/10) — enables cross-TU inlining *and* whole-program layout.
- **BOLT** / **Propeller** — post-link optimizers that reorder a already-
  built binary from a `perf` profile; used on huge binaries (databases,
  compilers, trading engines).
- Manual: a linker symbol-ordering file, or `__attribute__((section("hot")))`
  to gather hot functions.

### 4. Reduce hot-path code size
- Fewer template instantiations on the hot path (a `std::variant` of 3 types
  vs a template instantiated 3 ways — the variant shares code).
- Don't force-inline everything (`always_inline` a big function into a loop
  → the loop body explodes). Let the compiler / PGO decide; `always_inline`
  only tiny accessors.
- `-Os` for cold translation units.
- Avoid giant inlined call chains (deep `std::function` / lambda nesting
  that inlines into one enormous function).

### 5. Huge pages for `.text`
A large hot code footprint → iTLB misses. Map the text segment with huge
pages (`hugepage_text` / linker + loader support, or `madvise(MADV_HUGEPAGE)`
on the text mapping) → far fewer iTLB entries needed.

---

## `__attribute__((hot))` / `((cold))`

- `[[gnu::hot]]` on a function → the compiler optimizes it more aggressively
  and places it with other hot code.
- `[[gnu::cold]]` → optimized for size, placed in `.text.unlikely`,
  and calls to it are treated as unlikely (implies the call site is a cold
  branch).
- These are **hints**; PGO's measured profile beats hand-annotation. Use the
  attributes for the obvious cases (an error handler is `cold`), rely on PGO
  for the rest.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `always_inline` on everything
Inlining a large function into a hot loop bloats the loop body past the µop
cache → Frontend Bound. Inline tiny things; let PGO decide the rest.

### Trap 2 — assuming hot/cold split helps without measuring
`12` showed **no difference** in a micro-bench (hot loop fit L1i). Verify
`perf` says Frontend Bound before investing; measure the delta after.

### Trap 3 — cold code that isn't marked
An error handler / a `throw` path inline in the hot function, no `[[unlikely]]`,
no `[[gnu::cold]]` → it sits in the hot code's cache footprint. Split it.

### Trap 4 — PGO with an unrepresentative profile
Profile from a workload that exercises different branches than production →
the compiler lays out the wrong code as hot. Profile from a replayed real
session (33/11).

### Trap 5 — measuring I-cache effects in isolation
A micro-bench's hot loop is the only code running → it's always I-cache
resident. Production's hot path competes with everything. Measure on the
real binary (`perf`, 35/11).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "inline everything for speed" | bloats the µop cache → Frontend Bound; inline tiny things only |
| "hot/cold split always helps" | only when `perf` says Frontend Bound; micro-bench showed 0 (Rule 2) |
| "`[[likely]]` improves prediction" | it changes *layout*; the HW predictor already handles predictable branches |
| "PGO is a nice-to-have" | for Frontend-Bound C++ it's the biggest single lever |
| "I profiled the micro-bench, split helps" | test on the real binary under real load |

---

## Exercises

1. `perf stat -M TopdownL1` on the trading engine: `Retiring 30%`, `Frontend
   Bound 45%`, `Backend 18%`, `Bad Spec 7%`. The hot path is a big templated
   message dispatcher with inline handlers and inline error/logging code.
   Ordered plan to cut Frontend Bound.

   <details><summary>Answer</summary>

   Frontend Bound 45% = nearly half the pipeline slots wasted waiting for
   instruction fetch/decode — the hot path's code doesn't fit L1i / the µop
   cache. Ordered plan (biggest lever first):
   (1) **PGO** (33/11) — build with `-fprofile-generate`, run a **replayed
   real session**, rebuild with `-fprofile-use`. The compiler splits cold
   code (error/log handlers) into `.text.unlikely`, orders hot blocks/
   functions by measured hotness, and inlines only the callsites that are
   actually hot. This alone often takes 10–30% off Frontend Bound for a
   code-heavy hot path.
   (2) **LTO** (`-flto`) — cross-TU inlining decisions + whole-program
   function ordering; combine with PGO.
   (3) **Hot/cold split the obvious**: mark every error handler, logging
   path, and slow fallback `[[gnu::cold]] [[gnu::noinline]]` and their
   branches `[[unlikely]]`. Move logging to a ring + logger thread (17) so
   it's not even in the binary's hot region.
   (4) **De-bloat the dispatcher**: if it's a template instantiated N ways,
   consider `std::variant` + `visit` (one shared dispatch, N inlined bodies —
   lesson 13) or a tag+switch — less total code than N template
   instantiations. Remove `always_inline` from anything non-trivial.
   (5) **Huge pages for `.text`** if the hot footprint is large (iTLB).
   (6) **BOLT** the final binary from a `perf` profile if it's very large.
   Re-measure top-down after each step; expect Frontend Bound to fall and
   Retiring to rise.
   </details>

2. Ek engineer har handler function ko `[[gnu::hot]]` aur `always_inline`
   mark karta hai "to make them fast." Frontend Bound **badh gaya**. Kyun?

   <details><summary>Answer</summary>

   `always_inline` on *every* handler forces each one's full body into every
   call site. If the dispatcher calls 16 handlers and each is 200–500 bytes
   of code, the dispatcher function becomes one enormous function — several
   KB of code that all has to be fetched/decoded even though any given
   message uses only one handler's worth. That blows past the µop cache
   (~1.5–4K µops) and stresses L1i → **more** Frontend Bound, not less. And
   `[[gnu::hot]]` on everything is meaningless — if all code is "hot", the
   compiler has no signal about what to place together, so it can't improve
   layout.
   Correct: inline only **tiny** things (field accessors, a 2-line helper).
   Let the compiler (guided by **PGO's measured profile**) decide which
   callsites to inline — it will inline the genuinely hot ones and leave the
   rest as calls, keeping the hot function small. Mark only the **cold**
   paths (`[[gnu::cold]]` on error handlers) — that's the annotation that
   actually helps layout, by getting cold code *out* of the hot region.
   "Make it fast" for the frontend means "make the hot path's code
   **small**", which usually means inlining *less*.
   </details>

---

## Interview questions

1. Frontend Bound — what it means, how `perf` shows it, what causes it in C++.
2. Hot/cold splitting — `[[gnu::cold]]` + `[[unlikely]]`, `.text.unlikely`.
3. `12_hot_cold_split.cpp` measured no difference — why (Rule 2), and when it *would* help.
4. `[[likely]]`/`[[unlikely]]` as layout hints, not prediction hints.
5. PGO / LTO / BOLT for function ordering — which does what.
6. Why `always_inline` everything increases Frontend Bound.

---

## Next
→ [`22-zero-copy-patterns.md`](22-zero-copy-patterns.md)
