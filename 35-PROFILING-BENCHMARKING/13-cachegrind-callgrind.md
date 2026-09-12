# 13 — Valgrind: cachegrind, callgrind, massif, DHAT

## Prerequisites
- `10-perf-basics.md` (sampling profilers — contrast)
- `32-CACHE-MEMORY-PERFORMANCE` (cache model), `14-MEMORY` (heap)
- Linux/macOS (Valgrind; Windows: WSL)

## Yeh topic abhi kyun
`perf` (lesson 10–11) **sampling** karta — fast, low overhead, par
statistical aur non-deterministic (run-to-run thoda alag). Kabhi tumhe
**exact, repeatable** numbers chahiye: "yeh function exactly kitni
instructions execute karta", "yeh loop exactly kitne cache misses" — CI
regression gate ke liye, ya jab hardware counters available nahi (VM,
CI). Valgrind ke tools yeh dete — ek **simulated** CPU pe har instruction
count karke.

---

## Valgrind ka model: dynamic binary instrumentation

Valgrind program ko ek **synthetic CPU** pe chalata — har instruction ko
intercept karke instrument karta. Isliye:

- ✅ **Deterministic** — same input → same counts, har baar. CI-friendly.
- ✅ **No hardware counters needed** — works in VMs, containers, any CPU.
- ✅ **Exact** — actual instruction/access counts, not samples.
- ❌ **Slow** — **20–50x** (cachegrind), **~50–100x** (callgrind with cache
  sim). Ghante lag sakte bade workloads pe → chhota representative input.
- ❌ **Simulated cache model** — real CPU ke prefetchers, replacement
  policy, TLB, store buffers, MLB **approximate** hote. Trends sahi,
  absolute miss counts real hardware se alag.
- ❌ **Serializes threads** — Valgrind ek time pe ek thread chalata →
  contention / race timing distort. (Helgrind/DRD races ke liye alag.)

---

## Cachegrind — instruction counts + cache model

```bash
valgrind --tool=cachegrind ./app arg1 arg2
#  writes cachegrind.out.<pid>
cg_annotate cachegrind.out.<pid>              # per-function / per-line
cg_annotate cachegrind.out.<pid> src/hot.cpp  # source-annotated
```

Measures (per function, per source line):
- **Ir** — instructions executed (I refs).
- **I1 / LLi misses** — instruction-cache misses.
- **Dr / Dw** — data reads / writes.
- **D1 / LLd misses** — L1 / last-level data-cache misses.
- **Bc / Bcm / Bi / Bim** — conditional / indirect branches + mispredicts
  (branch prediction model).

```bash
# custom cache geometry (default: auto-detected or a generic model):
valgrind --tool=cachegrind --I1=32768,8,64 --D1=32768,8,64 --LL=8388608,16,64 ./app
#         size,assoc,linesize
```

**Best use:** `Ir` (instruction count) is the most reliable output — it's
**exact and hardware-independent**. Great for:
- "Did my change reduce work?" — `Ir` down = fewer instructions, period.
- **CI regression gate** — commit fails if `Ir` for a hot function grows
  > X%. Deterministic, no noisy hardware.
- Comparing two implementations' fundamental cost.

The cache-miss numbers are **directional** — "D1 misses dropped 10x after
the AoS→SoA change" is trustworthy as a *ratio*; the absolute count won't
match `perf`.

---

## Callgrind — call graph + costs

```bash
valgrind --tool=callgrind ./app
#  callgrind.out.<pid>
callgrind_annotate callgrind.out.<pid>
callgrind_annotate --inclusive=yes callgrind.out.<pid>   # self+children
kcachegrind callgrind.out.<pid>                          # GUI: call graph, maps
```

Cachegrind + **full call graph**: who calls whom, how many times, cost per
call path (self and inclusive). `--cache-sim=yes` adds the cache model,
`--branch-sim=yes` the branch model (both slower).

**KCachegrind** GUI is the payoff — interactive call graph, callee maps,
source annotation, "which call path dominates". The best free tool for
"where does the time go, structurally" when you want exact & repeatable.

```bash
# only instrument the interesting phase (skip startup):
valgrind --tool=callgrind --instr-atstart=no ./app
#  in code: CALLGRIND_START_INSTRUMENTATION / CALLGRIND_STOP_INSTRUMENTATION
#  or:  callgrind_control -i on / -i off   (from another shell)
```

