# 08 — Speculative execution: speculation, rollback, Spectre/Meltdown

## Prerequisites
- `06-out-of-order-execution.md` (ROB, squash, rollback)
- `07-branch-prediction.md` (prediction, mispredict)
- `29-LINUX-SYSTEMS` file 02 (syscall cost, `mitigations=`)

## Yeh topic abhi kyun
File `06`/`07` mein "speculation" baar-baar aaya. Yeh lesson use ek jagah pin
karta: CPU **guess pe kaam** karti hai (branch direction, load-store ordering,
memory dependence), aur galat nikle to rollback. Yeh performance ke liye zaroori
hai — par 2018 se yeh **security** ka bhi topic hai (Spectre/Meltdown), aur unki
mitigations har syscall ko ~2–5× mehnga karti hain (folder 29). HFT boxes aksar
`mitigations=off` chalate — is lesson mein woh trade-off.

---

## What "speculative" means

An instruction executes **speculatively** if the CPU isn't yet certain it should
run — its result is computed but held in the ROB, not committed to architectural
state, until it *retires*. If the speculation was wrong, the result is discarded
(squash, file `06`).

Kinds of speculation in a modern core:

| Speculation | The guess | Wrong → |
|---|---|---|
| **Branch direction** | taken / not-taken (file `07`) | squash younger µops, refill (~15–20 cyc) |
| **Branch target** | the address (BTB) | same |
| **Memory disambiguation** | "this load doesn't alias that older unknown-address store" | memory-order violation → squash + replay |
| **Value / address prediction** (some µarchs) | a load's result / a pointer | squash + replay |
| **`Hardware prefetch`** | "you'll want this cache line next" (folder 32) | wasted bandwidth (no correctness issue) |

All of this is invisible to correct programs: architectural state (registers,
memory) only ever reflects retired instructions, in program order. **But
microarchitectural state — cache contents, branch predictor entries, port
occupancy — is *not* rolled back.** That's the crack Spectre/Meltdown pried
open.

---

## Spectre / Meltdown — the one-paragraph version

**The idea:** get the CPU to *speculatively* access data it shouldn't, then leak
that data through a **side channel** — usually by making the speculative path
load `array[secret * 64]`, which pulls one specific cache line in. The
speculation is rolled back (architecturally nothing happened), but that cache
line is now *warm*. Afterwards the attacker times reads of `array[i]` for all
`i`; the fast one reveals `secret`.

| Variant | Speculation abused | What leaks |
|---|---|---|
| **Meltdown** (v3) | speculative load bypasses the kernel/user permission check (on affected Intel) | kernel memory from user space |
| **Spectre v1** (bounds-check bypass) | speculative execution past a mispredicted `if (i < len)` | in-bounds process memory (e.g. a sandbox escape) |
| **Spectre v2** (branch target injection) | attacker mistrains the indirect predictor → victim speculatively jumps to attacker-chosen gadget | arbitrary victim memory |
| **MDS / L1TF / Retbleed / ...** | various buffers / return predictor | across SMT siblings, VMs, etc. |

The **channel** is almost always the cache (timing), but port contention and
other µarch resources have been used too.

---

## The mitigations (and their cost)

| Mitigation | Against | Cost |
|---|---|---|
| **KPTI** (kernel page-table isolation) | Meltdown | separate page tables for user/kernel → TLB flush-ish on every syscall/interrupt → **syscalls ~2–3× slower** (folder 29 file 02) |
| **Retpoline** / IBRS / IBPB / eIBRS | Spectre v2 | indirect branches replaced with a `ret`-based trampoline or MSR writes → slower indirect calls; IBPB on context switch |
| **`lfence` after bounds checks** | Spectre v1 | serialising fence (~compiler-inserted at hot spots) — kills OoO overlap there |
| **`mds_clear` / `VERW`** | MDS | buffer flush on kernel exit / VM entry |
| **SMT disabled** | cross-sibling leaks (L1TF, MDS) | lose ~1.1–1.3× throughput; but HFT disables SMT anyway (file `12`) |
| **microcode updates** | various | small per-op costs baked in |

`cat /sys/devices/system/cpu/vulnerabilities/*` shows what's mitigated on a box.
Aggregate, these can add **10–30%+** to syscall-heavy / context-switch-heavy
workloads.

---

## `mitigations=off` — the HFT trade-off

Boot with `mitigations=off` (folder 29 file 18) and the kernel disables the lot:
syscalls get ~2–4× faster, context switches faster, indirect calls faster.

**When it's defensible:**
- The box is **single-tenant** — only your trusted trading code runs.
- **Not internet-facing** — colocation network, firewalled, no user logins.
- **No untrusted code ever** — no JIT of external input, no browser, no
  containers running third-party images.
