# 05 — SPSC optimizations

## Prerequisites
- `04-spsc-ring-buffer.md`, `03-cache-line-padding.md`
- [`examples/02_spsc_optimized.cpp`](examples/02_spsc_optimized.cpp)

## Yeh topic abhi kyun
Naive SPSC ring (`04`) ~52 ns/msg deta hai is box pe. Teen chhote changes ise
~16–20 ns tak le aate hain — aur ismein ek measurement hai jo "textbook" ke
against jati hai (padding alone ne is machine pe help nahi kiya). CLAUDE.md §12:
build simple → measure → optimize → re-measure → **kya badla explain karo**.

---

## The three optimizations

### 1. Power-of-two mask (already in `04`)
`idx % N` → `idx & (N - 1)`. Modulo ek division hai (~20–40 cycles); mask ek
`and` (1 cycle). `static_assert((N & (N-1)) == 0)`.

### 2. Cache-line padding (`03`)
`head_` (producer writes) aur `tail_` (consumer writes) alag 64-byte lines pe:
```cpp
alignas(64) std::atomic<std::size_t> head_{0};
alignas(64) std::atomic<std::size_t> tail_{0};
char pad_[64];   // trailing — stop the ring array sharing tail_'s line
```

### 3. Cached opposite index (**the real win**)
Producer `tail_` ki ek **private, non-atomic copy** rakhta hai. Har push pe real
`tail_` load karne ke bajaye:
```cpp
bool try_push(const T& v) {
    const std::size_t h = head_.load(std::memory_order_relaxed);
    const std::size_t n = (h + 1) & kMask;
    if (n == cached_tail_) {                                  // MAYBE full?
        cached_tail_ = tail_.load(std::memory_order_acquire); // refresh ONCE
        if (n == cached_tail_) return false;                  // really full
    }
    buf_[h] = v;
    head_.store(n, std::memory_order_release);
    return true;
}
```
Jab tak `cached_tail_` kehta hai "jagah hai", producer consumer ki
constantly-written `tail_` line ko **chhuta hi nahi**. Real `tail_` sirf tab
padhta jab cache full dikhaye — aur consumer ke aage badhne se woh entry saaf ho
jati hai. Consumer symmetrically `cached_head_` rakhta.

Layout: `{head_, cached_tail_}` producer ki line pe, `{tail_, cached_head_}`
consumer ki line pe.

---

## Measured (`examples/02`, this box, `-O2`, 30 M msgs)

| Version | ns/msg | vs V0 |
|---|---|---|
| **V0** naive (adjacent, reload every op) | ~25–27 | 1× |
| **V1** + cache-line padding | ~24–32 | **~1× (noise, kabhi thoda SLOWER)** |
| **V2** + cached opposite index | ~16–20 | **~1.3–1.6×** |

### The surprise: V1 (padding alone) ne help nahi kiya

**Kyun?** V1 mein bhi producer **har push pe** `tail_.load(acquire)` karta hai.
Yaani producer ko consumer ki line har op **padhni** hai (true sharing) —
`head_`/`tail_` ko alag line pe rakhne se bas woh do fields alag hote hain,
cross-core read traffic nahi hatta. Aur is laptop pe producer/consumer shayad ek
hi physical core ke SMT siblings pe schedule ho rahe (shared L1) → miss sasta →
padding ka farq noise mein doob gaya.

**V2 (cached index) ne kaam kiya** kyunki woh cross-line **read** hi khatam kar
deta — steady state mein producer sirf apni line touch karta.

**Sabak (CLAUDE.md Rule 2):** "padding hamesha fast karta" ek myth hai. Padding
**zaroori** hai (taaki `head_`/`tail_`/caches aapas mein na takrayein), par
headline win aata hai jab aap cross-core reads **eliminate** karte ho — aur woh
cached index karta hai. Zyada cores / cross-socket / pinned threads pe padding ka
apna faayda bada hota (`07`: pure false sharing pe ~4×) — is specific SPSC-reload
pattern pe nahi.

