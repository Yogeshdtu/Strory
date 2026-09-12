# 06 — MPMC bounded queue (Vyukov)

## Prerequisites
- `04-spsc-ring-buffer.md`, `05-spsc-optimizations.md`
- `27-ATOMICS-MEMORY-MODEL` file 05 (CAS loops)
- [`examples/03_mpmc_queue.cpp`](examples/03_mpmc_queue.cpp)

## Yeh topic abhi kyun
SPSC ring exactly ek producer aur ek consumer maangta. Jab **N producers, M
consumers** ho aur aap ek hi queue chahte ho, toh Dmitry Vyukov ka **bounded MPMC
queue** classic answer hai — fixed size, no allocation, no ABA, lock-free. Har
cell apna chhota turnstile (`sequence`) rakhta hai jis se producers aur consumers
kabhi ek doosre pe step nahi karte.

---

## The idea

```
  buf[N]  cells, N power of two.  Each cell:
     struct Cell { std::atomic<size_t> seq;  T data; };

  Initially:  buf[i].seq = i         (cell i "belongs to" producer position i)

  enqueue_pos_  — shared, next position a producer will try to claim
  dequeue_pos_  — shared, next position a consumer will try to claim
```

Cell ka `seq` ek **state machine** hai:
- `seq == pos`  → cell producer position `pos` ke liye **free & ready to write**.
- `seq == pos + 1` → cell **has data** written by producer `pos`, ready for consumer.
- After a consumer at `pos` reads: `seq = pos + N` → next lap ke producer ke liye ready.

Producers `enqueue_pos_` ko CAS se aage badhate hain; consumers `dequeue_pos_` ko.
CAS fail = koi aur pehle claim kar gaya → re-read → retry.

---

## The code (`examples/03`)

```cpp
bool enqueue(const T& v) {
    Cell* cell;
    std::size_t pos = enq_.load(std::memory_order_relaxed);
    for (;;) {
        cell = &buf_[pos & kMask];
        const std::size_t seq = cell->seq.load(std::memory_order_acquire);
        const std::ptrdiff_t dif = (std::ptrdiff_t)seq - (std::ptrdiff_t)pos;
        if (dif == 0) {                                   // cell free & ours to claim
            if (enq_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                break;                                    // claimed position `pos`
        } else if (dif < 0) {
            return false;                                 // seq behind pos -> queue full
        } else {
            pos = enq_.load(std::memory_order_relaxed);   // someone claimed it; re-read
        }
    }
    cell->data = v;
    cell->seq.store(pos + 1, std::memory_order_release);  // publish -> consumer-visible
    return true;
}

bool dequeue(T& out) {
    Cell* cell;
    std::size_t pos = deq_.load(std::memory_order_relaxed);
    for (;;) {
        cell = &buf_[pos & kMask];
        const std::size_t seq = cell->seq.load(std::memory_order_acquire);
        const std::ptrdiff_t dif = (std::ptrdiff_t)seq - (std::ptrdiff_t)(pos + 1);
        if (dif == 0) {
            if (deq_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                break;
        } else if (dif < 0) {
            return false;                                 // no data at pos -> empty
        } else {
            pos = deq_.load(std::memory_order_relaxed);
        }
    }
    out = cell->data;
    cell->seq.store(pos + kMask + 1, std::memory_order_release);  // free for next lap
    return true;
}
```

### Why it works

- The **shared position CAS** is `relaxed` — it only claims a slot number;
  ordering of the *data* is carried by the per-cell `seq`.
- The producer's `cell->seq.store(pos+1, release)` publishes `cell->data`; the
  consumer's `cell->seq.load(acquire)` that sees `pos+1` gets the data
  (message-passing pattern per cell — folder 27 file 08).
