# 04 — Common instructions: `mov`, `lea`, `add`, `cmp`, `jmp`, `call`, `ret`, `test`

## Prerequisites
- `02-x86-64-basics.md`, `03-att-vs-intel-syntax.md`

## Yeh topic abhi kyun
~20 instructions covers 95% of compiler-generated integer/control code. Inhe
pehchano to koi bhi `-O2` function padha ja sakta hai. Yeh lesson reference
hai — ise skim karo, phir examples pe apply karo. (Intel syntax.)

---

## Data movement

| Instruction | Meaning |
|---|---|
| `mov dst, src` | `dst = src` (reg↔reg, reg↔mem, imm→reg/mem). **Not** mem→mem. |
| `movzx dst, src` | load smaller, **zero**-extend (`uint8`/`bool` → `int`) |
| `movsx dst, src` / `movsxd` | load smaller, **sign**-extend (`int8`/`int` → `int64`) |
| `lea dst, [addr]` | `dst = address` (compute base+idx*scale+disp; **no memory access**, no flags) — heavily reused for plain arithmetic |
| `push src` / `pop dst` | `rsp -= 8; [rsp] = src` / `dst = [rsp]; rsp += 8` |
| `xchg a, b` | swap; on memory it's **implicitly `lock`ed** (atomic — folder 27) |
| `cmov`XX `dst, src` | `if (cond) dst = src` — **branchless** conditional move |
| `movbe dst, src` | load/store with byte-swap (big-endian wire — needs `-mmovbe`) |

`lea` is the workhorse: `lea rax, [rdi + rsi*4 + 8]` computes an address OR
`arr[i]` element address OR just `a + b*4 + 8` as arithmetic (3-operand add
without touching flags).

---

## Integer arithmetic / logic

