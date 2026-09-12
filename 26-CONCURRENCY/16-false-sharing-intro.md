# 16 — False sharing (first look)

## Prerequisites
- `25-OBJECT-MODEL` file 08 (alignment, `alignas(64)`, cache line)
- `05-race-conditions.md`, `07-mutex.md`
- [`examples/08_false_sharing.cpp`](examples/08_false_sharing.cpp)

## Yeh topic abhi kyun
Aapka code **bilkul correct** hai — koi race, koi lock bug — par 10× slow. Reason:
do threads jo **alag** variables likh rahe hain, woh variables **ek hi 64-byte
cache line** pe hain. Har write doosre core ki line-copy ko invalidate karta hai
→ cache line cores ke beech ping-pong → ~100 cycle latency per op. Yeh "false"
sharing hai — logically kuch shared nahi. Folder 32 mein deep; yahan pehla parichay
+ measured demo.

---

## Cache coherence in one paragraph

Memory cache lines mein aati hai (64 bytes). Har core ki apni L1/L2. Jab core A
ek line likhta hai, coherence protocol (MESI) doosre cores ki us line ki copies
ko **Invalid** mark karta hai. Agar core B ko us line ka koi (bhi alag) byte
chahiye, use line dob ara laani padti hai — from A's cache or memory. **Line ki
granularity byte nahi, 64 bytes hai** — isliye "alag variables, same line" =
shared cost.

---

## The demo (`examples/08`)

8 threads, har ek apna `std::atomic<long>` counter, 50M increments each.

```cpp
// PACKED — counters back-to-back -> same cache line(s)
struct Packed { std::atomic<long> c[8]; };

// PADDED — each counter on its own 64-byte line
struct alignas(64) PaddedCounter {
    std::atomic<long> value{0};
    char pad[64 - sizeof(std::atomic<long>)];
};
```

Measured (GCC 15, `-O2`, 8 cores, this box):

```
PACKED  (false sharing): 11258.5 ms
PADDED  (alignas 64)   :  1129.9 ms
speedup from padding: 9.96x
```

**Same logic. Same total work. 10× difference — purely from layout.** In PACKED,
all 8 counters live on ~1 line; every `fetch_add` on any counter invalidates that
line for the other 7 cores. In PADDED, each counter has its own line → each core
keeps its line in Modified state locally → near-local speed.

---

## Where it hides

- **An array of per-thread counters / stats** written by different threads
  (`long stats[NUM_THREADS]`).
- **A struct with two hot mutable fields** touched by different threads (`struct {
  atomic<uint64_t> head; atomic<uint64_t> tail; }` in an SPSC queue — producer
  writes `head`, consumer writes `tail`, same line → the queue crawls).
- **A `std::mutex` right next to the data it protects** — the lock word and the
  first data field share a line; every lock/unlock bounces the data's line too.
- **A `std::shared_mutex`'s reader-count word** — every reader writes it → it
  bounces between all reading cores (file 10).
- **`std::atomic<shared_ptr>` control blocks** for hot refcounted objects.
- **Adjacent elements of a `std::vector`** processed by different threads with a
  small chunk size (chunk boundary lands mid-line).

---

## Fixes

| Fix | How |
|---|---|
| **Pad to a cache line** | `struct alignas(64) X { HotField f; char pad[64 - sizeof(f)]; };` — each on its own line |
| **`alignas(std::hardware_destructive_interference_size)`** | the "portable 64" (file: watch `-Winterference-size` for cross-ABI) |
| **Separate the two hot fields** | put producer-written and consumer-written fields ≥ 64 bytes apart (SPSC: `alignas(64)` on `head` and `tail` blocks) |
| **Don't share — accumulate locally** | each thread sums into a stack local, one write to the shared slot at the end (`examples/03` version (c): 1.9 ms) |
| **Per-thread structures** | `thread_local` or an array indexed by thread with each entry padded |

**Best when possible: don't have per-thread state in a shared array at all.**
Local accumulation + one combine beats even perfect padding (no coherence traffic,
no extra memory).