---

## Further optimizations (not in the example, but real)

### Batch operations
`try_push_n(const T* arr, size_t k)` — ek baar space check, `k` slots `memcpy`,
ek `head_.store`. Per-message atomic store amortize ho jata. Consumer side
`try_pop_n` similarly. Disruptor ka core trick.

### Consumer reads a snapshot of `head_` once per batch
`const size_t h = head_.load(acquire);` phir loop `while (tail_ != h) { ... }` —
ek acquire load per batch, na ki per message.

### Avoid the copy — expose the slot
`T* prepare_push()` returns `&buf_[h]`; caller writes in place; `commit_push()`
does the `head_.store(release)`. Zero-copy for large `T`.

### `T` sizing
Slot ko cache-line ka divisor/multiple banao (16/32/64 bytes) taaki ek message do
lines cross na kare. 40-byte record → pad to 64.

---

## > **HFT relevance**
> - **Cached indices are standard** — `rigtorp::SPSCQueue`, folly
>   `ProducerConsumerQueue`, the LMAX Disruptor all keep a producer-local copy of
>   the consumer's position and vice versa. It's the single biggest SPSC win.
> - **Batch on both ends** where the protocol allows — one `head_.store(release)`
>   per burst of messages instead of per message; the `mfence`-free release store
>   is cheap but the cache-line handoff per message isn't.
> - **Zero-copy slot API** (`prepare`/`commit`) for anything bigger than ~32
>   bytes — the decoder writes the parsed record straight into the ring slot.
> - **Still pad** `head_`/`tail_` apart (+ trailing) — necessary so the caches and
>   indices don't collide; just know the measured jump comes from the cached
>   index, not the pad, on this class of workload.
> - **Measure on the target box, pinned.** This laptop's "padding ~= noise" result
>   would look different on a 2-socket server with the producer and consumer
>   pinned to different sockets.

---

## Hands-on

```bash
./build.ps1 fast 28-LOCK-FREE/examples/02_spsc_optimized.cpp
```

Run it a few times — note V1 (padding) bounces around V0, while V2 (cached index)
is consistently fastest. Then:
- Add a `V3` that batches: producer fills a local `T tmp[32]`, then one space
  check + `memcpy` + one `head_.store`. Measure.
- Remove the padding from V2 (make `head_`/`tail_` adjacent again) — does V2 still
  beat V0? (Usually yes — the cached index is doing the work.)
- Pin the two threads to different cores (Linux: `pthread_setaffinity_np`) and
  re-run — padding's contribution should grow.

---

## ⚠️ Traps

