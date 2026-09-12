# 11 — False sharing elimination

## Prerequisites
- **`27-ATOMICS-MEMORY-MODEL`** (MESI, cache-line coherence),
  **`32-CACHE-MEMORY-PERFORMANCE/07`** + **`28-LOCK-FREE/07`** (measured)
- `26-CONCURRENCY/08` (`08_false_sharing.cpp`)

## Yeh topic abhi kyun
Do threads jo **alag** variables likhte hain, par woh variables ek hi
**cache line** pe hain → line har write pe cores ke beech ping-pong karti
(MESI invalidate) → har "independent" write ~100+ cycles. Aur yeh **jittery**
hai (scheduling pe depend), sirf slow nahi. Yeh lesson: pehchaano aur
eliminate karo.

---

## Kya hota hai (recap)

Cache coherence line granularity pe kaam karti (64 B). Jab core A ek line
likhta, us line ki copies **doosre cores mein invalidate** hoti hain. Agar
core B usi line pe ek **alag** byte likh raha, B ko line dob>ara fetch karni
padti (Modified state doosre core se) → ~100-300 cycles, har baar.

```
  struct { std::atomic<uint64_t> producer_idx;    // core A writes
           std::atomic<uint64_t> consumer_idx; };  // core B writes
  // both in the same 64-byte line -> the line bounces A<->B every op
```

Neither thread shares data logically — they just happen to be **spatially**
co-located. "False" sharing.

---

## Measured

| Source | Effect |
|---|---|
| `28-LOCK-FREE/07` — SPSC index pair, adjacent vs `alignas(64)` | **~3.5–4×** (adjacent ~24 ns/op vs padded ~6 ns/op) |
| `26-CONCURRENCY/08` — 4 threads incrementing packed vs padded counters | **~10×** (packed ~11258 ms vs `alignas(64)` ~1130 ms) |
| `32-CACHE-MEMORY-PERFORMANCE/04` — 2 threads, packed vs padded | **~6× to ~44×, run-to-run** — *the variance is the lesson* |
| `36/09` ring buffer — `alignas(CL)` on `head_`/`tail_`/`buf_` | no false sharing between producer & consumer |

> The `32/04` finding — **6× to 44×, swinging run-to-run** — is the key
> point for this folder: false sharing isn't just a fixed slowdown, it's a
> **jitter source** whose magnitude depends on how the scheduler places the
> threads. Padded version: stable ~0.2 ns/inc. Packed: unpredictable.

---

## Fixes

### 1. Pad / align to a cache line
```cpp
struct alignas(64) PerThread {
    std::atomic<uint64_t> counter{0};
    char pad[64 - sizeof(std::atomic<uint64_t>)];   // fill the rest of the line
};
PerThread counters[NUM_THREADS];                     // each on its own line
```
Or `std::hardware_destructive_interference_size` (C++17) instead of literal
64 — ⚠️ but it's an ABI-fragile constant (can differ between TUs compiled
with different flags); many shops **hardcode 64 (or 128 for Intel adjacent-
line prefetch)** with a comment.

### 2. Per-thread data, combine later
Don't share a counter at all. Each thread increments its **own** (on its own
line / in its own struct), and a reader sums them periodically:
```cpp
thread_local uint64_t local_count = 0;             // no sharing
// reader: sum the registered thread-locals
```
"Accumulate locally, combine occasionally" beats even perfect padding —
zero coherence traffic (26/03 measured: local+combine ~1.9 ms vs mutex
~1248 ms vs atomic ~430 ms for 16M increments).

### 3. Separate hot-written fields onto different lines
In a shared struct, put fields written by different threads at least 64
bytes apart (with padding between), and group read-mostly fields together.
The SPSC ring (09) does this: `head_` (producer-written), `tail_`
(consumer-written), `buf_` — each `alignas(64)`.

