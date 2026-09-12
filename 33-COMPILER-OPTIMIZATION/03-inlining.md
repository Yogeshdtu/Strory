# 03 — Inlining: heuristics, `inline`, `always_inline`, `noinline`

## Prerequisites
- `08-FUNCTIONS/05-the-call-stack.md` (call/ret cost)
- `24-COMPILATION-LINKING/13-*` (ODR, `inline` linkage)
- `02-godbolt-workflow.md`

## Yeh topic abhi kyun
Inlining ek chhota transform lagta hai — "call ki jagah body paste karo" —
par yeh **sabse important enabling optimization** hai. Call overhead khatm
hona chhota fayda hai; asli baat yeh ki inline hone ke baad
constant-propagation, CSE, dead-code, vectorization, aur devirtualization ab
function boundary ke **paar** chal sakte hain. Isliye header-only hot code
aur LTO (lesson 10).

---

## `inline` keyword ≠ "inline karo"

C++ mein `inline` ka **primary meaning ODR hai**: "is function/variable ki
multiple definitions across TUs allowed hain (agar identical hon)". Isliye
header mein function define kar sakte ho. Compiler ko `inline` ek *hint*
deta ki "shayad inline karna chahoge", par woh **ignore kar sakta** hai, aur
`inline` na hone par bhi inline kar sakta hai (jo woh routinely karta hai).

```cpp
inline int f() { return 42; }   // "OK to define in a header"; inlining is separate
```

**Actual inlining** compiler ki heuristics decide karti hain:

| Factor | Inline ki taraf | Against |
|---|---|---|
| body size (instructions/estimated cost) | chhota | bada |
| call site count | ek/kum | bahut (bloat) |
| hotness (profile / loop-nested) | hot | cold |
| has variadic / `alloca` / SEH / recursion | — | ye inline block karte |
| `static` / anon-namespace, only caller | strongly yes | — |
| across TU boundary (no LTO) | **can't** (definition not visible) | — |
| `always_inline` attr | force | — |
| `noinline` attr / `-fno-inline` | — | force off |

GCC knobs: `-finline-limit=N`, `--param inline-unit-growth=N`,
`--param max-inline-insns-single=N`. Mostly leave them; tune only with data.

---

## Measured — example `02` (is box, ~2 GHz)

Tiny body `x*k + (x>>3)` in a 400M carried loop:
```
  noinline       : 1.31 ns/iter     <- real `call` every iteration
  normal         : 0.97 ns/iter     <- compiler inlined it (default)
  always_inline  : 1.00 ns/iter
  noinline / inline = 1.31x
```

Yahan `normal == always_inline` — compiler ne khud hi is tiny hot body ko
inline kiya (jaisa expected). `noinline` version ~1.3x slower: per iteration
ek `call` + `ret` + arg into `edi`/`esi` + result from `eax` + the OoO
engine can't optimize across the call. ~0.34 ns/iter overhead on a ~1 ns body.

**Yeh overhead ka fayda hai. Bada fayda measured nahi dikhta yahan** kyunki
body itna simple hai ki inline hone pe bhi kuch aur fold/vectorize nahi
hota. Example `04` (aliasing) aur `06` (LTO) mein dekho: inline hone ke baad
`__restrict`-style analysis + cross-TU folding se **2-3.5x** aata hai — woh
inlining ka *asli* value hai.

---

## Kyun inlining "enabling" hai

```cpp
int scale(int x) { return x * 4; }
void use(int* a, int n) {
    for (int i = 0; i < n; ++i) a[i] = scale(i) + scale(3);
}
```
- Inline nahi → `scale(i)` aur `scale(3)` dono real calls. `scale(3)` har
  iteration recompute (compiler nahi jaanta yeh invariant / pure hai — actually
  `-fipa-pure-const` isse const detect kar sakta, par simple example ke liye).
- Inline → `a[i] = (i << 2) + 12;` — `scale(3)` **constant-folded** to 12,
  `scale(i)` **strength-reduced** to a shift, loop **vectorizes**.

Har real optimization (const-fold, CSE, DCE, LICM, vectorize, devirtualize —
lesson 07) ek function ke andar kaam karti hai. Inlining function boundary
ko mita ke unhe bade scope pe chalne deta.

---

## `always_inline` / `noinline` — kab

