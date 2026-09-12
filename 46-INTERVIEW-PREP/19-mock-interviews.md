# 19 — Mock interviews: full scripts with rubrics

## Prerequisites
Lessons `01`–`18`. Do these **out loud**, ideally with a partner reading
the interviewer lines, on a timer. `examples/03_mock_scripts/` has longer
transcripts.

## How to use

Each mock has: interviewer prompts, a **strong** answer sketch, a **weak**
answer (what loses points), and a rubric. Don't read the strong answer
first — attempt it, then compare.

---

## Mock 1 — C++ deep-dive (30 min)

**Interviewer:** "What's the difference between `std::move` and
`std::forward`?"

<details><summary>Strong</summary>
"`std::move` is an unconditional cast to an rvalue reference — it doesn't
move anything, it just enables the move constructor/assignment to be
selected. `std::forward<T>` is a *conditional* cast: in a function
template with a forwarding reference `T&&`, if the caller passed an
lvalue, `T` deduces to `U&` and `forward` keeps it an lvalue; if they
passed an rvalue, `T` is `U` and `forward` casts to rvalue. You use
`forward` to pass arguments through a wrapper preserving their value
category; `move` when you definitely want to steal."
</details>

<details><summary>Weak</summary>
"`move` moves the object and `forward` forwards it." — no mechanism, and
"move moves" is wrong (it's a cast). Follow-up "what does move actually
do at runtime?" exposes it.
</details>

**Interviewer:** "Why does adding `noexcept` to a move constructor
sometimes make `std::vector` operations much faster?"

<details><summary>Strong</summary>
"On reallocation, `vector` needs the strong exception guarantee. If the
element's move constructor is `noexcept`, it moves elements to the new
buffer (cheap for heap-owning types). If the move *can* throw, a throw
midway would leave the vector in a broken state it can't recover, so it
**copies** instead (via `move_if_noexcept`). For a million heap-owning
elements that's a million allocations + deep copies on every growth. So
`noexcept` moves flip reallocation from O(1)-ish per element to O(n) deep
copy."
</details>

**Interviewer:** "Here's a class with a raw `char*` buffer and a
destructor that `delete[]`s it. What's missing?"

<details><summary>Strong</summary>
"Rule of Three/Five. The compiler-generated copy constructor and copy
assignment do a shallow pointer copy → two objects own the same buffer →
double free when both destruct, and a use-after-free if one is destroyed
first. I'd either (a) Rule of Zero: replace `char*` with `std::vector<char>`
or `std::string` and delete nothing, or (b) implement deep-copy copy
ctor + copy assignment (copy-and-swap) + `noexcept` move ctor + move
assignment, or (c) `= delete` copy if it shouldn't be copyable."
</details>

**Rubric:**

| | Weak | OK | Strong |
|---|---|---|---|
| move/forward | "move moves" | correct behaviour | mechanism + reference collapsing + when-to-use |
| noexcept move | doesn't know | "strong guarantee" | move-vs-copy relocation, `move_if_noexcept`, cost |
| Rule of 5 | "add a copy ctor" | names all 5 | Rule of Zero first, then the 5, + the specific bug |
| Communication | one-word answers | explains | explains + gives the fix + the trade-off |

---

## Mock 2 — Latency / systems (30 min)

**Interviewer:** "Your feed handler's p99.9 latency has a spike every
~100 ms. `perf record` shows nothing obviously hot. Where do you look?"

<details><summary>Strong</summary>
"A flat on-CPU profile with a periodic tail spike says the cost is
**off-CPU** — `perf record` only samples running code. I'd run
`offcputime` (bpftrace/bcc) to see where threads block and for how long,
and `perf sched timehist` for scheduler delays and involuntary context
switches. I'd also `perf stat -e page-faults,context-switches,cpu-migrations`
for per-run counts. The ~100 ms period smells like a timer or a batch
boundary — maybe a synchronous log flush, a minor page fault from an
allocation, a `futex` on a shared lock, or the thread being migrated /
preempted. If it's a log, move it to an async POD-enqueue path; if it's
page faults, `mlockall` + prefault; if it's preemption, isolate and pin
the core."
</details>

<details><summary>Weak</summary>
"I'd optimize the hot function." — there is no hot function; and it
ignores that the cost is off-CPU.
</details>

**Interviewer:** "How much latency does a cache miss cost, and a branch
mispredict?"

<details><summary>Strong</summary>
"Order of magnitude: L1 ~1 ns, L2 ~4 ns, L3 ~10–20 ns, a DRAM miss
~60–100 ns. A branch mispredict is a pipeline flush, ~15–20 cycles, so
~5 ns. So a single pointer-chase that misses to DRAM is worth ~15–20
mispredicts. That's why we use flat arrays over linked structures on the
hot path — the addresses are known ahead, the loads pipeline, the
prefetcher engages."
</details>

**Interviewer:** "You have a shared counter incremented by 4 threads.
It's giving wrong totals sometimes. Walk me through fixing it."

<details><summary>Strong</summary>
"That's a data race — `x += 1` is load-modify-store, and interleaved
threads lose updates; it's UB, which is why it's non-deterministic and
sometimes 'works' (e.g. at `-O2` the RMW can be register-promoted). First
I'd confirm with ThreadSanitizer — it'll point at the exact two racing
lines and the variable. Fix options: `std::atomic<long>` with
`fetch_add(1, relaxed)` — relaxed is fine because it's just a counter,
nothing else is published with it; or a mutex if multiple fields must
stay consistent; or, best, per-thread local counters summed at the end —
no shared write, no contention. Then verify: 100+ runs clean, or TSan
clean."
</details>

**Rubric:**

| | Weak | Strong |
|---|---|---|
| Tail spike | "profile the hot function" | recognizes off-CPU; names `offcputime`/`perf sched`; hypotheses + fixes |
| Latency numbers | vague / wrong order | correct orders of magnitude, uses them to justify a design choice |
| Data race | "add a lock" | names it as UB/data-race, TSan to confirm, 3 fixes with trade-offs, verification |

---

## Mock 3 — System design (45 min)

**Interviewer:** "Design an order book that consumes an ITCH-style feed
and exposes best bid/offer and depth. Assume equities, one exchange."

<details><summary>Strong arc</summary>
1. **Clarify:** L2 or L3? One instrument or many (→ shard)? Price range
   bounded (yes, tick grid)? Throughput (~M msg/s)? Latency target for an
   update (~tens of ns)? Determinism for replay (yes)?
2. **Data model:** flat `int64 bid_qty[N]`, `ask_qty[N]` indexed by
   `(price_tick − base)`; cached `best_bid_idx`, `best_ask_idx`; for L3,
   a per-level FIFO of `{order_id, qty}` + a dense `order_id → {side,
   level, slot}` index (`std::vector`, direct-indexed). Prices as integer
   ticks.
3. **Ops:** Add → `arr[idx] += qty`, push FIFO, bump BBO if better.
   Cancel → id index → subtract, mark dead; if touch level emptied,
   re-walk to next non-empty (BBO move, bounded). Trade → consume FIFO
   fronts at the touch.
4. **Hot path cost:** one array write + a short bounded re-walk; no
   allocation (FIFOs pre-reserved or pooled); no locks (single-threaded
   per instrument).
5. **Invariants / failure:** `resolve_cross()` guard; cancel for unknown
   id → ignore + count; qty underflow → clamp + assert + log.
6. **Trade-offs:** flat array (O(1), needs bounded range) vs `std::map`
   (any range, ~25× slower, per-msg alloc). L2 vs L3 (queue position).
7. **Measure:** compare BBO against a brute-force `std::map` reference on
   a replayed session (0 mismatch); `rdtsc` per-op histogram; CI replay.
</details>

<details><summary>Weak</summary>
Jumps straight to `std::map<double, Level>`; doesn't ask about price
range or scale; no failure modes; "it'll be fast enough."
</details>

**Rubric:** see `15`'s table — clarify-first, sound data model (flat
array not map), no hot-path lock, walks one event's cost, handles
failure, defends trade-offs, says how to measure.

