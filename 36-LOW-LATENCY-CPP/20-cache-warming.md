# 20 — Cache warming: keeping code and data hot

## Prerequisites
- **`32-CACHE-MEMORY-PERFORMANCE`**, **`35-PROFILING-BENCHMARKING/09`** (cold start)
- `18-page-fault-avoidance.md`, `19-cpu-pinning-strategy.md`
- `examples/11_page_fault_warmup.cpp`

## Yeh topic abhi kyun
The **first** time a code path runs after being idle: I-cache miss on the
instructions, D-cache miss on the data, BTB/branch-predictor cold (a few
mispredicts), TLB cold, maybe a page fault (lesson 18), maybe the core just
woke from a C-state. All of that lands on **the first event after a quiet
period** — which in trading is often the event that matters most (the market
just moved). Cache warming keeps the hot path hot so there is no "first
time".

---

## Two problems

### 1. Cold start (once, at go-live)
Fixed by the startup recipe: pre-fault memory (18), run every hot code path
once with dummy data, establish connections, calibrate `rdtsc`. After this,
everything is resident and the caches have *seen* the hot path.

### 2. Cache decay during quiet periods (continuous)
Even after warm-up, if the hot path is idle for even a few milliseconds:
- Its **instructions** get evicted from L1i / the µop cache by whatever else
  ran (housekeeping, a log flush, an interrupt handler).
- Its **data** (the book, the pools, lookup tables) gets evicted from L1/L2
  by the same.
- The **branch predictor** state ages.
So the next real event pays cold-miss cost. Markets have quiet stretches;
the event ending a quiet stretch is exactly when latency matters.

---

## Warming techniques

### Dry-run the hot path with dummy data
On a timer (e.g. every 100 µs) when idle, run the *actual* hot path with a
synthetic message that produces **no side effect** (no order sent):
```cpp
if (now - last_real_event > WARM_INTERVAL) {
    Message dummy = make_synthetic_quote();
    hot_path(dummy, /*dry_run=*/true);       // updates a scratch book, decides, but
                                             // the order stage is a no-op in dry_run
}
```
This keeps the instructions in I-cache, the data structures in L1/L2, and the
predictor trained. The `dry_run` flag must be **branchless / predicted** so
it doesn't itself distort the hot path (a `[[likely]] if (!dry_run)` at the
order stage, or a function pointer swapped to a no-op).

### Touch the working set
Periodically read (or checksum) the hot data structures — the book arrays,
the pools' active region, lookup tables — so they stay in L2/L3. Cheap, and
it doubles as a consistency check.

### Keep the core busy (no C-state)
An idle core drops into a C-state; waking costs µs (35/06). On the trading
core, **busy-poll** (lesson 17) — the poll loop itself keeps the core in C0
and keeps the poll-loop instructions hot. `idle=poll` / `processor.max_cstate=1`
as a backstop (19).

### Pin the cold path away
Housekeeping, logging, metrics — anything that runs between real events and
evicts your hot path — belongs on the **OS cores** (19), not the trading
core. The less that runs on the trading core, the less warming you need.

### Prefetch on wake (targeted)
If you know an event is imminent (a related symbol just moved), issue
`__builtin_prefetch` for the book / pool lines you're about to touch. ⚠️
measured caveat (32/06): prefetch is often neutral or harmful — measure it
for *this* path.

---

## The dry-run trade-off

- ✅ keeps the hot path genuinely hot — the next real event runs at
  steady-state speed.
- ❌ costs CPU (a full hot-path execution every WARM_INTERVAL) — negligible
  on a dedicated core.
- ❌ the dry-run must be **provably side-effect-free** — a bug that lets a
  synthetic order through is catastrophic. Isolate the order stage behind a
  hard `dry_run` check that's tested.
- ❌ synthetic data must be **representative** — warming the path with inputs
  that take a different branch than real data warms the wrong code.

Many shops run the *entire* strategy against a replayed/synthetic feed at
low rate continuously, precisely so the code is never cold.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — warming only memory, not code
Pre-faulting the heap doesn't put the hot path's *instructions* in I-cache
or train the predictor. Dry-run the code.

### Trap 2 — a `dry_run` branch that isn't predicted
An unpredictable `if (dry_run)` in the middle of the hot path adds a
mispredict to *every real* event. Make it `[[likely]]` straight-line, or
swap a function pointer.

### Trap 3 — synthetic data that takes the wrong branch
Warming with all-zeros / a trivial message trains the predictor for a path
real data never takes. Use representative synthetic inputs.

