# 15 — LTO and PGO

## Prerequisites
- `01-translation-units.md` (TU = optimization boundary), `09-static-vs-dynamic-linking.md`
- `13-build-performance.md`

## Yeh topic abhi kyun
Do "whole-program" optimizations jo aksar `-O3` se zyada real-world speedup deti
hain: **LTO** (compiler ko poore program ko ek saath dekhne do — cross-file
inlining/devirt) aur **PGO** (real run ke data se compiler ko batao kaunsi
branches/functions hot hain). HFT release builds mein dono common hain — par inki
build-process cost aur pitfalls samajhna zaroori hai.

---

## LTO — Link-Time Optimization

### Problem it solves

Normal build: compiler ek TU at a time dekhta (file 01). `main.cxx` compile karte
waqt use `mathx.cxx` ki `is_prime` ki **body nahi dikhti** — bas ek `call`. Toh:
- Cross-file inlining nahi.
- Cross-file devirtualization nahi.
- Cross-file constant propagation, dead-code elimination, alias analysis — sab
  TU boundary pe ruk jaate.

Header mein `inline`/`constexpr`/templates rakh ke aap manually cross-TU inlining
enable karte ho — par sab kuch header mein nahi daal sakte.

### LTO kya karta

```bash
g++ -O2 -flto -c a.cxx -o a.o        # a.o mein machine code NAHI — GIMPLE IR
g++ -O2 -flto -c b.cxx -o b.o
g++ -O2 -flto a.o b.o -o app         # LINK time pe: poora program IR ek saath,
                                     # phir re-optimize + codegen
```

- `-flto` se `.o` files mein compiler ka **intermediate representation** (GCC:
  GIMPLE, Clang: LLVM bitcode) hota hai, final machine code nahi.
- **Link time pe** linker plugin saare IR ko ek saath laata hai, whole-program
  optimize (cross-module inline, devirt, DCE, ICF, better inliner decisions),
  phir machine code emit.
- Effectively "sab kuch ek giant TU tha" ka fayda, bina source ko merge kiye.

### Variants

| Flag | Kya |
|---|---|
| `-flto` (GCC) | serial LTO — slow link on big projects |
| `-flto=<N>` (GCC) | parallel LTO with N jobs |
| `-flto=auto` (GCC) | N = nproc |
| `-flto=thin` (Clang) | **ThinLTO** — per-module summaries, mostly-parallel, scalable; near-full-LTO wins at a fraction of the cost |
| `-ffat-lto-objects` | `.o` has both IR *and* machine code (can link without LTO too) |

### Costs / pitfalls

- **Link step gets slow** — seconds → minutes on large projects (it's re-compiling
  at link). `-flto=auto` / ThinLTO mitigate. Bad for local iteration; fine for
  the release artifact / nightly CI.
- **Debugging harder** — inlined-across-modules code, `-g` + LTO can be less
  faithful (improving over time).
- **ODR violations become louder / more dangerous** — LTO sees mismatched
  definitions across TUs; `-Wodr` fires (good — file 04), but a real violation
  can now miscompile in new ways.
- **Toolchain match** — all `-flto` `.o` must come from the *same* compiler
  version; mixing → link errors or garbage. `-ffat-lto-objects` for libs shipped
  to others.
- **`__attribute__((used))` / `KEEP`** — LTO's aggressive DCE can remove things
  only referenced by name at runtime (plugin registration, `dlsym`).

### Typical usage

Production artifact: `-O2 -flto=auto -march=native` (or `-O3` if measured to
help). Local/CI-fast: no LTO. Header-only hot code + `-flto` on the rest = the
optimizer sees everything.

---

## PGO — Profile-Guided Optimization

### Idea

