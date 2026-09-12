# 13 — Testing lock-free code

## Prerequisites
- `08`–`12` (the structures), `27-ATOMICS-MEMORY-MODEL` files 01 (data race), 16 (litmus)
- `23-ERROR-HANDLING` / testing basics

## Yeh topic abhi kyun
Lock-free bugs (ABA, missing acquire/release, reclamation UAF, torn seqlock read)
casual testing se **nahi milte** — woh ek specific interleaving pe hi trigger hote
hain, jo chance se shayad hazaar-baar mein ek baar aaye, aur x86 pe toh aur bhi
kam. Isliye lock-free code ke liye ek alag testing toolbox chahiye.

---

## The testing pyramid for lock-free

```
              ┌─────────────────────────┐
              │  Model checkers          │  exhaustive over small kernels
              │  (herd7, CDSChecker,     │  — PROVES the ordering for a
              │   GenMC, Loom/rust)      │     bounded scenario
              ├─────────────────────────┤
              │  TSan + stress           │  runs the real code, finds
              │  (Linux/macOS)           │  races the schedule happens to hit
              ├─────────────────────────┤
              │  Invariant stress tests  │  millions of ops, N threads,
              │  + checksums             │  assert structural invariants
              ├─────────────────────────┤
              │  Single-thread unit tests│  logic correctness, edge cases
              └─────────────────────────┘
```

Each layer catches a different class. You need all of them.

---

## 1. Single-thread unit tests

Push/pop ordering, empty/full edges, wrap-around, capacity math. Fast, cheap,
catches logic bugs before concurrency muddies things. `examples/01`–`06` all
start with a correctness assertion (checksum, count, `torn == 0`).

---

## 2. Invariant stress tests

N threads hammer the structure for millions of ops; at the end assert a
**structural invariant**:

| Structure | Invariant to check |
|---|---|
| Queue / stack | multiset of all popped == multiset of all pushed; final structure empty |
| SPSC ring | consumer sees a strictly increasing `seq` (no loss, no reorder) — `examples/01` |
| Seqlock | every accepted snapshot is *internally consistent* (`consistent(q)` — `examples/05`) — count torn = 0 |
| MPMC | sum/count of consumed == produced — `examples/03` |

