# 07 — Branch prediction: predictors, BTB, misprediction cost

## Prerequisites
- `04-pipelining.md` (control hazard, flush/refill)
- `06-out-of-order-execution.md` (speculation, rollback)

## Yeh topic abhi kyun
Pipeline ko fed rakhne ke liye CPU ko fetch ke waqt hi pata hona chahiye "agli
instruction kahan se". Ek branch pe woh nahi pata — isliye CPU **predict** karta
aur speculatively aage badh jaata. Sahi → free. Galat → ~15–20 cycle pipeline
refill. Example `03` measure karta: ek unpredictable branch **~6–7× slower**. HFT
hot path pe har mispredict ek P99 jitter spike hai.

---

## Why prediction is necessary

A 4 GHz, 19-stage pipeline fetches ~4 instructions/cycle. By the time a
conditional branch's condition is *computed* (in EX, maybe cycle 10), the CPU has
already fetched ~40 instructions past it. If it *waited* for every branch, IPC
would collapse (a branch every ~5 instructions in typical code). So it **guesses
the direction and target**, fetches down that path, and executes speculatively
(file `06`). Correct guess → zero cost. Wrong → discard everything after the
branch, refill.

---

## What gets predicted

| Branch type | What's predicted | Predictor structure |
|---|---|---|
| **Conditional direction** (`je`, `jl`, loop back-edge) | taken / not-taken | history-based direction predictor (below) |
| **Branch target** (any taken branch) | the target address | **BTB** (Branch Target Buffer) — cache of `branch PC → target` |
| **Indirect branch / call** (`jmp rax`, vtable, function pointer, `switch` jump table) | one of many possible targets | **indirect predictor** (BTB + history); hard when polymorphic |
| **Return** (`ret`) | caller's address | **RAS** (Return Address Stack) — a small hardware stack pushed by `call` |

---

## Direction predictors (evolution)

- **Static:** backward branch → predict taken (loops), forward → not taken.
  Compilers arrange code to match (`[[likely]]`/`[[unlikely]]`, PGO).
- **1-bit / 2-bit saturating counter** per branch: remembers last outcome(s).
  Good for loops (`for` back-edge mispredicts only once, on exit).
- **2-level / gshare:** hashes the branch address with a **global history
  register** (last N branch outcomes) → indexes a table of counters. Captures
  *correlated* branches ("if A was taken, B usually is").
- **TAGE** (modern, ~2010+): multiple tables indexed by different history
  lengths, tagged, with a usefulness metric. Predicts patterns spanning
  hundreds of branches. ~99%+ on typical code.
- **Perceptron / neural** (some designs): weights over history bits.

Modern predictors are *very* good on structured code. They fail on:
- **Data-dependent random outcomes** (`if (price[i] > threshold)` on unsorted
  market data) — no pattern to learn (example `03`).
- **Aliasing** — two hot branches hashing to the same table entry, fighting.
- **Cold code** — first execution has no history.
- **Very long correlation** beyond the history length.

---

## Misprediction cost

```
correct prediction:   fetch keeps flowing, IPC unaffected     -> ~0 extra
misprediction:        squash younger µops (file 06),
                      restore rename map,
                      refetch + redecode + rerename from
                      the correct target                        -> ~15-20 cycles
```

At 4 GHz that's ~4–5 ns per mispredict. Cheap once; brutal at millions/sec on a
hot branch. **Example `03` measured (this box, if-conversion disabled):** RANDOM
order **~4–8 ns/element** vs SORTED order **~0.5–1.4 ns/element** → **~6–7×**.

> ⚠️ **Rule 2 note (example `03`):** with default `-O2`, GCC turns
> `if (x >= thr) s += x` into a **branchless `cmovge`** — no branch, no
> misprediction, RANDOM == SORTED. That's not the demo failing; it's the
> compiler already doing example `04`'s job. The example disables if-conversion
> (`#pragma GCC optimize("no-if-conversion")`) to expose the raw prediction
> cost. **This is itself the lesson:** a well-predicted branch and a `cmov` cost
> about the same; a *badly*-predicted branch is what you must avoid.

---

## Fixing branch problems

| Problem | Fix |
|---|---|
| Data-dependent random branch on the hot path | **branchless** (mask/`cmov`/arithmetic — file `08`, example `04`), or sort/partition the data so it's predictable, or SIMD predication (file `10`) |
| One side is ~always taken (error checks, rare cases) | `[[likely]]`/`[[unlikely]]`, or `__builtin_expect`, so the cold side is laid out away from the hot path (better I-cache + static prediction) |
| Polymorphic indirect call (virtual, `std::function`, fn ptr) in a hot loop | devirtualize: CRTP / `std::variant` + `visit` / tag-`switch` / templates (folder 16/21/36) — the target becomes fixed and predictable |
| `switch` over many cases, hot | jump table (compiler does it) + the input is often the same → predicted; or a perfect-hash / computed dispatch |
| Cold code polluting the hot path | `__attribute__((cold))` / `[[unlikely]]` on error handlers → compiler moves them to a separate section |
| Predictor cold at startup | **warm-up** — run the hot path with synthetic data before "go live" (folder 29 file 13) so predictors + BTB + I-cache are trained |

