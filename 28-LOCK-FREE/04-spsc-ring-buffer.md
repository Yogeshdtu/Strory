# 04 — SPSC ring buffer

## Prerequisites
- `03-cache-line-padding.md`
- `27-ATOMICS-MEMORY-MODEL` file 08 (acquire/release), file 14 (wait-free)
- [`examples/01_spsc_ring_buffer.cpp`](examples/01_spsc_ring_buffer.cpp)

## Yeh topic abhi kyun
SPSC (single-producer, single-consumer) bounded ring buffer **sabse simple aur
sabse important** lock-free structure hai. Koi CAS nahi, koi mutex nahi — sirf do
indices ka release-store / acquire-load hand-off. HFT hot path ke 90% cross-thread
hand-offs isse ban jate hain. Ise achhe se samjho — baaki structures iske upar
build hote hain.

---

## The idea

```
  buf[N]  (N = power of two)
  ┌───┬───┬───┬───┬───┬───┬───┬───┐
  │   │ D │ D │ D │   │   │   │   │      D = published data
  └───┴───┴───┴───┴───┴───┴───┴───┘
        ▲               ▲
       tail            head
   (consumer reads)  (producer writes)

  Producer OWNS head_  — only it writes head_ (release).
  Consumer OWNS tail_  — only it writes tail_ (release).
  Each side ACQUIRE-loads the OTHER index to check space / data.
```

- **Empty:** `head_ == tail_`.
- **Full:** `(head_ + 1) & mask == tail_` — ek slot hamesha khaali chhodte hain
  taaki full aur empty alag dikhein (warna dono `head_==tail_`).
- Usable capacity = `N - 1`.

---

## The code (`examples/01`)

```cpp
template <class T, std::size_t N>
class SpscRing {
    static_assert((N & (N - 1)) == 0, "N power of two");
public:
    bool try_push(const T& v) {                 // PRODUCER thread only
        const std::size_t h = head_.load(std::memory_order_relaxed);   // we own head_
        const std::size_t next = (h + 1) & (N - 1);
        if (next == tail_.load(std::memory_order_acquire)) return false;  // full
        buf_[h] = v;                                                     // (1) write slot
        head_.store(next, std::memory_order_release);                    // (2) publish
        return true;
    }
    bool try_pop(T& out) {                      // CONSUMER thread only
        const std::size_t t = tail_.load(std::memory_order_relaxed);   // we own tail_
        if (t == head_.load(std::memory_order_acquire)) return false;    // empty
        out = buf_[t];                                                  // (3) read slot
        tail_.store((t + 1) & (N - 1), std::memory_order_release);       // (4) free slot
        return true;
    }
private:
    T buf_[N];
    std::atomic<std::size_t> head_{0};
    std::atomic<std::size_t> tail_{0};
};
```

### Why the orders are exactly these

- `head_.store(next, release)` at (2) — everything sequenced-before it (the slot
  write at (1)) **happens-before** any consumer that `acquire`-loads `head_` and
  sees `next`. So (3) is guaranteed to read the data (1) wrote. This is the
  message-passing pattern (folder 27 file 08), verbatim.
- `head_.load(relaxed)` by the producer — the producer is the *only* writer of
  `head_`, so it always sees its own last value; no ordering needed.
- `tail_.load(acquire)` by the producer — pairs with the consumer's
  `tail_.store(release)` at (4), so once the producer sees the freed slot it also
  sees that the consumer is done reading it (matters if `T` is large / you reuse
  storage carefully).
- Symmetric for the consumer.

**Downgrade further?** `tail_.load` could be `relaxed` for *space checking* if you
never reuse a slot's storage before the consumer's `release` is visible — but
`acquire` is free on x86 and correct everywhere, so keep it.

---

## Why it's (effectively) wait-free

Each `try_push` / `try_pop` is a **fixed, bounded** sequence: one relaxed load,
one acquire load, one slot copy, one release store. **No CAS, no retry loop, no
waiting on the other thread to act.** If the ring is full/empty you return `false`
immediately (the caller chooses to spin or move on — that's policy, not the
structure blocking). Bounded steps per op for *both* threads → wait-free in
practice (folder 27 file 14).

---

## No ABA (folder 27 file 15)

`head_` / `tail_` are **monotonically increasing** `std::size_t` counters
(`% N` only for indexing). A counter value is never reused — no A→B→A. The slot
*array* is reused, but access is gated by the monotonic counters, not by a CAS on
a recycled value. **No tags, no hazard pointers needed.** (Using a raw `size_t`
that wraps at 2^64 — at 1 G ops/s that's ~585 years. Fine.)

Some implementations store `head_`/`tail_` already masked (`0..N-1`); then you
*do* need the "one empty slot" trick to tell full from empty, and wrap is at `N`.
The unmasked-counter version above is cleaner — mask only at the array access.

---

## Measured (`examples/01`, this box, `-O2`)

```
capacity 4096, 20 M messages of 32 bytes, 1 producer + 1 consumer
  throughput : ~19 M msg/s
  per message: ~52 ns
  checksum   : OK   (ordering + no loss verified)
```

~52 ns/msg for the **naive** layout (head_/tail_ adjacent, opposite index reloaded
every op). `05` gets this down to ~16–20 ns/msg with cached indices + padding.
(At `-O0` the folder check still compiles it; timing is meaningless there.)

---

## > **HFT relevance**
> - **The default cross-thread hand-off.** NIC/feed-handler thread → decoder →
>   strategy → order-gateway: each stage a pinned thread, each link an SPSC ring
>   of POD records. No locks, no syscalls, bounded latency.
> - **One writer, one reader — enforce it.** SPSC correctness *depends* on exactly
>   one producer and one consumer. Two producers → corruption. If you need
>   multiple producers, shard (one SPSC ring each) or use MPMC (`06`).
> - **Bounded → backpressure is a policy choice.** Ring full = consumer behind.
>   Drop + count (market data you can lose), overwrite oldest (last-value-wins
>   quotes), or spin (strategy that must not lose). The ring never blocks.
> - **POD slots, `memcpy` copy.** A fixed-size record (or a pointer/index into a
>   pool for variable-size). No allocation.
> - **Pad `head_` and `tail_` apart, and cache the opposite index** (`05`) — the
>   difference between ~6 ns and ~24 ns per op on a real box (`07`).

---

## Hands-on

```bash
./build.ps1 fast 28-LOCK-FREE/examples/01_spsc_ring_buffer.cpp
```

Then:
- Make `try_pop`'s `head_.load` `relaxed` — still correct on x86? (It compiles and
  "works", but you've dropped the synchronizes-with edge → the slot read is a data
  race → UB. Keep `acquire`.)