Run at `-O2` (and `-O3 -march=native` — that's what ships), with **more threads
than cores** (forces preemption), for a long time. A non-deterministic failure =
a race. `-O0` hides most of them (folder 27 file 01).

**Weakness:** passing a billion iterations on x86 doesn't exercise a POWER-only
IRIW bug, or an interleaving that just never came up. Necessary, not sufficient.

---

## 3. ThreadSanitizer

`-fsanitize=thread` instruments every memory access and every sync operation; on a
race it prints **both** racing accesses, both stacks, and the (missing)
synchronization. This is *the* tool for "did I get the acquire/release right".

- **Not available on this MinGW** (`cannot find -ltsan`) — run on Linux/macOS in
  CI: `g++ -O1 -g -fsanitize=thread ...`.
- Catches: missing acquire/release (a plain load/store where an atomic was
  needed), a genuine data race in a seqlock payload, a reclamation UAF (also flags
  under ASan).
- Doesn't catch: ABA (the accesses are properly atomic — TSan sees no race),
  logic bugs, liveness/livelock.

Run the **full invariant stress suite under TSan** in CI, at `-O1` or `-O2`.

---

## 4. Model checkers (the only thing that *proves*)

Take a **small kernel** (2–4 threads, a few ops each) and explore **every**
allowed interleaving *and* every allowed memory-model reordering:

| Tool | Scope |
|---|---|
| **herd7** | `.litmus` files — small kernels under a chosen model (`x86tso`, `AArch64`, `Power`, C++11). Best for "is this ordering pattern sound on ARM?" |
| **CDSChecker** | C11/C++11 atomics, explores memory-model behaviours of a bounded program |
| **GenMC** | modern, faster; C/C++ programs with `assert`s, exhaustive under RC11 |
| **Loom** (Rust) | the equivalent for Rust lock-free code |

Use them on the **core protocol** — the enqueue/dequeue CAS sequence, the seqlock
read/write, the hazard-pointer protect/retire handshake — with the invariant as an
`assert`. If it passes exhaustively for 3 threads it's *very* likely correct in
general (most lock-free bugs show up with ≤ 3 threads).

**This is how you actually gain confidence** — not by running x86 stress tests a
billion times.

---

## 5. Targeted interleaving injection

Force the exact bad schedule:
- **`examples/07` (folder 27)** does this for ABA — a `phase` handshake makes
  thread B run its pop/pop/push at exactly the point thread A is stalled.
- Insert `std::this_thread::yield()` / a spin delay at the suspicious point and
  run under load.
- `SIGSTOP` a reader mid-traversal (reclamation test) and check memory / UAF.

---

## What each bug class needs

| Bug | Caught by |
|---|---|
| Missing acquire/release (data race) | **TSan** + stress; model checker |
| ABA | **model checker** or **forced interleaving** (`27/07`); *not* TSan |
| Reclamation UAF | **ASan** + stress with a stalled reader; model checker |
| Torn seqlock read | invariant stress (`consistent()` check) + TSan on the payload |
| Livelock / retry storm | throughput/latency **benchmark** under contention (`examples/04`) |
| Weak-memory bug (works on x86, breaks on ARM) | **herd7 / GenMC** with an ARM model; ARM CI leg |

---

## > **HFT relevance**
> - **CI matrix:** every lock-free structure gets (1) unit tests, (2) an invariant
>   stress test at `-O2`/`-O3 -march=native` with 2–4× oversubscription, (3) the
>   same under TSan on x86-64 Linux, (4) a herd7/GenMC model of the core protocol,
>   (5) a stress run on an **ARM** box (Graviton) — because "passes on x86" proves
>   nothing about the memory model (folder 27 file 12).
> - **Model-check before you ship** any new lock-free primitive. The 3-thread
>   exhaustive check is worth more than weeks of x86 stress testing.
> - **Benchmark contention explicitly** — a structure can be correct and still
>   lose to a mutex (`examples/04`). The perf test is part of the correctness
>   gate: "is lock-free actually buying us anything here?"
> - **Keep a forced-interleaving regression test** for every ABA/reclamation bug
>   you ever hit — the `phase`-handshake style (`27/07`) so it deterministically
>   reproduces.

---

## Hands-on

```bash
# invariant stress (Windows / MinGW — no TSan, but the checksums still catch a lot)
./build.ps1 fast 28-LOCK-FREE/examples/03_mpmc_queue.cpp      # sum/count invariant
./build.ps1 fast 28-LOCK-FREE/examples/04_lock_free_stack.cpp # multiset invariant + ABA guard
./build.ps1 fast 28-LOCK-FREE/examples/05_seqlock.cpp         # torn == 0 invariant

# Linux CI:
g++ -std=c++20 -O1 -g -fsanitize=thread 28-LOCK-FREE/examples/03_mpmc_queue.cpp -o mpmc_tsan && ./mpmc_tsan
```

Then: write a 3-thread `.litmus` for the SPSC publish (`buf write` + `head
release` / `head acquire` + `buf read`) and run `herd7 -model C11` — it should
report the stale-read outcome as **forbidden**.

---

## ⚠️ Traps

### Trap 1 — "stress test passed a billion times, it's correct"
It exercised the interleavings the scheduler happened to pick on x86. Model-check
the protocol; run on ARM.

### Trap 2 — testing at `-O0`
`-O0` suppresses the optimizations that expose memory-model bugs. Test at `-O2`+.

### Trap 3 — TSan on ABA
ABA uses properly atomic accesses → no data race → TSan is silent. Use a model
checker or a forced interleaving.

### Trap 4 — no oversubscription
With ≤ cores threads, preemption is rare and stalled-thread bugs (reclamation,
lagging `tail`) don't surface. Use 2–4× threads.

### Trap 5 — skipping the contention benchmark
A correct lock-free structure can be slower than a mutex (`examples/04`). If you
don't measure, you ship a slower, riskier thing.

### Trap 6 — model-checking with too many threads/ops
The state space explodes. 2–4 threads, 2–4 ops each — that's where the bugs live
anyway.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "lots of stress iterations = proof" | Only a model checker proves; stress finds what the schedule hits |
| "TSan finds all lock-free bugs" | Finds data races; misses ABA, logic, livelock |
| "`-O0` is a fair test" | Hides memory-model bugs — test `-O2`/`-O3` |
| "passes on my x86 laptop → correct" | Weak-memory bugs need ARM / herd7 to surface |
| "correctness is separate from perf" | For lock-free, "does it beat the mutex?" is part of the gate |
| "model-check with 8 threads for realism" | 2–4 threads; the state space and the bugs are both there |

---

## Exercises

1. **Tool per bug:** which tool catches (a) a `relaxed` where `release` was
   needed; (b) an ABA on a Treiber `pop`; (c) a use-after-free in MS-queue
   reclamation; (d) a retry storm.

   <details><summary>Answer</summary>

   (a) TSan (flags the data race) or a model checker. (b) a model checker or a
   forced interleaving — *not* TSan. (c) ASan + a stalled-reader stress test, or a
   model checker. (d) a contention benchmark measuring throughput/latency vs a
   mutex.
   </details>

2. **Why oversubscribe?** what bug class needs more threads than cores to show up?

   <details><summary>Answer</summary>

   Bugs that require a thread to be *preempted mid-operation*: lagging-`tail`
   helping paths, reclamation UAF (a thread stalled holding a pointer), seqlock
   torn reads. With ≤ cores threads the OS rarely preempts a running thread, so
   these interleavings almost never occur. 2–4× threads forces frequent
   preemption.
   </details>

3. **herd7 outcome:** you model the SPSC publish as W(buf)=1; W_rel(head)=1 ‖
   R_acq(head); R(buf). What outcome should herd7 report as forbidden, and what
   does it mean if it's *allowed*?

   <details><summary>Answer</summary>

   Forbidden: `head` observed as 1 with `buf` read as 0. If herd7 reports it
   *allowed*, your memory orders are wrong (e.g. `relaxed` instead of
   `release`/`acquire`) — the consumer could see the published index with stale
   data. Tightening to `release`/`acquire` makes it forbidden.
   </details>

4. **ABA regression test:** how do you make an ABA bug reproduce deterministically
   in CI?

   <details><summary>Answer</summary>

   A `phase`/handshake between two threads (as in `27/07`): thread A reads
   `head`/`next` then waits on a flag; thread B does pop/pop/push and sets the
   flag; A then runs its CAS. This forces the exact interleaving every run, so a
   regression (tag removed, tag too narrow) fails immediately instead of once in a
   billion.
   </details>

5. **CI matrix:** list the minimum set of test configs for a new lock-free queue
   before it's allowed on the hot path.

   <details><summary>Answer</summary>

   (1) single-thread unit tests (edges, wrap, capacity). (2) invariant stress at
   `-O2`/`-O3 -march=native`, 2–4× oversubscription, minutes-long. (3) the same
   under TSan on x86-64 Linux. (4) a herd7/GenMC model of the core CAS/publish
   protocol with the invariant as an assert. (5) an invariant stress run on ARM
   (Graviton). (6) a contention benchmark vs a mutex baseline.
   </details>

---

## Interview questions

1. Lock-free bugs casual testing se kyun nahi milte?
2. Testing pyramid — har layer kaunsa bug class pakadta?
3. TSan kya pakadta, kya nahi (ABA)?
4. Model checker kyun zaroori — "proves" ka matlab, kitne threads?
5. Oversubscription kyun (kaunsa bug class)?
6. `-O0` pe test kyun invalid?
7. "x86 pe pass" ke baad ARM / herd7 kyun chahiye?

---

## Next
→ [`14-when-not-lock-free.md`](14-when-not-lock-free.md)