---

## Indirect branches and the return stack

- **Virtual call / `std::function` / function pointer:** `call [rax]` — the
  target is a value. If it's the same target every time (monomorphic in
  practice), the indirect predictor nails it. If it varies unpredictably
  (different derived types per iteration) → mispredict per call + the missed
  inline. Measured elsewhere in this repo: virtual ~2.4 ns vs CRTP ~0.6 ns at a
  real call boundary (folder 21).
- **`ret` misprediction:** normally the RAS handles it perfectly. But
  **mismatched call/ret** (setjmp/longjmp, some coroutine/fiber switches,
  hand-rolled asm) corrupts the RAS → every subsequent `ret` mispredicts until
  it resyncs. Another reason to avoid exotic control flow on the hot path.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — benchmarking a branch that the compiler made branchless
Modern `-O2` if-converts many `if`s to `cmov`. Your "branch misprediction
benchmark" shows nothing. Check the asm (`cmov*` vs `j*`); example `03` disables
if-conversion to measure the real effect.

### Trap 2 — `[[likely]]` on a genuinely 50/50 branch
The hint is a lie → static prediction now wrong ~50% *and* the code layout put
the wrong side inline. Only hint branches that really are ~90%+ one way (error
checks, bounds that rarely fail).

### Trap 3 — assuming the predictor learns *data*
It learns *branch history patterns*. `if (rand() & 1)` is unlearnable. If the
outcome depends on unpredictable input, no predictor helps — restructure.

### Trap 4 — polymorphism in the hottest loop
A `for` loop calling `shape->area()` with mixed types → indirect mispredict per
element + no inlining. If the set of types is small and known, use a
`std::variant` + `visit` or a tag switch (folder 21/36).

### Trap 5 — ignoring BTB / I-cache pressure from bloated code
A huge hot function or aggressive inlining can blow the BTB and I-cache → target
mispredicts and instruction-fetch stalls even for well-predicted directions.
Keep the hot path lean; `__attribute__((cold))` the rare branches.

### Trap 6 — no warm-up
The first N thousand iterations after startup mispredict constantly (cold
predictor, cold BTB, cold I-cache). Dry-run the pipeline before market open
(folder 29 file 13).

---

## > **HFT relevance**

> - **Hot-path branches must be predictable or absent.** Data-dependent random
>   branches (price crossed? order matched? side == BUY?) → branchless (file
>   `08`, example `04`) or arrange the data so the branch is predictable.
> - **`[[likely]]`/`[[unlikely]]`** on the branches that really are lopsided —
>   error paths, risk-check failures, rare message types — so the compiler exiles
>   the cold code and the hot path is straight-line.
> - **Devirtualize the hot dispatch** — no virtual calls / `std::function` /
>   function pointers in the tick→order loop; CRTP / `variant` / tag-switch so
>   the target is fixed.
> - **Warm the predictors** at startup — thousands of dry-run iterations with
>   synthetic data before going live trains the direction predictors, BTB, RAS,
>   and I-cache (folder 29 file 13).
> - **Measure:** `perf stat -e branches,branch-misses` (miss rate),
>   `perf record -e branch-misses` + `perf report` to find *which* branch. A hot
>   branch with a high miss rate is a concrete, fixable jitter source.

---

## Hands-on

