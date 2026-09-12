# 12 — Hardware memory models: x86-TSO vs ARM/POWER

## Prerequisites
- `06-why-memory-ordering.md` … `11-fences.md`

## Yeh topic abhi kyun
C++ ka memory model **portable abstraction** hai. Neeche har CPU architecture ka
apna model hai, aur woh alag-alag strength ka hai. Yeh samajhna zaroori hai
kyunki: (1) x86 pe test-pass hona ARM pe correctness prove nahi karta, (2) HFT
boxes ab x86 *aur* ARM (Graviton, Ampere) dono hote hain, (3) jab aap asm dekhte
ho toh pata hona chahiye kaunsa barrier kyun aaya.

---

## The spectrum

```
   STRONGER (fewer reorderings allowed)                     WEAKER (more)
   ├───────────────┬──────────────┬─────────────┬──────────────┤
 sequential      x86-TSO         ARMv8         ARMv7         Alpha
 consistency   (Intel/AMD)     POWER, RISC-V   (old)       (historical)
```

- **Sequential consistency (SC):** the theoretical ideal — program behaves as some
  interleaving of thread steps, everyone agrees on one order. No real CPU is pure
  SC (too slow).
- **x86-TSO (Total Store Order):** strong. The *only* reordering allowed is
  **store → later load** (to a different address). Store→store, load→load,
  load→store are all kept in order. Each core has a FIFO store buffer; stores
  become globally visible in program order.
- **ARMv8 / POWER / RISC-V:** weak. **Any** pair of independent memory ops can be
  reordered (store→store, load→load, load→store, store→load) unless a barrier or a
  dependency forbids it. POWER additionally allows non-multi-copy-atomic
  behaviour (two readers seeing two writers in different orders — IRIW without
  barriers).

---

## x86-TSO in detail

| Reordering | Allowed on x86? |
|---|---|
| Store then load (different addr) | **YES** — the store sits in the store buffer while the load goes ahead |
| Store then store | No — stores drain in FIFO order |
| Load then load | No |
| Load then store | No |
| A core sees its **own** store before it's global | Yes — store-forwarding from the buffer |
| Two cores see two other cores' stores in different orders | No — x86 is **multi-copy-atomic** (IRIW can't happen) |

Consequence: on x86 you get, **for free**:
- `load(acquire)` == plain `mov`
- `store(release)` == plain `mov`
- `relaxed` load/store == plain `mov`
- Only `seq_cst` **store** needs work (`xchg` / `mov;mfence`) — to stop store→load.
- Every `lock`-prefixed RMW is a full barrier already.

This is why `examples/05` shows the store-buffering `r1==r2==0` outcome even with
`release`/`acquire` — x86 allows exactly that one reordering, and acq/rel doesn't
forbid it.

---

## ARMv8 in detail

Nothing is ordered by default. Ordering comes from:
- **Barriers:** `dmb ish` (full), `dmb ishld` (load-load + load-store), `dmb ishst`
  (store-store).
- **Acquire/release instructions:** `ldar` (load-acquire), `stlr`
  (store-release) — one-way, cheaper than a full `dmb`. `ldar`/`stlr` also have a
  special pairing giving something close to `seq_cst` when used together.
