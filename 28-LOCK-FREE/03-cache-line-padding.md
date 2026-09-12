# 03 — Cache-line padding & false sharing

## Prerequisites
- `02-lock-free-rules.md`, `26-CONCURRENCY` file 16 (false sharing intro)
- `27-ATOMICS-MEMORY-MODEL` file 12 (MESI / coherence)
- [`examples/07_false_sharing_fix.cpp`](examples/07_false_sharing_fix.cpp)

## Yeh topic abhi kyun
Lock-free structure ka control block (`head`, `tail`, cached indices, flags) do+
threads roz likhte hain. Agar do aise fields **ek hi 64-byte cache line** pe hain,
to ek field ki write doosre field ki line ko doosre core par **invalidate** kar
deti — bhale hi threads logically alag variables chhu rahe hon. Yeh **false
sharing** hai aur lock-free performance ka #1 silent killer.

---

## Coherence recap (folder 27 file 12)

Cache coherence **cache line** (64 bytes typical) ke granularity pe kaam karta,
variable ke nahi:

```
Core A: X.store(...)   -> A needs the line holding X in EXCLUSIVE/MODIFIED
                       -> every other core's copy of that line -> INVALID
Core B: Y.load()       -> Y is in the SAME line -> B's copy is INVALID
                       -> B must re-fetch the whole line (coherence miss, ~tens-100+ cy)
```

X aur Y ka koi rishta nahi — bas same line. Har baar A `X` likhe, B ko `Y` ke
liye miss. Line cores ke beech **ping-pong**.

---

## Measured — pure false sharing (`examples/07`)

Do threads, ek sirf `head.fetch_add`, doosra sirf `tail.fetch_add` (ek doosre ko
padhte bhi nahi → 100% false sharing):

| Layout | ns/op | |
|---|---|---|
| `head`, `tail` adjacent (same line) | **~24 ns/op** | |
| `head`, `tail` `alignas(64)` (own lines) | **~6 ns/op** | |
| speedup | **~3.5–4×** | |

Sirf memory layout badla. Logic identical.

---

## The fix

```cpp
struct alignas(64) PaddedIndex {
    std::atomic<std::uint64_t> value{0};
    char pad[64 - sizeof(std::atomic<std::uint64_t>)];   // fill the rest of the line
};
```

Ya field-level:
```cpp
struct RingControl {
    alignas(64) std::atomic<std::size_t> head{0};   // producer writes
    alignas(64) std::atomic<std::size_t> tail{0};   // consumer writes
    char tail_pad_[64];                              // stop the NEXT object sharing tail's line
};
```

`alignas(64)` sirf field ko line-start pe rakhta — trailing pad zaroori hai warna
struct ke baad ka data `tail` ki line share kar sakta.

---

## `std::hardware_destructive_interference_size`

C++17: "do objects ko is se zyada door rakho to woh false-share nahi karenge."
Is box pe = **64**.

```cpp
#include <new>
#ifdef __cpp_lib_hardware_interference_size
  constexpr std::size_t kLine = std::hardware_destructive_interference_size;
#else
  constexpr std::size_t kLine = 64;
#endif
```

**Gotcha:** GCC `-Winterference-size` warn karta — iski value compiler versions ke
beech badal sakti, to agar aap ise ABI (struct layout jo alag TUs/binaries share
karein) mein use karein to mismatch ho sakta. **Practical:** `64` (ya `128` on
some Intel — adjacent-line prefetch) hardcode karo + `static_assert`. `examples/07`
`__cpp_lib_hardware_interference_size` guard karke use karta hai.

`std::hardware_constructive_interference_size` uska ulta — "itne ke andar rakho to
ek saath cache mein aayenge" (jaan-boojh kar co-locate karne ke liye).

---

## Kahan padding chahiye (aur kahan nahi)

