# 12 — Seqlock

## Prerequisites
- `11-epoch-based-reclamation.md`, `03-cache-line-padding.md`
- `27-ATOMICS-MEMORY-MODEL` files 08 (acquire/release), 11 (fences)
- [`examples/05_seqlock.cpp`](examples/05_seqlock.cpp)

## Yeh topic abhi kyun
Seqlock ek **1-writer, N-reader** snapshot mechanism hai. Readers kabhi lock nahi
lete, writer kabhi block nahi hota. HFT mein yeh **market-data snapshots** ke liye
sabse zyada use hone wala lock-free pattern hai — BBO, greeks, risk limits,
position — koi bhi chhoti struct jo ek thread update karta aur bahut threads
padhte hain. `examples/05` mein seqlock `std::shared_mutex` se **~80–100× faster
reads** nikla.

---

## The mechanism

```
  seq_   — std::atomic<uint64_t>, starts at 0 (EVEN)
  data   — the plain struct being published

  WRITER (single):
    s = seq_ (relaxed)
    seq_.store(s + 1, relaxed)                 // -> ODD  = "update in progress"
    release fence
    data = new_value                          // plain writes
    release fence
    seq_.store(s + 2, release)                 // -> EVEN = published

  READER (many):
    for (;;) {
      s1 = seq_.load(acquire)
      if (s1 & 1) continue;                    // writer mid-update -> retry
      snapshot = data                          // plain reads
      acquire fence
      s2 = seq_.load(relaxed)
      if (s1 == s2) break;                     // clean: no write overlapped
      // else the writer bumped seq_ during our copy -> torn -> retry
    }
```

- **Odd `seq_` = write in progress.** A reader that sees odd retries immediately.
- **Reader brackets its copy with two reads of `seq_`.** Same value, and even →
  no writer touched `data` during the copy → the snapshot is consistent.
- **Writer never waits.** No reader can hold anything the writer needs.
- **Reader "wait-free-ish":** wait-free only if the writer isn't continuously
  writing; under a flat-out writer a reader can retry many times (bounded by
  writer rate, not unbounded).

---

## Why the fences

- Writer: the first `seq_++` (odd) must be visible **before** the `data` writes,
  and the `data` writes must be visible **before** the final `seq_` (even). The
  `release` fences (or a `release` store on the closing `seq_`) enforce that
  ordering so a reader that sees the even `seq_` also sees the completed `data`.
- Reader: the `data` reads must happen **before** the second `seq_` load, so the
  `acquire` fence sits between them. Otherwise the compiler/CPU could hoist the
  `seq_` recheck before the copy finished, defeating the torn-read detection.

---

## The formal wrinkle: the reader's `data` reads race the writer's `data` writes

Strictly, a reader copying `data` while the writer writes it **is a data race**
(non-atomic, conflicting, unordered) → UB. Practically:
- On x86 with aligned fields it doesn't tear, and the seq recheck catches any
  overlap, so it "works".
- **Correct C++:** make each `data` field a `std::atomic<T>` accessed with
  `relaxed` (or use `std::atomic_ref` over a plain struct — folder 27 file 13),
  and keep the `seq_` acquire/release as the consistency gate. Then the field
  accesses are defined; the seqlock logic is unchanged.
- `examples/05` uses plain fields for x86 demo simplicity and **says so** in the
  output — real code should use relaxed atomics / `atomic_ref` for the payload.

---

## Measured (`examples/05`, this box, `-O2`)

```
1 writer (max rate) + 4 readers x 5 M reads,  Quote = 40 bytes
  seqlock       : ~180 M reads/s   torn=0   retries ~600-850%
  shared_mutex  : ~2 M reads/s     torn=0   retries 0%
  ratio         : ~80-100x
```

- **torn = 0** — no reader ever accepted a half-old/half-new `Quote`.
- **retries ~600–850%** — the writer is spinning *flat out*, so most reads catch a
  write in progress and redo. A retry is cheap (re-read local cache lines, no
  syscall) → still ~80–100× faster than the lock. **With a realistic writer
  cadence** (a few M updates/s, not a tight spin) the retry rate drops toward 0.