---

## Massif — heap profiler over time

```bash
valgrind --tool=massif ./app
ms_print massif.out.<pid>                     # ASCII graph of heap over time
massif-visualizer massif.out.<pid>            # GUI
```

Plots **heap size vs time**, and at each snapshot, the **allocation stacks**
(who allocated the bytes that are live now). Finds:
- **Peak memory** and what's in it.
- **Growth** — a leak-like climb (not a leak per se — memcheck for that —
  but "this cache never evicts").
- Which call site is responsible for the bulk of the heap.

`--pages-as-heap=yes` → also counts `mmap`/`brk` (total process memory, not
just `malloc`).

---

## DHAT — "dynamic heap analysis tool"

```bash
valgrind --tool=dhat ./app
#  dhat.out.<pid>  ->  open in the DHAT viewer (dh_view.html)
```

Per allocation site, over the whole run:
- **Total blocks / bytes** allocated, **max live**, **lifetime**.
- **Access counts** — reads/writes to the allocated memory. Finds:
  - **Under-used allocations** — allocated 1 MB, read 4 KB → over-allocating.
  - **Short-lived churn** — millions of tiny allocs freed immediately → use
    a pool / stack buffer (folder 14).
  - **Write-only / read-only** regions.
  - **`realloc` chains** — a `vector` growing element-by-element.

Excellent for "why is this allocating so much" and "is this buffer the
right size".

---

## Valgrind tools vs `perf` — kab kaunsa

| Want | Tool |
|---|---|
| Fast, real hardware behaviour, production-ish | **`perf`** (sampling) |
| Exact instruction count, deterministic, CI gate | **cachegrind `Ir`** |
| Exact call graph + costs, GUI exploration | **callgrind + KCachegrind** |
| Cache-miss **ratios** without HW counters (VM/CI) | cachegrind (directional) |
| Heap over time / peak / who allocated | **massif** |
| Per-alloc-site size vs usage, churn, realloc | **DHAT** |
| Real cache misses, absolute, with prefetchers | `perf` / `perf mem` |
| Sub-microsecond events, jitter, scheduling | `perf` / tracing (not Valgrind) |

**Rule of thumb:** `perf` for "what's slow on real hardware"; Valgrind for
"exactly how much work / memory, repeatably". They answer different
questions — use both.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — cachegrind miss counts ko absolute truth maanna
Simulated model — no real prefetchers, generic replacement. **Ratios**
trustworthy, absolutes not. Real misses → `perf` / `perf mem`.

### Trap 2 — timing under Valgrind
Everything is 20–100x slower and serialized. **Wall-clock time under
Valgrind is meaningless.** Use it for *counts*, not *time*.

### Trap 3 — full production workload under callgrind
Would take hours/days. Use `--instr-atstart=no` + `CALLGRIND_*` macros to
instrument only the hot phase, and a small representative input.

### Trap 4 — multithreaded contention analysis under Valgrind
Threads serialized → lock contention / false sharing timing is fake. Use
`perf c2c` (lesson 11) or Helgrind (for races, not perf).

### Trap 5 — `Ir` regression gate without accounting for input
`Ir` scales with input size. The gate must use a **fixed** input, or
normalize (`Ir` per item). Otherwise a bigger test corpus "regresses" it.

### Trap 6 — massif "growth" = leak
Massif shows *live* heap. A growing curve might be a legit cache / buffer /
arena that's supposed to grow. For actual leaks: `valgrind --tool=memcheck
--leak-check=full` (lesson 15).

---

## Hands-on (Linux / WSL)

```bash
# instruction count of a function (deterministic):
g++ -std=c++20 -O2 -g app.cpp -o app
valgrind --tool=cachegrind --cachegrind-out-file=cg.out ./app
cg_annotate cg.out | head -40                 # Ir per function

# call graph GUI:
valgrind --tool=callgrind --callgrind-out-file=cg2.out ./app
kcachegrind cg2.out

# heap over time:
valgrind --tool=massif ./app ; ms_print massif.out.* | head -60

