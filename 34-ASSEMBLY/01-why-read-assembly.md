# 01 — Why read assembly: verification, optimization, debugging

## Prerequisites
- `33-COMPILER-OPTIMIZATION` (poora — yeh folder uska verification-side hai)
- `08-FUNCTIONS/05-the-call-stack.md`

## Yeh folder kyun
Aapko assembly **likhni nahi** hai — 99.9% cases mein hand-written asm compiler
se bura hoga aur portable bhi nahi. Aapko usse **padhni** aani chahiye, teen
kaam ke liye:

1. **Verification** — "maine `__restrict` / `inline` / `[[unlikely]]` /
   `-march` diya; compiler ne wahi kiya?" (folder 33 ki har technique).
2. **Optimization** — "yeh hot loop itna dheema kyun? `div` critical path
   mein? spill ho raha? branch banaya jahan `cmov` chahiye tha?"
3. **Debugging** — "crash yahan hua — is address pe kaunsi instruction, kaunsi
   source line, register mein kya tha?"

Har cheez godbolt / `./build.ps1 asm` / `objdump` / `perf annotate` se
5 minute mein. Bina padhe, aap andhere mein optimize karte ho.

---

## Verification examples (folder 33 se connected)

| Aapne kiya | Asm mein confirm karo |
|---|---|
| `__restrict` on kernel params (33/09) | loop `mulps`/`vfmadd...ps` (packed), not `mulss`; no per-iter reload of `*scale` |
| `inline` / header-only hot fn (33/03) | no `call` in the hot loop body |
| `[[unlikely]]` on error path (33/08) | the handler's `call` sits **after** the function body / loop (out-of-line) |
| `constexpr` tick size (33/06) | `imul`/shift, not a `mov reg, [rip+...]` memory load of a config value |
| `-march=x86-64-v3` (33/12) | `ymm` (32-byte) vectors, `vfmadd`; `movbe`/`popcnt` where applicable |
| `final` + concrete type (33/07) | direct `call` / inlined `imul`, not `call [rax+off]` (vtable) |
| Benchmark barrier (33/14) | `asm volatile("")` produces **zero** instructions; the loop still there |
| No `-ffast-math` on pricing (33/13) | float reduction is a scalar `addsd` chain (not `addps` partials) — as intended |

---

## Optimization: "why is this slow?"

`perf annotate` / `./build.ps1 asm` on the hot function, then look for
(lesson 08 + folder 33 lesson 15):

- **`div` / `idiv` / `__divdi3` in the loop** → non-constant divisor
  (~20-40 cyc). Hoist / `libdivide` / power-of-two.
- **`call` in the loop** → not inlined (header/`-flto`), or a `virtual` /
  `std::function` dispatch.
- **scalar `movss`/`addss` where you expected `movups`/`addps`** → not
  vectorized (aliasing / reduction / `-march`).
- **lots of `mov [rsp+..], reg` / `mov reg, [rsp+..]`** → register spills
  (too many live values, over-unroll, big struct by value).
- **the same `mov reg, [mem]` every iteration** → a value that could be
  hoisted isn't (aliasing).
- **a long dependency chain** (each instruction uses the previous result) →
  latency-bound (folder 31 lesson 04). `llvm-mca` quantifies it.
- **`cmov` on a predictable branch** / a real branch on an unpredictable one
  → mismatch (folder 31 lesson 08).

---

## Debugging

- **Crash backtrace** — a core dump / `perf` gives you `func+0x2a`.
  `objdump -dS` or `addr2line -e binary 0x...` maps the offset to the exact
  instruction and (with `-g`) the source line + inlined frames.
- **`<optimized out>` in gdb** — the variable is only in a register that got
  reused. Read the asm to see what's actually in which register at that PC.
- **"impossible" behaviour** — UB the optimizer exploited (folder 25/33): the
  asm shows the check you wrote is *gone* (e.g. `-ffinite-math-only` deleted
  an `isnan`; a null-check removed after a deref). The asm doesn't lie.
- **Data race / memory-order bug** (folder 27) — is that store a plain `mov`
  or `xchg`/`lock`? Is there an `mfence`? The asm shows the actual ordering.

---

## What you need to be able to do (this folder's goal)

1. Recognize the **shape**: prologue/epilogue, a loop (backward branch), an
   `if` (forward branch or `cmov`), a function call, a `switch` (jump table),
   a virtual call (indirect through `[reg+off]`).
2. Read an **addressing mode**: `[base + index*scale + disp]`.
3. Know the ~20 **common instructions** (`mov`, `lea`, `add`, `imul`, `cmp`,
   `test`, `jXX`, `call`, `ret`, `push`/`pop`, `cmovXX`, `setXX`, `movXX`
   SIMD).
4. Know the **calling convention** enough to say "arg 1 is in `rdi` (SysV) /
   `rcx` (Win64), return in `rax`".
5. Map an instruction back to a **source line** (`objdump -dS`, `perf
   annotate`).

You do **not** need to: write asm, memorize every instruction, know
micro-op encodings, or read the linker's relocation tables.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — reading `-O0` asm
Literal, verbose, every variable on the stack. Meaningless for judging
codegen. Always `-O2`/`-O3` — that's what ships (folder 33 lesson 01).

### Trap 2 — panicking at a `call`
`call memcpy` / `call memset` where you wrote a loop is the compiler helping.
`call your_tiny_hot_helper` is the problem. Know which.

### Trap 3 — assuming the ABI
godbolt defaults to Linux (System V: args `rdi, rsi, ...`). This repo's MinGW
is Windows x64 (args `rcx, rdx, r8, r9` + 32-byte shadow space). Same
instructions, different arg registers (lesson 07).