- **Dependencies:** an address or data dependency between two loads is respected
  (a genuine `ptr->field` after `load(ptr)` won't be reordered — the hardware
  can't). Control dependencies are **not** reliably respected for loads.

C++ → ARMv8 mapping (roughly):
| C++ | ARMv8 |
|---|---|
| `load(relaxed)` | `ldr` |
| `load(acquire)` | `ldar` |
| `store(relaxed)` | `str` |
| `store(release)` | `stlr` |
| `load(seq_cst)` | `ldar` |
| `store(seq_cst)` | `stlr` (+ `dmb` on older cores) |
| `fetch_add(relaxed)` | `ldxr`/`stxr` loop (or `ldadd` on ARMv8.1 LSE) |
| `fetch_add(seq_cst)` | `ldaddal` (LSE) or `ldaxr`/`stlxr` loop |
| `atomic_thread_fence(seq_cst)` | `dmb ish` |

So on ARM, **the memory order you pick has a real, per-op cost** — unlike x86.

---

## Multi-copy atomicity (the IRIW subtlety)

- **x86 & ARMv8:** multi-copy-atomic — if core A's store is visible to core B, it's
  visible to *all* cores at that point. Two readers can't disagree about the
  order of two independent writes (given enough barriers, IRIW is forbidden).
- **POWER & older ARM:** **not** multi-copy-atomic — reader X can see writer 1's
  store before writer 2's, while reader Y sees the opposite, *even with acquire
  loads*. Only `seq_cst` (heavy `sync` barriers) restores agreement.

This is why `seq_cst` exists as a distinct level: it's the only order that
guarantees a single global order on **all** platforms (`examples/08` IRIW test —
0 disagreements on x86; would need `seq_cst` + `sync` to guarantee that on POWER).

---

## What this means for you

1. **Write the memory order the C++ model requires**, not the one x86 makes free.
   `release`/`acquire` on a publish even though x86 gives it for nothing — because
   the compiler still needs the constraint, and ARM needs the instruction.
2. **Test on the target arch.** A CI leg on ARM (or under `herd7` / a model
   checker) catches weak-model bugs that x86 will never show.
3. **`-O0` on x86 is the weakest testing environment possible** — least compiler
   reordering *and* strongest hardware model. `-O2` on ARM under TSan is the
   strongest.
4. **Reading asm:** an `mfence` / `dmb` / `lock` you didn't expect = a `seq_cst`
   somewhere (often a default-order atomic you forgot to annotate).

---

## > **HFT relevance**
> - **ARM trading boxes are real** (AWS Graviton, Ampere Altra in colo). Code
>   tuned to x86-TSO — "store-store just works", bare `relaxed` flags — breaks
>   there. The lock-free queues in folder 28 use explicit `acquire`/`release` so
>   they port.
> - **On x86, `acquire`/`release` are free — use them generously**; the cost of
>   "too strong" here is zero. Save the profiling effort for the `seq_cst` stores.
> - **On ARM, audit every atomic's order** — `ldar`/`stlr`/`dmb` each cost real
>   cycles on the hot path; that's where downgrading `seq_cst`→acq/rel→relaxed
>   pays measurable latency.
> - **`lock`-prefixed RMW on x86 ≈ 13 ns even uncontended** (it's a full
>   barrier + possible cache-line lock) — `examples/06`; ~27 ns/op contended
>   here. On the hot path, prefer a single-writer design with `release` stores
>   over shared `fetch_add`.
> - **Never ship a lock-free structure that's only been run on x86.** herd7 /
>   GenMC the core protocol, or run it on ARM in CI.

---

## Hands-on

```bash
./build.ps1 fast 27-ATOMICS-MEMORY-MODEL/examples/05_reordering_demo.cpp
./build.ps1 fast 27-ATOMICS-MEMORY-MODEL/examples/08_litmus_tests.cpp
```
`05`: store-buffering — x86 allows it (relaxed & rel/acq), forbids it (seq_cst).
`08`: MP and IRIW — x86 shows 0 bad reads / 0 disagreements at every order
(strong + multi-copy-atomic). Then:
- Cross-compile one atomic op for ARM: `aarch64-linux-gnu-g++ -O2 -S` a
  `store(release)` → `stlr`; a `store(seq_cst)` → `stlr` (+ maybe `dmb`); a
  `store(relaxed)` → `str`. Contrast with x86 where all three are `mov`.
- On x86, `g++ -O2 -S` a default `x.store(1)` — spot the `xchg`/`mfence` that a
  `release` annotation would remove.

---

## ⚠️ Traps

### Trap 1 — "passes on x86" == correct
x86-TSO is one of the strongest models. Weak-model bugs (missing acq/rel on ARM)
are invisible on x86.

### Trap 2 — bare `relaxed` flags relying on store-store order
x86 keeps store→store; ARM doesn't. Your relaxed "write payload, write flag"
publishes garbage on ARM.

### Trap 3 — assuming IRIW can't happen
On POWER / old ARM, two readers can disagree on two writers' order even with
acquire loads. Needs `seq_cst`.

### Trap 4 — `-O0` as a race test
Weakest test: minimal compiler reordering, strongest hardware model. Use `-O2` +
TSan, ideally on ARM.

### Trap 5 — thinking x86 `lock xadd` is cheap because "no fence shown"
It *is* a full barrier and costs ~13 ns even uncontended (`examples/06`).

### Trap 6 — control dependency assumed to order loads on ARM
Address/data dependencies are respected; a *control* dependency (`if (x) y = *p;`)
is not a reliable load-load barrier on ARM. Use `acquire`.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "all CPUs reorder the same way" | x86-TSO allows only store→load; ARM/POWER allow all four |
| "x86 has no reordering" | Store→load *is* reordered on x86 (store buffer) — `examples/05` |
| "acquire/release costs the same on x86 and ARM" | Free on x86 (`mov`); real instructions (`ldar`/`stlr`/`dmb`) on ARM |
| "IRIW is impossible everywhere" | POWER / old ARM aren't multi-copy-atomic — needs `seq_cst` |
| "if x86 asm has no fence, the op is cheap" | `lock`-prefixed RMW is a full barrier, ~13 ns uncontended (~27 ns contended here) |
| "`-O0` proves thread-safety" | Weakest possible test environment for concurrency |

---

## Exercises

1. **Which reorderings** does x86-TSO permit? Which does ARMv8 permit?

   <details><summary>Answer</summary>

   x86-TSO: store→load only (to different addresses). ARMv8: all four —
   store→store, store→load, load→load, load→store — unless a barrier, an
   `ldar`/`stlr`, or an address/data dependency forbids it.
   </details>

2. **Free on x86?** `load(acquire)`, `store(release)`, `store(seq_cst)`,
   `fetch_add(relaxed)`.

   <details><summary>Answer</summary>

   `load(acquire)` — free (`mov`). `store(release)` — free (`mov`).
   `store(seq_cst)` — **not** free (`xchg`/`mfence`). `fetch_add(relaxed)` — a
   `lock xadd`, always a full barrier (~13 ns here), so "not free" but its *order*
   argument is free.
   </details>

3. **Port the bug:** `data = 7; flag.store(1, relaxed);` /
   `while(!flag.load(relaxed)); r = data;` — behaviour on x86 vs ARM?

   <details><summary>Answer</summary>

   x86: store→store preserved, so `flag==1` implies `data==7` is visible — "works"
   (still formally UB). ARM: the two stores can reorder, and the two loads can
   reorder — reader can see `flag==1` with `data==0`. Fix: `release`/`acquire`.
   </details>

4. **Why seq_cst exists as a level:** give the one guarantee it provides that
   acquire/release cannot, and name a platform where the difference is observable.

   <details><summary>Answer</summary>

   A single total order over all `seq_cst` ops that every thread agrees on —
   which prevents store-buffering (both-zero) and IRIW disagreement. Observable on
   POWER / older ARM (not multi-copy-atomic) for IRIW; observable on x86 for
   store-buffering (`examples/05`: rel/acq ~1000–4200 both-zero, seq_cst always 0).
   </details>

5. **Reading asm:** you see an unexpected `mfence` in your `-O2` build. Most
   likely cause?

   <details><summary>Answer</summary>

   A `seq_cst` store — almost always a default-order atomic (`x.store(v)` /
   `x = v` on a `std::atomic`) that should have been annotated `release` or
   `relaxed`.
   </details>

---

## Interview questions

1. x86-TSO kaunsi ek reordering allow karta hai? ARMv8 kaunsi (saari)?
2. x86 pe kaunse C++ memory orders "free" hain, kaunsa nahi (seq_cst store)?
3. Multi-copy atomicity kya hai — kaunse arch me nahi (POWER)?
4. "x86 pe pass" ARM correctness prove kyun nahi karta?
5. ARM pe `load(acquire)` / `store(release)` / fence — kaunse instructions?
6. Address dependency vs control dependency — ARM pe load-load ordering ke liye?
7. `-O0` on x86 concurrency test ke liye sabse kamzor environment kyun?

---

## Next
→ [`13-atomic-ref.md`](13-atomic-ref.md)
