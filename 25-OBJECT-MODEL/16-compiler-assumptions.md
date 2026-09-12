# 16 — How the compiler exploits UB — real examples

## Prerequisites
- `15-undefined-behaviour-catalog.md`, `23-ERROR-HANDLING` file 13
- [`examples/08_ub_examples.cpp`](examples/08_ub_examples.cpp)

## Yeh topic abhi kyun
"UB is bad" abstract hai. Yeh file **concrete** karta hai: aapka null-check
**delete** ho jaata, aapka overflow-check `1` ban jaata, aapka loop
mis-vectorize hota. Mechanism ek hi hai — **compiler maan leta hai UB kabhi nahi
hoti**, aur us assumption ko poore program mein propagate karta (range analysis,
dead-code elimination, load/store optimization). Ye samajhna = UB ko seriously
lena.

---

## The mental model

> Jab compiler ko koi operation dikhta jo UB *ho sakti* hai, woh **maan leta hai
> woh input kabhi nahi aayega jispe UB hoti**. Phir woh us assumption ko facts ki
> tarah use karta hai baaki code optimize karne ke liye.

Isliye ek UB ek **local** galti nahi rehti — woh ek **global** assumption ban
jaati jo aapke doosre (bilkul theek) code ko bhi tod sakti.

---

## Case 1 — null-check removal

```cpp
int foo(int* p) {
    int x = *p;          // (A)
    if (p == nullptr)    // (B)
        return -1;
    return x + 1;
}
```

- `(A)` `*p` — agar `p == nullptr` hota, yeh UB. Compiler: "toh `p != nullptr`."
- `(B)` `if (p == nullptr)` — compiler ke facts ke hisaab se **hamesha false** →
  **dead code** → removed at `-O2`.
- Result: `foo(nullptr)` ab seedha null deref karta hai (crash) — aapka safety
  check *tha*, par compiler ne hata diya.

**Fix:** check **pehle**: `if (!p) return -1; int x = *p;`

`examples/08`'s `deref_then_check` — the `if (!p)` is dead at `-O2` (never call it
with `nullptr`).

---

## Case 2 — signed overflow makes checks vanish

```cpp
bool check(int x) { return x + 1 > x; }
```

- Signed overflow UB → compiler: "`x + 1` never overflows" → "`x + 1 > x` is
  **always true**" → `return 1;`.
- Measured (`examples/08`, GCC 15): `check(INT_MAX)` returns **`1` at every `-O`
  level** (GCC folds it in the frontend). Only `-fwrapv` (which *defines* signed
  overflow as wrap) gives `0`.

Related: `a + b < a` as an overflow check → folds to `false`. `abs(x) >= 0` →
`true` (even though `abs(INT_MIN)` is UB). `x * 2 / 2 == x` → `true`.

---

## Case 3 — overflow lets loops be assumed finite

```cpp
void sum(int* a, int n) {
    int total = 0;
    for (int i = 0; i <= n; ++i)   // note: <= (bug), and `i` is `int`
        total += a[i];
    // ...
}
```

- If `n == INT_MAX`, `i <= n` with `++i` overflows `i` → UB → compiler assumes
  **the loop terminates** → it can vectorize / unroll aggressively, assuming a
  finite trip count.
- With an **unsigned** or **`size_t`** counter (no overflow UB), the compiler must
  handle the wrap-around case → sometimes *less* optimization, but correct.
- The `<= n` is also an out-of-bounds read of `a[n]` (M1) — a second UB.

This is why GCC's manual recommends signed loop counters for auto-vectorization —
the "no overflow" assumption removes a wrap-around edge case the vectorizer would
otherwise have to guard.

---

## Case 4 — `[[assume]]` / `std::unreachable` propagate

```cpp
int div_by(int x, int d) {
    [[assume(d != 0)]];       // "trust me, d is never 0"
    return x / d;             // compiler skips the div-by-zero handling
}
```

- The compiler takes `d != 0` as a **fact** and optimizes on it — no check, tighter
  code. If you ever call `div_by(x, 0)`, it's UB (whatever the div instruction
  does, plus any optimizations that assumed `d != 0` elsewhere).

```cpp
switch (op) {
    case Add: return a + b;
    case Sub: return a - b;
    case Mul: return a * b;
}
std::unreachable();   // "op is always one of the above"
```

- The compiler drops the implicit "fell off the switch" path → no spurious return,
  tighter jump table. If `op` is ever a 4th value → UB.
- **Always `assert(false)` before `std::unreachable()`** (folder 23 file 12) — the
  assert catches it in debug/CI; `std::unreachable` optimizes release.

---