| Chahiye | Nahi chahiye |
|---|---|
| SPSC `head` (producer) vs `tail` (consumer) — alag lines | Do fields jo **hamesha ek hi thread** touch kare |
| Per-thread counters ek array mein (folder 26 `08` — ~10×) | Read-only / immutable data (share karo freely) |
| Lock word vs data woh protect karta | Fields jo saath-saath ek hi op mein padhe jaate (constructive sharing) |
| `std::atomic` flags jo alag threads set karein | Cold fields |

**Over-padding ka cost:** zyada memory, kam cache-density, zyada TLB/prefetch
pressure. Har mutable-shared-hot field ko blindly pad mat karo — profile.

---

## The subtlety: padding alone kabhi kaafi nahi (`examples/02`)

`examples/02` SPSC ladder mein: `head`/`tail` ko alag line pe daalna (V0→V1) is box
pe **barely help kiya / kabhi thoda slow** — kyunki producer **har push pe**
`tail.load(acquire)` karta hai. Yaani producer ko consumer ki constantly-written
line har op **padhni** hi hai (true sharing) — padding sirf `head` aur `tail` ko
alag karta, cross-core read traffic nahi hatata.

Asli fix (V2): **cached index** — producer `tail` ki private copy rakhta aur real
`tail` tabhi padhta jab cache "full" kahe. Ab steady-state mein producer consumer
ki line ko **chhuta hi nahi** → padding + caching saath = win.

**Sabak:** padding zaroori hai (taaki `head`/`tail`/caches aapas mein na takrayein),
par bada win aata cross-line **reads khatam** karne se (`05`).

---

## > **HFT relevance**
> - **SPSC/MPSC control block layout is deliberate**: `{head, cached_tail}` on the
>   producer's line, `{tail, cached_head}` on the consumer's line, trailing pad so
>   neither shares with the ring array or the next object. This + cached indices is
>   the difference between ~6 ns and ~24 ns per op (`07`).
> - **Per-connection / per-symbol state** in an array → pad each entry to a cache
>   line (or a small multiple) so worker threads updating different symbols don't
>   ping-pong (folder 26 `08`: ~10× on 8 threads).
> - **Hardcode 64 (or 128), `static_assert`** — don't put
>   `hardware_destructive_interference_size` in a struct shared across
>   translation units / binaries (ABI drift).
> - **Profile before padding everything** — `perf c2c` (Linux) points at the exact
>   false-shared lines. Blind padding wastes cache.

---

## Hands-on

```bash
./build.ps1 fast 28-LOCK-FREE/examples/07_false_sharing_fix.cpp   # ~3.5-4x
./build.ps1 fast 28-LOCK-FREE/examples/02_spsc_optimized.cpp      # padding alone ~= noise; cached idx wins
```

Then:
- `07`: change `alignas(kLine)` to `alignas(8)` on `Padded` → it collapses back to
  the adjacent time.
- `02`: compare V1 (padding, still reloads tail) vs V2 (cached tail). The cached
  index is what removes the cross-core read.
- Linux: `perf c2c record ./fsfix && perf c2c report` — see the HITM (hit-modified)
  events on the shared line vanish after padding.

---

## ⚠️ Traps

### Trap 1 — `alignas` on the field but no trailing pad
The struct's tail (or the next object) shares the last hot field's line. Add
`char pad_[64];` after the last padded field.

### Trap 2 — `hardware_destructive_interference_size` in a cross-TU struct
Value can differ between compiler versions → ABI mismatch. Hardcode 64/128 +
`static_assert`.

### Trap 3 — padding fields that one thread owns
No false sharing if only one thread writes them. Padding just wastes cache.

### Trap 4 — assuming padding fixes an SPSC ring by itself
If the producer still `acquire`-loads `tail` every push, it still pulls the
consumer's line. Cache the opposite index (`05`, `examples/02` V2).

### Trap 5 — ignoring adjacent-line prefetch
Some Intel CPUs prefetch the *pair* of lines → effective false-sharing unit is
128 bytes. Pad to 128 if `perf c2c` still shows HITM at 64-byte spacing.