- Add a `size()` method (`head_ - tail_`, all relaxed) and print ring occupancy
  during the run — watch the producer stay ~full because the consumer is the
  bottleneck (or vice-versa).
- `g++ -O2 -S` — confirm `head_.store(release)` is a plain `mov` on x86 (no fence).

---

## ⚠️ Traps

### Trap 1 — more than one producer or consumer
SPSC is *single* each. A second producer racing `head_` → lost/overwritten slots,
no error. Shard or use MPMC.

### Trap 2 — `N` not a power of two
`& (N-1)` masking breaks. Either `static_assert` power-of-two, or use `% N` (a
division — slower, and `-Wpedantic`-clean but avoid on the hot path).

### Trap 3 — no empty slot reserved (masked indices)
If you store `head_`/`tail_` already masked and use all `N` slots, `head_==tail_`
means both full and empty. Reserve one slot, or keep a separate count (another
shared field — worse).

### Trap 4 — `relaxed` on the publishing store
`head_.store(next, relaxed)` → the consumer can see the new `head_` before the
slot write lands → reads garbage. Must be `release`.

### Trap 5 — non-trivial `T`
Assignment/destruction into a reused slot with a non-trivial type = lifetime
bugs. Use POD, or store a pointer/index into a pool.

### Trap 6 — assuming `try_push` failure is an error
Full just means the consumer is behind. The caller decides: drop, overwrite, or
spin. Not an exception.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "SPSC works with a few producers if they're careful" | Exactly one producer, one consumer — else corruption |
| "need a CAS for a lock-free queue" | SPSC needs none — just release-store / acquire-load of indices |
| "SPSC has an ABA problem" | No — monotonic counters, never reused |
| "publishing store can be relaxed" | Must be `release` (it publishes the slot write) |
| "capacity N means N usable slots" | N−1 (one slot reserved to distinguish full/empty), unless you keep a count |
| "ring full = bug" | Consumer is behind; it's a backpressure signal |

---

## Exercises

1. **Full/empty:** with unmasked `size_t` counters, write the full and empty
   conditions.

   <details><summary>Answer</summary>

   Empty: `head_ == tail_`. Full: `head_ - tail_ == N` (i.e. `(head_ + 1) & mask
   == tail_ & mask` for the masked-access view, keeping one slot free). Occupancy
   = `head_ - tail_`.
   </details>

2. **Order check:** which single memory order, if downgraded to `relaxed`, breaks
   correctness — `head_.store` in push, `head_.load` in pop, `tail_.load` in push?

   <details><summary>Answer</summary>

   `head_.store` (release → relaxed) is the fatal one: it publishes the slot
   write; without release the consumer can see the new head with stale slot data →
   data race. `head_.load` in pop must stay `acquire` to pair with it.
   `tail_.load` in push can *often* be relaxed (space check only) but keep
   `acquire` — free on x86, correct everywhere.
   </details>

3. **Two producers:** describe one concrete corruption if a second thread calls
   `try_push`.

   <details><summary>Answer</summary>

   Both read `head_ == h`, both compute `next = h+1`, both write `buf_[h]` (one
   overwrites the other — a lost message), both `head_.store(h+1)`. Net: `head_`
   advanced by 1 but two messages were "pushed", one silently lost. No error
   raised.
   </details>

4. **Backpressure policy:** for (a) a raw market-data feed, (b) best-bid/offer
   updates, (c) orders to send — what do you do when the ring is full?

   <details><summary>Answer</summary>

   (a) drop + increment a "dropped" counter (you'll resync). (b) overwrite oldest
   / keep only the latest (last-value-wins — stale quotes are useless anyway).
   (c) spin (or hard-stop) — you must not silently drop an order; if the gateway
   ring is full something is badly wrong.
   </details>

5. **Wait-free argument:** why is `try_push` wait-free but a Treiber stack `push`
   only lock-free?

   <details><summary>Answer</summary>

   `try_push` is a fixed straight-line sequence — 2 loads, a copy, 1 store — with
   no loop and no dependency on another thread acting. Bounded steps every time.
   Treiber `push` is a CAS loop: under contention it can fail and retry an
   unbounded number of times (some thread always progresses → lock-free, but *this*
   thread isn't bounded → not wait-free).
   </details>

---

## Interview questions

1. SPSC ring: kaunsa thread kaunsa index likhta, kaunsa order?
2. `head_.store` `release` kyun — kya publish hota?
3. Full vs empty kaise distinguish (ek slot reserve / count)?
4. SPSC mein ABA kyun nahi (monotonic counters)?
5. SPSC wait-free kyun, Treiber stack sirf lock-free kyun?
6. `N` power-of-two kyun (mask vs modulo)?
7. Ring full ho to kya — kaunse cases mein drop / overwrite / spin?

---

## Next
→ [`05-spsc-optimizations.md`](05-spsc-optimizations.md)
