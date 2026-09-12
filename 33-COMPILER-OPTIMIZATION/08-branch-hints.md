# 08 — Branch hints: `[[likely]]`/`[[unlikely]]`, `__builtin_expect`, PGO

## Prerequisites
- `31-CPU-ARCHITECTURE/07-branch-prediction.md`
- `06-CONDITIONS/08-branch-prediction-intro.md`
- `05-branch-hints.cpp` example

## Yeh topic abhi kyun
`[[likely]]` / `[[unlikely]]` naye C++20 attributes hain aur log inhe
"branch ko tez karo" samajh lete hain. Yeh **hardware branch predictor ko
kuch nahi karte** — woh runtime pe khud seekh leta. Yeh **code layout** ko
affect karte: hot path straight-line, cold path out-of-line. Isliye inka
measured effect aksar chhota hota (jab tak I-cache pressure na ho), aur inse
behtar **PGO** hai. Yeh lesson batata kab yeh actually help karte, aur kab
galat hint ULTA padta.

---

## Yeh kya karte (aur kya nahi)

**Karte:**
- **Block layout** — likely block ko fall-through (no taken branch) banao,
  unlikely block ko function ke end mein / `.text.unlikely` section mein
  (out-of-line). Hot instructions compact → better I-cache / iTLB density.
- **Register allocation / spill placement** — spills ko unlikely paths mein.
- **Inlining decisions** — unlikely callee ko inline mat karo (space bachao).
- **Static prediction hint** — CPUs jinke paas cold BTB entry ke liye static
  hint hota (old x86 used branch-direction; modern x86 ignores it, ARM
  varies) — but this is mostly historical.

**Nahi karte:**
- Modern HW dynamic predictor ko override — woh 1-2 executions mein pattern
  seekh leta. Ek 99.9%-one-way branch bina hint ke bhi ~perfectly predicted.

---

## Syntax

```cpp
// C++20 standard attributes -- on a statement/label
if (x == nullptr) [[unlikely]] { handle_error(); }
else [[likely]]                { fast_path(); }

for (auto& e : events) {
    if (e.type == RARE) [[unlikely]] slow(e);
    else                            fast(e);
}

switch (op) {
    [[likely]]   case ADD: ...; break;
    [[unlikely]] case NAN_OP: ...; break;
}

// GCC/Clang builtin -- an expression, returns the condition value
if (__builtin_expect(x == nullptr, 0)) { ... }   // 0 = expected false
if (__builtin_expect(ready, 1))        { ... }   // 1 = expected true
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

// probability variant (GCC):
if (__builtin_expect_with_probability(cond, 1, 0.95)) { ... }

// strongest: "this can't happen" -- UB if it does
if (i >= size) std::unreachable();        // C++23
__builtin_unreachable();                  // GCC/Clang
[[assume(i < size)]];                     // C++23, unchecked
```

---

## Measured — example `05` (is box, ~2 GHz)

Hot loop over 64k bytes, ~1 in 1000 triggers an expensive rare path:
```
  no hint             : 0.56 ns/elem
  [[likely/unlikely]] : 0.42 ns/elem
  __builtin_expect    : 0.41 ns/elem
  no-hint / hinted ~= 1.3x
```
The gap is **small** — the HW predictor already nails a 1/1000 branch without
help. What the hint changed (check `./build.ps1 asm`): the `slow_path` call
moved **out of the loop body** to a cold region, reached by a rarely-taken
`jmp`; the hot path is straight-line fall-through. In a big function with
*many* such rare checks, that layout compaction adds up (I-cache). In this
micro-loop it's ~noise-to-1.3x.

---

## Kab yeh actually help karte

1. **Big hot function with many rare checks** — error handling, bounds
   checks, slow-path fallbacks scattered through a hot loop body. Marking
   them `[[unlikely]]` pulls all that code out-of-line → the hot `.text`
   shrinks → fits L1i → real gain.
2. **A branch the HW predictor genuinely struggles with** — e.g. it's
   long-period-periodic or the BTB is under pressure — and you *know* the
   bias. Rare.
3. **First execution / cold BTB** — the very first time through, the static
   hint (if the arch honours it) or the layout means the cold path isn't
   speculatively fetched. Matters for latency-of-the-first-event.