### Trap 6 — padding the whole struct instead of the hot fields
`alignas(64) struct Ring` aligns the start; the hot fields inside can still share
a line with each other. Pad *per field*.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "different variables can't interfere" | Same 64-byte line → coherence ping-pong (false sharing) |
| "`alignas(64)` on the struct is enough" | Aligns the start; pad individual hot fields + a trailing pad |
| "always use `hardware_destructive_interference_size`" | Fine locally; hardcode 64/128 for cross-TU layouts (ABI drift) |
| "pad every shared field" | Only mutable fields written by *different* threads; profile |
| "padding fixes any SPSC perf problem" | Only false sharing; the bigger win is caching the opposite index |
| "false-sharing unit is always 64 B" | Adjacent-line prefetch can make it 128 B on some Intel |

---

## Exercises

1. **Predict:** two threads, each `fetch_add`s its own `std::atomic<long>`; the two
   atomics are 8 bytes apart vs 64 bytes apart. Which is faster and roughly by how
   much (this box)?

   <details><summary>Answer</summary>

   64-byte spacing is ~3.5–4× faster (`examples/07`: ~24 ns/op adjacent vs ~6
   ns/op padded). At 8 bytes apart they share a line → every `fetch_add` on one
   invalidates the other core's copy → coherence miss each op.
   </details>

2. **Trailing pad:** why does `struct { alignas(64) atomic<size_t> head, tail; }`
   still risk false sharing without a `char pad_[64]` after `tail`?

   <details><summary>Answer</summary>

   `tail` starts a new line but only occupies 8 bytes of it. Whatever is placed
   right after the struct (the ring array, another control field, the next array
   element) lands in the remaining 56 bytes of `tail`'s line and false-shares with
   `tail`. The trailing pad reserves the rest of the line.
   </details>

3. **ABI trap:** you put `alignas(std::hardware_destructive_interference_size)` on
   a struct in a header shared by two `.so`s built with different GCC versions.
   What can go wrong?

   <details><summary>Answer</summary>

   The two builds can compute different values (it's implementation-defined and
   version-dependent) → the struct has different size/layout in each `.so` → any
   object passed across the boundary is misinterpreted (ODR/ABI violation). Use a
   fixed literal (64 or 128) and `static_assert` it's ≥ the real value where you
   can.
   </details>

4. **Padding didn't help:** you pad `head` and `tail` in your SPSC ring and see no
   improvement. Most likely cause and fix?

   <details><summary>Answer</summary>

   The producer still does `tail.load(acquire)` on every `push` (and the consumer
   `head.load` every `pop`), so each side keeps pulling the other's
   constantly-written line regardless of padding. Fix: cache the opposite index —
   keep a private `cached_tail` / `cached_head` and only reload the real atomic
   when the cache says full/empty (`05`, `examples/02` V2).
   </details>

5. **When NOT to pad:** name two fields in a lock-free structure you should
   *not* cache-line pad.

   <details><summary>Answer</summary>

   (1) An immutable / read-only field (capacity, mask) — sharing is free, padding
   wastes a line. (2) A field only ever written by one thread and never read by
   others on the hot path. Also: two fields always read together in the same
   operation (constructive sharing — you *want* them co-located).
   </details>

---

## Interview questions

1. False sharing kya hai — coherence line granularity se samjhao.
2. `examples/07` ka ~4× — kis cheez ka? Layout ke alawa kya badla? (kuch nahi)
3. `alignas(64)` field pe + trailing pad kyun dono chahiye?
4. `std::hardware_destructive_interference_size` — kya deta, ABA/ABI gotcha kya?
5. Padding kab bekaar (single-writer fields, immutable, constructive sharing)?
6. Padding alone SPSC ring ko kyun theek nahi karta (cross-line read → cached index)?
7. Adjacent-line prefetch — false-sharing unit 128 B kab?

---

## Next
→ [`04-spsc-ring-buffer.md`](04-spsc-ring-buffer.md)