```bash
# example 03: sorted vs random -> the misprediction cost
./build.ps1 fast 31-CPU-ARCHITECTURE/examples/03_branch_prediction.cpp

# see the compiler if-convert (default) vs not (pragma) -> cmov vs jl/jge
./build.ps1 asm 31-CPU-ARCHITECTURE/examples/03_branch_prediction.cpp | grep -E 'cmov|jl|jge|jge'

# Linux: which branch is missing
perf stat -e branches,branch-misses ./prog
perf record -e branch-misses ./prog && perf report
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "predictor learns the data" | learns *history patterns*; random data is unlearnable |
| "mispredict = a few cycles" | ~15–20 (pipeline depth); ~5 ns at 4 GHz |
| "`[[likely]]` always helps" | a lie on a 50/50 branch makes it worse |
| "compiler kept my branch" | often if-converted to `cmov`; check the asm |
| "virtual call in a hot loop is fine" | indirect mispredict + no inline; devirtualize |
| "no need to warm up" | cold predictor/BTB/I-cache mispredict for thousands of iters |

---

## Exercises

1. Ek `for (i=0; i<N; i++)` loop ka back-edge branch (`i<N` → jump back) kitni
   baar mispredict hota, N=1000 par?

   <details><summary>Answer</summary>

   **Once** — on the final iteration (i=999→1000) when the loop exits and the
   "taken" prediction is wrong. The first 999 iterations are all "taken" and the
   predictor learns that after 1-2 iterations. So a simple counted loop costs ~1
   mispredict total, regardless of trip count. (Nested loops: the inner loop's
   exit mispredicts once per outer iteration.) This is why loop overhead is
   negligible and why *data-dependent* branches *inside* the loop body are the
   real concern.
   </details>

2. `if (msg.type == HEARTBEAT) [[unlikely]] { ... }` — heartbeats 1% of
   messages. Yeh hint kya karta, aur agar heartbeats actually 40% hon to?

   <details><summary>Answer</summary>

   At 1%: the compiler lays out the `HEARTBEAT` handling code **out of line**
   (separate section, often past the function end), so the common path
   (99% of messages) is straight-line contiguous machine code → better I-cache
   density, and static/cold prediction favours "not taken" which is right 99%
   of the time. Good.
   At 40%: the hint is wrong 40% of the time → the branch mispredicts far more
   than an unhinted predictor would (which would learn the ~60/40 split), *and*
   the frequently-needed handler code is now a taken jump to a cold section
   (I-cache miss). Net loss. Only hint branches that are genuinely ~90%+ one way.
   </details>

3. Example `03` default `-O2` pe RANDOM == SORTED dikhata (no effect). Yeh bug
   hai ya feature? Explain.

   <details><summary>Answer</summary>

   Feature (of the compiler), and a lesson. GCC's if-conversion turned
   `if (x >= thr) s += x` into `s += (x >= thr) ? x : 0` implemented with a
   branchless `cmovge` (or a masked add). With no branch, there's no direction
   to predict and no misprediction — so data order stops mattering. The example
   disables if-conversion to *show* what a real branch would cost. The takeaway:
   for a simple predicate like this, the compiler already does example `04`'s
   branchless transform for you; you only need to intervene when the branch body
   is too complex to if-convert (a call, a loop, a long chain) *and* the outcome
   is unpredictable.
   </details>

4. Ek `std::map<int, Order>::find` hot loop mein hai. Branch prediction ke
   nazariye se kya problem, aur fix?

   <details><summary>Answer</summary>

   `std::map` is a red-black tree — `find` walks it comparing keys, taking a
   **data-dependent branch at each node** ("go left or right?"). With random
   lookup keys the direction at each node is ~50/50 → mispredict per level, and
   *also* a cache miss per node (pointer chasing, file `06`). ~log₂(N) mispredicts
   + misses per lookup. Fix: a **flat sorted `std::vector` + branchless binary
   search** (folder 20 file 05) — contiguous memory (prefetcher-friendly), and
   the binary-search step can be made branchless (`idx += (key > mid) * half`).
   Or an **open-addressing hash map** (one probe, often one cache line) for O(1)
   average. Both are standard HFT replacements for node-based containers.
   </details>

5. Warm-up ke bina ek freshly-started trading process ke pehle ~5000 ticks slow
   kyun, aur kaunse structures cold hain?

   <details><summary>Answer</summary>

   Cold: (1) **branch direction predictors** — no history, every data-dependent
   branch guesses randomly for a while; (2) **BTB** — branch targets not cached,
   target mispredicts; (3) **RAS** — fine unless deep calls; (4) **I-cache /
   µop cache** — the hot code isn't resident, fetch stalls; (5) **D-cache /
   TLB** — the working set (order book, lookup tables) isn't loaded (folder 32),
   plus page faults (folder 29 file 13); (6) **the allocator / container
   buffers** haven't grown. Warm-up = run the full pipeline with synthetic
   market data (orders suppressed) N thousand times before going live, so all of
   the above are trained/resident. p50 and p99 both drop sharply after warm-up.
   </details>

---

## Interview questions

1. Why does the CPU need to predict branches at all (pipeline depth vs when the condition is known)?
2. What gets predicted — direction, target, indirect target, return — and the structure for each (counter/BTB/RAS).
3. Modern direction predictors (gshare/TAGE) — what patterns they catch, what they can't.
4. Misprediction cost — cycles, ns at 4 GHz, why it scales with pipeline depth.
5. `[[likely]]`/`[[unlikely]]` — what the compiler does with them, when they backfire.
6. A hot polymorphic dispatch — the branch-prediction problem and how to devirtualize.
7. Predictor warm-up — what's cold at process start, how HFT firms handle it.

---

## Next
→ [`08-speculative-execution.md`](08-speculative-execution.md)
