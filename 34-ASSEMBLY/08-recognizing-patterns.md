# 08 — Recognizing patterns: loops, branches, calls, virtual calls

## Prerequisites
- `04-common-instructions.md`, `05-addressing-modes.md`, `06-stack-frames.md`
- `07_asm_puzzles.md` example

## Yeh topic abhi kyun
Individual instructions ke baad, **shapes** pehchanna aata chahiye — ek `if`
kaisa dikhta, ek loop, ek `switch`, ek virtual call, ek constant-folded
function. Yeh lesson catalogue hai. Isse aa jaaye to `-O2` ka koi bhi
function ~30 second mein "read" ho jaata.

---

## `if` / `if-else`

```asm
; if (a > b) { X } else { Y }
        cmp   edi, esi
        jle   .Lelse            ; !(a > b) -> Y
        <X>
        jmp   .Lend
.Lelse:
        <Y>
.Lend:
```
Or **if-converted** to branchless (simple bodies, `-O2`):
```asm
; r = (a > b) ? x : y;
        cmp   edi, esi
        mov   eax, esi_or_y
        cmovg eax, edi_or_x     ; <- no jump
```
Tell: a `cmp`/`test` followed by a **forward** `jXX` = a branch; followed by
`cmovXX`/`setXX` = if-converted (folder 31 lesson 08).

---

## Loops

```asm
; for (i = 0; i < n; ++i) body;   (rotated: guard + do-while)
        test  esi, esi
        jle   .Ldone           ; n <= 0 -> skip   (ENTRY GUARD)
        xor   eax, eax         ; i = 0  (or a pointer = base)
.Lloop:                        ; <- BODY START
        <body using [rdi + rax*k]>
        add   rax, 1           ; ++i   (or `add rdi, k` -- strength reduced pointer)
        cmp   rax, rsi
        jne   .Lloop           ; <- BACKWARD BRANCH = loop
.Ldone:
        ret
```
Tells:
- **backward `jXX`** = the loop back-edge. The label it targets = body start.
- **entry guard** (`test`/`cmp` + `jle`/`jbe` before the label) = the `n > 0`
  check hoisted out (loop rotation).
- **`add rax, 16` / `add rdi, 32`** (not 4/8) in the body = **vectorized**
  (16/32 bytes per iteration).
- **counter replaced by a pointer compare** (`cmp rdi, rend / jne`) =
  induction-variable elimination.
- **unrolled**: body has `[rdi]`, `[rdi+8]`, `[rdi+16]`, `[rdi+24]` and
  `add rdi, 32` = unrolled ×4.

Nested loop = two labels, inner `jXX` target inside the outer's range.

---

## `while` with unknown trip count

```asm
; while (*p) ++p;
.Lloop:
        cmp   BYTE PTR [rax], 0
        je    .Ldone
        add   rax, 1
        jmp   .Lloop           ; unconditional back-edge
.Ldone:
```
No entry guard hoist (can't — the count isn't known). Often the compiler
just emits `call strlen` instead.

---

## `switch`

### Dense (jump table)
```asm
        cmp   edi, 5
        ja    .Ldefault        ; out of range
        jmp   [rip + .Ltable + rdi*8]     ; <- INDIRECT jump through a table
.Ltable:
        .quad .Lcase0
        .quad .Lcase1
        ...
```
One indirect jump, O(1). `[rip + table + idx*8]` is the signature.

### Sparse (if-chain / binary tree of compares)
```asm
        cmp   edi, 100
        je    .LcaseA
        cmp   edi, 5000
        je    .LcaseB
        ...
```
For few, spread-out case values. PGO can order the hot cases first (folder 33
lesson 11).

---

## Function calls

```asm
        mov   ecx/edi, ...     ; set up arg 1  (Win64 / SysV)
        mov   edx/esi, ...     ; arg 2
        call  Foo::bar(int)    ; DIRECT -- name resolved at link
        ; result in eax/rax
```
- **`call name`** = direct call (may be a PLT stub for a shared lib →
  `call bar@plt`).
- **no `call` where you wrote one** = inlined (good for a small hot fn).
- **`jmp name`** at the end instead of `call ... ; ret` = **tail call**.
- **`call memcpy` / `call memset`** where you wrote a loop = idiom
  recognition.

---

## Virtual call / function pointer / `std::function`

