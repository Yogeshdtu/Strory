# 01 — Optimization levels: `-O0` `-O1` `-O2` `-O3` `-Os` `-Ofast`

## Prerequisites
- `32-CACHE-MEMORY-PERFORMANCE` (kya optimize karna hai woh samajh)
- `24-COMPILATION-LINKING/15-*` (release build config ka intro)

## Yeh topic abhi kyun
Compiler flag `-O<n>` ek switch nahi — woh **sau se zyada individual passes**
ka bundle hai. Kaunsa bundle kab, aur kya trade-off — yeh jaane bina aap ya
to development mein `-O2` chala ke debugging ka time barbaad karoge, ya
production mein `-O0` ship karke 5-20x performance chhod doge, ya `-Ofast`
laga ke silently galat numbers produce karoge.

---

## Har level kya karta

| Flag | Iska matlab | Kab |
|---|---|---|
| **`-O0`** | koi optimization nahi (default). Har variable stack pe, har statement literal. | **debugging** — 1:1 source↔asm, breakpoints/watch reliable, fast compile |
| **`-O1`** | "obvious" wins: dead code elimination, basic CSE, register allocation, simple inlining, jump threading. Compile fast rehta. | jab `-O2` debugging todta ho par `-O0` bahut dheema |
| **`-O2`** | + aggressive inlining, loop opts (unroll/rotate/unswitch), auto-vectorization (GCC 12+), if-conversion → `cmov`, instruction scheduling, tail-call. **Production default.** | **release builds** |
| **`-O3`** | + aggressive vectorization (bigger cost model), loop interchange (`-ftree-loop-interchange`), unroll-and-jam, function cloning (`-fipa-cp-clone`), `-fpredictive-commoning`. | jab profile kahe koi loop hot hai + `-O3` measurably tez (aksar `-O2` ke barabar ya code-bloat se ULTA) |
| **`-Os`** | `-O2` minus jo size badhata (aggressive unroll, some inline, alignment padding). | I-cache/flash-bound code (embedded, huge hot `.text`); kabhi `-O2` se tez |
| **`-Oz`** (Clang) / `-Os` harder | size ke liye speed bhi sacrifice | firmware |
| **`-Ofast`** | `-O3` + **`-ffast-math`** + `-fallow-store-data-races` + ... | ⚠️ IEEE float rules todta (lesson 13). Sirf jaan-boojh ke, reproducibility chhod ke |
| **`-Og`** | `-O1` minus jo debugging todta. "optimized but debuggable". | dev builds jahan `-O0` bahut dheema |

`-O` levels **cumulative nahi hain** exactly — `-O2` `-O1` ka superset hai, par
`-Os` `-O2` se kuch cheezein *hataata* hai. `-O3` `-O2` ka superset hai par
har workload pe faster nahi.

---

## Measured — example `01` (is box, AMD Zen 2 ~2 GHz)

Ek 16.7M-iteration reduction loop (`s += mix(a[i]) + j`), `mix` ek chhota hash:

```
 -O0 : 80.2 ms/pass      <- baseline
 -O1 : 13.4 ms/pass      <- ~6x faster  (THE big cliff)
 -O2 : 13.5 ms/pass      <- ~same as -O1 for this workload
 -O3 : 13.3 ms/pass      <- ~same
 -Os : 13.5 ms/pass      <- ~same
```

**Sabak:** `-O0 → -O1` hi asli jump tha (~6x) — registers + inline `mix` +
dead code + basic CSE. `-O1 → -O2 → -O3` is workload pe kuch nahi de rahe,
kyunki `mix` ka data-dependent shift/xor vectorize nahi hota aur reduction
already tight hai. **Jo code -O2/-O3 se fayda uthata hai woh alag shape ka
hota** — dense arrays, no carried dep (example `03` MAP: -O2 vectorization ne
3.5x diya). Isliye: **level chuno workload dekh ke, aur measure karo.**

---

## `-O0` itna dheema kyun

`-O0` pe compiler har local variable ko **memory (stack) mein** rakhta, aur
har statement ke baad use likh deta / har use pe padh leta. Ek `for (int i =
0; ...)` loop mein `i` har iteration 2-3 baar stack se load/store hota. Koi
inlining nahi → har `mix()` ek real `call`. Koi CSE → `a[i * N + j]` ka
address har baar recompute. Isliye 5-20x slower is normal.

`-O0` ka **fayda**: debugger mein `p i` hamesha sahi value dikhata, `step`
source lines pe rukta, `<optimized out>` nahi milta.

---

## `-O2` ke andar (kya "aggressive" hai)

`gcc -O2 -Q --help=optimizers` se poori list. Highlights:
- **Inlining** — `-finline-functions`, size + hotness heuristic (lesson 03)
- **Loop** — `-funroll-loops` (no, `-O2` pe nahi; `-O3` ya explicit), rotate,
  unswitch, invariant hoisting, strength reduction (lesson 04)
