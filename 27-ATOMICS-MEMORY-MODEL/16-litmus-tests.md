# 16 — Litmus tests: SB, MP, LB, IRIW

## Prerequisites
- `06-why-memory-ordering.md` … `12-hardware-memory-models.md`
- [`examples/05_reordering_demo.cpp`](examples/05_reordering_demo.cpp),
  [`examples/08_litmus_tests.cpp`](examples/08_litmus_tests.cpp)

## Yeh topic abhi kyun
Litmus test = ek **minimal** concurrent program (2–4 threads, 2–3 shared atomics,
handful of ops) jiska ek specific "weird" outcome allowed/forbidden hota hai
depending on the memory order. Yeh woh vocabulary hai jisme hardware architects,
compiler writers aur the C++ standard memory ordering ke baare mein baat karte
hain. Inhe naam se pehchanna = kisi bhi ordering bug ko ek known shape mein map
kar paana.

---

## Reading a litmus test

- Each thread runs a tiny straight-line sequence.
- All shared variables start at `0`.
- We ask: **can `r1 == …, r2 == …` (the register results) happen?**
- "Allowed" under a model = some legal execution produces it. "Forbidden" = no
  legal execution does.

Notation: `Wx=1` write 1 to x; `Rx` read x into a register.

---

## SB — Store Buffering

```
Thread 1        Thread 2
x.store(1)      y.store(1)
r1 = y.load()   r2 = x.load()

Weird outcome: r1 == 0 && r2 == 0
```

Both threads store, then read the *other* variable, and **neither sees the
other's store**. Requires **store→load reordering** (a store sits in the buffer
while the later load executes).