- Corporate/regulatory policy allows it, and you've **measured the actual gain**
  on your workload (a pure busy-poll no-syscall hot path benefits little; a
  syscall-heavy one benefits a lot).

**When it's not:** shared infra, cloud multi-tenant, anything running code you
didn't write, or where policy forbids it. The mitigations exist for real
reasons.

---

## Speculation you *want* (and can help)

- **Memory disambiguation:** the fewer aliasing possibilities, the more
  aggressively the CPU speculates loads ahead of stores. `__restrict`, separate
  buffers, SoA (example `06`, file `06`) reduce false aliasing → fewer replays,
  better scheduling.
- **Branch prediction:** predictable branches let speculation run correctly →
  the pipeline stays full (file `07`). Warm-up trains it.
- **Prefetch:** speculative cache-line fetches (hardware, or `__builtin_prefetch`)
  hide memory latency for predictable access patterns (folder 32).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — assuming your box has mitigations off because "HFT"
Check: `grep . /sys/devices/system/cpu/vulnerabilities/*` and `cat /proc/cmdline`.
Many shared/cloud boxes have them fully on. If your syscall benchmark (folder 29
example 01) shows ~500–900 ns traps, mitigations are on.

### Trap 2 — turning mitigations off on a shared or internet-facing box
Now a bug in *anything* on that box can read *anything else's* memory. This is a
real, exploited class of attack. Only on isolated, single-tenant, trusted boxes.

### Trap 3 — expecting `mitigations=off` to speed up a no-syscall hot loop
A pure busy-poll compute loop with no syscalls, no context switches, no indirect
calls barely changes. The win is on syscall/ctx-switch/indirect-heavy paths.
Measure before and after.

### Trap 4 — writing "constant-time" code and assuming the CPU respects it
Speculation, prefetch, variable-latency ops (`div`, denormals), and data-
dependent cache behaviour make true constant-time execution hard. For crypto,
use vetted constant-time primitives and hardware AES; don't hand-roll.

### Trap 5 — `lfence` sprinkled "for safety" in hot code
`lfence` serialises — it kills OoO overlap at that point (~10s of cycles of lost
parallelism). The compiler/kernel insert them where needed for Spectre v1; don't
add your own without a specific reason.

### Trap 6 — ignoring that SMT off is also a Spectre mitigation
If you disabled SMT for performance/determinism (file `12`), you also closed
the cross-sibling leak channels — one fewer thing to worry about.

---

## > **HFT relevance**

> - **`mitigations=off` on the trading boxes** (folder 29 file 18) — *if* they're
>   isolated, single-tenant, trusted, not internet-facing, and policy allows.
>   Measure the gain (big on syscall/ctx-switch-heavy paths, small on pure
>   busy-poll). Document it; compensate with network isolation + host firewall +
>   no logins.
> - **`SMT off`** (file `12`) — determinism *and* closes cross-sibling leak
>   channels.
> - **`__restrict` / SoA / separate buffers** — fewer memory-order-violation
>   replays, more aggressive (correct) load speculation (file `06`, example `06`).
> - **Predictable branches + warm-up** — speculation running *correctly* is what
>   keeps the deep pipeline full (file `07`).
> - **Know your syscall cost** — folder 29 example 01 tells you whether
>   mitigations are on. If a hot path still makes traps, that number is your
>   budget line item.

---

## Hands-on