---

## Andar kya hota hai

- Core A does `lock xadd` on `c[0]`. The line containing `c[0..7]` goes to
  **Modified** in A's L1; every other core's copy → **Invalid**.
- Core B does `lock xadd` on `c[1]` (different variable, same line). B's L1 has
  the line Invalid → **RFO** (read-for-ownership): fetch the line (from A's L1 via
  the interconnect, ~40-80 ns), invalidate A's copy, B goes Modified.
- Now A's next `c[0]` op misses again. The line bounces A→B→A→B... every op is a
  cross-core cache miss instead of an L1 hit (~1 ns). That's the 10×.
- Padding puts `c[0]` and `c[1]` on different lines → A owns its line, B owns its
  line, no invalidation, both hit L1.
- `perf c2c` (cache-to-cache) / `perf stat -e cache-misses` shows the storm.

---

## > **HFT relevance**
> - **SPSC/MPSC lock-free queues** (folder 28, 41) — the #1 place false sharing
>   bites. Producer writes the head index, consumer writes the tail index; if
>   they're in the same line the queue's throughput collapses. Standard fix:
>   `alignas(64)` on the head block and the tail block, and pad the slots.
> - **Per-core stats / counters** — pad every entry to 64 bytes, or (better)
>   accumulate per-thread and merge periodically.
> - **Hot shared structs** — audit every struct written by more than one thread;
>   `alignas(64)` the independently-written fields, or split the struct.
> - **A lock next to its data** — keep the mutex on its own line (or don't use a
>   lock on the hot path at all).
> - Measured 10× (`examples/08`) — this is not a micro-optimization, it's a
>   design constraint. `perf c2c` in the profiling toolkit (folder 32, 35).

---

## Hands-on

```bash
./build.ps1 fast 26-CONCURRENCY/examples/08_false_sharing.cpp
```

PACKED vs PADDED, ~10× on this box. Then:
- Change `kThreads` and re-measure — false sharing scales with core count.
- Make `PaddedCounter` `alignas(32)` (half a line) — partial fix, partial speedup.
- Build an SPSC queue with `head`/`tail` in the same struct with no padding, then
  with `alignas(64)` on each — measure the throughput difference.
- (Linux) `perf c2c record ./fs && perf c2c report` on the PACKED build — see the
  hot line.

---

## ⚠️ Traps

### Trap 1 — assuming "different variable" = "no interference"
Coherence is per **line** (64 B), not per variable. Adjacent hot variables written
by different cores → false sharing.

### Trap 2 — `alignas(64)` on the struct but hot fields still adjacent inside
```cpp
struct alignas(64) Q { atomic<int> head; atomic<int> tail; };   // ⚠️ head & tail same line
struct Q { alignas(64) atomic<int> head; alignas(64) atomic<int> tail; };   // ✅
```
Align the *fields* that are written by different threads, not just the struct.

### Trap 3 — padding but forgetting the array/vector allocation isn't line-aligned
`std::vector<PaddedCounter>` — the vector's buffer must start 64-aligned for the
padding to actually separate lines. `std::vector` respects `alignof(T)`, so
`alignas(64)` on the element type handles it; a raw `malloc` array wouldn't.

### Trap 4 — over-padding everything
64 bytes per counter × thousands = wasted cache/memory. Pad only the genuinely
hot, multi-writer fields. Prefer local accumulation.

### Trap 5 — `std::hardware_destructive_interference_size` in a shared header
`-Winterference-size` — its value can vary with `-mtune`. Use a `constexpr` 64 for
layout, `static_assert` against the std value (file: object-model file 08).

### Trap 6 — measuring at `-O0`
Both versions are dominated by unoptimized overhead → no difference. Measure at
`-O2`+ (`./build.ps1 fast`).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "false sharing is a correctness bug" | Correctness is fine; it's a 2–10× *performance* bug from layout |
| "different variables never interfere" | Same 64-byte line → cross-core invalidation on every write |
| "`alignas(64)` on the struct fixes it" | Only if the multi-writer *fields* are also on separate lines |
| "padding is a premature micro-opt" | On SPSC queues / per-core counters it's a design requirement (measured 10×) |
| "just pad everything to 64" | Wastes cache; pad the hot multi-writer fields, or accumulate locally |
| "atomics avoid false sharing" | Atomics *are* the writes that cause it — the line still bounces |