### `[[gnu::always_inline]]` (ya `__attribute__((always_inline))`)
- Sirf jab measured proof ho ki compiler galat decide kar raha (ek hot tiny
  function jise woh size-heuristic pe skip kar raha).
- SIMD wrapper functions (`_mm256_*` wrappers) — inline na hone pe har call
  register spill.
- ⚠️ recursion / very large body pe error ya bloat.
- Force karta bhale `-O0`? Nahi — `always_inline` `-O0` pe bhi try karta par
  guarantee nahi.

### `[[gnu::noinline]]`
- **Benchmarking** — ek function ko measure karne ke liye use "opaque" rakhna
  (example `04`/`05` yahi karte).
- **Code layout** — ek cold error handler ko hot path se bahar rakhna (isse
  hot `.text` compact).
- **Debugging** — ek frame ko backtrace mein visible rakhna.
- Breaking an over-inlining bloat problem the profiler flagged.

### `[[gnu::flatten]]`
Function ke andar ke **saare** calls ko inline karo (jahan possible). Ek hot
top-level dispatch function pe use hota — poora subtree ek unit ban jaata.
Bloat risk high.

---

## Cross-TU: the LTO gap (lesson 10 detail)

Bina LTO ke, `main.cpp` `math.cpp` ke function ki **definition nahi dekh
sakta** → inline impossible, chahe function kitna chhota ho. `-flto` compile
ke object files mein GIMPLE bytecode daal deta, aur link stage pe optimizer
poore program ko dekh ke cross-TU inline karta.

Example `06`: `hot_transform` (defined in `mathx.cxx`) ko `main.cxx` ke
throughput loop mein call karna — **NO LTO 1.75 ns/elem, WITH LTO 0.76
ns/elem (~2.3x)**. LTO ne cross-TU inline kiya → call gaya + body schedule
hua.

Isliye header-only libraries (Boost, Eigen, `{fmt}` header mode) fast: sab
kuch har TU ko visible.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `inline` likh ke "ab yeh inline hoga" maan lena
`inline` = ODR permission. Inlining decision alag. `always_inline` = force.

### Trap 2 — bade function pe `always_inline`
Har call site pe poora body copy → `.text` bloat → I-cache misses → **net
slower**, aur compile time badhta. Measure.

### Trap 3 — virtual function inline karne ki koshish
`virtual` call indirect hai — target runtime pe pata chalta → inline nahi
hota (jab tak compiler devirtualize na kare, lesson 07). `inline` keyword on
a virtual = ODR only.

### Trap 4 — cross-TU hot path bina LTO
Ek hot function alag `.cpp` mein, no `-flto` → har call opaque. Ya header mein
le jao (`inline`), ya `-flto` on.

### Trap 5 — `noinline` bhool jaana benchmark mein
Micro-benchmark mein function measure kar rahe ho par compiler ne use inline
+ hoist kar diya → 0 ns. `[[gnu::noinline]]` + `DoNotOptimize` (lesson 14).

### Trap 6 — recursion + `always_inline`
Self-recursive function inline nahi ho sakta (infinite). `always_inline` pe
GCC warning/error deta. Mutual recursion bhi. Tail-recursion ko compiler
loop bana deta (`-O2`) — woh alag.

---

## > **HFT relevance**

> - **Hot path = header-only + `inline` + `-flto`.** Order book, matcher,
>   parser inner functions ko visible rakho har hot TU ko. Cross-TU `call`
>   on the critical path = wasted budget + a wall the optimizer can't see past.
> - **SIMD helper wrappers `always_inline`.** A `load8(p)` / `hadd(v)` wrapper
>   that isn't inlined spills 4 vector registers per call.
> - **Cold paths `noinline`.** Error handling, slow-path recovery, logging —
>   pull them out of the hot function body so the hot `.text` stays in L1i.
> - **Watch for over-inlining regressions.** After a template-heavy refactor,
>   `size` the hot object; if `.text` ballooned and `perf` shows Frontend
>   Bound up, `noinline` the biggest offenders or lower `--param
>   max-inline-insns-*`.
> - **PGO (lesson 11) drives inlining by real hotness** — better than manual
>   attributes at scale.

---

## Hands-on