| Order on all 4 ops | `r1==0 && r2==0`? | This box (`examples/05`, 200k rounds, run-to-run) |
|---|---|---|
| `relaxed` | **allowed** | ~70–330 |
| `release`/`acquire` | **allowed** (acq/rel doesn't stop store→load) | ~1000–4200 |
| `seq_cst` | **forbidden** | **0** (always) |

SB is *the* reason `seq_cst` exists as a level, and *the* reason Dekker/Peterson
locks need `seq_cst` (or a `seq_cst` fence between each thread's store and load).

---

## MP — Message Passing

```
Thread 1 (producer)     Thread 2 (consumer)
data = 42               while (flag.load() == 0) {}
flag.store(1)           r = data

Weird outcome: flag seen as 1, but r == 0 (stale data)
```

The publish/subscribe pattern. Requires **store→store reordering** on the
producer (flag becomes visible before data) or **load→load reordering** on the
consumer (data read hoisted before the flag check).

| Producer store / consumer load order | stale `data` possible? | This box (`examples/08` MP, 1M rounds) |
|---|---|---|
| `relaxed` / `relaxed` | **allowed** (formally; x86 TSO happens to forbid it in practice) | 0 observed — but UB |
| `release` / `acquire` | **forbidden** | 0 |
| `seq_cst` / `seq_cst` | forbidden | 0 |

x86 keeps store→store and load→load, so plain relaxed MP "works" on x86 — and
breaks on ARM. `release`/`acquire` makes it correct **by the model**, everywhere.
This is the single most important litmus test for everyday code.

---

## LB — Load Buffering

```
Thread 1            Thread 2
r1 = x.load()       r2 = y.load()
y.store(1)          x.store(1)

Weird outcome: r1 == 1 && r2 == 1
```

Both threads read, then write — and each read sees the *other thread's* later
write. Requires **load→store reordering** (a store hoisted before an earlier
load). Forbidden on x86-TSO (x86 never does load→store). Allowed on ARM/POWER
with `relaxed`; forbidden with `acquire` loads (or `release` stores).

---

## IRIW — Independent Reads of Independent Writes

```
Thread 1     Thread 2     Thread 3            Thread 4
x.store(1)   y.store(1)   r1 = x.load()       r3 = y.load()
                          r2 = y.load()       r4 = x.load()

Weird outcome: r1==1, r2==0  AND  r3==1, r4==0
(Thread 3 says "x happened before y"; Thread 4 says "y happened before x")
```

Two independent writes by two threads; two other threads read both — and
**disagree on the order**. Requires a **non-multi-copy-atomic** hardware model.

| | IRIW disagreement possible? |
|---|---|
| x86-TSO | **no** — multi-copy-atomic |
| ARMv8 | no (multi-copy-atomic since v8) |
| POWER, ARMv7 | **yes** with `relaxed`/`acquire`; **no** with `seq_cst` |

`examples/08` IRIW test: **0 disagreements** on this x86 box at every order —
expected, x86 is multi-copy-atomic. On POWER you'd need `seq_cst` (and heavy
`sync` barriers) to get 0.

---

## Summary table

| Test | Threads | Reordering needed | Killed by | x86 allows (relaxed)? |
|---|---|---|---|---|
| **SB** | 2 | store→load | `seq_cst` (or seq_cst fence) | **yes** |
| **MP** | 2 | store→store or load→load | `release`/`acquire` | no (TSO) |
| **LB** | 2 | load→store | `acquire`/`release` | no (TSO) |
| **IRIW** | 4 | non-multi-copy-atomic | `seq_cst` | no (multi-copy-atomic) |

x86-TSO forbids everything except **SB** with weaker-than-seq_cst orders. That's
exactly why "works on x86" is such a weak signal — the one reordering x86 *does*
allow (SB) is the rare one you hit only in hand-rolled mutual exclusion; the
common bug (MP with relaxed) is masked.

---

## > **HFT relevance**
> - **MP is your daily bread** — every queue publish, every snapshot swap. Use
>   `release`/`acquire`; never rely on x86 masking a relaxed MP bug that ARM will
>   expose.
> - **SB shows up only in hand-rolled locks / mutual exclusion** — if you're
>   writing Dekker/Peterson (you probably shouldn't be — use a well-tested
>   spinlock or `std::atomic_flag`), it needs `seq_cst` or a `seq_cst` fence
>   between store and load.
> - **IRIW essentially never matters for a single x86/ARMv8 trading box** — both
>   are multi-copy-atomic. Only relevant if you target POWER.
> - **Model-check the core protocol** — herd7 / GenMC take a litmus-shaped kernel
>   and enumerate every execution under a chosen model. That's how you *prove* a
>   lock-free queue's ordering, not by running it on x86 a billion times
>   (`examples/08` does the stress version; a model checker does the proof).

---

## Hands-on

```bash
./build.ps1 fast 27-ATOMICS-MEMORY-MODEL/examples/05_reordering_demo.cpp   # SB
./build.ps1 fast 27-ATOMICS-MEMORY-MODEL/examples/08_litmus_tests.cpp      # MP + IRIW
```
`05`: SB counts for relaxed / rel-acq / seq_cst — see rel/acq still non-zero,
seq_cst zero. `08`: MP bad-reads and IRIW disagreements — all zero on x86 across
orders (that's the expected, informative result: x86 only allows SB).

Then:
- Add an **LB** test to `08` (mirror of SB: load then store). Expect 0 on x86 at
  every order.
- Run `05` at `-O0` — still non-zero SB (the CPU's store buffer, not the
  compiler).
- If you have `herd7`: write `SB`, `MP`, `IRIW` in `.litmus` syntax and run under
  `x86tso` then `AArch64` — see MP flip from forbidden-in-practice to
  allowed-with-relaxed.

---

## ⚠️ Traps

### Trap 1 — "MP works, I tested on x86"
x86 forbids the MP reordering. Relaxed MP is still UB and breaks on ARM. Use
`release`/`acquire`.

### Trap 2 — using `release`/`acquire` for SB / Dekker
Acq/rel does **not** stop store→load. SB needs `seq_cst` (`examples/05`:
rel/acq ~1000–4200 both-zero, seq_cst always 0).

### Trap 3 — worrying about IRIW on x86/ARMv8
Both are multi-copy-atomic. IRIW disagreement can't happen there. Don't add
`seq_cst` "for IRIW" on those targets.

### Trap 4 — `-O0` proves no reordering
SB still fails at `-O0` — the store buffer is hardware. `-O0` only reduces
*compiler* reordering.

### Trap 5 — treating stress-test-passes as a proof
Litmus outcomes can be rare (SB ~0.03–2% here). A billion x86 runs won't exercise
a POWER-only IRIW bug at all. Model-check.

### Trap 6 — mixing up which reordering each test needs
MP = store→store / load→load. SB = store→load. LB = load→store. IRIW =
non-multi-copy-atomic. Memorize the shapes.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "if a litmus test passes on x86 it's fine" | x86 forbids all but SB (weak orders) — MP/LB bugs are masked |
| "release/acquire fixes store buffering" | Only `seq_cst` (or a seq_cst fence) forbids SB |
| "IRIW is a real risk everywhere" | Only on non-multi-copy-atomic HW (POWER, ARMv7); not x86/ARMv8 |
| "litmus tests are academic" | They're the exact shapes real ordering bugs take — name the shape, know the fix |
| "seq_cst needed for message passing" | `release`/`acquire` is sufficient and cheaper for MP |
| "-O0 removes reordering" | Removes *compiler* reordering; the CPU store buffer still causes SB |

---

## Exercises

1. **Name the test:** (a) publish data then a flag, reader sees flag but stale
   data; (b) two threads set their own flag then read the other's, both read 0;
   (c) two readers disagree on the order of two writers' stores.

   <details><summary>Answer</summary>

   (a) MP (message passing). (b) SB (store buffering). (c) IRIW.
   </details>

2. **Fix each:** minimal memory order for MP, for SB.

   <details><summary>Answer</summary>

   MP: `release` on the flag store, `acquire` on the flag load. SB: `seq_cst` on
   all four ops (or keep them relaxed and put an `atomic_thread_fence(seq_cst)`
   between each thread's store and load).
   </details>

3. **x86 predictions:** for SB, MP, LB, IRIW with all-`relaxed` ops on x86 — which
   weird outcomes are possible?

   <details><summary>Answer</summary>

   Only **SB** (`r1==r2==0`). x86-TSO forbids MP's and LB's reorderings
   (store→store, load→load, load→store all kept) and is multi-copy-atomic (no
   IRIW).
   </details>

4. **Why `examples/08` shows all zeros:** is that a bug in the test or the
   expected result?

   <details><summary>Answer</summary>

   Expected. `08` runs MP and IRIW on x86, which forbids the MP reordering and is
   multi-copy-atomic — so 0 bad reads / 0 disagreements at every order is exactly
   right. The *informative* test on x86 is SB (`05`), the one reordering x86
   allows.
   </details>

5. **Dekker's algorithm** uses "set my flag; if other's flag set, back off". Which
   litmus test is hiding in it, and what order does it need?

   <details><summary>Answer</summary>

   Store buffering — each thread stores its own flag then loads the other's; if
   both stores sit in store buffers, both loads see 0 and both enter the critical
   section. Needs `seq_cst` on the flag store+load (or a `seq_cst` fence between
   them).
   </details>

---

## Interview questions

1. Litmus test kya hota — kaise padhte hain (start 0, "can this outcome happen")?
2. SB — kaunsi reordering, kaunsa order isse rokta (seq_cst)?
3. MP — kaunsi reordering, `release`/`acquire` kyun kaafi?
4. LB aur IRIW — kya, kis hardware pe relevant?
5. x86-TSO kaunse litmus outcomes allow karta (sirf SB)?
6. "x86 pe pass" MP bug ko kyun chhupa deta hai?
7. Litmus kernel ko *prove* kaise karein — model checker vs stress test?

---

## Next
→ [`17-exercises.md`](17-exercises.md)
