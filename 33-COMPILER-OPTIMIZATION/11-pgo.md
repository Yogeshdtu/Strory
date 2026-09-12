# 11 — Profile-Guided Optimization (PGO)

## Prerequisites
- `08-branch-hints.md` (PGO is the measured version of hinting)
- `10-lto.md`, `07-devirtualization.md`
- `07_pgo_workflow.sh` example

## Yeh topic abhi kyun
Compiler bahut sari decisions **guess** karta hai: kaunsi branch hot,
kaunsa function inline, loop kitna unroll, kaunse functions paas rakho.
Static heuristics OK hain, par woh aapke actual workload ko nahi jaante.
**PGO** ek asli run ka profile deta hai, aur compiler phir data se decide
karta — guess nahi. Bade / branchy codebases pe **5-20%**, aur HFT ke
release recipe ka core hissa (folder 24 lesson 15).

---

## The 3-step workflow

```bash
# 1. BUILD instrumented
g++ -O2 -fprofile-generate[=dir] app.cpp -o app_instr
#    -> binary mein counters inject: har branch, har call site, har edge

# 2. RUN a representative workload  (writes *.gcda profile files)
./app_instr < representative_input        # or a replayed capture
./app_instr < another_representative_run  # a few runs = more stable
#    -> app.gcda / *.gcda in [dir] (or next to the .o)

# 3. REBUILD with the profile
g++ -O2 -fprofile-use[=dir] -fprofile-correction app.cpp -o app
#    -> compiler reads the .gcda, optimizes for the observed behaviour
```

Modern alternative — **AutoFDO** (GCC/Clang): no instrumented build; sample
a normal `-O2 -g` binary with `perf record`, convert with `create_llvm_prof`
/ `autofdo`, feed via `-fauto-profile=...`. Lower overhead to collect (real
production sampling), slightly less precise.

Clang: `-fprofile-instr-generate` / `llvm-profdata merge` /
`-fprofile-instr-use=profile.profdata`.

---

## Kya PGO badalta

| Decision | Static guess | With PGO |
|---|---|---|
| **branch layout** | heuristic (e.g. "== literal" → unlikely) | exact taken-rate → hot path straight-line, cold out-of-line |
| **inlining** | size + call-count | inline the callsites that were *actually hot*; don't inline cold ones (smaller `.text`) |
| **loop unroll / vectorize** | trip-count unknown | observed trip-count distribution → unroll hot short loops, skip rarely-run ones |
| **function ordering / layout** | source/link order | hot functions placed adjacent → I-cache / iTLB locality; cold functions to `.text.unlikely` |
| **register allocation / spills** | uniform | spills pushed into cold blocks |
| **speculative devirtualization** | can't pick a type | devirt on the type that dominated the call site |
| **`switch` layout** | dense→jump table | hot cases first in an if-chain; jump table if all hot |
| **block/section splitting** (`-freorder-blocks-and-partition`) | — | hot/cold basic blocks physically separated |

---

## Measured expectations (honest)

- **Tight numeric kernels** (matmul, a dsp loop): **~0-5%** — the compiler
  already had enough info; the loop is compute-bound and layout doesn't matter.
- **Branchy / large code** (parsers, interpreters, compilers, databases,
  **trading engines**): **~5-20%**, occasionally more. This is where branch
  layout + function ordering + selective inlining pay.
- **Startup / cold code**: PGO can *shrink* the binary (don't inline cold
  paths) → faster load, better I-cache for the parts that run.
- Combined with LTO (`-fprofile-use -flto`): the profile also guides
  cross-TU inlining and whole-program layout — the two stack.