```bash
./build.ps1 fast 33-COMPILER-OPTIMIZATION/examples/02_inlining_demo.cpp
./build.ps1 asm  33-COMPILER-OPTIMIZATION/examples/02_inlining_demo.cpp | grep -n "call\|imul\|shr"
#   noinline loop: `call _Z...`  ;  inline loop: `imul`/`shr` inline, no call

# what did GCC decide? (per-callsite inline report)
g++ -O2 -fopt-info-inline 33-COMPILER-OPTIMIZATION/examples/02_inlining_demo.cpp -o x 2>&1 | head
# why NOT inlined:
g++ -O2 -fopt-info-inline-missed ... 2>&1 | head

# LTO effect:
cd 33-COMPILER-OPTIMIZATION/examples/06_lto_demo && ./build.ps1   # (or ./build.sh)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`inline` keyword = inline hoga" | ODR permission; inlining is a separate compiler decision |
| "inlining = call overhead bachana" | that's minor; the win is cross-boundary const-fold/CSE/vectorize |
| "`always_inline` sab jagah lagao" | big bodies → `.text` bloat → I-cache misses → slower |
| "virtual + `inline` = inlined virtual" | indirect call; inline only via devirtualization |
| "cross-TU chhota function inline hoga" | no — definition not visible without `-flto` |
| "compiler ki inline heuristic buri hai" | mostly good; override only with measured data |

---

## Exercises

1. `static int helper(int x) { return x*x + 1; }` ek `.cpp` mein, ek hi jagah
   se call hota. `always_inline` chahiye?

   <details><summary>Answer</summary>

   **Nahi.** `static` + single call site + tiny body → the compiler will
   inline this at `-O1`+ every time, and then likely delete the standalone
   `helper` entirely. `always_inline` adds nothing and, if you later add a
   second hot call site where inlining *would* bloat, forces a possibly-bad
   choice. Trust the default; reach for `always_inline` only when
   `-fopt-info-inline-missed` shows a hot tiny function being skipped (usually
   because its estimated cost tripped a `--param` limit).
   </details>

2. Ek 300-line `process_message()` function jo har tick chalta. Tumne use
   `[[gnu::flatten]]` laga diya, ab binary 40% bada aur throughput 8% *kam*.
   Kya hua, fix?

   <details><summary>Answer</summary>

   `flatten` inlined **every** callee (parsers, validators, the slow-path
   handlers) into the one function → its `.text` exploded → it no longer
   fits L1i / spans more iTLB entries → frontend stalls on every tick (lower
   IPC, higher Frontend Bound in `perf --topdown`). Fix: remove `flatten`.
   Inline only the genuinely hot, small helpers (`[[gnu::always_inline]]` on
   those specifically, or just let the default do it), and force the rare
   handlers `[[gnu::noinline]]` so they stay out-of-line and cold. Re-measure
   `.text` size + Frontend Bound.
   </details>

3. Do TUs: `book.cpp` defines `apply_update()` (20 lines, hot), `engine.cpp`
   calls it in the tick loop. `perf annotate` shows a `call` on the hot path.
   Do fixes, trade-offs.

   <details><summary>Answer</summary>

   (1) **Move `apply_update` into a header** as `inline` (or a header-only
   `.hpp` included by `engine.cpp`) → visible → inlined → the optimizer sees
   through it (const-fold the tick size, keep book pointers in registers).
   Trade-off: `book.hpp` now heavier to include / recompile; watch build
   times. (2) **Enable `-flto`** for the release build → the linker-stage
   optimizer inlines `apply_update` across the TU boundary without moving
   code. Trade-off: slower final link, and you must build *all* TUs with
   `-flto` and matching flags. For HFT both are standard: hot code header-only
   *and* `-flto` on. Verify with `./build.ps1 asm` / `perf annotate` that the
   `call` is gone.
   </details>

---

## Interview questions

1. `inline` keyword ka C++ mein primary meaning (ODR), aur actual inlining se farak.
2. Inlining ki 3 enabling optimizations (kya inline hone ke baad ab possible hota).
3. Compiler ki inline heuristic ke 3 inputs.
4. `always_inline` kab justified, aur uska risk.
5. `noinline` ke 3 legitimate uses.
6. Cross-TU inlining kyun `-flto` ke bina impossible.
7. Over-inlining ka symptom `perf` mein, aur fix.

---

## Next
→ [`04-loop-optimizations.md`](04-loop-optimizations.md)
