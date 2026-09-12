# 12 — Branch-free programming: cmov, masks, tables — and when NOT

## Prerequisites
- **`31-CPU-ARCHITECTURE/03-04-07-08`** (branch prediction, mispredict cost,
  branch-free, if-conversion), **`33-COMPILER-OPTIMIZATION/08`** (branch hints)
- `examples/06_branchless.cpp`

## Yeh topic abhi kyun
Ek mispredicted branch ~10–15 cycles flush (folder 31). Ek hot path pe ek
**unpredictable** branch har iteration = a jitter source. Branchless code
usko hataata — **par sirf tab jab branch genuinely unpredictable ho.**
Predictable branch pe branchless **dheema** hai. This is the most-abused
"trick" — so this lesson leads with the trade-off.

---

## Techniques

### 1. Compiler if-conversion (often automatic at `-O2`)
```cpp
int m = (a > b) ? a : b;          // -> cmovg  (no branch)
s += (x > t) ? x : 0;             // -> cmov / masked add
```
GCC/Clang at `-O2` **already** turn simple `if`/ternary bodies into `cmov`.
`06_branchless.cpp` measured: the "branchy" `if (a[i] > t) s += a[i]` gave
**identical** times on sorted vs unsorted data → it was if-converted, there
was no real branch. **Check the asm** (`./build.ps1 asm` — `cmovg` vs `jg`)
before assuming your `if` costs a mispredict.

### 2. Bit masks
```cpp
int32_t mask = -(int32_t)(x > t);         // 0x00000000 or 0xFFFFFFFF
s += x & mask;                             // add x if x>t, else add 0
y = (a & mask) | (b & ~mask);             // select a or b, branchless
```
Masks compose and **vectorize** — `06` measured the mask version ~0.26
ns/elem (SIMD, ~2 elem/cyc) vs branchy 0.32.

### 3. Lookup tables (data-driven dispatch)
```cpp
static constexpr int64_t TAB[5] = {1, 3, 7, 15, 31};
s += TAB[cls[i]];                          // vs a 5-way switch
```
`06` measured: **~12×** (table 0.34 vs switch 3.99 ns/elem) on **random**
`cls[i]` — the switch's jump table is an *unpredictable indirect jump*
(mispredict every element); the array load is predicted + vectorizes. For
data-driven multi-way selection, `TAB[key]` >> `switch`.

### 4. Arithmetic instead of branches
```cpp
int clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }  // 2 cmov
abs:  x ^ (x>>31)  -  (x>>31)                     // branchless (34/13 A3)
sign: (x > 0) - (x < 0)
min:  b ^ ((a ^ b) & -(a < b))
```

### 5. `[[likely]]` / `[[unlikely]]` — *keep* the branch, hint its layout
When a branch **is** predictable (an error check hit ~never), you don't want
branchless — you want the branch, laid out so the common path is
straight-line and the rare path is out-of-line (folder 33/08, lesson 21).
`[[unlikely]]` does that. Branchless would force the rare work every time.

---

## The decision

```
  Is the branch on the hot path taken ~50/50 with no pattern (data-dependent,
  e.g. "is this order a buy?", "did the price cross?")?
        |
    YES ── branchless (mask / cmov / table). Measure: it should win.
        |
    NO (predictable: taken ~always or ~never — a config flag, an error check,
        a warm-up guard)
        |
        └── keep the branch + [[likely]]/[[unlikely]] + out-of-line the rare
            side (lesson 21). Branchless here is SLOWER (does both sides +
            longer dep chain).
```

Measured proof (`06`, sorted data): branchless mask/ternary **same** as their
unsorted times, while the (if-converted) branchy version was also flat — so
on predictable data there's no misprediction to save, and the branchless
extra work is pure cost. On a *real* branch (not if-converted) with
predictable data, branchless would be visibly slower.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — assuming your `if` costs a mispredict
`-O2` if-converts simple ones. Check the asm; if it's already `cmov`,
there's nothing to "fix".

### Trap 2 — branchless on a predictable branch
Does both sides + longer dependency chain → slower than a well-predicted
branch. Only for genuinely unpredictable branches.

### Trap 3 — branchless that doesn't vectorize
The win from masks is often the *vectorization* they enable. If the loop
can't vectorize anyway (carried dependency, function call), a `cmov` branch
is just as good and clearer.