### Trap 4 — `long` is 8 bytes
On this MinGW toolchain `long` is **4 bytes** (Windows LLP64). Asm shows
`DWORD PTR` / `*4` where you expected `QWORD`/`*8`. Use `int64_t` in code and
in your mental model.

### Trap 5 — reading a stale build
Edited the source, forgot to rebuild. Or read a different `-O`. Script it
(`./build.ps1 asm <file>`).

### Trap 6 — over-reading
You don't need to understand every `.cfi_*` directive or every `nop` padding.
Focus on the loop body and the call sites.

---

## > **HFT relevance**

> - **Every hot function ships with an asm review** — the loop body, the call
>   sites, the branches. Written intent → checklist (folder 33 lesson 15).
> - **Snapshot it in the PR** (godbolt permalink / `.s` diff) so a refactor
>   or GCC bump that regresses it is visible in review.
> - **`perf annotate` is the production triage tool** — a p99 spike points at
>   the instruction; you need to read it to know if it's a `div`, a missing
>   load, a mispredicted `jne`, or a spill.
> - **The asm is ground truth** — when a hint "should" have worked or a
>   result is "impossible", the assembly tells you what actually happens.

---

## Hands-on

```bash
# the fastest loop of your life: read a function's asm
./build.ps1 asm 34-ASSEMBLY/examples/02_reading_loops.cpp

# map an instruction to a source line:
g++ -std=c++20 -O2 -g -c 34-ASSEMBLY/examples/02_reading_loops.cpp -o /tmp/x.o
objdump -dS --demangle /tmp/x.o | less        # asm interleaved with C++

# a crash address -> source:
addr2line -e ./your_binary -f -C 0x401a2c

# godbolt: paste any function, set -O2 -std=c++20, click a source line
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "I need to write assembly" | you need to **read** it — for verification/debug |
| "read `-O0` to understand the code" | read `-O2`/`-O3` — that's what runs |
| "any `call` = slow" | `call memcpy` is fine; `call my_helper` isn't |
| "godbolt shows my ABI" | Linux/SysV by default; this repo is Windows x64 |
| "`long` is 64-bit" | 32-bit on MinGW/Windows (LLP64) — asm shows it |
| "I must understand every line" | focus on the loop body + call sites |

---

## Exercises

1. Tumne ek hot `parse_price()` ko `__restrict` + `inline` banaya. Asm review
   mein kya-kya confirm karoge (3 cheezein)?

   <details><summary>Answer</summary>

   (1) **No `call` in the caller's hot loop** where `parse_price` is used —
   `inline` (+ visible definition / `-flto`) should have folded it in; grep
   the caller's asm for its mangled name. (2) **The parse loop is vectorized
   or at least reload-free** — `__restrict` on the byte-buffer pointer means
   the compiler isn't reloading a length/scale value every iteration
   (`-fopt-info-vec-missed` should not say "possible aliasing"). (3) **No
   surprise `call strtod`/`__divdi3`/`idiv`** — decimal parsing done with
   integer multiply-add, not a library call or a runtime division. Also check
   constants (tick size, field widths) are immediates, not memory loads.
   </details>

2. `perf annotate` ek hot loop dikhata hai jismein ek instruction pe 40%
   samples hain: `divsd xmm0, xmm1`. Diagnosis + fix.

   <details><summary>Answer</summary>

   A **floating-point division** (`divsd`, ~13-20 cycles, poorly pipelined)
   in the hot loop is eating 40% of the time. Find the `/` in the source
   (`objdump -dS` / `perf annotate` shows the line). Fixes: (1) if the
   divisor is **loop-invariant**, hoist `double inv = 1.0 / d;` and multiply
   (folder 33 lesson 04/09 — also removes an aliasing reload if `d` was via a
   pointer). (2) if it's `x / constant`, the compiler should already do
   reciprocal-multiply — check it's actually `constexpr`, not a `const` runtime
   value. (3) if you need many divisions by the same runtime `d`, precompute
   `1/d` once. (4) if it's genuinely per-element division by per-element
   values, consider `rcpps` + one Newton-Raphson step (approximate, ~2-3x
   faster, check precision). Re-`annotate` to confirm `divsd` is gone.
   </details>

3. gdb `<optimized out>` dikha raha ek variable ke liye jise tumhe crash ke
   waqt dekhna hai. Asm reading se kaise madad milegi?

   <details><summary>Answer</summary>

   The variable was kept only in a register (no stack slot). Read the asm
   around the crash PC (`disassemble` in gdb, or `objdump -dS`): trace
   backwards from the faulting instruction to see which register holds that
   value at that point — the compiler's DWARF location info was lossy, but
   the instructions aren't. `info registers` at the crash gives you the
   register contents; match the register to the variable by reading how it
   was computed (which `mov`/`lea`/`add` produced it, from which source
   line via `-g`). Often you can reconstruct the value even though gdb won't
   name it. If you control the repro, rebuild that TU at `-Og` or `-O1` for a
   cleaner debug session.
   </details>

---

## Interview questions

1. Three reasons to read assembly (verification / optimization / debugging).
2. Two folder-33 techniques and what you'd check in the asm to confirm each.
3. What "why is this loop slow?" looks for in the asm (name 4 signs).
4. What you do NOT need to know to read asm usefully.
5. `objdump -dS` / `perf annotate` — how they connect asm to source.
6. Why reading `-O0` asm is a mistake.
7. The ABI difference between godbolt's default and this repo's toolchain.

---

## Next
→ [`02-x86-64-basics.md`](02-x86-64-basics.md)