---

## Mock 4 — Market-making game + probability (20 min)

**Interviewer:** "Make me a market on the sum of two dice."

<details><summary>Strong</summary>
"Expected value is 7. I'll quote **6.5 at 7.5** — I'll buy at 6.5, sell
at 7.5." *(Interviewer: "I sell you at 6.5.")* "Okay, you sold to me, so
you might think it's worth less than 6.5, and I'm now long. I'll move
down and skew: **5.8 at 6.8**." *(Interviewer: "I sell you again at
5.8.")* "Again a seller — I'll keep coming down: **5.2 at 6.2**, and I'm
getting cautious about size."
</details>

<details><summary>Weak</summary>
"7." — no two-sided quote, no spread, and when the interviewer "trades"
the candidate doesn't update. The game is inventory + information, not a
point estimate.
</details>

**Interviewer:** "Expected number of coin flips to see two heads in a
row?"

<details><summary>Strong</summary>
"Let E be the answer. From the start: flip (cost 1); half the time tails
→ back to start; half the time heads → a 'one H' state. From 'one H':
flip (cost 1); half → done, half → tails → back to start.
E = 1 + ½E + ½(1 + ½E) → E = 1.5 + ¾E → E = **6**. And HT would be 4 —
the asymmetry catches people."
</details>

**Rubric:**

| | Weak | Strong |
|---|---|---|
| Making a market | a point number | two-sided quote with a spread; updates on the trade; manages inventory + information |
| Probability | freezes or guesses | sets up states/equations, solves cleanly, sanity-checks |
| Composure | flustered when wrong | "let me redo that" and re-derives calmly |

---

## Self-scoring

After each mock, rate yourself 1–5 on: **correctness**, **communication
(thinking out loud, structure)**, **handling hints / being wrong**,
**time management**. Anything ≤ 3 → drill that Layer / lesson again and
re-run the mock in a few days.

## Next
→ [`20-resume-and-projects.md`](20-resume-and-projects.md)