- **Vectorize** — `-ftree-vectorize` (GCC 12+ `-O2` pe ON; before → `-O3`)
- **`-ftree-loop-if-convert`** — `if (c) x = a` → `cmov` (folder 31/32 ne dekha:
  yeh branch-prediction demos ko "defeat" karta)
- **`-fipa-*`** — inter-procedural: constant propagation across calls, pure/const
  detection, `-fipa-icf` (identical code folding)
- **`-fschedule-insns2`** — instruction scheduling for the pipeline
- **`-fstrict-aliasing`** — TBAA (lesson 09) — ON at `-O2`+

---

## `-O3` kab ULTA padta hai

1. **Code bloat → I-cache pressure.** Aggressive unroll + cloning bade
   functions ko aur bada karta. Agar hot loop already I-cache-bound tha,
   `-O3` (ya `-Os` se bhi bura) → slower. Ye real hai bade codebases mein.
2. **Bad vectorization.** `-O3` ka cost model kabhi ek loop vectorize kar
   deta jahan setup/teardown + gather actual gain se zyada — net loss.
   `-fopt-info-vec` dekho, aur measure.
3. **AVX-512 downclock** (folder 31 lesson 13) — `-O3 -march=native` pe ek
   512-bit loop frequency gira ke poore program ko dheema kar sakta.

**Regel:** production default `-O2`. `-O3` sirf specific hot files pe (per-file
`-O3` via `#pragma GCC optimize` ya build-system), aur sirf agar A/B measurement
gain dikhaye. `-O3` blanket-lagana cargo cult hai.

---

## Per-file / per-function level override

```cpp
// ek function ko alag level pe
__attribute__((optimize("O3", "unroll-loops")))
void hot_kernel() { ... }

// poori file (GCC), top of file
#pragma GCC optimize("O3")

// GCC/Clang: ek region
#pragma GCC push_options
#pragma GCC optimize("O3")
// ... hot code ...
#pragma GCC pop_options
```

⚠️ `optimize` attribute GCC pe kuch flags ko silently drop karta
(`-Wattributes` warning) — build-system se per-file `-O3` zyada reliable.

---

## Debug + optimize saath

```bash
g++ -O2 -g file.cpp        # optimized WITH debug info -- yeh normal release hai
```
`-g` optimization ko affect nahi karta — sirf DWARF debug tables add karta
(binary bada, runtime same). Release binaries **hamesha `-g` ke saath** build
karo (phir alag `.debug` file mein `objcopy --only-keep-debug` se nikaal lo)
— warna production crash ka backtrace bekaar.

`-O2 -g` mein debugging "lossy" hoti: variables kabhi `<optimized out>`,
line numbers jump karte (scheduling), inlined frames. `-Og` beech ka.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `-O0` benchmark
Folder 31/32 ne baar-baar dekha: `-O0` pe har number stack traffic hai,
CPU behaviour nahi. **Benchmark = `-O2` minimum**, hamesha.

### Trap 2 — `-O3` blanket "kyunki bada number achha"
Measure. Aksar `-O2` ke barabar; kabhi bloat se ULTA. `-O3` targeted use karo.

### Trap 3 — `-Ofast` / `-ffast-math` production mein
Silently `NaN`/`Inf` handling, associativity, denormals todta (lesson 13).
Ek din koi edge case galat answer dega. Sirf jaanke, isolated.

### Trap 4 — release build bina `-g`
Crash backtrace / `perf` symbols chahiye → `-g` hamesha. Strip separately.

### Trap 5 — level mismatch across TUs
Ek TU `-O0`, doosra `-O2`, aur `inline` function dono mein — ODR/IFNDR
territory (folder 24). Poora project ek level pe build karo.

### Trap 6 — `-Os` ko "thoda dheema -O2" samajhna
`-Os` kuch loops ko *poora* de-optimize kar deta (no unroll, no vectorize).
Numeric-heavy code pe `-Os` kaafi dheema. Sirf genuinely I-cache-bound code
ke liye.

---

## > **HFT relevance**

> - **Release: `-O2` (ya profiled `-O3`) + `-march=<target>` + `-flto` + PGO +
>   `-g`.** Folder 24 lesson 15 + this folder lessons 10–12.
> - **`-O3` sirf measured hot kernels pe** — order-book update, matching inner
>   loop, market-data parse — per-file, A/B verified, watch I-cache + freq.
> - **`-ffast-math` NAHI** blanket. Agar ek specific kernel ko reassociation
>   chahiye (a dot-product, a sum), use `#pragma omp simd reduction` ya
>   `-fassociative-math` sirf us file pe, aur numerically verify.
> - **`-g` on the shipped binary.** A p99 spike at 3am needs a real
>   `perf annotate` / core-dump backtrace.
> - **Lock the toolchain version.** GCC 13 vs 15 same flags → different codegen.
>   Pin it; re-benchmark on upgrade.