```asm
        mov   rax, QWORD PTR [rdi]      ; load vptr (offset 0 of the object)
        call  QWORD PTR [rax + 16]      ; call vtable[2] -- INDIRECT
```
- **`call [reg]` / `call [reg + off]`** = indirect: vtable dispatch, a raw
  function pointer, or `std::function`'s target.
- 2 dependent loads (vptr, then slot) for a virtual; 1 for a plain fn-ptr.
- Not inlined; BTB-predicted (folder 33 lesson 07).
- `std::function` also: a check for the small-buffer vs heap target, then the
  indirect call.

---

## Constant-folded / DCE'd

```asm
t():
        mov   eax, 3628800     ; the whole function is a constant
        ret
```
Or an empty body (`ret` only, or the function is gone entirely) = the result
was unused / provably a no-op (folder 33 lesson 06, 14).

---

## Division

```asm
; x / 3  (constant) -- reciprocal multiply, NO idiv:
        movabs rdx, <magic>
        imul   rdx
        sar    ... ; + sign correction

; x / d  (runtime):
        cdq / xor edx,edx
        idiv   ecx             ; <- ~20-40 cyc, NOT pipelined
```
`idiv`/`div` in a hot loop = a non-constant divisor = a target for
optimization (folder 33 lesson 04/15).

---

## SIMD (lesson 09 detail)

```asm
.Lloop:
        movups xmm1, [rdi + rax]        ; or vmovups ymm1, ...
        mulps  xmm1, xmm2               ; 4 (or 8) floats at once
        addps  xmm0, xmm1
        add    rax, 16                  ; (or 32)
        cmp    rdx, rax
        jne    .Lloop
        ; horizontal reduce of xmm0/ymm0, then a scalar tail
```
`ps`/`pd` (packed) + `add rax, 16/32` = vectorized. `ss`/`sd` (scalar) =
not.

---

## Atomics / ordering (folder 27)

```asm
; relaxed / acquire load:      mov  rax, [rdi]
; release store:               mov  [rdi], rax
; seq_cst store:               xchg [rdi], rax     (or mov + mfence)
; fetch_add:                   lock xadd [rdi], rax
; CAS:                         lock cmpxchg [rdi], rsi
; atomic_thread_fence(seq_cst): mfence
```
A **`lock` prefix** or **`xchg` on memory** or **`mfence`** = the real
synchronization. Plain `mov` for relaxed/acquire/release on x86 (x86-TSO).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — every forward jump is an `if`
Forward jumps are also: early `return`, `break`, `continue` (to the loop
increment), the entry guard, and the "small n → scalar path" branch before a
vectorized loop. Look at the target.

### Trap 2 — `cmov` = "the compiler removed my branch, great"
Only good for **unpredictable** branches. `cmov` adds a data dependency (no
speculation past it) → slower for a **predictable** branch (folder 31 lesson
08). Match the codegen to the branch's behaviour.

### Trap 3 — `jmp [rip + table + idx*8]` panic
It's a `switch` jump table. Normal, fast.

### Trap 4 — thinking `call [reg]` is a bug
It's just indirect dispatch — sometimes intended (a plugin, a callback).
Decide if it's worth devirtualizing (folder 33 lesson 07).

### Trap 5 — reading an unrolled loop as a straight-line block
Four `[rdi]`, `[rdi+8]`, `[rdi+16]`, `[rdi+24]` + `add rdi, 32` = one
unrolled iteration, not four separate statements.

### Trap 6 — missing the scalar tail
A vectorized loop is followed by a small scalar loop for `n % width`. Don't
mistake the tail for the main loop.

---

## > **HFT relevance**

> - **30-second read of any hot function**: prologue → loop (backward jXX) →
>   body (addressing mode → data structure) → call sites (direct? indirect?
>   `idiv`? SIMD?) → epilogue. Do this on every hot path.
> - **The signatures that mean "fix this"**: `call [reg]` (devirtualize),
>   `idiv`/`__divdi3` (constant divisor), `movss` where you wanted `movups`
>   (aliasing / `-march`), `[rsp+..]` churn (spills), a `call` you expected
>   to be inlined.
> - **The signatures that mean "good"**: no `call` in the loop, `vfmadd...ps
>   ymm`, `cmov` on the genuinely-random branch, constants as immediates.
> - **Atomics**: confirm a `relaxed` is a bare `mov` and a `seq_cst` is
>   `xchg`/`mfence` — the ordering cost is right there (folder 27).

---

## Hands-on

