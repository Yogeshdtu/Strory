# 06 — Out-of-order execution: ROB, register renaming, reservation stations

## Prerequisites
- `05-superscalar.md` (ports, ILP)
- `04-pipelining.md` (hazards)
- `02-registers.md` (renaming preview)

## Yeh topic abhi kyun
File `05` ne kaha "CPU tumhare instruction stream mein ILP dhoondhti hai". **Yeh
lesson batata woh dhoondhti kaise hai:** instructions ko program order mein
fetch/decode/retire karta, par beech mein **jo bhi ready hai woh pehle execute**
karta. Yeh machinery — reorder buffer, register renaming, reservation stations —
hi wajah hai ki ek cache miss ke peeche ka independent kaam ruk-ta nahi, aur 4
parallel chains 4× tez chalti (example `02`).

---

## The problem OoO solves

In-order execution: `load rax, [mem]` misses cache (~200 cyc). Every instruction
after it — even ones that don't use `rax` — waits. 200 cycles of nothing.

OoO execution: the missing load goes off to memory; meanwhile the CPU executes
**younger independent instructions** whose inputs are ready. When the load
completes, its dependents fire. The 200-cycle stall is (partly) *hidden* by
useful work.

---

## The pipeline, OoO version

```
  IN ORDER:  Fetch -> Decode -> Rename/Allocate -> [dispatch into the window]
                                                        |
  OUT OF ORDER:                        +---------------- v ----------------+
                                       |  Scheduler / Reservation Stations |
                                       |  (µop waits here until its inputs |
                                       |   are ready AND a port is free)   |
                                       +-------+--------------------+------+
                                               |  execute on ports  |
                                       +-------v--------------------v------+
  IN ORDER:                            |   Reorder Buffer (ROB) retire     |
                                       |   -> commit results, free regs,   |
                                       |      in PROGRAM ORDER              |
                                       +----------------------------------+
```

Three key structures:

### 1. Reorder Buffer (ROB)
A FIFO of all **in-flight** µops, in program order. ~200–500+ entries (this box,
Zen 2: ~180). A µop enters at allocate, executes whenever ready, and **retires**
(commits its result to architectural state) only when it reaches the head of the
ROB *and* is complete. Retirement is **in order** → precise exceptions,
consistent architectural state, correct rollback on mispredict.

The ROB size is your **out-of-order window** — how far ahead the CPU can look for
independent work. A load miss at the head + a 180-entry ROB → the CPU can run up
to ~180 younger µops before it *must* stall (ROB full).

### 2. Register renaming
The 16 architectural registers (file `02`) are mapped to a large **physical
register file** (~150–300 entries). Every write to `rax` gets a *fresh* physical
register.

- Kills **WAW** (write-after-write): `rax = a; rax = b;` — different physical
  regs, can execute in any order / parallel.
- Kills **WAR** (write-after-read): `use(rax); rax = c;` — the write doesn't wait
  for the read.
- **RAW** (true data dependency) remains — `b = a+1; c = b*2;` — `c` genuinely
  needs `b`. This is the only real chain.

This is why example `02`'s 4 accumulators (`a,b,c,d`) run independently even
though C++-level they're all just `uint64_t` — the CPU renames each to its own
physical register per iteration.

### 3. Reservation Stations / Scheduler
Renamed µops wait in the scheduler until: (a) all their source operands are
ready (produced by an already-executed µop, or forwarded), and (b) a suitable
execution port is free. Then they **issue**. Multiple µops issue per cycle
(superscalar, file `05`). This is where "execute whatever's ready" happens.

---

## Speculation and rollback (file `07`, `08`)

OoO execution runs **speculatively** past branches (predicted direction) and past
loads (assuming no aliasing store). If a prediction was wrong:

- Every µop after the mispredicted branch in the ROB is **squashed**.
- The renamer's mapping is rolled back to the checkpoint at the branch.
- Fetch restarts from the correct target. ~15–20 cycle refill (file `04`).

**Memory speculation:** a load can execute before an older store whose address
isn't known yet (assuming they don't alias). If they *did* alias → **memory
order violation**, squash and replay. This is why `__restrict` (example `06`) and
avoiding pointer aliasing helps — the CPU (and compiler) can be less conservative.

---

## What limits the OoO window in practice

| Resource full → stall | Typical size (varies by µarch) |
|---|---|
| **ROB** entries | ~200–500 |
| **Physical registers** (int / vec) | ~150–300 each |
| **Load buffer** / **store buffer** entries | ~64–128 / ~50–100 |
| **Scheduler / RS** entries | ~90–160 |
| **Branch order buffer** (in-flight branches) | ~48–128 |

