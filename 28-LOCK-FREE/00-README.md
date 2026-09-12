# 28 — LOCK-FREE PROGRAMMING (PHASE 17)

## Prerequisites
`27-ATOMICS-MEMORY-MODEL` (poora)

## Yeh folder kyun
Lock-free structures HFT ka roz ka tool hain. Yahan hum unhe **banayenge**,
**benchmark** karenge, aur samjhenge kab woh worth it hain (aur kab nahi).

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-why-lock-free.md` | Lock contention ka problem, priority inversion, blocking ki cost |
| 02 | `02-lock-free-rules.md` | Rules aur constraints, kya allowed hai kya nahi |
| 03 | `03-cache-line-padding.md` | False sharing elimination, `hardware_destructive_interference_size` |
| 04 | `04-spsc-ring-buffer.md` | **SPSC ring buffer** — build it, correctness, benchmark |
| 05 | `05-spsc-optimizations.md` | Cached indices, batch operations, power-of-2 masking |
| 06 | `06-mpmc-queue.md` | MPMC queue design, Vyukov-style bounded queue |
| 07 | `07-michael-scott-queue.md` | Classic lock-free linked queue |
| 08 | `08-lock-free-stack.md` | Treiber stack, ABA problem yahan |
| 09 | `09-memory-reclamation.md` | **Reclamation problem** — kab delete karein |
| 10 | `10-hazard-pointers.md` | Hazard pointers ka mechanism |
| 11 | `11-epoch-based-reclamation.md` | EBR, RCU basics |
| 12 | `12-seqlock.md` | **Seqlock** — HFT mein market data snapshots ke liye bahut use hota hai |
| 13 | `13-testing-lock-free.md` | Testing strategies, TSan, stress tests, model checking |
| 14 | `14-when-not-lock-free.md` | **Kab lock-free NAHI use karein** — honest trade-offs |
| 15 | `15-exercises.md` | Practice + implementations |

## Examples

| File | Kya |
|---|---|
| `examples/01_spsc_ring_buffer.cpp` | Poora SPSC — build + benchmark |
| `examples/02_spsc_optimized.cpp` | Optimized version — before/after numbers |
| `examples/03_mpmc_queue.cpp` | Bounded MPMC |
| `examples/04_lock_free_stack.cpp` | Treiber stack |
| `examples/05_seqlock.cpp` | Seqlock for snapshots |
| `examples/06_mutex_vs_lockfree.cpp` | Mutex vs lock-free — measured comparison |
| `examples/07_false_sharing_fix.cpp` | Padding ka asar |

## Time
3 hafte

## Status
✅ **COMPLETE (Batch 8 — PHASE 17/18).** 14 lessons (`01`–`14`) + `15-exercises.md`
+ 7 examples (`examples/`), sab compile-verified (`./build.ps1 folder 28-LOCK-FREE`
→ 7/7 OK). Benchmarks real measured numbers ke saath — aur do **anti-intuitive**
results teach kiye (CLAUDE.md Rule 2): (1) SPSC ring mein cache-line padding *akela*
is box pe ~noise deta hai — asli win **cached opposite index** hai (`examples/02`);
(2) ek contended lock-free Treiber stack `std::mutex + std::vector` se **~5× SLOWER**
nikla (`examples/04`) — lock-free ≠ fast. Jahan shape sahi hai (SPSC, seqlock)
wahan lock-free 4–100× jeeta (`examples/05`, `06`).

## Next
→ [`../29-LINUX-SYSTEMS/00-README.md`](../29-LINUX-SYSTEMS/00-README.md)