- `dif < 0` (cell's `seq` is *behind* where we want it) means the previous lap's
  consumer/producer hasn't finished → treat as full/empty and return.
- `dif > 0` means another thread already advanced past this position → re-read the
  shared `pos` and try the new cell.

---

## No ABA, no allocation

- `enq_` / `deq_` are **monotonic** — never reused → no A→B→A (folder 27 file 15).
- Fixed `buf_[N]` — zero allocation in `enqueue`/`dequeue`.
- Each cell's `seq` monotonically increases by `N` per lap → also no ABA on the
  per-cell turnstile.

**This is why bounded MPMC is preferred over an unbounded linked queue (`07`) on
the hot path** — no tags, no hazard pointers, no reclamation problem.

---

## Progress: lock-free, not wait-free

A producer whose CAS keeps failing (other producers keep winning) can retry an
unbounded number of times → **lock-free** (some producer always progresses), not
wait-free. Under heavy producer contention this is a retry cost — mitigate by
**sharding** (per-producer SPSC rings feeding one consumer, or K MPMC queues) if
the retry rate is high.

---

## Measured (`examples/03`, this box, `-O2`)

```
3 producers x 2 M  +  3 consumers,  cap 4096,  6 M items
  lock-free (Vyukov) : ~6.5-7.5 M op/s
  mutex + std::deque : ~3.9-4.3 M op/s
  ratio              : ~1.5-1.75x
  checksum           : OK
```

A modest win — the shared `enq_`/`deq_` CAS is itself a contention point, and the
benchmark's own `consumed.fetch_add` per item caps both. Real gain shows more in
the **latency tail** (no lock convoy, no priority inversion) than raw throughput.

---

## > **HFT relevance**
> - **Use it when the topology is genuinely N→M and you want one queue.** Order
>   router fed by several strategy threads; a work queue drained by a pool.
> - **Prefer sharding first.** K independent SPSC rings (one per producer) into one
>   consumer avoid the shared-position CAS entirely and are wait-free per link.
>   Reach for MPMC only when the consumer count varies or work-stealing is needed.
> - **Bounded is a feature** — no allocation, no ABA, no reclamation. An unbounded
>   Michael-Scott queue (`07`) buys flexibility at the cost of a reclamation
>   scheme.
> - **Watch the retry rate** — instrument the CAS-loop iteration count under
>   production load. High → the design is contended; shard or add backoff.
> - **`relaxed` on the position CAS, `release`/`acquire` on the cell `seq`** — the
>   ordering lives on the cell, not the counter.

---

## Hands-on

```bash
./build.ps1 fast 28-LOCK-FREE/examples/03_mpmc_queue.cpp
```

Then:
- Add a per-thread retry counter (increment on each `else` branch of the CAS loop)
  and print total retries / total ops. Bump producers to 8 — watch retries climb.
- Replace the 3-MPMC design with 3 SPSC rings (one per producer) into one consumer
  that round-robins. Compare throughput and retry rate.
- `g++ -O2 -S` the `enqueue` — see `lock cmpxchg` for the position CAS and the
  plain `mov` for the `relaxed` load.

---

## ⚠️ Traps

### Trap 1 — ordering on the position CAS instead of the cell `seq`
The `enq_`/`deq_` CAS is `relaxed` — it only picks a slot number. The data
happens-before edge is the cell's `seq` release/acquire. Swapping these is a
subtle data race.

### Trap 2 — signed/unsigned in the `dif` compare
`seq - pos` must be done as a **signed** difference (`ptrdiff_t`) so "cell is
behind" (`dif < 0` → full/empty) works across wrap. Cast explicitly.

### Trap 3 — treating a failed `enqueue` as fatal
`false` = full (a consumer is behind) — a backpressure signal, same as SPSC.

### Trap 4 — assuming it's wait-free
It's lock-free. A contended producer can retry unboundedly. Shard if the retry
rate is high.

### Trap 5 — `N` not a power of two
`& kMask` breaks. `static_assert`.

### Trap 6 — using it for SPSC
For one producer + one consumer, the SPSC ring (`04`) is simpler, faster, and
wait-free. MPMC's per-cell CAS is overhead you don't need.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "MPMC just needs a mutex around an SPSC ring" | That's a locked queue; Vyukov is lock-free with per-cell turnstiles |
| "the position CAS carries the data ordering" | `relaxed` — the cell `seq` release/acquire carries it |
| "Vyukov MPMC is wait-free" | Lock-free — contended producers retry unboundedly |
| "unbounded is more flexible so use a linked queue" | Bounded avoids allocation + ABA + reclamation; prefer it on the hot path |
| "one MPMC queue beats sharded SPSC" | Often the opposite — sharded SPSC has no shared-position CAS |
| "failed enqueue = error" | = full = backpressure |

---

## Exercises

1. **Cell `seq` values:** for `N = 8`, cell index 3, trace `seq` through: initial,
   after producer at pos 3 writes, after consumer at pos 3 reads, after producer
   at pos 11 writes.

   <details><summary>Answer</summary>

   Initial `seq = 3`. Producer pos 3: writes data, `seq = 4` (pos+1). Consumer pos
   3: reads, `seq = 3 + 8 = 11` (pos + N). Producer pos 11 (same cell, `11 & 7 ==
   3`): sees `seq == 11 == pos` → free → writes, `seq = 12`.
   </details>

2. **`dif < 0`:** when does `enqueue` see the cell's `seq` *behind* its `pos`, and
   what does it mean?

   <details><summary>Answer</summary>

   When the consumer for the *previous* lap at this cell hasn't run yet, so `seq`
   is still `pos - N + 1` (or lower) while the producer wants position `pos`. It
   means the queue is full (the slot is still occupied a lap behind) → return
   `false`.
   </details>

3. **Memory orders:** which op publishes the data, and which reads it? What order
   is the `enq_` CAS?

   <details><summary>Answer</summary>

   `cell->seq.store(pos + 1, release)` publishes `cell->data`; the consumer's
   `cell->seq.load(acquire)` seeing `pos + 1` reads it. The `enq_`
   `compare_exchange_weak` is `relaxed` — it only claims the slot number.
   </details>

4. **Sharding alternative:** you have 4 producers and 1 consumer. Compare Vyukov
   MPMC vs 4 SPSC rings.

   <details><summary>Answer</summary>

   4 SPSC rings: each producer has its own wait-free ring; the consumer
   round-robins / drains all four. No shared-position CAS, no producer-vs-producer
   contention, simpler. Vyukov MPMC: one queue, but every producer CASes the same
   `enq_` → retries under load. With a fixed small producer count, sharded SPSC
   almost always wins. MPMC earns its keep when producers come and go dynamically.
   </details>

5. **Wrap correctness:** why must `seq - pos` be computed as a signed difference?

   <details><summary>Answer</summary>

   `seq` and `pos` are unsigned and both wrap at 2^64. The meaningful quantity is
   how far apart they are (0 = ready, +1 = has data, negative = behind/full). An
   unsigned subtraction of a smaller from a larger wraps to a huge positive number
   instead of a small negative one, breaking the `dif < 0` "full/empty" test.
   Casting both to `ptrdiff_t` gives the correct small signed distance.
   </details>

---

## Interview questions

1. Vyukov MPMC: cell ka `seq` — teen states aur transitions.
2. Shared position CAS `relaxed` kyun — data ordering kaun carry karta?
3. `dif < 0` / `dif > 0` — kya matlab, kya karte ho?
4. Bounded MPMC linked queue se kyun better hot path pe (no ABA / alloc / reclamation)?
5. Lock-free hai ya wait-free — kyun?
6. Sharded SPSC vs single MPMC — kab kaunsa?
7. `seq - pos` signed kyun (wrap)?

---

## Next
→ [`07-michael-scott-queue.md`](07-michael-scott-queue.md)