### Trap 4 — a big `switch` on random data
Jump table = unpredictable indirect jump = mispredict/element. `TAB[key]`
if the cases are just "return a value"; a computed-goto / perfect-hash for
more complex cases.

### Trap 5 — unreadable bit tricks with no comment / no measurement
`b ^ ((a ^ b) & -(a < b))` with no comment and no benchmark showing it beat
`std::min` → delete it. Every trick needs a measured justification and a
comment (spec Rule 13).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "branchless is always faster" | only for unpredictable branches; else slower |
| "my `if` = a mispredict" | `-O2` often if-converts it to `cmov` — check asm |
| "switch compiles to a fast jump table" | on random data that's an unpredictable indirect jump |
| "`[[unlikely]]` = branchless" | it *keeps* the branch, moves the cold side out of line |
| "clever bit trick, ship it" | needs a measured win + a comment, or delete it |

---

## Exercises

1. Ek hot loop mein `if (side == Side::Buy) bid_qty += q; else ask_qty += q;`.
   `side` roughly 50/50 aur random. Tumne ise branchless kiya:
   `bool b = (side == Side::Buy); bid_qty += q & -(int64_t)b; ask_qty += q &
   -(int64_t)(!b);` aur measure kiya — **koi farak nahi**. Kyun ho sakta,
   aur kya check karo?

   <details><summary>Answer</summary>

   Two likely reasons:
   (1) **`-O2` already if-converted the original.** `if (b) x += q; else
   y += q;` with two simple accumulators is a textbook if-conversion → two
   `cmov`s (or a masked pair) with no branch. So the "branchy" version had
   no misprediction to begin with, and your manual mask version does the
   same work → identical. Check `./build.ps1 asm` — look for `cmov` in the
   original's loop body. If it's there, there was nothing to fix.
   (2) **The loop is bound by something else** — if `q` comes from a
   dependent chain, or the loop is memory-bound loading `side`/`q` from a
   large array (each a cache miss), the branch cost (predicted or not) is
   hidden behind the stall. `perf stat` — is it Backend/Memory bound? Then
   the branch isn't your problem.
   Also: if `side` is *actually* predictable in production (e.g. bursts of
   same-side quotes), the branchy version wins there and your synthetic
   50/50 test is misleading. Test with production-representative data.
   </details>

2. Ek 16-way `switch (msg_type)` hot path pe hai; `msg_type` distribution
   skewed hai — 90% ek type (`Quote`), 10% split among 15 others. Branchless
   table? `[[likely]]`? Kya karo?

   <details><summary>Answer</summary>

   90/10 skew → the branch predictor / branch target predictor will do
   **very well** on this (it'll predict `Quote` almost every time). So:
   (1) **Don't** convert the whole thing to a branchless `TAB[msg_type]` of
   function pointers — that's an *indirect call* per message that the
   predictor now can't specialize (it varies 10% of the time), and you lose
   inlining of the common `Quote` path.
   (2) **Do**: peel the common case out with a hint —
   `if (msg_type == Quote) [[likely]] { handle_quote(...); return; }` then a
   `switch` (or table) for the rare 15. Now the `Quote` path is a single
   predicted branch + an inlined `handle_quote`, straight-line; the rare
   dispatch (10% of traffic) can be a jump table or an if-chain, its
   mispredict cost amortized over rare messages.
   (3) Put the rare handlers out-of-line (`[[gnu::cold]]`, lesson 21) so
   they don't bloat the hot `Quote` path's I-cache footprint.
   General rule: skewed distribution → **peel + hint the common case**,
   don't flatten everything to a uniform-cost branchless dispatch.
   </details>

---

## Interview questions

1. When branchless wins vs loses — the predictable/unpredictable decision.
2. `-O2` if-conversion — how to tell if your `if` already became `cmov`.
3. Bit-mask select (`(a & m) | (b & ~m)`) — and why the real win is vectorization.
4. A `switch` on random data — why it can be slow; `TAB[key]` alternative.
5. `[[unlikely]]` vs branchless for a rare error check — why they're opposite tools.
6. Skewed dispatch distribution — peel + hint, not flatten.

---

## Next
→ [`13-virtual-dispatch-elimination.md`](13-virtual-dispatch-elimination.md)