| Instruction | Meaning | Flags |
|---|---|---|
| `add` / `sub` | `dst += src` / `dst -= src` | ZF SF CF OF |
| `inc` / `dec` | `dst += 1` / `-= 1` | (not CF) |
| `neg` | `dst = -dst` (two's complement) | |
| `imul` | signed multiply (`imul dst, src` 2-op, or `imul src` → `rdx:rax`) | OF CF |
| `mul` | unsigned multiply → `rdx:rax` | |
| `idiv` / `div` | signed/unsigned divide `rdx:rax / src` → quotient `rax`, remainder `rdx` — **slow (~20-40+ cyc), not pipelined** | |
| `and` / `or` / `xor` / `not` | bitwise | ZF SF (CF/OF cleared) |
| `shl` / `shr` / `sar` | shift left / logical right / arithmetic right | CF |
| `rol` / `ror` | rotate | |
| `bt` / `bts` / `btr` | bit test / set / reset | CF |
| `bsf` / `bsr` / `tzcnt` / `lzcnt` | find lowest/highest set bit | |
| `popcnt` | count set bits (needs `-mpopcnt`) | |

`xor eax, eax` = "eax = 0" (idiom). `test rax, rax` = `and` for flags only
(doesn't store). `shl rax, 3` = `* 8`; `sar rax, 2` = signed `/ 4`.

---

## Comparison & flags

| Instruction | Meaning |
|---|---|
| `cmp a, b` | compute `a - b`, set flags, **discard result** |
| `test a, b` | compute `a & b`, set flags, discard |
| `set`XX `r8` | `r8 = (cond) ? 1 : 0` (e.g. `sete al` after `cmp` → equality as 0/1) |
| `cmov`XX `dst, src` | conditional move (branchless select) |

The `XX` condition codes: `e`/`ne` (== / !=), `l`/`le`/`g`/`ge` (signed
< ≤ > ≥), `b`/`be`/`a`/`ae` (unsigned), `s`/`ns` (sign), `z`/`nz` (zero, same
as e/ne), `o`/`no` (overflow).

---

## Control flow

| Instruction | Meaning |
|---|---|
| `jmp target` | unconditional jump. `jmp [reg]` / `jmp [rip+table+rax*8]` = **indirect** (jump table / tail-called virtual) |
| `j`XX `target` | conditional jump (reads flags). **backward** = loop; **forward** = if/break/return-early |
| `call target` | `push (return address); jmp target` — a function call |
| `call [reg+off]` | **indirect call** — vtable dispatch / function pointer / `std::function` |
| `ret` | `pop rip` — return (uses the address `call` pushed) |
| `leave` | `mov rsp, rbp; pop rbp` — epilogue shortcut (frame-pointer builds) |
| `endbr64` | CET landing pad (start of an indirect-call target; ignore it) |
| `nop` / `nopw`/`.p2align` padding | alignment filler — ignore |
| `ud2` | "undefined instruction" — the compiler puts it after `__builtin_unreachable()` / a `noreturn` call, as a trap |

**Reading control flow:** find the labels (`.L3:`), then the `jXX` that
targets them. A `jne .L3` where `.L3` is *above* = the loop back-edge. A
`je .Lreturn` where `.Lreturn` is *below* = an early exit.

---

## String / block ops (you'll see these where you wrote a loop)

| Instruction | Meaning |
|---|---|
| `rep movsb` / `rep movsq` | block copy `rcx` units from `[rsi]` to `[rdi]` — the compiler emits this (or `call memcpy`) for a copy loop |
| `rep stosb` / `rep stosq` | block fill from `al`/`rax` — for `memset` / array-zero |
| `rep cmpsb` / `scasb` | block compare / scan — rarely from a compiler |

Seeing `rep movsb` / `call memcpy` where you wrote `for (i) dst[i] = src[i]`
is the compiler recognizing the idiom (folder 33 lesson 04, 14).

---

## The ~20 you must know cold

```
mov  lea  movzx movsx           ; data + address
add  sub  imul  and  or  xor  shl  shr  sar   ; arithmetic/logic
cmp  test                        ; set flags
jmp  je  jne  jl  jle  jg  jge  jb  ja        ; branches
call ret  push pop               ; calls / stack
cmovXX setXX                     ; branchless
```
Plus recognize (don't need to trace): `div`/`idiv` (slow!), `rep movs`/`stos`
(block op), `lock`/`xchg`/`mfence` (atomics), `xmm`/`ymm` ops (SIMD, lesson 09).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — reading `lea` as a memory load
`lea` **never** touches memory. `lea rax, [rdi+8]` = `rax = rdi + 8`. `mov
rax, [rdi+8]` = `rax = *(rdi + 8)`. The `[]` is the address expression, not a
dereference, for `lea`.

### Trap 2 — `cmp`/`test` "storing" a result
They only set flags. The next `jXX`/`setXX`/`cmovXX` uses those flags.
A lone `cmp` with no consumer = dead (compiler bug, or you misread).

### Trap 3 — `xor reg, reg` looks like it does bitwise work
It zeroes the register (idiom). Same for `sub reg, reg`.

### Trap 4 — missing that a `call` is indirect
`call foo` = direct (resolved). `call [rax]` / `call QWORD PTR [rax+16]` =
**indirect** — vtable / function pointer / `std::function` — not inlined,
BTB-predicted (folder 33 lesson 07).

### Trap 5 — `jmp [reg]` panic
It's usually a **jump table** (`switch`) or a tail-called indirect. Look for
`jmp [rip + .L_table + rax*8]` nearby — that's a dense `switch`.

### Trap 6 — `idiv` in the loop not registering as "the problem"
`idiv` is ~20-40 cycles and not pipelined. One in a hot loop dominates.
It only appears for **non-constant** divisors (lesson 08 puzzle, folder 33
lesson 04).

---

## > **HFT relevance**

> - **Scan the hot loop for `idiv`/`div`** — the single most common
>   "why is this slow". Make divisors `constexpr`.
> - **`call` in the loop** — inline / `-flto` it (folder 33 lesson 3/10).
> - **`call [reg]`** — virtual/`std::function` dispatch; kill it (folder 33
>   lesson 07).
> - **`rep movsb` / `call memcpy`** where you wrote a loop — fine, but know
>   your libc's threshold for NT stores (folder 32 lesson 12).
> - **`lock`/`xchg`/`mfence`** — the real atomic/ordering cost (folder 27) —
>   a `seq_cst` store is an `xchg` or `mov`+`mfence`; a `relaxed` is a plain
>   `mov`.

---

## Hands-on

```bash
./build.ps1 asm 34-ASSEMBLY/examples/02_reading_loops.cpp
#   find: the backward jXX (loop), a forward je (early return in find_first),
#   a cmovg (if-converted sum_if_positive), a call (or not)

# what does an atomic compile to?
echo '#include <atomic>
void s(std::atomic<long>& a){ a.store(1, std::memory_order_seq_cst); }
void r(std::atomic<long>& a){ a.store(1, std::memory_order_relaxed); }' \
 | g++ -O2 -S -masm=intel -std=c++20 -xc++ - -o -
#   seq_cst store -> `xchg` (or mov+mfence) ; relaxed -> plain `mov`
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`lea` loads from memory" | it computes an address; no memory access, no flags |
| "`cmp` produces a value" | only flags; the next `jXX`/`setXX` uses them |
| "`xor reg,reg` does bit work" | idiomatic zero |
| "`call foo` vs `call [rax]` are the same" | direct (resolved) vs indirect (vtable/fn-ptr) |
| "`jmp [reg]` is scary" | usually a `switch` jump table |
| "`idiv` is just another instruction" | ~20-40 cyc, not pipelined; dominates a hot loop |

---

## Exercises

1. `lea rax, [rax + rax*2]` phir `shl rax, 3` — final value `rax` ke terms mein?

   <details><summary>Answer</summary>

   `lea rax, [rax + rax*2]` → `rax = rax + rax*2 = 3*rax`. Then `shl rax, 3`
   → `rax = (3*rax) << 3 = 3*rax*8 = 24*rax`. So the pair computes `x * 24`
   using an address-generation `lea` (~1 cyc) + a shift (~1 cyc) instead of
   an `imul rax, rax, 24` (~3 cyc). Source: `long f(long x){ return x*24; }`.
   Compilers decompose small constant multiplies into `lea`/`shl`/`add`
   chains when it's cheaper than `imul`.
   </details>

2. Ek function ke end mein: `cmp rbx, r12` / `jne .L4` jahan `.L4` upar hai,
   phir `.L4` ke baad `add rbx, 8`. Yeh kaunsa loop shape hai?

   <details><summary>Answer</summary>

   A **pointer-increment loop with the back-edge at the bottom** (a
   `do-while`-shaped or rotated `for`). `.L4:` is the loop body start
   (above). At the bottom: `add rbx, 8` (advance a pointer by one 8-byte
   element — so iterating over `long`/`double`/`ptr`), `cmp rbx, r12`
   (compare against the end pointer held in the callee-saved `r12`), `jne
   .L4` (repeat if not at the end). This is the compiler's induction-variable
   elimination: no counter `i`, just `current_ptr != end_ptr`. `rbx` and
   `r12` being callee-saved means they're live across the whole loop (and
   were `push`ed in the prologue).
   </details>

3. Asm: `mov rax, QWORD PTR [rdi]` / `call QWORD PTR [rax + 24]`. Do lines.
   Kya, aur performance-wise kya sochoge?

   <details><summary>Answer</summary>

   A **virtual call**. `[rdi]` (arg 1 — the object pointer, or `this`) loads
   the **vptr** (vtables live at offset 0). `[rax + 24]` is the 4th 8-byte
   slot of the vtable → `call` it. Cost: 2 **dependent** loads (vptr, then
   the slot — the second can't start until the first finishes) + an
   **indirect call** (BTB-predicted; mispredicts if the concrete type varies
   across calls) + the callee is **not inlined** so no cross-call
   optimization. If this is in a hot loop over a heterogeneous container,
   it's the folder-32-lesson-10 anti-pattern — consider `variant`+`visit`,
   CRTP, a tag `switch`, `final` + `-flto` for devirt, or batching by type
   (folder 33 lesson 07).
   </details>

---

## Interview questions

1. `lea` vs `mov [..]` — the key difference.
2. `cmp` and `test` — what they compute, what they set, who consumes it.
3. `xor reg, reg` — why compilers use it.
4. Direct `call foo` vs indirect `call [reg+off]` — what each implies.
5. `jmp [rip + table + idx*8]` — what construct.
6. `rep movsb` / `call memcpy` in place of your loop — what happened.
7. `idiv` in a hot loop — why it matters, what it implies about the source.

---

## Next
→ [`05-addressing-modes.md`](05-addressing-modes.md)