- `shared_mutex` is slow here because every `shared_lock` is an atomic RMW +
  bookkeeping, and 5 threads on one `shared_mutex` serialize hard.

---

## When seqlock fits (and when not)

| Fits | Doesn't fit |
|---|---|
| 1 writer, N readers | Multiple writers (need a writer lock among them, or CAS the seq) |
| Small, fixed-size POD (≤ ~1 cache line ideal) | Large payloads (long copy = long retry window) |
| Read-heavy, write-moderate | Write-heavy relative to reads (retry storm on readers) |
| Readers can tolerate "retry a few times" | Readers that must be strictly wait-free |
| Latest-value-wins semantics | Readers that need *every* version (use a queue) |

Multiple writers: either serialize writers with their own mutex (readers still
lock-free — a common and good design), or make the `seq_` bump a CAS.

---

## Seqlock vs the alternatives

| Mechanism | Reader cost | Writer cost | Payload size | Notes |
|---|---|---|---|---|
| **Seqlock** | 2 atomic loads + copy (+ retry) | 2 atomic stores + copy | small POD | writer never blocks; latest-wins |
| `std::atomic<Struct>` | 1 lock-free load | 1 lock-free store | ≤ 8 (16 with DWCAS) bytes | simplest when it fits |
| `atomic<const T*>` pointer swap + RCU | 1 acquire load | build + swap + reclaim | any | needs reclamation (`11`) |
| `std::shared_mutex` | lock/unlock (RMW) | exclusive lock | any | readers block each other + the writer; ~80–100× slower here |

**Decision:** payload ≤ 8 bytes → `std::atomic<T>`. Small POD, 1 writer → seqlock.
Big object or many writers → RCU pointer swap.

---

## > **HFT relevance**
> - **Seqlock is *the* market-data snapshot primitive.** Feed handler writes the
>   BBO / greeks / theo into a seqlock-protected struct; every strategy thread
>   reads a consistent snapshot with two atomic loads and no lock. Writer's tick
>   rate is never throttled by readers.
> - **Payload = relaxed atomics or `atomic_ref`** for a strictly-correct build;
>   plain fields "work" on x86 but are formally UB (`examples/05` note).
> - **Keep the struct ≤ 1–2 cache lines** — the copy time is the reader's retry
>   window; a fat struct under a fast writer means high retry rates.
> - **Multiple feed threads → serialize the writers** with a small mutex (readers
>   stay lock-free), or shard one seqlock per symbol so each has a single writer.
> - **`seq_` on its own cache line** — it's read by every reader every attempt and
>   written by the writer twice per update; false sharing here is expensive (`03`).

---

## Hands-on

```bash
./build.ps1 fast 28-LOCK-FREE/examples/05_seqlock.cpp
```

Then:
- Slow the writer to ~2 M updates/s (a small `spin_until` between writes) → reader
  retry% drops toward 0, throughput stays high.
- Grow `Quote` to 4 cache lines → retry% climbs (longer copy window).
- Replace the plain `Quote` fields with `std::atomic<double>` etc. accessed
  `relaxed` → same numbers, now formally race-free.
- Add a second writer without serialization → watch `torn` go non-zero.

---

## ⚠️ Traps

### Trap 1 — reader doesn't recheck `seq_` after the copy
Without the second load + compare, a write that started mid-copy is undetected →
torn snapshot accepted.

### Trap 2 — no fence between the copy and the recheck
The compiler/CPU can reorder the `seq_` recheck before the copy completes →
detection defeated. `acquire` fence between them.

### Trap 3 — multiple writers, no serialization
Two writers interleaving `seq_++` → `seq_` can go even while `data` is
half-written → readers accept garbage. Serialize writers.

### Trap 4 — large payload under a fast writer
Copy time = retry window. Big struct + flat-out writer = readers livelock-ish
(still bounded by writer rate, but throughput craters). Keep it small.