### Trap 4 — a dry-run that can emit a real order
Test the isolation hard. A synthetic quote must not be able to send an
order, ever.

### Trap 5 — housekeeping on the trading core between events
Every log flush / metric push on that core evicts the hot path. Move them
off (19) — that reduces the need for warming in the first place.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "warm-up at startup is enough" | caches decay in quiet periods; keep warming continuously |
| "pre-fault the heap = warm" | + I-cache + predictor + TLB via dry-runs |
| "just add an `if (dry_run)`" | must be predicted/branchless or it taxes every real event |
| "any synthetic message warms it" | must be representative (same branches as real data) |
| "warming is the fix for a cold path" | first move the cold stuff off the trading core |

---

## Exercises

1. Ek strategy thread busy-polls the feed. Markets quiet for ~5 ms, phir ek
   burst. Pehla message of the burst measures p99.99-level latency (~4 µs vs
   steady 300 ns). Tumne memory pre-fault + `mlockall` kiya hua hai. Kya
   cold hai, aur ek warming scheme.

   <details><summary>Answer</summary>

   Memory is resident (no fault), but during the 5 ms quiet stretch:
   (1) **I-cache / µop cache** — the ~10s of KB of hot-path instructions
   (decode + book update + strategy + risk + encode) got evicted by the
   poll loop's own code churn, interrupt handlers, and any housekeeping that
   sneaked onto the core. First real message → I-cache misses walking the
   whole path.
   (2) **D-cache** — the book arrays, pools, lookup tables cooled out of
   L1/L2 (poll loop touches almost no data).
   (3) **Branch predictor / BTB** — untrained for the hot path's branches.
   (4) Possibly a **frequency ramp** if the core throttled during idle, and
   a **C-state exit** if the poll loop ever slept (it shouldn't).
   Warming scheme: a timer in the poll loop — `if (now - last_msg >
   500us) run_dry();` where `run_dry()` feeds a **synthetic quote** through
   the *real* decode→book→strategy→risk path with the order stage hard-
   disabled (a tested `dry_run` flag, laid out `[[likely]]` so it doesn't
   tax real events). This keeps instructions in I-cache, data in L2, and the
   predictor trained. Also: ensure nothing else runs on the core during
   quiet periods (move housekeeping off — 19), lock the frequency (19), and
   `processor.max_cstate=1`. After this, the first burst message should run
   at ~steady speed.
   </details>

2. Ek `dry_run` warming pass add karne ke baad, **real** messages ka p50
   200 ns se 260 ns ho gaya (30% worse). Warming ne hot path ko slow kar
   diya. Kaise ho sakta, aur fix?

   <details><summary>Answer</summary>

   The warming code is now **part of the hot path's instruction stream /
   branch behaviour** and is distorting it:
   (1) **An unpredictable `if (dry_run)`** somewhere central — real messages
   have `dry_run == false`, synthetic ones `true`, and they interleave, so
   the predictor can't lock it → a mispredict on real messages too. Fix:
   make the dispatch `[[likely]] if (!dry_run)` at the *order stage only*
   (the one place it matters), or swap a function pointer (`order_fn = real`
   vs `= noop`) so there's no per-message branch at all.
   (2) **The warming call site bloats the hot function** — the synthetic-
   message construction / the timer check got inlined into the hot path,
   pushing real code out of the µop cache. Fix: put the warming logic in a
   separate `[[gnu::cold]] [[gnu::noinline]]` function called from the poll
   loop's idle branch, not from inside `hot_path`.
   (3) **The dry-run pollutes the D-cache with the wrong lines** — if
   `run_dry()` uses a *separate* scratch book, it warms *that* book's lines,
   evicting the real book. Fix: dry-run against the **real** data structures
   (read-mostly, or a copy-back), so it warms exactly what real messages
   touch.
   Net: the warming must be invisible to real messages — separate cold
   function, no per-message branch, warms the real working set.
   </details>

---

## Interview questions

1. Cold start vs cache decay during quiet periods — two different problems.
2. What's cold on the first event after a quiet stretch (I-cache, D-cache, BTB, TLB, C-state).
3. Dry-run warming — how, and the `dry_run` flag's layout requirement.
4. The dry-run trade-offs (CPU, side-effect isolation, representative data).
5. Why moving housekeeping off the trading core reduces the need for warming.

---

## Next
→ [`21-instruction-cache.md`](21-instruction-cache.md)