A hot loop that stalls even though there's independent work nearby → you may be
hitting one of these. Long-latency misses (folder 32) fill the ROB; too many
in-flight loads fill the load buffer; deeply nested branches fill the BOB.
`perf` events like `RESOURCE_STALLS.*` (Intel) tell you which.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — "OoO hides all memory latency"
It hides *up to a window's worth*. A single load miss: hidden if ~180 independent
µops follow. A **dependent chain** of misses (pointer chasing — linked list,
`std::map` traversal, folder 20/32) → each miss stalls the next, nothing to
overlap, full ~200–400 cyc each. This is why array-of-structs-with-pointers is
so much slower than a flat array (folder 32).

### Trap 2 — thinking renaming removes RAW chains
It removes *false* dependencies (WAW/WAR). The true data-flow chain (`a → f(a) →
g(f(a))`) is physics — the CPU cannot execute `g` before `f` produces its input.
Shorten the chain, don't expect the CPU to.

### Trap 3 — huge basic blocks assuming the window "sees everything"
The ROB is ~200–500 µops. A 5000-instruction straight-line block: the CPU only
looks ~200–500 ahead. Structure hot code so independent work is *near* the stall,
not 3000 instructions away.

### Trap 4 — ignoring memory-order-violation replays
`a[i] = x; y = b[j];` where the compiler/CPU can't prove `&a[i] != &b[j]` → the
load may speculate, then get squashed+replayed if they alias. `__restrict`,
separate buffers, SoA layouts reduce this.

### Trap 5 — assuming retirement order == execution order
It doesn't. A `perf` sample or a crash address points at the *retiring*
instruction; the actual slow instruction executed earlier and its result was
sitting in the ROB. Reason about data flow, not source line order.

### Trap 6 — very long dependency chain + expecting SMT to help
SMT (file `12`) shares the ROB/RS/PRF between two threads. A thread stuck on a
dependency chain leaves resources for the sibling — that's the *one* case SMT
helps. But for two latency-critical threads it just halves everyone's window.

---

## > **HFT relevance**

> - **Pointer chasing is the enemy.** A dependent chain of cache misses (linked
>   list, node-based tree, `std::map`, `std::unordered_map` with chaining) can't
>   be hidden by OoO — each miss stalls the next. Flat arrays, open addressing,
>   implicit trees, indices instead of pointers (folder 20/32). This is often
>   the single biggest hot-path win.
> - **Give the window independent work near the stall.** Prefetch the *next*
>   record while processing the current one; interleave two independent
>   computations; software-pipeline the loop.
> - **`__restrict` / separate buffers / SoA** — fewer memory-order-violation
>   replays and better compiler scheduling (example `06`).
> - **Short critical paths** — the OoO engine can't beat the sum-of-latencies on
>   the true dependency chain. Fewer `imul`/`div`, hoist invariants, split
>   reductions.
> - **`perf` `RESOURCE_STALLS` / `CYCLE_ACTIVITY.STALLS_*`** to see *which*
>   structure is the bottleneck.

---

## Hands-on

