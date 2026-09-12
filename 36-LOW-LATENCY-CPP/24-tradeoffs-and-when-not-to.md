# 24 — HONEST trade-offs: when all of this is wrong

## Prerequisites
- Lessons `01`–`23` of this folder
- `35-PROFILING-BENCHMARKING/01` (measure, Amdahl), CLAUDE.md spec Rule 12–13

## Yeh topic abhi kyun
Spec ka rule: **"koi micro-optimization trick bina context ke nahi. Har
technique ke saath trade-off — kab use karo aur kab NAHI."** Yeh lesson woh
hai. Har technique is folder ki ek **cost** hai jo tab tak nuksaandeh hai
jab tak profiling na dikhaye ki faayda cost se zyada hai.

---

## Rule 0: measure first, always

Har lesson mein "profile → hotspot → change one thing → re-measure → explain
what changed" (35/01). If you can't point at a `perf` number that says this
stage is over budget, and a `perf` number after that says it improved —
**you don't know if you helped.** The `12_hot_cold_split.cpp` result (no
measurable difference) is in this folder *on purpose*: a technique that
"should" help showed nothing in that context. That's the norm, not the
exception.

---

## The cost of each technique

| Technique | The hidden cost |
|---|---|
| **memory pools** (06) | fixed capacity → an overflow policy; per-thread → cross-thread frees need a ring hop; a whole class of bugs (double-free into a corrupt free-list) that `malloc` diagnoses for you |
| **arenas** (08) | no individual free → lifetime coupling; non-trivial dtors not run; a pointer that escapes the scope = UAF; overflow policy |
| **pre-allocation** (05) | RAM committed up front; a wrong worst-case guess → production overflow *or* wasted RAM + TLB pressure; you must *prove* zero-alloc steady state |
| **branchless** (12) | slower on predictable branches; unreadable without a comment + a measured win; often the compiler already if-converted your `if` |
| **CRTP / templates** (13, 23) | code bloat (one instantiation per type/value) → I-cache pressure; closed set; longer compiles; viral through APIs |
| **`std::variant`/switch** (13) | closed set; `sizeof` = largest alternative; every `visit` touched when you add a type |
| **lock-free ring** (15) | SPSC only (MPMC = harder); a subtle memory-order bug is a rare, load-dependent heisenbug; a full ring needs a drop policy; a ring where a call would do just adds a hop |
| **busy-poll** (17) | 100% CPU on a core forever (power, heat, a core gone); only safe on an isolated dedicated core |
| **kernel bypass** (17) | you now own (part of) the TCP/IP stack, buffer management, no kernel firewall/routing; a large, ongoing engineering commitment |
| **`mlockall` + huge pages** (18) | `RLIMIT_MEMLOCK` config; explicit hugetlbfs reservation; less RAM available for page cache; misconfig → partial lock, silent |
| **CPU pinning + isolation** (19) | fewer cores for everything else; a mis-tuned `SCHED_FIFO` can hang the box; ops complexity (BIOS + cmdline + IRQ + NUMA) |
| **cache warming / dry-run** (20) | CPU cost; the dry-run must be *provably* side-effect-free (a leaked synthetic order is catastrophic); representative data required |
| **hot/cold split / PGO** (21) | PGO needs a representative profile + a two-stage build; `[[gnu::cold]]` is a hint; measured *nothing* in a micro-bench |
| **zero-copy / overlays** (22) | alignment, endianness, lifetime, aliasing hazards; a view outliving its buffer = UAF |
| **compile-time dispatch** (23) | 2^N instantiations → I-cache bloat that can *worsen* p99 |

**Every row is a bug surface, a maintenance burden, or a resource cost.**
The general-purpose version (`std::vector`, `new`, `virtual`, a blocking
`recv`, an unpinned thread) is *simpler, safer, and well-understood*. You
trade that away for latency you can measure you need.

---

## When NOT to do any of this

### 1. This code is not on the hot path
`perf` says this function is 0.3% of the time. Amdahl: 10× faster → 0.27%
saved. Leave it idiomatic. Spend the effort on the 40% hotspot.