## Case 5 — strict aliasing reuses loads

```cpp
int trap(int* ip, long* lp) {
    int a = *ip;
    *lp = 0x11223344L;
    int b = *ip;        // compiler: int* and long* can't alias -> b == a
    return b - a;       // -> return 0;
}
```

- `examples/06` measured: `-O2 -fstrict-aliasing` → `delta == 0` even when `ip`
  and `lp` point at the same bytes. The compiler didn't re-load `*ip` because it
  "knew" the `long` store couldn't touch an `int`.
- File 10 — fix with `bit_cast` / `memcpy`.

---

## Case 6 — DCE of a whole function / branch

```cpp
int compute(int i) {
    if (i < 0)
        return a[i];      // negative index -> UB
    return a[i];
}
```

- The `i < 0` branch does `a[i]` with a negative index → UB → the compiler can
  assume `i >= 0` on entry → the `if (i < 0)` branch is dead → removed.
- Extreme version: if a function *always* hits UB, the compiler may replace its
  body with `ud2` (illegal instruction) or nothing, since "it's never called
  validly."

---

## Why no warning (usually)

- These optimizations happen in the **middle end** (GIMPLE/SSA passes) after the
  frontend. The compiler doesn't "know it's deleting your safety check" — it just
  sees a provably-`false` condition.
- Some do warn: `-Wnull-dereference`, `-Warray-bounds`, `-Wstrict-aliasing`
  (with `-O2`), `-Waggressive-loop-optimizations`, `-Wunsafe-loop-optimizations`.
  These catch a fraction. **Sanitizers** (`-fsanitize=undefined,address`) are the
  reliable detector — they inject a runtime check *before* the UB op.

---

## Andar kya hota hai

- The optimizer maintains **value ranges** and **assertions** for each SSA value.
  A dereference asserts "pointer non-null and points at a valid object of the
  right type." A signed `+` asserts "no overflow." A division asserts "divisor
  non-zero."
- **Jump threading / conditional constant propagation** then uses those assertions
  to prove branches dead and eliminate them.
- **Type-based alias analysis (TBAA)** uses the strict-aliasing rule to decide two
  memory ops don't conflict → enables load reuse, store sinking, reordering,
  vectorization.
- **Loop optimizations** (`-ftree-loop-*`) assume no signed-overflow wrap in the
  induction variable → simpler trip-count analysis.
- `-fwrapv` / `-fno-strict-aliasing` / `-fno-delete-null-pointer-checks` each turn
  off one family of these assumptions (at a perf cost).

---

## > **HFT relevance**
> HFT is where these assumptions bite hardest because the builds are
> `-O2`/`-O3 -march=native -flto` — maximum assumption propagation. A UB that's
> invisible at `-O0` can, at release optimization:
> - **Delete a risk check** (null / bounds) because an unrelated deref implied the
>   pointer was valid.
> - **Fold an overflow guard** on a position/notional accumulator to a constant.
> - **Mis-vectorize** a hot market-data loop because a signed counter "can't
>   wrap."
> - **Reuse a stale load** across a differently-typed store (aliasing) — wrong
>   price / wrong quantity.
>
> Defence: **sanitizers in CI** (every hit is a bug), **fixed-width integers +
> explicit overflow checks** (`__builtin_*_overflow`), **`std::bit_cast`/`memcpy`**
> for all punning, **`assert` before `std::unreachable`/`[[assume]]`**, **bounds
> discipline** on the hot path (invariant-guaranteed, `assert`ed in debug). Treat
> `-Wnull-dereference -Warray-bounds -Wstrict-aliasing` as errors.

---

## Hands-on

```bash
./build.ps1 25-OBJECT-MODEL/examples/08_ub_examples.cpp                    # -O0
g++ -std=c++20 -O2 25-OBJECT-MODEL/examples/08_ub_examples.cpp -o u2 && ./u2
g++ -std=c++20 -O2 -fwrapv 25-OBJECT-MODEL/examples/08_ub_examples.cpp -o uw && ./uw
```

- Case 2: `overflow_check` = `1` normally, `0` with `-fwrapv`.
- `g++ -O2 -S` the null-check example → see the `if (!p)` gone.
- `-O2 -fdump-tree-optimized` → the SSA form with the dead branch removed.
- Case 5: `examples/06`'s `aliasing_trap` `delta` = `0` at `-O2`, real at
  `-O0`/`-fno-strict-aliasing`.

---

## ⚠️ Traps

### Trap 1 — safety check *after* the UB op
```cpp
int v = *p; if (!p) return err;   // ⚠️ check removed — put it BEFORE *p
```

