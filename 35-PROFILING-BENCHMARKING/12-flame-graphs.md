# 12 — Flame graphs: banana aur padhna

## Prerequisites
- `10-perf-basics.md` (`perf record`, `perf script`, `-g`)
- `examples/07_flamegraph.sh`

## Yeh topic abhi kyun
`perf report --stdio` ek text tree deta — bade programs mein woh hazaaron
lines ka ho jaata, aur "kahan time ja raha" ek nazar mein dikhta nahi.
**Flame graph** wahi data ek SVG mein compress karta: har call stack ek
horizontal bar, width = time. Ek nazar mein hot path dikh jaata. Brendan
Gregg ka invention, ab har profiler mein (perf, VTune, async-profiler,
pprof, Instruments) built-in.

---

## Data → flame graph: 3 steps

```bash
# 1. sample with call graphs
perf record -F 999 -g -- ./app
#    ya bada C++: perf record -F 999 --call-graph dwarf -- ./app

# 2. "fold" stacks: har unique stack -> ek line "a;b;c count"
perf script | ./FlameGraph/stackcollapse-perf.pl > out.folded

# 3. render SVG
./FlameGraph/flamegraph.pl out.folded > flame.svg
```

`FlameGraph` = Brendan Gregg ka repo
(`github.com/brendangregg/FlameGraph`) — do Perl scripts,
`stackcollapse-perf.pl` (perf-specific) + `flamegraph.pl` (generic
renderer). Alag `stackcollapse-*.pl` har profiler ke liye.

**`out.folded` format** (ek line per unique stack):
```
main;run;parse;hash_bytes 1420
main;run;parse;utf8_decode 610
main;run;emit 205
```
Yeh format hi "the interchange" hai — koi bhi source (perf, dtrace, eBPF,
JFR) isme convert ho sakta.

---

## Padhna: 5 rules

```
   +-----------------------------------------------+   <- widest = whole run
   | main                                          |
   +--------------------------------+--------------+
   | run                            | setup        |
   +----------------+--------------+ +-------------+
   | parse          | emit         |
   +-------+-------+ +-----+
   |hash   |utf8   | |fmt  |         <- "plateau": kaam yahan ho raha
   +-------+-------+ +-----+
```

1. **Width = time.** Sirf width maayne rakhta. A wide box = a lot of samples
   had that function on the stack (itself or via its callees).
2. **Y = stack depth.** Bottom = entry (`main` / thread root), up = callees.
   Top of a column = the leaf that was actually **on-CPU** at sample time.
3. **Top plateaus = where work happens.** A wide box at the *top* (no wide
   children) = a hot leaf — that's your optimize target. A wide box lower
   down with narrow spread above = time is *distributed* among its callees.
4. **X is alphabetical, NOT time-order.** Left-to-right ≠ chronological.
   Don't read "A then B". Sibling boxes are just sorted by name (so the same
   function lands in the same place across two graphs → diffable).
5. **Colour is meaningless** (random hue, or by module/type in some
   variants). Don't read into it.

Interactive SVG: **click** a box to zoom into that subtree; **Ctrl-F** /
search highlights matching frames (magenta) and shows their combined % —
great for "how much total time is under `malloc`, anywhere".

---

## Variants

| Variant | Kya | Command |
|---|---|---|
| **On-CPU** (default) | where CPU cycles go | `perf record -g` → fold → render |
| **Off-CPU** | where the thread was **blocked** (I/O, lock, sleep) | `perf sched` / eBPF `offcputime` → fold → render |
| **Icicle** | flipped (root at top) — good for off-CPU / merged | `flamegraph.pl --inverted` |
| **Differential** | red = got worse, blue = got better, between two profiles | `difffolded.pl old.folded new.folded \| flamegraph.pl` |
| **Hot/cold** | on-CPU + off-CPU merged (full wall-clock picture) | combine both folded files |
| **Event flame** | flame by cache-misses / page-faults / allocations, not cycles | `perf record -e cache-misses -g` → same pipeline |
| **Memory flame** | allocation stacks (who allocated how much) | `bcc`/`bpftrace` `malloc` uprobe, or jemalloc prof |