---

## Hands-on

```bash
# example 01: same code, 5 levels
cd 33-COMPILER-OPTIMIZATION/examples
for L in O0 O1 O2 O3 Os; do
  g++ -std=c++20 -$L 01_optimization_levels.cpp -o o_$L && ./o_$L
done

# what -O2 actually enables:
g++ -O2 -Q --help=optimizers | grep enabled | head -40
# diff two levels:
diff <(g++ -O2 -Q --help=optimizers) <(g++ -O3 -Q --help=optimizers)

# size impact:
for L in O2 O3 Os; do g++ -$L -c 01_optimization_levels.cpp -o /dev/null; done
g++ -O2 01_optimization_levels.cpp -o a2 && g++ -O3 01_optimization_levels.cpp -o a3 && size a2 a3
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`-O` ek switch hai" | 100+ passes ka bundle; levels alag bundles |
| "`-O3` hamesha `-O2` se tez" | workload-dependent; kabhi bloat/freq se ULTA |
| "`-O0` se benchmark theek" | stack traffic; `-O2` minimum |
| "`-Os` = slightly slower `-O2`" | kuch loops poora de-optimized; numeric code slow |
| "`-g` binary dheema karta" | sirf debug tables; runtime same |
| "`-Ofast` = faster `-O3`" | + `-ffast-math`; IEEE rules toote, silent wrong results |

---

## Exercises

1. Ek loop `for i: c[i] = a[i] + b[i]` (1M floats). `-O0` vs `-O1` vs `-O2`
   ka approx ratio kya expect karoge, aur `-O2 → -O3`?

   <details><summary>Answer</summary>

   `-O0 → -O1`: **~4-8×** — the big cliff (registers, no per-statement stack
   round-trips, index strength reduction). `-O1 → -O2`: **~2-4×** more —
   auto-vectorization kicks in (dense, independent, no aliasing if `__restrict`
   or the compiler versions) → SSE/AVX. `-O2 → -O3`: **~1.0-1.5×** — maybe a
   wider vector cost model or unroll-and-jam; often nothing. This loop is the
   *ideal* vectorization case so -O2 helps a lot; a reduction or a
   pointer-chase would look like example `01` (only the -O0 cliff matters).
   </details>

2. Tumhara binary `-O3` pe `-O2` se 3% *dheema* hai, IPC bhi thoda kam. Kya
   ho raha hoga, kaise confirm?

   <details><summary>Answer</summary>

   Likely **code bloat → I-cache / iTLB pressure**: `-O3`'s aggressive
   unrolling + function cloning grew the hot `.text`; now it doesn't fit
   L1i as well → frontend stalls (lower IPC). Confirm: `size` the two
   binaries (`.text` bigger at -O3), `perf stat --topdown` (Frontend Bound
   up at -O3), `perf stat -e L1-icache-load-misses,iTLB-load-misses`. Also
   check for a bad `-O3` vectorization (`-fopt-info-vec` diff) or, with
   `-march=native`, an AVX-512 downclock. Fix: stay on `-O2`, or `-O3
   -fno-unroll-loops`, or per-file `-O3` only where it wins.
   </details>

3. `-O2 -g` build mein gdb `<optimized out>` dikha raha ek variable ke liye
   jise tumhe dekhna hai. Options?

   <details><summary>Answer</summary>

   The variable was kept only in a register that got reused, or was
   folded away. Options: (1) rebuild that TU at **`-Og`** (optimized but
   debug-friendly) or `-O1`. (2) `-O2 -g -fno-omit-frame-pointer` +
   `-gstatement-frontiers` (GCC) improves location coverage. (3) Add a
   temporary `volatile` copy or a `DoNotOptimize`-style barrier around it so
   it must live in memory at that point. (4) Use `-O2` but compile just that
   function with `__attribute__((optimize("O0")))`. (5) For a crash, a core
   dump + `perf`/`addr2line` on the `-O2` binary often tells you enough
   without a live variable.
   </details>

---

## Interview questions

1. `-O0` `-O1` `-O2` `-O3` `-Os` — ek line each, aur "production default" kaunsa.
2. `-O0` 5-20x slow kyun (do concrete reasons).
3. `-O3` kab `-O2` se dheema — do scenarios.
4. `-Ofast` / `-ffast-math` kya todta, kab use.
5. `-g` optimization ko affect karta? release binary `-g` ke saath kyun.
6. Per-file / per-function optimization level kaise set karte.
7. Ek loop jo `-O2` se bahut fayda uthata vs ek jo sirf `-O0→-O1` se — dono ka shape.

---

## Next
→ [`02-godbolt-workflow.md`](02-godbolt-workflow.md)