# per-alloc-site usage:
valgrind --tool=dhat ./app                     # open dhat.out.* in dh_view.html
```

(No Valgrind on native Windows — use WSL. `perf` also Linux-only. This is
why the repo's HFT track assumes a Linux target.)

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "cachegrind miss count = real misses" | ratios yes, absolutes no (simulated model) |
| "time under valgrind tells me something" | 20–100x slow + serialized — counts only |
| "callgrind on the full workload" | instrument the hot phase only, small input |
| "massif growth = memory leak" | live heap; could be legit cache — memcheck for leaks |
| "valgrind replaces perf" | different questions — exact/repeatable vs real/fast |
| "`Ir` gate, any input" | fixed input or per-item normalization |

---

## Exercises

1. `perf` says function `X` is 30% of runtime. Cachegrind says `X` is only
   8% of `Ir` (instructions). Both ran the same input. Contradiction?

   <details><summary>Answer</summary>

   **No contradiction — `X` is stalling, not executing.** `perf` (cycles)
   measures *time*; cachegrind `Ir` measures *instruction count*. If `X` is
   only 8% of instructions but 30% of time, its instructions are **expensive
   in cycles** — each one stalling. Likely causes: cache misses (check
   cachegrind's `D1mr`/`DLmr` for `X` — probably high), a dependency chain,
   or expensive ops (`div`, `sqrt`). This is exactly the "IPC is low for
   this function" signal. The fix targets *why the cycles/instruction is
   high* (locality, prefetch, algorithm) — not reducing instruction count.
   Conversely, a function that's 40% of `Ir` but 15% of time is running at
   high IPC (well-pipelined) and reducing its instruction count (SIMD,
   algo) is the lever there.
   </details>

2. You want a CI check that fails a PR if it makes the hot path do more
   work. Why is cachegrind `Ir` a better gate than `perf stat` cycles or
   wall-clock time? What's the catch?

   <details><summary>Answer</summary>

   `Ir` is **deterministic and hardware-independent**: the same binary +
   same input → the exact same instruction count on any machine, any CI
   runner, every time. `perf` cycles and wall-clock time are **noisy**
   (frequency scaling, co-tenants, ASLR/layout, thermal — lesson 09) → a
   ±5–15% gate either flaps or is too loose to catch a 3% regression. `Ir`
   catches "this refactor added 2% more instructions to the hot loop" cleanly.
   **Catches:** (a) `Ir` doesn't see *stalls* — a change that keeps
   instruction count flat but wrecks cache locality passes the gate (add a
   cachegrind `D1/DLL` miss gate too, as a ratio). (b) must use a **fixed
   input** or normalize per-item. (c) cachegrind is 20–50x slow — the CI job
   needs a small representative input, or runs on a schedule not every PR.
   (d) `Ir` can go *up* for a legit reason (added a necessary check) — the
   gate is a *review trigger*, not an auto-reject.
   </details>

3. DHAT output: one allocation site — `std::vector<Node>` in a graph loader
   — shows "total: 4,000,000 blocks, 512 MB; at t-max: 40 MB; reads: 2 GB,
   writes: 520 MB". What does this tell you, and what would you change?

   <details><summary>Answer</summary>

   **4,000,000 allocations** but only **40 MB live at peak** → massive
   **churn**: vectors being created and destroyed (or reallocated)
   constantly, 512 MB cumulative through a 40 MB working set. The
   reads (2 GB) > writes (520 MB) → data is written once and read ~4x
   (reasonable), so the data itself isn't wasted — the **allocation pattern**
   is. Changes: (a) if it's `vector` **growth** (push_back element by
   element) → `reserve()` the final size up front → one allocation instead
   of ~log₂(n) reallocs each copying. (b) if it's **many small short-lived
   vectors** → use a single reused buffer, an arena/pool allocator (folder
   14), or `small_vector` (inline storage) so the common small case doesn't
   hit the heap. (c) 4M allocations at ~50–100 ns each = 200–400 ms of pure
   allocator time, plus fragmentation and cache pollution — eliminating the
   churn is likely a big win. Confirm with `perf` before/after.
   </details>

---

## Interview questions

1. Valgrind ka DBI model — deterministic kyun, slow kyun, cache model ki limit.
2. cachegrind `Ir` — kyun woh sabse trustworthy output, CI gate mein kyun.
3. cachegrind/callgrind vs `perf` — kaunsa question kaunse tool ke liye.
4. callgrind `--instr-atstart=no` + macros — kyun zaroori bade programs pe.
5. massif vs DHAT — dono heap tools, kya farak.
6. Valgrind ke andar timing / threading numbers kyun bharose ke nahi.

---

## Next
→ [`14-vtune-intro.md`](14-vtune-intro.md)