**On-CPU + Off-CPU together** = the complete story. On-CPU alone misses "we
spent 3 seconds blocked on a mutex" (that thread wasn't running, so no
`cycles` samples). Off-CPU flame graph (from `sched:sched_switch`
tracepoints or eBPF) shows blocked time and *why*.

---

## When flame graphs shine / fall short

**Shine:**
- Large codebase, unknown hot path — one glance finds it.
- "Death by a thousand cuts" — a wide but shallow graph = flat profile, no
  single hotspot (architectural problem).
- Regression triage — differential flame graph pinpoints what grew.
- Communicating a perf problem to others — visual, self-explanatory.

**Fall short:**
- **Instruction / line level** — flame graph stops at function granularity;
  drop to `perf annotate` (lesson 11).
- **Time-ordering** — can't see "slow at startup then fine" (x is
  alphabetical). Use a **timeline** view (Firefox Profiler, Perfetto,
  `perf timechart`) for that.
- **Very deep recursion** — towers of identical frames; fold recursion or
  use `--inverted`.
- **Short spiky events** — 999 Hz sampling misses sub-ms events; raise `-F`
  or use tracing.
- **Broken stacks** → "[unknown]" towers → fix with `-fno-omit-frame-pointer`
  / `--call-graph dwarf` / debuginfo.

---

## `07_flamegraph.sh` ka demo

Ek 3-function program: `heavy()` (sqrt loop), `light()` (cheap), `mid()`
(calls `heavy(n/4) + light(n)`). `main` calls all three per iteration.

Expected flame graph:
- `heavy` = **widest plateau** — most CPU. It appears in two places: called
  directly from `main`, and under `mid`. (Two narrow-ish boxes, not merged,
  because different parents.)
- `light` = thin (cheap work).
- `mid` = a frame with `heavy` + `light` split above it.

Script prints the top folded stacks by sample count so you can see the
`main;...;heavy` line dominating even without opening the SVG.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — reading X as time
"Function on the left runs first" — **wrong**, X is alphabetical. Use a
timeline tool for ordering.

### Trap 2 — on-CPU only for a latency problem
Blocked-on-lock / blocked-on-I/O time is **invisible** in an on-CPU flame
graph (no cycles samples while off-CPU). Add an off-CPU flame graph.

### Trap 3 — broken stacks ("[unknown]" towers)
`-g` without frame pointers, or stripped binary, or JIT. Rebuild with
`-fno-omit-frame-pointer`, or `--call-graph dwarf`, install debuginfo.