4. **Guiding the compiler's other decisions** — `[[unlikely]]` on a branch
   makes the compiler not inline the callee there, not vectorize that path,
   allocate its spills there.

---

## Kab ULTA padta (galat hint)

- **`[[likely]]` on the rare side** → the hot path now has a taken branch to
  reach it, and the actually-hot code is laid out as the "cold" out-of-line
  block. Measurable regression.
- **Hint on a well-predicted balanced branch** → no benefit, and if wrong,
  a small penalty. Don't hint 50/50 branches.
- **`std::unreachable()` / `[[assume]]` with a condition that CAN be false**
  → UB. The compiler deletes the "impossible" path; at runtime you fall into
  deleted code / garbage.

---

## PGO does this better (lesson 11)

`[[likely]]`/`[[unlikely]]` is you *guessing* the bias. **PGO measures it** —
`-fprofile-generate`, run a real workload, `-fprofile-use` → the compiler
knows the exact taken-rate of every branch and lays out *all* of them
optimally, plus drives inlining, unrolling, and function ordering from real
hotness. For anything beyond a couple of obvious hot-path error checks, PGO
beats manual hints and doesn't rot when the code changes.

Manual hints are for: (a) the handful of "this is an error path, obviously
rare" cases where you want the layout guarantee without a PGO pipeline, and
(b) cases PGO can't see (a workload-dependent branch your profile doesn't
exercise).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — "`[[likely]]` makes the branch faster"
It changes **layout**, not prediction. The HW predictor is already good.
Effect is small unless I-cache-bound.

### Trap 2 — hinting every branch
Noise, and the compiler starts distrusting your hints / they conflict.
Hint only clear, strongly-biased error/slow paths.

### Trap 3 — wrong hint = regression
`[[likely]]` on the rare side inverts the layout. If you're not sure, don't
hint — or use PGO.

### Trap 4 — `[[assume]]` / `unreachable()` as a "hint"
These are **facts**, not hints. False → UB, deleted code paths, security
bugs. Only for invariants you assert in debug builds.

### Trap 5 — expecting `__builtin_expect` to help a data-dependent random branch
The `if (v[i] >= threshold)` random-data case (folder 31 example 03) — the
branch is genuinely unpredictable; a hint doesn't fix that (it's ~50/50).
Branchless (`x & -(cond)`) or predictable-by-construction data is the fix.

### Trap 6 — `[[likely]]` on a `case` in a jump-table `switch`
A dense `switch` compiles to a jump table (one indirect, no per-case
branch) — the hint has nothing to reorder. It matters for sparse `switch`es
that become if-chains.

---

## > **HFT relevance**

> - **`[[unlikely]]` on hot-path error/slow branches** — feed gap detected,
>   order rejected, risk check tripped, malformed message. Pulls the handler
>   out-of-line → the steady-state hot `.text` stays in L1i. Low risk (these
>   really are rare), modest but free gain.
> - **PGO for everything else** — profile = a replayed capture of a real
>   trading session; let the compiler lay out every branch and order every
>   function. Folder 33 lesson 11 + folder 24 lesson 15.
> - **`[[assume]]` for verified invariants** — "seq monotonic", "depth ≤ MAX",
>   "price > 0" — after asserting them in debug builds — lets the optimizer
>   drop handling for the impossible case.
> - **Don't hint the genuinely unpredictable** — matched? crossed? — make
>   those branchless or data-predictable (folder 31 lesson 08, folder 32).
> - **Measure the layout** — `perf annotate` / `./build.ps1 asm`: is the cold
>   handler actually out-of-line? Is the hot loop straight-line?

---

## Hands-on

