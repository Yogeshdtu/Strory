# 08 — Project 7: SPSC ring buffer + benchmark

## Prerequisites
- `07-project-object-pool.md`
- `28-LOCK-FREE` (SPSC ring built + measured), `41-HFT-CONCURRENCY`
  (`spsc_queue.hpp` packaged, benchmarked)

## Yeh topic abhi kyun

Pipeline ka pehla thread boundary: ek "wire reader" thread NIC se bytes
padhta, ek "engine" thread unhe process karta. Beech mein ek lock-free
SPSC queue. Folder 28/41 ne yeh banaya + measure kiya; yahan hum **41 ka
`spsc_queue.hpp` `#include`** karke capstone ke MdMessage hand-off ke liye
use karte — copy-paste nahi.

## `SpscQueue<T, Capacity>` (folder 41, reused)

```cpp
#include "../../41-HFT-CONCURRENCY/examples/spsc_queue.hpp"
```

Design (41/02-04):
- **Single-writer principle**: exactly one thread calls `try_push`, exactly
  one (different) thread calls `try_pop`. Two pushers = data race = UB.
  The class does *not* defend against that — defending is the cost an SPSC
  queue exists to avoid.
- `alignas(64)` on `head_` and `tail_` + padding → no false sharing (43/08).
- **Cached-opposite-index trick**: the producer keeps a private
  `cached_tail_`, only reloads the real (contended) `tail_` when the cache
  says "full". Most pushes touch zero shared cache lines.
- Power-of-two `Capacity` → `& kMask` instead of `%` (43/10).
- `try_push` returns `false` on full → caller's policy: spin, drop, or
  backpressure.

## Measure (`07_spsc_queue.cpp`)

Two passes (41/02 idiom):
- **Blast**: push as fast as possible → throughput.
- **Paced**: push every ~2 µs (`kPaceTicks`) → queue stays shallow →
  measures *pure hand-off latency*, not queue-depth latency.

```
throughput (blast, 8M msgs) : ~6 M msg/s
hand-off latency (paced)    : p50 ~350   p99 ~640   p99.9 ~16000   max ~190000  ns
consumer saw 8.4M msgs, in-order: yes
```

- **Correctness**: the consumer tracks `expect = last_seq + 1` and asserts
  every popped message's `seq` matches — **no loss, no duplication, no
  reorder** across the thread boundary. 8.4M messages, all in order.
- The p99.9 / max tail (~16 µs / ~190 µs) is OS-scheduler jitter on this
  unpinned box — the consumer thread getting descheduled (41/13). Pin both
  threads to isolated cores and the tail collapses.

## HFT relevance

The canonical HFT threading posture (41/01): **one pinned thread per
stage**, connected by lock-free SPSC queues. No mutexes on the hot path.
The queue is the *only* synchronization, and it's wait-free-ish for the
common case (no CAS, just an atomic load/store of an index).

- Wire reader → parser: SPSC of raw frames.
- Parser → strategy: SPSC of parsed events.
- Strategy → order gateway: SPSC of order requests.

Backpressure policy matters: for market data you often **drop + resync**
(conflation, 38/15) rather than block the reader; for orders you never
drop (block or reject).

## ⚠️ Traps

### Trap 1 — two producers
Someone adds a second thread that also calls `try_push` "just for this one
message". Data race, UB, corrupted queue. If you need it, you need an MPMC
queue (28/03) or the Disruptor's multi-claim (41/03) — a different, slower
structure.

### Trap 2 — `try_push` returns false, caller ignores it
Message silently dropped. For orders that's a lost order. Handle the
`false` — the whole point of the return value.

### Trap 3 — huge `T` by value
`buf_[h] = v` copies `sizeof(T)` bytes into the ring. A 4 KB `T` → the ring
is huge and every push is a big memcpy. Put big payloads in a pool, push a
small handle/pointer.

### Trap 4 — measuring latency in the blast pass
Blast fills the queue → measured latency = queue-depth latency, not
hand-off. Pace the producer so the queue stays near-empty (41/02).

### Trap 5 — unpinned threads, then blaming the queue
p99 of 30 ms on a paced pipeline (41/13) was *scheduler jitter*, not the
queue. Pin + isolate before trusting tail numbers.

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Lock-free = wait-free = no stalls | SPSC push/pop are wait-free-ish; but the *thread* can still be descheduled |
| One SPSC queue handles many producers | Exactly one producer, exactly one consumer — else UB |
| `try_push` failing is an error | It's backpressure — caller's policy (spin/drop/reject) |
| Bigger ring = better | Bigger ring = more cache footprint; size to burst, not to "big" |

## Exercises

1. `07_spsc_queue.cpp` mein `kCap` 4096 se 16 kar do. Throughput pe asar?
   <details><summary>Answer</summary>
   Blast pass mein producer aksar "full" hit karega → `while(!try_push)`
   spin → producer aur consumer ko lock-step chalna padega → throughput
   gir sakta (context-switch-ish stalls). Bada enough ring producer/consumer
   ko decouple karta. Par bahut bada = wasted cache. Burst size + margin.
   </details>

2. `Slot` mein `MdMessage m` ki jagah `MdMessage* p` (pooled) push karo.
   Kya badalta?
   <details><summary>Answer</summary>
   Ring ab pointer-sized entries → chhota footprint, faster push (8 bytes
   vs 38). Par ab lifetime manage karna padta — consumer ke process karne
   tak `*p` valid rehna chahiye. Pool + generation (project 6) us lifetime
   ko safe banata. Trade-off: copy cost vs lifetime complexity.
   </details>

3. Producer ka backpressure `while(!try_push){}` se `if(!try_push) ++dropped;`
   kar do. Kab yeh sahi, kab galat?
   <details><summary>Answer</summary>
   Market data: sahi-ish — drop + resync (conflation). Losing a tick is
   recoverable if you resync the book. Orders: **galat** — a dropped order
   is a lost trade / a stuck position. Never drop orders; block or
   explicitly reject upstream.
   </details>

## Interview questions

1. Single-writer principle — kya hai, aur SPSC queue kyun ise assume karta?
2. Cached-opposite-index trick kya bachata (cache-coherence terms)?
3. Blast vs paced benchmark — kaunsa hand-off latency measure karta aur
   kyun?
4. Market data vs orders — SPSC full hone pe backpressure policy alag kyun?

## Next
→ [`09-project-strategy-simulator.md`](09-project-strategy-simulator.md)