Compiler ke inliner, branch layout, register allocation, and function ordering
sab **heuristics** pe chalte hain ("small functions inline karo", "forward branch
usually not taken"). PGO in heuristics ko **real data** se replace karta hai.

### 3-step flow

```bash
# 1. INSTRUMENT — build with counters
g++ -O2 -fprofile-generate -o app_instr *.cxx

# 2. TRAIN — run on representative workload (this writes *.gcda profile files)
./app_instr < representative_input          # e.g. replay a market-data capture

# 3. USE — rebuild with the profile
g++ -O2 -fprofile-use -fprofile-correction -o app *.cxx
```

Clang: `-fprofile-instr-generate` → run → `llvm-profdata merge` → `-fprofile-use=
prof.profdata`. Also **sampling PGO** (`-fauto-profile` from `perf` data — no
instrumented build needed, less precise).

### Kya better hota hai

- **Branch prediction / block layout** — hot path straight-lined, cold path
  (error handling) moved out-of-line → fewer taken branches, better I-cache
  density.
- **Inlining decisions** — inline the calls that are actually hot; *don't* inline
  cold ones (keeps hot `.text` small).
- **Function reordering** — hot functions placed together → fewer I-cache/iTLB
  misses.
- **Register allocation / spill placement** biased toward hot blocks.
- **`switch` lowering**, devirtualization of monomorphic-in-practice call sites.

Typical gain: **5–20%** on branch-heavy / large-working-set code — often *more*
than `-O2`→`-O3`, and without the code-size blowup.

### Costs / pitfalls

- **Two builds + a training run** — build-system complexity, CI time.
- **Training workload must be representative** — profile from a synthetic
  micro-benchmark → optimizes for the wrong thing → can *hurt* real workload.
  Use a real captured session.
- **Profile staleness** — code changed a lot since the profile → `-Wmissing-profile`
  / mismatches; `-fprofile-correction` tolerates some. Regenerate periodically.
- **Non-determinism in training** → noisy profiles; run a few times / long enough.

### LTO + PGO together

`-flto` + `-fprofile-use` — LTO does whole-program transforms *guided by* the
profile (cross-module hot/cold splitting, global function ordering). This is the
"max effort" production configuration and what most HFT release pipelines converge
on. Also `-fprofile-use` enables extra LTO partitioning heuristics.

---

## BOLT (bonus — post-link)

`llvm-bolt` re-optimizes an **already-linked binary** using `perf` samples —
reorders basic blocks/functions for I-cache/iTLB, independent of (and stackable
with) LTO+PGO. Used by large latency-sensitive services.

---

## Andar kya hota hai (chhota)

- **LTO:** `-flto` → `cc1` emits IR into `.o` (`.gnu.lto_*` sections). At link, the
  **linker plugin** (`liblto_plugin`) hands all IR back to the compiler
  (`lto1`/`lto-wrapper`), which does WPA (whole-program analysis) → partitions →
  parallel codegen → real `.o` → final link.
- **PGO instrument:** `-fprofile-generate` inserts `__gcov`/`__llvm_profile`
  counters at every edge/function; at exit they're flushed to `.gcda`/`.profraw`.
- **PGO use:** the compiler reads the counts, annotates the CFG with edge
  frequencies, and every frequency-aware pass (inliner, block layout, `bbpart`,
  regalloc) consults them.

---

## > **HFT relevance**
> - **Production artifact = `-O2 -flto=auto -march=native` (+ often PGO)**, built
>   once, slowly, in CI — never locally. The whole-program view lets the compiler
>   inline across the feed-handler / book / strategy boundaries and devirtualize
>   call sites that are monomorphic in practice.
> - **PGO with a captured session** — replay a real (or realistic) market-data
>   capture as the training workload so hot/cold splitting matches production.
>   The error paths (sequence gaps, rejects — folder 23) get pushed out-of-line,
>   tightening the hot `.text` → better I-cache behaviour → lower tail latency.
> - **Static-link everything** so LTO can actually reach all the code (a `.so`
>   boundary is opaque to LTO — file 09).
> - **`-Wodr` under LTO is a feature** — it surfaces the silent ODR violations
>   (file 04) that mixed flags introduce; treat every one as a bug.
> - **Watch code size** — `-O3`/LTO can grow hot `.text` and *hurt* I-cache;
>   measure P50/P99 latency, not just a micro-benchmark. PGO tends to *shrink*
>   hot code (cold-splitting) — often the better lever.
> - **`__attribute__((used))`** on plugin-registration globals / anything reached
>   only by name — LTO/`--gc-sections` will otherwise delete it.

---

## Hands-on

```bash
cd 24-COMPILATION-LINKING/examples/05_makefile_project

# no LTO vs LTO (time the link, check the binary):
mingw32-make clean && time mingw32-make CXXFLAGS="-std=c++20 -O2 -MMD -MP -Iinclude"
mingw32-make clean && time mingw32-make CXXFLAGS="-std=c++20 -O2 -flto -MMD -MP -Iinclude"

# see IR-only object with -flto:
g++ -std=c++20 -O2 -flto -c src/hash.cxx -o /tmp/hash_lto.o
objdump -h /tmp/hash_lto.o | grep -i lto      # .gnu.lto_* sections, little/no .text

# PGO sketch (single-file toy):
cat > /tmp/p.cpp <<'EOF'
#include <cstdio>
#include <cstdlib>
int hot(int x){ return x*3+1; }
int cold(int x){ for(int i=0;i<1000;++i) x^=i; return x; }
int main(int c,char**v){ long s=0; int n=atoi(v[1]);
  for(int i=0;i<n;++i){ if(i%1000==0) s+=cold(i); else s+=hot(i); }
  std::printf("%ld\n",s); }
EOF
g++ -O2 -fprofile-generate -o /tmp/pi /tmp/p.cpp
/tmp/pi 20000000                                  # training run -> *.gcda
g++ -O2 -fprofile-use -fprofile-correction -o /tmp/pu /tmp/p.cpp
objdump -d -C /tmp/pu | sed -n '/<main>:/,/ret/p' | head -40   # cold path moved out-of-line?
```

---

## ⚠️ Traps

### Trap 1 — `-flto` locally for iteration
Link becomes minutes. Use it only for the release artifact / nightly.

### Trap 2 — mixing LTO objects from different compiler versions
`lto1: internal compiler error` / garbage. Same toolchain for all `-flto` `.o`.

### Trap 3 — LTO deletes a runtime-only symbol
Plugin registration via a global ctor, `dlsym` targets → gone. Mark
`__attribute__((used))` / linker `KEEP`.

### Trap 4 — PGO training on a micro-benchmark
Optimizes for a workload you don't run. Train on a real/representative capture.

### Trap 5 — stale PGO profile after a big refactor
Mismatched counts → warnings, possibly worse code. Regenerate; use
`-fprofile-correction` to tolerate minor drift.

### Trap 6 — assuming `-O3`/LTO/PGO always faster
Can grow hot `.text` and hurt I-cache; can expose ODR bugs. **Measure P50/P99
latency on the real workload**, not a synthetic loop (folder 35).

### Trap 7 — LTO across a `.so` boundary
Doesn't happen — `.so` is opaque to LTO. Static-link the code you want optimized
together (file 09).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "LTO is just `-O3`" | LTO removes the TU boundary — cross-module inline/devirt/DCE at link |
| "`-flto` `.o` files are normal objects" | They hold compiler IR; final codegen happens at link |
| "PGO needs `-O3`" | Works with `-O2`; often beats `-O3` on branch-heavy code, smaller too |
| "any training run works for PGO" | Must be representative; a bad profile can regress real perf |
| "LTO+PGO always win" | Measure real latency; watch code-size/I-cache; fix ODR bugs it surfaces |
| "LTO optimizes across shared libraries" | No — `.so` boundary is opaque; static-link to benefit |

---

## Exercises

1. **Why can't this inline without LTO:** `main.cxx` calls `hot()` defined
   (non-`inline`) in `hot.cxx` a billion times. What does `-flto` change?

   <details><summary>Answer</summary>

   Without LTO, `main.cxx`'s TU only sees `int hot(int);` → it must emit a `call`.
   With `-flto`, `hot.cxx`'s IR is available at link → the optimizer can inline
   `hot` into the loop in `main`, then const-fold / vectorize around it. (Putting
   `hot` `inline` in a header would also fix it, but LTO does it without moving
   code.)
   </details>

2. **PGO wins:** name three things a profile lets the compiler do better than its
   default heuristics.

   <details><summary>Answer</summary>

   (1) Block layout — straight-line the measured-hot path, move cold (error)
   blocks out-of-line → fewer taken branches, denser hot I-cache. (2) Inlining —
   inline the calls that are actually hot, leave cold ones out (keeps hot `.text`
   small). (3) Function reordering — cluster hot functions → fewer I-cache/iTLB
   misses. (Also: better regalloc/spill placement, `switch` lowering,
   speculative devirtualization.)
   </details>

3. **Build pipeline:** design the release build for a latency-sensitive engine.
   Which of `-O2/-O3`, `-flto`, PGO, `-march=native`, static linking — and how do
   you validate?

   <details><summary>Answer</summary>

   `-O2 -march=native -flto=auto`, static-linked, plus PGO trained on a replayed
   real market-data capture; `-O3` only if measured to help. Build once in CI (not
   locally). Validate by replaying the capture and comparing **P50/P99/P99.9
   latency** and throughput against the previous artifact — accept only if tail
   latency improves or holds. Keep `-Wodr` on and treat any warning as a blocker.
   </details>

4. **LTO ODR:** after enabling `-flto`, the build now emits `warning: type 'struct
   Cfg' violates the C++ One Definition Rule`. Is LTO the bug? What do you do?

   <details><summary>Answer</summary>

   No — LTO just *revealed* a pre-existing ODR violation (two different `Cfg`
   layouts in two TUs, file 04) that was silently miscompiling before. Fix the
   type to have one definition in one shared header; don't suppress the warning.
   </details>

5. **Regression:** you add `-O3 -flto` and P99 latency gets *worse* while the
   micro-benchmark improves. Plausible cause?

   <details><summary>Answer</summary>

   Code-size growth: `-O3`/LTO inlined and unrolled aggressively → hot `.text`
   footprint grew → more I-cache/iTLB misses under the real (larger) working set,
   which the micro-benchmark (tiny footprint) doesn't show. Try `-O2 -flto`, PGO
   (which cold-splits and shrinks hot code), `-Os` for cold paths, and measure on
   the real workload.
   </details>

---

## Interview questions

1. LTO — kaunsa problem solve karta, `-flto` `.o` mein kya hota?
2. Header-only `inline`/templates vs LTO — dono cross-TU inlining deti hain, farq?
3. LTO ke costs/pitfalls — build time, ODR, toolchain match, DCE.
4. PGO ka 3-step flow. Representative training kyun critical?
5. PGO compiler ke kaunse decisions ko improve karta (branch layout, inlining, ordering)?
6. `-O3` vs PGO — code size / I-cache ke context mein.
7. LTO `.so` boundary ke paar kyun kaam nahi karta?
8. HFT release build — LTO + PGO + static + `-march=native`: kyun, validate kaise?

---

## Next
→ [`16-exercises.md`](16-exercises.md)