### Trap 4 — narrow box ignored, but it's your code
A 3%-wide box for *your* function while 60% is in `memcpy`/`malloc`/kernel
— the fix is often in *your* 3% (it's calling `memcpy` too much). Search
(Ctrl-F) your namespace to see total time under your code.

### Trap 5 — comparing two flame graphs by eyeballing
Use a **differential** flame graph (`difffolded.pl`) — red/blue directly
shows what changed. Eyeballing two SVGs misses shifts.

### Trap 6 — sampling rate too low for the question
999 Hz = 1 sample/ms. A function that runs for 200 µs total gets ~0
samples. Raise `-F` (to 4000–10000) or switch to tracing for short events.

---

## Hands-on (Linux)

```bash
git clone https://github.com/brendangregg/FlameGraph ~/FlameGraph
bash 35-PROFILING-BENCHMARKING/examples/07_flamegraph.sh
# builds a demo, and if perf + FlameGraph present, produces _flame/cpu.svg
# and prints the top folded stacks (heavy() dominates).
```

Open `_flame/cpu.svg` in a browser: click `heavy` to zoom, Ctrl-F "heavy"
to see its total %.

Alternatives to the Perl pipeline:
- **`hotspot`** (Qt GUI) — opens `perf.data` directly, flame graph +
  timeline + caller/callee, no scripts.
- **Firefox Profiler** — import `perf.data` (`perf script -F ...`), timeline
  + flame + inverted stacks in the browser.
- **Speedscope** (`speedscope.app`) — import `perf script` output, 3 views
  (time-order, left-heavy, sandwich).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "left box runs first" | X is alphabetical, not time |
| "on-CPU flame = full picture" | add off-CPU for blocked time |
| "colour means something" | random hue; ignore |
| "widest box = optimize that" | widest **top plateau** (leaf); wide-lower = distributed |
| "eyeball two graphs to compare" | differential flame graph (red/blue) |
| "flame graph shows the slow line" | function-level only; annotate for line/instr |

---

## Exercises

1. A flame graph is very **wide and very shallow** — `main` at the bottom,
   then ~40 sibling boxes each 2–4% wide, almost nothing stacked above them.
   What does this shape mean, and what's the strategy?

   <details><summary>Answer</summary>

   **Flat profile — no single hotspot.** Time is spread roughly evenly
   across ~40 functions, each a small slice, each doing its work directly
   (little sub-call depth). This is "death by a thousand cuts" (lesson 01).
   No amount of optimizing one function helps much (Amdahl: 3% × any speedup
   ≈ nothing). Strategies: (a) **step back to architecture** — is there a
   cross-cutting inefficiency? e.g. every function re-parses / re-allocates /
   re-hashes the same thing → a shared cache or a data-layout change helps
   *all* of them at once. (b) **`perf stat` top-down** — maybe they're *all*
   frontend-bound (code too big → PGO / LTO / function ordering) or *all*
   memory-bound (working set too big → shrink it). (c) **reduce call
   volume** — are these 40 functions called more often than necessary
   (redundant work, missing memoization)? The flame graph shape itself is
   the diagnosis: fix the common factor, not the individuals.
   </details>

2. On-CPU flame graph shows your request handler at 100% width, with
   `compute()` 70% and `wait_for_data()` only 5%. But end-to-end latency
   measurement says requests take 40 ms and `compute()` should be ~5 ms.
   Where's the missing 35 ms?

   <details><summary>Answer</summary>

   **Off-CPU time.** The on-CPU flame graph only shows time the thread was
   *running*. `wait_for_data()` is 5% *of on-CPU samples* but the thread was
   probably **blocked** (not scheduled) for most of the 35 ms — waiting on a
   socket read, a condition variable, a lock, a disk read, a downstream RPC.
   While blocked, zero `cycles` samples are attributed to it, so it looks
   tiny in an on-CPU graph. Get the real picture: an **off-CPU flame graph**
   (`perf sched record` + `sched:sched_switch`, or eBPF `offcputime`) — it
   will show a huge `wait_for_data` → `read`/`futex`/`epoll_wait` tower
   accounting for the 35 ms. Or a **timeline** view. On-CPU + off-CPU
   together = the 40 ms explained.
   </details>

3. You have `before.folded` and `after.folded` from a change meant to speed
   up JSON parsing. The two flame-graph SVGs "look about the same". How do
   you actually tell if it worked, and what would regression look like?

   <details><summary>Answer</summary>

   Generate a **differential flame graph**:
   `~/FlameGraph/difffolded.pl before.folded after.folded | ~/FlameGraph/flamegraph.pl > diff.svg`.
   In `diff.svg`: **blue** frames = shrank (fewer samples in "after"),
   **red** = grew. If the change worked, the `parse_json` subtree is solidly
   blue (and the *total* sample count in `after.folded` — `awk '{s+=$NF}
   END{print s}'` — is lower if the workload was fixed-work). Regression
   would show as **red elsewhere**: e.g. you sped up parsing but the new
   code allocates more → a red `malloc`/`operator new` tower appeared, net
   neutral or worse. Also compare raw totals and re-run `perf stat` (cycles,
   not just samples) since sample counts alone don't capture frequency /
   absolute time. Eyeballing two similar SVGs is exactly where differential
   flame graphs earn their keep.
   </details>

---

## Interview questions

1. Flame graph axes — X (alphabetical, width=time), Y (stack depth), colour (none).
2. "Top plateau" vs "wide box lower down" — kaunsa optimize target.
3. On-CPU vs off-CPU flame graph — kya har ek dikhata, dono kyun chahiye.
4. Differential flame graph — kab use, red/blue ka matlab.
5. `stackcollapse` / folded format — kyun woh "the interchange" hai.
6. Flame graph kab kaafi nahi (line-level, time-ordering, short events).

---

## Next
→ [`13-cachegrind-callgrind.md`](13-cachegrind-callgrind.md)