### Trap 2 — `x + 1 > x` / `a + b < a` overflow checks
Folded away. `__builtin_add_overflow`.

### Trap 3 — `[[assume]]` / `std::unreachable` without a debug assert
If the assumption is ever false → UB propagates. `assert(cond)` /
`assert(false)` first.

### Trap 4 — trusting `-O0` behaviour
`-O0` doesn't propagate these assumptions much. Test at `-O2`+.

### Trap 5 — `-fno-strict-aliasing` / `-fwrapv` as a fix
They mask the bug and cost performance. Fix the code.

### Trap 6 — assuming the compiler warns
Most of these are silent. Sanitizers + the specific `-W` flags catch a subset.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "UB affects only the buggy line" | The assumption propagates — it can delete *other*, correct code |
| "the compiler warns when it removes my check" | Usually silent; some `-W` flags + sanitizers catch a fraction |
| "`x + 1 > x` is a valid overflow test" | Folded to `1` (signed overflow assumed impossible) |
| "`-O0` and `-O2` behave the same for UB" | `-O2` propagates assumptions aggressively; `-O0` mostly doesn't |
| "null-check after deref still protects" | The deref implied non-null → the check is dead code |
| "aliasing UB is theoretical" | `examples/06` shows a wrong runtime answer at `-O2` |

---

## Exercises

1. **Predict `-O2`:** `int f(int* p){ if(!p) return 0; return *p; } int g(int* p){
   int v = *p; if(!p) return 0; return v; }` — behaviour of `f(nullptr)` and
   `g(nullptr)`?

   <details><summary>Answer</summary>

   `f(nullptr)` → `0` (check is before the deref, honoured). `g(nullptr)` → crash
   — the `*p` implies `p != nullptr`, so `if(!p)` is dead code removed at `-O2`;
   the null deref happens.
   </details>

2. **Why does it return 1?** `bool safe(int a){ return a + 100 > a; }` at `-O2`.

   <details><summary>Answer</summary>

   Signed overflow is UB → the compiler assumes `a + 100` never overflows → `a +
   100 > a` is always true → `return 1;`. For `a` near `INT_MAX` the "check" does
   nothing. Use `a <= INT_MAX - 100` or `__builtin_add_overflow`.
   </details>

3. **Loop:** why might `for (int i = 0; i <= n; ++i) sum += a[i];` be *faster* but
   *wrong* than the `size_t` version?

   <details><summary>Answer</summary>

   With `int i` and signed-overflow UB, the compiler assumes the induction
   variable never wraps → simpler trip-count analysis → more aggressive
   vectorization/unrolling. It's "wrong" because `<= n` reads `a[n]` (OOB) and, if
   `n == INT_MAX`, the loop's termination relies on UB. The `size_t` version is
   correct but the compiler must handle wrap-around, sometimes optimizing less.
   </details>

4. **`std::unreachable` misuse:** `int classify(int x){ if(x>0) return 1; if(x<0)
   return -1; std::unreachable(); }` — what if `x == 0`?

   <details><summary>Answer</summary>

   `std::unreachable()` is reached → UB. The compiler assumed `x` is never `0`, so
   `classify(0)` does whatever the pruned code path leads to (garbage return,
   fall-through into the next function, etc.). Fix: handle `x == 0` (return `0`),
   or `assert(x != 0)` before `std::unreachable()`.
   </details>

5. **Aliasing:** rewrite `float fast_inv(float x){ int i = *(int*)&x; i = 0x5f3759df
   - (i >> 1); return *(float*)&i; }` to be UB-free.

   <details><summary>Answer</summary>

   ```cpp
   float fast_inv(float x) {
       std::int32_t i = std::bit_cast<std::int32_t>(x);
       i = 0x5f3759df - (i >> 1);
       return std::bit_cast<float>(i);
   }
   ```
   Same bit trick (the classic fast inverse sqrt), but via `std::bit_cast` — no
   strict-aliasing UB, so `-O2 -flto` can't miscompile it.
   </details>

---

## Interview questions

1. "Compiler assumes UB can't happen" — samjhao with the null-check example.
2. `x + 1 > x` (signed) `-O2` pe kya, kyun?
3. Signed vs unsigned loop counter — vectorization pe kya farq?
4. `[[assume]]` / `std::unreachable` — kaise propagate hote, kis ke saath pair karein?
5. Strict aliasing se load reuse — `examples/06`'s `delta == 0` kyun?
6. Ye optimizations warn kyun nahi karti? Detect kaise?
7. `-fwrapv` / `-fno-strict-aliasing` — kya karte, kyun "fix" nahi?

---

## Next
→ [`17-exercises.md`](17-exercises.md)