```bash
# what's mitigated on this box, and is mitigations=off set
grep . /sys/devices/system/cpu/vulnerabilities/*
grep -o 'mitigations=[^ ]*' /proc/cmdline || echo "mitigations: default (on)"

# the syscall-cost consequence (folder 29 example 01)
./build.ps1 fast 29-LINUX-SYSTEMS/examples/01_syscall_cost.linux.cpp   # Linux only

# example 03 / 04: the (benign) speculation you DO want — predictable branches
./build.ps1 fast 31-CPU-ARCHITECTURE/examples/03_branch_prediction.cpp
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "speculation only matters for security" | it's *why* the pipeline stays full; security is a side effect of µarch state not rolling back |
| "rollback undoes everything" | architectural state yes; cache/predictor state **no** — that's the leak |
| "`mitigations=off` = free speed everywhere" | big on syscall/ctx-switch/indirect paths; ~0 on pure busy-poll |
| "HFT boxes always have mitigations off" | check `/sys/.../vulnerabilities/*` and cmdline |
| "turning them off is standard, do it" | only isolated, single-tenant, trusted, non-internet boxes |
| "my code is constant-time so it's safe" | speculation/prefetch/variable-latency ops defeat naive constant-time |

---

## Exercises

1. Speculative execution rollback "architectural state" restore karta par
   "microarchitectural state" nahi. In dono mein kya-kya aata, aur Spectre isi
   farak ka kaise faayda uthata?

   <details><summary>Answer</summary>

   Architectural state = the ISA-visible stuff: general/SIMD registers, flags,
   RIP, memory contents. Rolled back on a bad speculation, so a correct program
   never sees a difference. Microarchitectural state = cache line contents, TLB
   entries, branch-predictor/BTB entries, prefetcher state, port occupancy
   history. **Not** rolled back. Spectre: coerce the CPU to speculatively do
   `tmp = probe_array[secret * 64]`. The load is squashed (register `tmp` never
   commits), but `probe_array`'s line for `secret*64` is now cached. The attacker
   then times `probe_array[i]` reads for all `i`; the fast index = `secret`. The
   secret leaked through the cache's timing, not through any architectural value.
   </details>

2. KPTI (Meltdown mitigation) syscalls ko ~2–3× slow kyun karta?

   <details><summary>Answer</summary>

   Pre-KPTI, the kernel was mapped into every process's page tables (in the upper
   half), so a syscall just switched privilege level — the TLB stayed valid.
   Meltdown showed a speculative load could read those kernel mappings from user
   mode. KPTI gives the process **two page-table sets**: a user set with the
   kernel *unmapped*, and a full set used only in kernel mode. Every syscall/
   interrupt now switches `CR3` (the page-table root) on entry and exit → the
   TLB largely invalidates (mitigated by PCIDs on newer CPUs, but not free) →
   the first memory accesses after the switch re-walk page tables. That
   per-transition cost is the ~2-3× (folder 29 file 02).
   </details>

3. Ek HFT firm ka box: colocated at the exchange, one trading process, no SSH,
   firewalled to the exchange network only, security team signs off.
   `mitigations=off` — reasonable? Ek risk jo phir bhi rehta hai?

   <details><summary>Answer</summary>

   Reasonable given all those conditions (single-tenant, no untrusted code, not
   internet-facing, policy approval) — the performance win on syscall/ctx-switch
   paths is real and there's no local attacker to exploit the leak. Residual
   risks: (1) a **supply-chain / dependency compromise** — if a linked library
   or a build-time tool were backdoored, it now runs with no speculation
   barriers; (2) **a future vulnerability** in the exchange-facing parsing code
   (a malformed feed packet triggering a bug) could be leveraged further with no
   mitigations; (3) **firmware/BMC** attack surface. Compensating controls:
   minimal dependencies, static linking with audited libs, fuzzed parsers,
   locked-down BMC, and monitoring. It's a deliberate, bounded risk acceptance,
   not "off because fast".
   </details>

4. `mitigations=off` set karne ke baad tumhare pure busy-poll market-data
   receiver ki latency almost same hai. Kyun, aur kaunsa component fayda uthata?

   <details><summary>Answer</summary>

   A busy-poll receiver on an isolated core makes **no syscalls** on the hot path
   (it spins on `recv(MSG_DONTWAIT)` or polls a NIC ring — folder 30), takes **no
   context switches** (pinned, `nohz_full`), and has **no indirect calls** in the
   inner loop (devirtualized). The mitigations tax exactly those three things —
   so there's nothing for `mitigations=off` to speed up here. Where it *does*
   help on the same box: the **control plane** (epoll loop, config, logging —
   syscall-heavy), process **startup**, and any path that still traps. If your
   architecture already removed syscalls from the hot path, `mitigations=off` is
   mostly a control-plane and safety-margin win, not a hot-path one — which is
   another argument for the "remove syscalls" work being primary.
   </details>

5. `__restrict` speculative execution se kaise juda hai (file `06` se connect)?

   <details><summary>Answer</summary>

   The CPU speculatively executes a load *before* an older store whose address
   isn't yet known, betting they don't alias (memory disambiguation). If they
   do alias → memory-order-violation squash + replay. `__restrict` tells the
   **compiler** the pointed-to regions are disjoint, so (a) it generates code
   without conservative reload-after-store sequences and can vectorize/reorder
   freely, and (b) the resulting instruction stream has fewer store-then-load
   pairs on the same region for the CPU to mis-speculate on. Net: fewer replays,
   better scheduling, and (example `06`) ~2-4× on a map loop. Separate buffers /
   SoA layouts achieve the same by making aliasing structurally impossible.
   </details>

---

## Interview questions

1. What does "speculative execution" mean — architectural vs microarchitectural state.
2. Kinds of speculation in a modern core (branch, target, memory disambiguation, prefetch).
3. Spectre/Meltdown in one sentence — speculate access + leak via a side channel (usually cache timing).
4. KPTI — why it makes syscalls ~2–3× slower.
5. `mitigations=off` — the performance win and the exact preconditions for it to be defensible.
6. Why does `mitigations=off` barely help a pure busy-poll hot loop?
7. How `__restrict` / SoA relate to (correct) load speculation and replay avoidance.

---

## Next
→ [`09-instruction-latency-throughput.md`](09-instruction-latency-throughput.md)