### 2. The latency budget is already met
p99.9 is comfortably under budget with the simple code. Adding a pool /
branchless / a template here buys nothing measurable and adds risk. Stop.

### 3. It's the cold path
Error handling, startup, config reload, admin RPCs, logging *format* code,
shutdown. Optimizing these is wasted effort — and inlining/complicating them
*hurts* the hot path's I-cache (lesson 21). Keep them simple; mark them
`[[gnu::cold]]`.

### 4. Throughput, not latency, is the goal
A backtest, a risk report, an EOD reconciliation, a data-loading job.
Batching, big allocations, `std::vector` growth, `std::function`,
multi-threading for parallelism — all fine, even preferred. The whole folder
inverts.

### 5. The simple version is fast enough and the complex one is not clearly faster
`12_hot_cold_split.cpp`: A/B/C all ~1.49 ns. If your change isn't *measurably*
better (beyond run-to-run noise — 35/09), it's not better. Revert it.

### 6. You can't maintain it
A branchless bit-trick nobody on the team can read, a lock-free structure
without a model-checked proof, a 6-deep template that takes 40s to compile
and produces 200 lines of error on a typo. If the team can't safely change
it, the latency win is a liability. Prefer the version you can reason about.

### 7. Correctness is at risk
A recycled object read with stale fields. An arena pointer that escapes. A
`relaxed` atomic that races on ARM. A `dry_run` that could emit a real
order. A pool double-free. In HFT, a correctness bug on the hot path can
lose more in one event than the latency optimization saves in a year.
**Deterministic-and-correct beats fast-and-subtly-wrong**, always.

---

## The process (spec Rule 12, restated)

```
  1. Build the SIMPLE version. Make it correct. Ship it (internally).
  2. MEASURE — end-to-end p50/p99/p99.9 per stage (folder 35).
  3. Is a stage over budget?  NO -> stop. You're done.
  4. PROFILE that stage — what's it bound on? (alloc / syscall / cache /
     branch / dispatch / I-cache — `perf`, folder 35/10-14).
  5. Apply ONE technique from this folder that targets that bound.
  6. RE-MEASURE. Better beyond noise?  NO -> revert it.
  7. EXPLAIN what changed and why — in a comment, with the numbers.
  8. Loop 3-7 until the budget is met or you hit a floor you document honestly.
```

Never skip to step 5. Never keep a change that step 6 didn't validate.

---

## Documenting a floor honestly

Sometimes you can't hit the budget. Say so, with the number and the reason:
> "book-update stage p99.9 = 420 ns vs a 350 ns budget. The 70 ns gap is
> the `symbol_id`→`Book` load, which is an L2 hit (~14 cyc) because the book
> array is 3 MB and doesn't fit L1. Options considered: hot/cold split the
> `Book` struct (tried — 15 ns, not enough), huge pages (in place),
> per-core book shards (would fit L1 but doubles memory and complicates the
> cross-shard quote). Accepting the 70 ns; revisit if the budget tightens."

That's worth more than a heroic, fragile optimization that shaves 40 ns and
breaks in six months.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — optimizing without a budget
"Faster is better" → endless complexity for latency nobody needs. A budget
tells you when to stop.

### Trap 2 — keeping an unvalidated change
It "should" be faster → measured noise → shipped anyway → now the code is
complex *and* not faster.

### Trap 3 — applying hot-path techniques to the cold path
Wasted effort, and it bloats the hot path's I-cache.

### Trap 4 — a latency optimization that risks correctness
On the hot path in HFT, a subtle bug can cost more than the optimization
ever saves. Correct + deterministic first.

### Trap 5 — code the team can't maintain
The latency win is a liability if nobody can safely modify it. Readable +
slightly slower often wins.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "low-latency = always use pools/branchless/templates" | only where profiling shows a bound this technique targets, and re-measure |
| "faster is always better" | a budget defines "fast enough"; past it, only risk |
| "the technique should help, keep it" | if step 6 didn't validate it beyond noise, revert |
| "optimize the whole codebase" | hot path only; cold path stays simple (and it helps the hot path) |
| "a fragile 40 ns win is worth it" | not if the team can't maintain it or it risks correctness |

---

## Exercises