### Trap 1 — cached index never refreshed
If `try_push` returns false without refreshing `cached_tail_`, and the consumer
*has* advanced, you deadlock the producer (it thinks it's permanently full).
Refresh on the "maybe full" branch.

### Trap 2 — cached index used for correctness, not just fast-path
The cache is a hint. The *real* full/empty decision must use a fresh
`acquire`-load of the real atomic. `try_push` above does: cache says full →
reload → decide.

### Trap 3 — expecting padding alone to speed up an SPSC ring
It won't much if you still `acquire`-load the opposite index every op. Cache it.

### Trap 4 — modulo instead of mask on the hot path
`% N` is a division. `& (N-1)` with a power-of-two `N`.

### Trap 5 — batching without a fence-free release still per-message
If you `head_.store(release)` per message inside the batch loop, you didn't
amortize anything. One store after the whole batch.

### Trap 6 — slot `T` straddling cache lines
A 40-byte record at an 8-byte offset spans two lines → 2 coherence transfers per
message. Pad `T` to a line-divisor.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "cache-line padding is the big SPSC optimization" | The cached opposite index is; padding is necessary but small here |
| "the cached index replaces the real check" | It's a fast-path hint; the real full/empty check reloads the atomic |
| "`% N` is fine, compilers optimize it" | Only if `N` is a compile-time power of two → then use `& (N-1)` explicitly anyway |
| "batching means calling push in a loop" | One space-check + `memcpy` + one release store per batch |
| "V1 padding was slower → padding is bad" | Padding is still needed; it just wasn't the bottleneck for this reload-heavy pattern |
| "these numbers are universal" | Measure pinned, on the target CPU — SMT/socket topology changes everything |

---

## Exercises

1. **Why cached index helps:** in steady state (ring half full), how many times
   does the producer touch the consumer's cache line per `try_push` — naive vs
   cached?

   <details><summary>Answer</summary>

   Naive: once per push (`tail_.load(acquire)` every call) — a coherence miss each
   time the consumer has advanced. Cached: ~zero — `cached_tail_` is producer-
   local; the real `tail_` is only loaded when `cached_tail_` says full, which in
   steady state (half full) is never.
   </details>

2. **Deadlock risk:** what happens if `try_push` returns `false` on `n ==
   cached_tail_` *without* reloading the real `tail_`?

   <details><summary>Answer</summary>

   `cached_tail_` is stale. If the consumer has since drained slots, the real
   `tail_` moved but `cached_tail_` didn't → the producer permanently believes the
   ring is full and never pushes again → livelock/deadlock. Must reload on the
   "maybe full" branch.
   </details>

3. **Batch math:** 64-byte messages, cache-line handoff ~20 ns, release store ~1
   ns. Per-message cost unbatched vs batched-32 (ignoring the copy)?

   <details><summary>Answer</summary>

   Unbatched: ~20 ns handoff + ~1 ns store ≈ 21 ns/msg. Batched-32: one handoff +
   one store per 32 messages ≈ (20 + 1)/32 ≈ 0.66 ns/msg of *sync* overhead, plus
   the `memcpy` (which is bandwidth-bound and cheap). Batching amortizes the
   per-message sync almost entirely.
   </details>

4. **V1 result:** the padding-only version wasn't faster than naive on this box.
   Give the mechanism, and name a machine where padding *would* clearly help this
   ring.

   <details><summary>Answer</summary>

   The producer still `acquire`-loads the constantly-written `tail_` every push,
   so it keeps pulling the consumer's line regardless of whether `head_` and
   `tail_` are on separate lines — padding doesn't remove that traffic. A 2-socket
   server with producer and consumer pinned to different sockets: the coherence
   miss becomes a cross-socket transfer (~100+ ns), and separating the lines at
   least stops `head_` writes from also invalidating `tail_`'s line — but the
   cached index is still the bigger fix.
   </details>

5. **Zero-copy API:** sketch `prepare_push()` / `commit_push()` and say when
   they're worth it.

   <details><summary>Answer</summary>

   ```cpp
   T* prepare_push() {                    // returns slot, or nullptr if full
       const auto h = head_.load(relaxed);
       if (((h + 1) & kMask) == cached_tail_) { refresh; if still full return nullptr; }
       return &buf_[h];
   }
   void commit_push() {                   // after the caller wrote *slot
       head_.store((head_.load(relaxed) + 1) & kMask, release);
   }
   ```
   Worth it when `T` is bigger than a few words and the producer builds the record
   field-by-field anyway (e.g. a decoder parsing straight into the slot) — you
   save one full-record copy per message.
   </details>

---

## Interview questions

1. SPSC ki teen standard optimizations — mask, padding, cached index.
2. Cached opposite index kaise kaam karta — fast path vs real check?
3. `examples/02` mein padding-alone ne kyun help nahi kiya (is box pe)?
4. Batching per-message kaunsi cost amortize karta?
5. Zero-copy `prepare`/`commit` API — kab worth?
6. Cached index refresh na karne pe kya bug (producer deadlock)?
7. Slot `T` ko cache-line-divisor kyun rakhna?

---

## Next
→ [`06-mpmc-queue.md`](06-mpmc-queue.md)