```bash
./build.ps1 asm 34-ASSEMBLY/examples/02_reading_loops.cpp     # every loop shape
./build.ps1 asm 34-ASSEMBLY/examples/03_virtual_call_asm.cpp  # direct vs [reg] call
# read 07_asm_puzzles.md, guess each, verify on godbolt

# a switch:
echo 'int f(int x){switch(x){case 0:return 10;case 1:return 20;case 2:return 30;
      case 3:return 40;case 4:return 50;default:return -1;}}' \
 | g++ -O2 -S -masm=intel -xc++ - -o - | sed -n '/f(int)/,/ret/p'
#   -> `jmp [rip + .L4 + rdi*8]` -- jump table
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "forward jump = `if`" | also early return / break / guard / scalar-path split |
| "`cmov` is always a win" | only for unpredictable branches |
| "`jmp [table + idx*8]` is weird" | `switch` jump table |
| "`call [reg]` is a bug" | indirect dispatch — maybe intended, maybe fixable |
| "4 loads in a row = 4 statements" | one unrolled iteration |
| "the loop is the whole thing" | vectorized loop + scalar tail for `n % width` |

---

## Exercises

1. `cmp rdx, 8` / `jbe .L7` ... `.L7:` <scalar loop> ... aur upar ek `ymm`
   loop. Poori structure kya hai?

   <details><summary>Answer</summary>

   This is a **vectorized loop with a scalar fallback for small n**. `cmp
   rdx, 8 / jbe .L7` — if the element count is ≤ 8 (less than one AVX2 vector
   plus some), jump straight to the scalar path `.L7` (vectorizing 3 elements
   isn't worth the setup/peel/reduce overhead). Otherwise fall through to the
   `ymm` loop (8 floats/iter, `vaddps`/`vfmadd`), which handles `n & ~7`
   elements, then a horizontal reduction, then `.L7` also serves as the
   **remainder loop** for `n % 8`. So: guard → vector main loop → horizontal
   reduce → scalar tail (shared with the small-n path). Classic auto-vec
   output.
   </details>

2. Asm: `mov rax, [rdi]` / `mov rax, [rax]` / `mov rax, [rax]` / ... 8 baar,
   ek loop mein. Kya, aur performance?

   <details><summary>Answer</summary>

   A **pointer chase** — `p = p->next` (or `p = *p`) repeated, unrolled ×8.
   Each `mov rax, [rax]` **depends on the previous** load's result → the
   addresses form a serial chain → **zero memory-level parallelism**. If the
   nodes aren't cache-resident, each hop pays the full latency (~L2 ~12 cyc,
   ~L3 ~40, ~DRAM ~90+ — folder 32) *serially*: 8 hops = 8× that. Unrolling
   ×8 didn't help (there's nothing independent to overlap). This is the
   worst-case memory pattern (linked list / tree walk). Fix: flat array /
   arena with `uint32` index links, or a cache-conscious structure (B-tree,
   open-addressing hash) — folder 32 lesson 08.
   </details>

3. Ek function `foo` ke asm mein sirf: `xor eax, eax` / `ret`. Aur `bar` mein
   sirf `ret`. Dono ka kya matlab?

   <details><summary>Answer</summary>

   **`foo`: `xor eax, eax ; ret`** = `return 0;` (or `return false;` /
   `return nullptr;`) — a function whose entire result is the constant 0,
   either written literally, constant-folded, or DCE'd down to it (e.g. the
   body computed something unused and just returns 0). **`bar`: `ret`
   only** = a `void` function whose body has **no side effects** and was
   entirely eliminated — an empty function, a function whose only work was
   dead code / a call the compiler proved pure-and-unused, or (in a
   benchmark) a loop with no observable output that got deleted (folder 33
   lesson 14 — this is why you need `DoNotOptimize`). Both are the compiler
   telling you "this computes nothing observable".
   </details>

---

## Interview questions

1. The asm signature of a `for` loop (guard, body label, backward jXX).
2. If-converted branch vs a real branch — how to tell, and when each is right.
3. Dense `switch` vs sparse `switch` — the two asm shapes.
4. Direct call vs indirect (`call [reg+off]`) — what each implies.
5. Vectorized loop signature (packed ops + `add ptr, 16/32` + reduce + tail).
6. Pointer-chase signature and why it's slow.
7. `xor eax,eax; ret` and a lone `ret` — what they mean.

---

## Next
→ [`09-simd-assembly.md`](09-simd-assembly.md)