```bash
# example 02: renaming lets 4 "identical" C++ variables be independent chains
./build.ps1 fast 31-CPU-ARCHITECTURE/examples/02_pipeline_stall.cpp

# pointer-chase vs flat-array (dependent misses can't be hidden) -- folder 20/32
# a quick feel: linked list traversal vs vector sum of the same N ints

# Linux: which OoO resource is stalling
perf stat -e cycles,instructions,cycle_activity.stalls_mem_any,resource_stalls.any ./prog
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "OoO hides all cache-miss latency" | only ~window's worth; a *chain* of misses stalls fully |
| "renaming removes dependency chains" | removes WAW/WAR; the true RAW chain remains |
| "the CPU sees my whole function" | it sees ~200–500 µops ahead (ROB size) |
| "execution order = source order" | fetch/retire in order; execute out of order |
| "a crash address = the slow instruction" | that's the *retiring* one; the stall was earlier |
| "SMT gives each thread a full window" | shared ROB/RS/PRF, split two ways |

---

## Exercises

1. `for (node = head; node; node = node->next) sum += node->val;` over a 1M-node
   list, nodes scattered in memory. Kyun OoO isse hide nahi kar paati?

   <details><summary>Answer</summary>

   Each iteration's `node = node->next` is a **load that depends on the previous
   iteration's loaded pointer**. So the misses form a serial chain: miss on
   `node->next` (~200-400 cyc) → get the pointer → miss on the *next*
   `node->next` → ... There is no independent work to overlap — the very thing
   OoO needs. Result: ~200-400 cycles *per node*. A `std::vector<int>` sum over
   1M ints touches contiguous memory, the hardware prefetcher stays ahead, and
   it's ~1 cycle/element — often **50-100× faster** for the same logical work.
   This is the core lesson of cache-aware data structures (folder 20/32).
   </details>

2. Register renaming ke bina `mov rax, [p1]; add rbx, rax; mov rax, [p2]; add
   rcx, rax;` mein kaunsa false hazard hota, aur renaming usse kaise todta?

   <details><summary>Answer</summary>

   Without renaming, the third instruction `mov rax, [p2]` has a **WAR hazard**
   with the second (`add rbx, rax` reads `rax`) and a **WAW hazard** with the
   first (`mov rax, [p1]` wrote `rax`) — so `mov rax, [p2]` can't execute until
   `add rbx, rax` has read the old `rax`. That serialises the two independent
   load-add pairs. With renaming, `mov rax, [p2]` writes a **new physical
   register**; the old `rax` (feeding `add rbx`) lives in a different physical
   register. Now both `[p1]→rbx` and `[p2]→rcx` pairs execute in parallel,
   limited only by the two load ports.
   </details>

3. ROB 224 entries hai. Ek load L3-misses (~250 cyc). Uske baad 300 independent
   ALU instructions hain. Kya hota?

   <details><summary>Answer</summary>

   The load sits at (or near) the ROB head, waiting ~250 cycles for memory. The
   CPU allocates and executes the following independent ALU instructions — but
   only until the **ROB fills** (224 entries). After ~224 in-flight µops, no new
   µop can be allocated → the front end stalls even though there's ready work.
   So ~224 of the 300 independent instructions get overlapped with the miss; the
   rest wait. If the miss latency × issue-rate exceeds the ROB size, you stall
   regardless of how much independent work exists. Bigger ROBs (newer µarchs)
   and prefetching (starting the miss earlier) both help.
   </details>

4. "Memory order violation replay" kya hai, aur `__restrict` ise kaise kam
   karta?

   <details><summary>Answer</summary>

   The CPU speculatively executes a load *before* an older store whose address
   isn't computed yet, betting they don't alias. If they turn out to alias (the
   store writes the address the load read), the load got stale data → the CPU
   **squashes the load and everything after it and replays** — a mini-mispredict
   penalty. `__restrict` on pointers tells the *compiler* the pointed-to regions
   don't overlap, so it can schedule loads/stores more aggressively and doesn't
   emit conservative code; separate/SoA buffers make aliasing impossible so
   neither compiler nor CPU has to be cautious. Example `06`: the `no __restrict`
   version is ~2-4× slower partly for this reason (plus lost vectorization).
   </details>

5. Tumhare hot loop mein ek `sqrt` hai (latency ~15 cyc, not fully pipelined).
   OoO iski latency kaise hide kar sakti — aur kab nahi?

   <details><summary>Answer</summary>

   OoO hides it **if** the `sqrt`'s result isn't needed for ~15 cycles of
   following independent work — the scheduler runs other µops while the sqrt
   unit computes. So `d = sqrt(x); ... 15 cycles of unrelated work ...; use(d);`
   → hidden. It **cannot** hide it if `sqrt` is *on the critical path*: `y =
   sqrt(x); z = y * a; w = sqrt(z); ...` — each sqrt feeds the next, nothing to
   overlap, so you pay ~15 cyc each. Fix: pull the sqrt off the dependency chain
   (compute it in parallel with other work), use `rsqrtps` + one Newton-Raphson
   step (~5 cyc, lower precision), or restructure the math to avoid it.
   </details>

---

## Interview questions

1. The problem OoO execution solves — in-order stall on a cache miss.
2. Reorder Buffer — what it holds, why retirement is in-order, what its size limits.
3. Register renaming — which hazards it removes (WAW/WAR), which it can't (RAW).
4. Reservation stations / scheduler — the two conditions for a µop to issue.
5. Speculation past branches and loads — what a mispredict/violation costs.
6. Why can't OoO hide a *dependent chain* of cache misses (pointer chasing)?
7. The OoO resources that fill up and stall the front end (ROB, PRF, load/store buffers).

---

## Next
→ [`07-branch-prediction.md`](07-branch-prediction.md)