### 4. Read-mostly → fine to share
False sharing only bites on **writes** (that's what invalidates). Data that
all threads only *read* can sit on one line happily (constructive sharing).

---

## Detecting it

- **`perf c2c`** (folder 35/11) — *the* tool. Records cache-line contention,
  shows the offending line, the byte offsets, the two code locations, and
  **HITM** (hit-modified-in-another-core) counts.
- **`perf stat -e ...HITM...` / `mem_load_l3_hit_retired.xsnp_hitm`** — a
  quick "is there cross-core contention" check.
- **Symptom**: multithreaded code that doesn't scale (or gets *slower*) with
  more threads, low IPC under threading, high run-to-run variance.
- **Code review**: any `struct` / array where different threads write
  different elements/fields and the stride is < 64 bytes.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — padding one side only
`alignas(64)` on `head_` but `tail_` right after it in the same struct →
they may still share `head_`'s line's second half... actually `alignas(64)`
on the *member* forces `tail_` to the next line too. But `alignas` on a
struct only aligns the *start* — you also need trailing `pad` so the next
array element / field doesn't share the tail.

### Trap 2 — `hardware_destructive_interference_size` across TUs
It can differ between translation units (an ODR/ABI hazard). Hardcode 64
(128 for Intel L2 adjacent-line prefetch) with a comment, or ensure uniform
build flags.

### Trap 3 — padding a `std::atomic` but the array element is smaller
`std::atomic<int> a[8]` — 8 ints in ~2 lines, threads hammering `a[0]` and
`a[1]` false-share. Wrap each in an `alignas(64)` struct.

### Trap 4 — false sharing via the allocator
Two per-thread objects `new`-d back-to-back can land on the same line.
Allocate per-thread state from per-thread pools, or `alignas(64)`.

### Trap 5 — "it's just 2× slower"
On `32/04` it was **6–44×, swinging**. It's a jitter source. Fix it, don't
tolerate it.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "different variables can't conflict" | same cache line → they do (on writes) |
| "false sharing = a fixed slowdown" | 6–44× run-to-run — a jitter source |
| "read sharing is bad too" | only writes invalidate; read-only sharing is fine |
| "`hardware_destructive_interference_size` is the safe choice" | ABI-fragile; hardcode 64/128 |
| "align the struct start" | + trailing pad so the next element doesn't share |

---

## Exercises

1. Ek thread pool ke worker threads har ek `stats_[worker_id].jobs_done++`
   karte hain, jahan `struct Stats { uint64_t jobs_done; uint64_t bytes; };`
   aur `Stats stats_[8];`. Yeh scale nahi karta. Fix, aur measure kaise
   confirm karo.

   <details><summary>Answer</summary>

   `Stats` is 16 bytes → 4 `Stats` per 64-byte line. Workers 0-3 all write to
   the same line, 4-7 to the next. Every `jobs_done++` by worker 0
   invalidates worker 1/2/3's copy of that line → the line ping-pongs, each
   increment ~100+ cycles instead of ~1. Fix: `struct alignas(64) Stats {
   uint64_t jobs_done; uint64_t bytes; char pad[48]; };` so each `Stats` is
   its own line. Or better: `thread_local Stats my_stats;` (each on its own
   thread's stack / TLS, zero sharing) + a registry the reader sums.
   Confirm: (1) `perf c2c record`/`report` — before, the `stats_` array
   shows up as a hot HITM line with offsets 0/16/32/48; after, gone. (2)
   `perf stat -e ...hitm...` count drops ~to zero. (3) IPC under 8 threads
   recovers to near the single-thread value. (4) run-to-run variance of the
   pool's throughput collapses.
   </details>

2. Tumne `SpscRing` ke `head_` aur `tail_` ko `alignas(64)` kiya. `perf c2c`
   abhi bhi producer aur consumer ke beech ek hot line dikhata — `buf_`
   ke pehle element pe. Kya ho raha, aur fix?

   <details><summary>Answer</summary>

   The producer writes `buf_[h & mask]` and the consumer reads
   `buf_[t & mask]`. When the ring is nearly empty (`h` and `t` close),
   the producer's write to `buf_[h]` and the consumer's read of `buf_[t]`
   can hit the **same cache line** if `buf_` elements are small and `h`, `t`
   differ by less than `line_size / sizeof(T)`. E.g. `T = uint64_t` (8 B) →
   8 elements per line → if producer is at index 5 and consumer at index 2,
   they share a line → the producer's write invalidates the consumer's line
   → contention on `buf_`, not just the indices.
   Fixes: (1) **Batch** — the consumer drains several elements at once so it
   spends most of its time far from the producer's write position; the
   producer likewise. Cached opposite-index (09) helps here — the producer
   only re-reads `tail_` when its cache says full, so mostly it's writing
   well ahead of where the consumer reads. (2) Make each slot a full cache
   line (`struct alignas(64) Slot { T value; };`) — wastes memory for small
   `T` but eliminates slot-level false sharing; worth it for a small ring
   with a tiny `T` under heavy contention. (3) Size the ring generously so
   `head_` and `tail_` are rarely within a line of each other. Usually
   batching + a big-enough ring + cached indices is enough; per-slot
   line padding only for pathological small-`T` heavy-contention cases.
   </details>

---

## Interview questions

1. False sharing — MESI mechanism, why "false", why only on writes.
2. Measured magnitudes (3.5–44×) — and why the run-to-run variance matters here.
3. Three fixes: pad/align, per-thread + combine, field separation.
4. `hardware_destructive_interference_size` — the ABI hazard; what to do instead.
5. `perf c2c` — what it reports (line, offsets, HITM, code locations).
6. Ring buffer — how `alignas(64)` on `head_`/`tail_`/`buf_` prevents it, and the residual slot-level case.

---

## Next
→ [`12-branch-free-programming.md`](12-branch-free-programming.md)