### Trap 5 — plain non-atomic payload in strictly-correct code
Formally a data race. Use `relaxed` atomics / `atomic_ref` for the fields; keep
`seq_` as the gate.

### Trap 6 — `seq_` sharing a cache line with the payload or another hot var
Every reader hammers `seq_`; false sharing tanks it. Put `seq_` alone on a line.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "odd `seq_` means error" | Means "write in progress" — reader just retries |
| "one `seq_` read is enough" | Two — before and after the copy, must match and be even |
| "seqlock supports multiple writers" | One writer; serialize writers separately if you have several |
| "the payload copy is race-free because of `seq_`" | Formally still a race; use relaxed atomics / `atomic_ref` for fields |
| "readers are wait-free" | Wait-free only if the writer isn't continuously writing; else bounded retries |
| "any size payload is fine" | Copy time is the retry window — keep it ≤ 1–2 cache lines |

---

## Exercises

1. **Trace a torn read caught:** reader loads `s1 = 4` (even), starts copying, the
   writer runs (`seq_` → 5 → 6). What does the reader's recheck see and do?

   <details><summary>Answer</summary>

   After the copy the reader loads `s2 = 6` (or catches `5`). `s2 != s1` (6 ≠ 4)
   → the writer touched `data` during the copy → the snapshot may be torn → the
   reader discards it and loops. Next attempt: `s1 = 6` (even), copy, `s2 = 6`,
   match → clean.
   </details>

2. **Why odd/even instead of a boolean "writing" flag?** 

   <details><summary>Answer</summary>

   The counter also detects a *complete* write that happened entirely between the
   reader's two `seq_` loads (a boolean would read `false` both times and miss
   it). Odd/even gives "in progress" (odd) *and* a version number so
   `s1 == s2 && even` proves no write overlapped at all.
   </details>

3. **Multiple writers fix:** you have 3 feed threads updating one `Quote`. Two
   designs that keep readers lock-free.

   <details><summary>Answer</summary>

   (a) A small `std::mutex` among the 3 writers only — they serialize with each
   other, but readers still use the pure seqlock read path (no lock). (b) Shard:
   one seqlock per symbol, and route each symbol to a single writer thread — every
   seqlock then has exactly one writer.
   </details>

4. **Size vs retry:** writer at 5 M updates/s, reader copy of the struct takes 40
   ns. Rough probability a given read attempt is torn (needs retry)?

   <details><summary>Answer</summary>

   Writes are ~200 ns apart (5 M/s). Each write's "in progress + copy overlap"
   window is roughly the writer's write time plus the reader's 40 ns copy. If the
   writer's write takes ~40 ns too, the vulnerable window per 200 ns is ~80 ns →
   ~40% of attempts retry. Halve the struct (20 ns copy) → ~30%. This is why
   payload size matters.
   </details>

5. **Choose the mechanism:** (a) an 8-byte sequence number; (b) a 48-byte BBO
   struct, 1 writer; (c) a 4 KB order-book snapshot, 1 writer, 50 readers.

   <details><summary>Answer</summary>

   (a) `std::atomic<uint64_t>` — fits a lock-free atomic, no seqlock needed.
   (b) seqlock — small POD, single writer, exactly its use case. (c) RCU / atomic
   pointer swap (`11`) — 4 KB is too big to copy per read attempt; publish a new
   immutable snapshot and swap the pointer, reclaim after a grace period.
   </details>

---

## Interview questions

1. Seqlock: writer aur reader ka exact protocol (odd/even, two reads).
2. Reader ke do `seq_` reads ke beech fence kyun?
3. Odd/even counter vs ek "writing" boolean — counter kyun better?
4. Payload formally race hai — strictly-correct kaise (relaxed atomics / atomic_ref)?
5. Multiple writers — readers lock-free rakhte hue kaise handle?
6. Payload size retry rate ko kaise affect karta?
7. Seqlock vs `atomic<T>` vs RCU pointer-swap — kab kaunsa?

---

## Next
→ [`13-testing-lock-free.md`](13-testing-lock-free.md)