Example `07_pgo_workflow.sh` runs the full pipeline on a folder example and
prints `time` for baseline vs PGO — on the small vectorization example the
delta is in the noise (it's a tight kernel), which is itself the lesson:
**PGO's value scales with how branchy and how large your code is.**

---

## Getting a representative profile (the hard part)

PGO is only as good as the profile. **Wrong workload → pessimization** — it
optimizes for branches your training run took, which may be the opposite of
production.

- **Replay a real capture.** For HFT: a recorded market-data session (a busy
  day + a quiet day), fed through the real event loop. Not a synthetic
  micro-benchmark.
- **Cover the modes.** If the system has "normal" and "volatile/burst"
  regimes with different hot paths, train on both (merge profiles).
- **Merge multiple runs.** `-fprofile-use` reads all `.gcda`; or
  `llvm-profdata merge` several. Reduces run-to-run noise.
- **Re-train on code changes.** A stale profile → `-Wcoverage-mismatch`
  (GCC) / silently-ignored counters. Regenerate whenever the hot code
  changes materially. CI can do this: build-instrumented → run replay →
  rebuild.
- **`-fprofile-correction`** — tolerate a slightly stale / multi-threaded
  profile instead of erroring.

---

## PGO in a build system

```
stage 1:  cmake -DCMAKE_CXX_FLAGS="-O2 -fprofile-generate=$PROF" ; build
stage 2:  ./bench_replay  (or the app against a captured session)
stage 3:  cmake -DCMAKE_CXX_FLAGS="-O2 -fprofile-use=$PROF -fprofile-correction -flto" ; build
```
CI keeps `$PROF` as a cached artifact; regenerate on a schedule or when the
hot files change. Ship the stage-3 binary.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — unrepresentative training workload
Train on a synthetic loop, ship, and production takes different branches →
PGO made it *worse*. Train on replayed real traffic.

### Trap 2 — shipping the instrumented binary
`-fprofile-generate` binary is ~2-4x slower and writes `.gcda` files on
exit. It's a build artifact, never a release.

### Trap 3 — stale profile after refactor
Code changed, profile didn't → counters don't match → GCC warns and ignores
them (`-Wcoverage-mismatch`), or applies them to the wrong blocks. Regenerate.

### Trap 4 — profiling a debug build
Train and use at the **same `-O`** you'll ship. `-fprofile-generate` at
`-O0` gives a profile that doesn't match `-O2` code structure.

### Trap 5 — multi-threaded counter contention / skew
Instrumented counters have some races; use `-fprofile-correction`, run
long enough, merge runs. Or use AutoFDO (sampling, no counters).

### Trap 6 — expecting PGO to fix memory/algorithm problems
PGO optimizes layout and dispatch. A pointer-chasing hot loop or an O(n²) is
still slow. Folder 32 work first.

---

## > **HFT relevance**

> - **Release recipe: `-O2` (or profiled `-O3`) + `-march=<target>` + `-flto`
>   + PGO + `-g`.** The single biggest "free" win after LTO for a branchy
>   trading binary.
> - **Profile = replayed real sessions** — a busy open, a quiet mid-day, a
>   volatile burst. Merge them. Re-train when the strategy/parser changes.
> - **PGO drives the things you'd otherwise hand-hint** — branch layout,
>   inlining, function ordering — better and without bit-rot. Keep manual
>   `[[unlikely]]` only for obvious error paths and PGO blind spots.
> - **Validate on P50/P99 of the replay**, not a micro-benchmark (folder 24
>   lesson 15). A PGO build that improves throughput but worsens tail on the
>   burst regime is a regression for HFT.
> - **AutoFDO for continuous tuning** — sample the real production process
>   with `perf`, feed it back — no instrumented build, always-fresh profile.
> - **Function ordering / hot-cold splitting** is where PGO helps a big hot
>   `dispatch()` — the I-cache stays warm on the steady path.

---

## Hands-on

```bash
bash 33-COMPILER-OPTIMIZATION/examples/07_pgo_workflow.sh 33-COMPILER-OPTIMIZATION/examples/03_vectorization.cpp
#   step 0 baseline / step 1 instrument / step 2 run / step 3 -fprofile-use, with `time` + `size`

# a branchy example shows more:
bash 33-COMPILER-OPTIMIZATION/examples/07_pgo_workflow.sh 33-COMPILER-OPTIMIZATION/examples/05_branch_hints.cpp

# PGO + LTO together:
g++ -O2 -flto -fprofile-use=$PROF -fprofile-correction app.cpp -o app

# AutoFDO sketch (Linux):
g++ -O2 -g app.cpp -o app
perf record -b -e cycles:u ./app < replay        # -b = LBR for edge profile
create_llvm_prof --binary=./app --out=app.afdo   # (autofdo tools)
g++ -O2 -fauto-profile=app.afdo app.cpp -o app_afdo
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "PGO = 2x" | ~5-20% on branchy/large code; ~0-5% on tight kernels |
| "any workload trains it" | must be representative; wrong → pessimization |
| "ship the `-fprofile-generate` binary" | it's 2-4x slower + writes files; build artifact only |
| "train once, done" | stale profile after refactor → regenerate |
| "profile at -O0" | train/use at the shipping `-O` |
| "PGO fixes slow loops" | layout/dispatch only; algorithm/memory first |

---

## Exercises

1. Ek team `-fprofile-generate` binary ko ~5 minutes ke ek quiet-market
   capture pe train karti hai, phir ship karti hai. Live pe volatile open ke
   dauran latency *badh* gayi. Kya hua?

   <details><summary>Answer</summary>

   The training capture was **not representative of the hot regime**. During
   a quiet market, the branches taken are: mostly "no update", "no signal",
   "no order" — so PGO laid those out as the hot straight-line path and
   pushed the "there IS an update / signal / order" code out-of-line as
   "cold". At the volatile open, *those* paths are the hot ones — now every
   one pays a taken branch to reach out-of-line code, plus I-cache misses on
   code PGO assumed was rare. Fix: train on a **merged** profile — a busy
   open + a volatile burst + a quiet period — so the genuinely-always-hot
   code (parse, book update, risk check) is prioritized and only the
   truly-rare code (errors, recovery, admin) is de-prioritized. Validate on
   P99 of the burst replay.
   </details>

2. PGO build ne binary ko 15% *chhota* kar diya aur throughput 8% better,
   par ek specific rarely-used report command ab 3x slower hai. Acceptable?

   <details><summary>Answer</summary>

   For most systems: **yes, that's PGO working as intended.** It stopped
   inlining and de-prioritized the cold report path (hence smaller binary,
   better I-cache for the hot path → 8% throughput), at the cost of the cold
   path being slower (out-of-line, not inlined, further from hot code). If
   the report command is genuinely rare and not latency-sensitive, the trade
   is good. If that command is on a latency SLA (e.g. a risk query that must
   answer in <1ms), then either: include it in the training workload so PGO
   keeps it warm, or exclude that function from PGO
   (`__attribute__((no_profile_instrument_function))` / keep it `-O2` without
   the profile), or split it into its own binary/service. Decide by whether
   the 3x-slower absolute latency still meets that path's requirement.
   </details>

3. AutoFDO vs instrumented PGO — HFT ke liye kaunsa, kyun?

   <details><summary>Answer</summary>

   **Both have a place; AutoFDO is attractive for continuous tuning.**
   Instrumented PGO: more precise counts, but needs a separate slow
   instrumented build + a controlled replay run in CI, and the profile is a
   snapshot. AutoFDO: sample a **normal `-O2 -g` production binary** with
   `perf record -b` (LBR gives edge/branch data), convert, rebuild — you can
   collect from the *actual live process* under real traffic, continuously,
   with ~1% overhead, and the profile is always current. Downsides: sampling
   is lossy (rare-but-important paths may be under-sampled), and the
   perf→profile toolchain is finicky. Practical HFT setup: instrumented PGO
   with a curated merged replay for the release build (deterministic,
   reviewable), *plus* periodic AutoFDO runs from production to catch drift
   and inform the next replay set. Always validate P50/P99 on the replay
   before shipping either.
   </details>

---

## Interview questions

1. The 3-step PGO workflow, and what each `.gcda` contains.
2. 4 decisions PGO improves over static heuristics.
3. Realistic PGO gain — for a tight kernel vs a branchy engine.
4. "Representative profile" — why it matters, failure mode if wrong.
5. Stale profile after a refactor — what happens, what to do.
6. AutoFDO vs instrumented PGO — trade-offs.
7. PGO + LTO — how they combine.

---

## Next
→ [`12-march-and-mtune.md`](12-march-and-mtune.md)