```bash
./build.ps1 fast 33-COMPILER-OPTIMIZATION/examples/05_branch_hints.cpp
./build.ps1 asm  33-COMPILER-OPTIMIZATION/examples/05_branch_hints.cpp | grep -n "slow_path\|jmp\|jne"
#   hinted version: `call ...slow_path...` sits AFTER the loop (out-of-line)

# PGO comparison (lesson 11):
bash 33-COMPILER-OPTIMIZATION/examples/07_pgo_workflow.sh 33-COMPILER-OPTIMIZATION/examples/05_branch_hints.cpp

# block frequencies GCC guessed:
g++ -O2 -fdump-tree-optimized-blocks=/dev/stdout -c file.cpp | grep -i "freq\|count"
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`[[likely]]` speeds up the branch" | changes block layout; HW predictor unchanged |
| "hint every conditional" | only strongly-biased error/slow paths |
| "wrong hint is harmless" | inverts layout → measurable regression |
| "`[[assume]]` is a strong hint" | it's an unchecked fact; false → UB |
| "hints beat PGO" | PGO measures every branch; hints are for the obvious few + blind spots |
| "hint fixes an unpredictable branch" | ~50/50 stays ~50/50; go branchless |

---

## Exercises

1. `int parse(const char* p, int n) { for (int i=0;i<n;++i) { if (p[i] < '0'
   || p[i] > '9') [[unlikely]] return -1; acc = acc*10 + (p[i]-'0'); } return
   acc; }` — is the `[[unlikely]]` justified? What does it buy?

   <details><summary>Answer</summary>

   **Justified** if the function is only ever called on strings you expect to
   be valid digits (the error is a genuine rare case). It buys: the `return
   -1` error path is laid out out-of-line, so the hot loop body is just
   `load, compare-range, multiply-add` straight-line — smaller, denser, one
   fewer forward-branch target in the hot region. Measured effect on this
   isolated loop: small (the range check predicts well). In a parser with 20
   such checks across the message, the cumulative `.text` compaction is
   real. If invalid input is actually common (untrusted feed), the hint is
   wrong — drop it or measure.
   </details>

2. Kisi ne likha: `if (ptr) [[likely]] use(ptr); [[assume(ptr != nullptr)]];`
   — dono lines. Kya galat/redundant/dangerous?

   <details><summary>Answer</summary>

   Redundant and dangerous together. The `if (ptr) [[likely]]` handles the
   null case safely (just skips `use`). But the following `[[assume(ptr !=
   nullptr)]]` tells the optimizer `ptr` is **never** null from here on — so
   it may **delete the `if (ptr)` check itself** (it "proved" ptr is
   non-null), and any later `if (!ptr)` handling. If `ptr` can actually be
   null, you now have a null deref / deleted safety code = UB. Pick one: if
   null is possible, keep the `if` and drop the `assume`. If null is a
   contract violation you've asserted in debug builds, drop the `if` and keep
   the `assume` (the caller guarantees non-null).
   </details>

3. Ek 500-line hot `dispatch()` function. PGO available nahi. Kaunse 3-4
   jagah `[[unlikely]]` lagana worth hai, aur kaise verify karoge fayda hua?

   <details><summary>Answer</summary>

   Mark `[[unlikely]]` on: (1) the input-validation / malformed-message early
   returns, (2) the "slow path" branches (resize, reallocate, recover from
   gap, escalate to full recompute), (3) error/exception-raising branches,
   (4) rare protocol cases (heartbeat, admin message) if the function also
   handles the common data messages. Leave the common data-message paths and
   any ~balanced branches un-hinted. Verify: `size` the object before/after
   (hot part of `.text` should shrink; total may grow as cold code moves to
   `.text.unlikely`), `./build.ps1 asm` to confirm those handlers are now
   after the main body, then `perf stat --topdown` on a replay — Frontend
   Bound / L1-icache-load-misses should drop if the function was I-cache
   pressured. If no counter moves, the hints are cosmetic here; keep only the
   clearly-correct error-path ones.
   </details>

---

## Interview questions

1. `[[likely]]`/`[[unlikely]]` — what they actually affect (layout, not prediction).
2. Why the measured speedup is usually small (HW predictor).
3. When branch hints genuinely help (big function, many rare checks, I-cache).
4. Wrong hint — what goes wrong.
5. `[[assume]]` / `std::unreachable()` vs `[[unlikely]]` — fact vs hint.
6. Why PGO beats manual hints, and what manual hints are still for.
7. `[[unlikely]]` on a random data-dependent branch — why it doesn't help.

---

## Next
→ [`09-aliasing-and-restrict.md`](09-aliasing-and-restrict.md)