---

## Exercises

1. **Predict:** two threads, `long a, b;` adjacent. T1 does `for(...) ++a;`, T2
   does `for(...) ++b;` (no atomics, no race — separate variables). Fast or slow,
   why?

   <details><summary>Answer</summary>

   Slow (false sharing) — `a` and `b` are almost certainly on the same cache
   line, so every `++a` invalidates T2's line and every `++b` invalidates T1's.
   The line ping-pongs; each increment is a cross-core miss instead of an L1 hit.
   `alignas(64)` each (or put them in separate padded structs).
   </details>

2. **SPSC queue:** `struct Ring { std::atomic<size_t> head, tail; T buf[N]; };` —
   where's the false sharing and the fix?

   <details><summary>Answer</summary>

   `head` (written by the producer) and `tail` (written by the consumer) share a
   line → every push invalidates the consumer's view of `tail`'s line and vice
   versa. Fix: `struct alignas(64) { std::atomic<size_t> head; char p1[56]; };`
   and a separate `alignas(64)` block for `tail` — so the two indices are on
   different lines. (Also pad `buf` boundaries if slots are tiny.)
   </details>

3. **Explain the 10×:** in `examples/08`, PACKED is 11258 ms and PADDED is 1130 ms
   for the same 400M increments. What's the per-op cost difference?

   <details><summary>Answer</summary>

   PADDED: each core's counter is in its own L1 line, kept Modified locally →
   `fetch_add` ≈ a few ns. PACKED: every `fetch_add` on any of the 8 counters
   invalidates the shared line for the other 7 cores; the next op on another core
   is a read-for-ownership across the interconnect (~40–100 ns). ~10× more per op
   → ~10× total.
   </details>

4. **Best fix:** `examples/03` version (c) (local accumulate + combine) takes
   1.9 ms for the same 16M increments the atomic version does in 430 ms. Why does
   it beat even a perfectly-padded atomic array?

   <details><summary>Answer</summary>

   There's **no shared write in the hot loop at all** — each thread increments a
   stack local (a register), zero coherence traffic, zero atomics. Only one write
   to the shared `partials[i]` at the very end. Perfect padding still pays the
   atomic RMW cost and keeps a line per counter warm; local accumulation pays
   nothing until the combine.
   </details>

5. **Audit:** you have `struct Engine { std::mutex m_; OrderBook book_; Stats
   stats_; };` where `stats_` is bumped by a telemetry thread and `book_` is
   updated under `m_` by the hot thread. What's the layout risk?

   <details><summary>Answer</summary>

   `m_`, the start of `book_`, and `stats_` may share lines. Every telemetry write
   to `stats_` can invalidate the line holding `m_` or `book_`'s hot fields,
   slowing the hot thread's lock/update. Fix: `alignas(64)` `stats_` (and ideally
   keep telemetry counters `thread_local` + merged), and ensure `m_` is on its
   own line away from `book_`'s hot fields — or don't share `book_` under a lock
   at all.
   </details>

---

## Interview questions

1. False sharing kya hai — "false" kyun?
2. Cache coherence (MESI) — ek core ka write doosre cores ki line ka kya karta?
3. `examples/08` ka 10× — per-op cost mein kya farak (L1 hit vs cross-core RFO)?
4. SPSC queue mein false sharing kahan (head/tail), fix?
5. `alignas(64)` struct pe kaafi hai? (fields bhi separate lines pe chahiye)
6. Padding vs "accumulate locally + combine" — kaunsa behtar, kyun?
7. False sharing detect karne ka tool (`perf c2c`)?

---

## Next
→ [`17-concurrency-bugs.md`](17-concurrency-bugs.md)