1. Ek junior engineer ne poore feed handler ko rewrite kar diya: sab
   `std::vector` → custom pools, sab `if` → branchless, sab `virtual` →
   CRTP, `std::string` → `string_view` everywhere. Benchmark dikhata p50
   180 ns → 175 ns, p99.9 900 ns → 890 ns. Code review mein tum kya kahoge?

   <details><summary>Answer</summary>

   The change bought **~3% on p50 and ~1% on p99.9** — within run-to-run
   noise (35/09) on any real box. In exchange it added: a fixed-capacity
   pool per structure (each needs an overflow policy and is a double-free
   bug surface), branchless code that's harder to read and may be *slower*
   where the branches were predictable (12), CRTP that closes the type set
   and bloats code, and `string_view`s that now have lifetime hazards
   (dangling past buffer reuse — 22). None of it is validated by a
   meaningful improvement.
   Feedback: **revert almost all of it.** Ask: (1) Was the feed handler even
   over budget? If p99.9 900 ns is within the stage's budget, no optimization
   was warranted. (2) Which *one* thing was the bound? `perf`-profile the
   original — if it's, say, allocation in the parse temporaries, then apply
   **one** technique (an arena for the parse scope — lesson 08) and
   re-measure *that*. If that moves p99.9 meaningfully, keep it, comment it
   with the numbers, and stop. (3) The `string_view` change specifically:
   audit for any view stored past the buffer's lifetime — that's a
   correctness bug, not a perf win. Blanket-applying every technique is the
   anti-pattern this whole lesson warns against; targeted, measured,
   one-at-a-time is the process.
   </details>

2. Tumhari book-update stage p99.9 = 500 ns, budget 400 ns. `perf` dikhata
   ~40% Backend/Memory Bound — the `Book` struct (2 KB, has hot px/qty and
   cold client-tag/history fields) doesn't fit L1 well. Tumhare paas 3
   options: (a) hot/cold field split, (b) per-core book shards so each fits
   L1, (c) accept the gap. Har ek ka trade-off, aur tum kaise decide karoge?

   <details><summary>Answer</summary>

   (a) **Hot/cold field split** (lesson 10): put `{px, qty, side}` (~24 B)
   in a dense array, move `{client_tag[64], fill_history[...]}` to a
   separate structure keyed by order/book id, allocated lazily. Now the hot
   iteration touches ~2.6 books/cache-line instead of straddling 2 KB
   structs. Trade-off: two lookups where the cold fields are needed (rare
   path), and the split must be maintained as fields are added. Low risk,
   moderate effort. **Try this first** — measure the p99.9 delta.
   (b) **Per-core book shards**: partition symbols across N strategy cores,
   each core owns ~1/N of the books → each shard's hot data fits L1.
   Trade-off: ~N× the memory (each shard needs its own structures, or
   careful sharing), cross-shard quotes (a quote for a symbol on another
   core) need a ring hop, load imbalance if some symbols are hotter,
   and the whole strategy has to become shard-aware. High effort, high
   architectural impact — only if (a) isn't enough and the latency really
   matters.
   (c) **Accept the 100 ns gap** and document it (as in the lesson): the
   simple code, the measured floor, the reason, and the options considered.
   Valid if the budget is soft, or if (a)/(b)'s complexity/risk outweighs
   100 ns.
   Decide by: **cost of the 100 ns** (does it lose trades / fills? by how
   much $?) vs **cost + risk of each fix**. Try (a) — cheap, low-risk,
   might close the gap. If it gets to ~430 ns, maybe accept that. Reach for
   (b) only if 100 ns is genuinely expensive *and* (a) fell well short.
   Never do (b) "to be safe" — it's a large, permanent complexity increase.
   </details>

---

## Interview questions

1. The measure → profile → one change → re-measure → explain process — why each step.
2. Three techniques from this folder and the specific hidden cost of each.
3. Six situations where NOT to apply hot-path optimization.
4. `12_hot_cold_split.cpp` measured no difference — why that result is *in* the folder.
5. Documenting a latency floor you can't beat — what to write.
6. "A fragile 40 ns win" — the argument against keeping it.

---

## Next
→ [`25-exercises.md`](25-exercises.md)
