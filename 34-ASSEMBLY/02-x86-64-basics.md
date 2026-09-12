# 02 — x86-64 basics: registers, sizes, calling convention overview

## Prerequisites
- `31-CPU-ARCHITECTURE/02-registers.md` (register file, renaming)
- `01-why-read-assembly.md`

## Yeh topic abhi kyun
Assembly padhne ke liye aapko x86-64 ki **shape** pata honi chahiye: kaunse
registers hain, unke chhote naam kya (`rax`/`eax`/`ax`/`al`), aur arguments
kis register mein aate hain. Yeh 15 minute ka foundation hai jiske baad
`./build.ps1 asm` ka output "text" nahi "code" lagne lagta.

---

## General-purpose registers (16, each 64-bit)

```
64-bit   32-bit  16-bit  8-bit    typical role (SysV / Win64)
------   ------  ------  -----    ----------------------------
rax      eax     ax      al       return value; also rdx:rax for 128-bit results
rbx      ebx     bx      bl       callee-saved
rcx      ecx     cx      cl       arg 4 (SysV) / arg 1 (Win64); shift counts
rdx      edx     dx      dl       arg 3 (SysV) / arg 2 (Win64); rdx:rax
rsi      esi     si      sil      arg 2 (SysV)   / callee-saved (Win64)
rdi      edi     di      dil      arg 1 (SysV)   / callee-saved (Win64)
rbp      ebp     bp      bpl      frame pointer (often omitted at -O2)
rsp      esp     sp      spl      STACK POINTER (never a general value)
r8       r8d     r8w     r8b      arg 5 (SysV) / arg 3 (Win64)
r9       r9d     r9w     r9b      arg 6 (SysV) / arg 4 (Win64)
r10      r10d    r10w    r10b     scratch / static chain
r11      r11d    r11w    r11b     scratch
r12-r15  ...                     callee-saved
rip                              instruction pointer (not directly writable; RIP-relative addressing)
rflags                           status flags: ZF CF SF OF PF ... (set by cmp/test/arith)
```

**Sub-register rule (important):** writing a **32-bit** sub-register
(`mov eax, 5`) **zeroes the upper 32 bits** of the 64-bit register. Writing
`ax` or `al` does **not** zero the rest (partial-register write → can cause a
stall). So `mov eax, edi` = "zero-extend edi into rax"; `xor eax, eax` = the
idiomatic "rax = 0" (also breaks a false dependency).

---

## SIMD / FP registers

```
xmm0-xmm15   128-bit   SSE / scalar float+double (xmm0 low 64 = a double)
ymm0-ymm15   256-bit   AVX / AVX2   (xmm is the low half of ymm)
zmm0-zmm31   512-bit   AVX-512      (+ k0-k7 mask registers)
mxcsr                  SIMD control/status: rounding mode, FTZ/DAZ, exception flags
```
Scalar `float`/`double` live in `xmm` (`addss` = scalar single, `addsd` =
scalar double). Packed ops: `addps` (4 floats), `addpd` (2 doubles),
`vaddps ymm` (8 floats). Seeing `ymm`/`vfmadd...ps` = vectorized (lesson 09).

---

## Instruction operand order

- **Intel syntax** (this repo's `./build.ps1 asm`, godbolt option, MSVC):
  `op  dest, src`  → `mov rax, rbx` means `rax = rbx`.
- **AT&T syntax** (GCC/Clang default, `objdump` default): `op  src, dest` with
  `%` on registers and `$` on immediates → `mov %rbx, %rax` means `rax = rbx`.

Same machine code, reversed reading order. Lesson 03 covers the switch.
This lesson uses **Intel**.

---

## Operand sizes (the suffix / `PTR` keyword)

```asm
mov  al,  [rdi]        ; 1 byte  (BYTE)
mov  ax,  [rdi]        ; 2 bytes (WORD)
mov  eax, [rdi]        ; 4 bytes (DWORD)   <- also zeroes rax's top 32
mov  rax, [rdi]        ; 8 bytes (QWORD)
movzx eax, BYTE PTR [rdi]   ; load 1 byte, zero-extend to 32
movsx rax, DWORD PTR [rdi]  ; load 4 bytes, sign-extend to 64
```
In Intel syntax an ambiguous memory operand gets a size: `mov DWORD PTR
[rdi], 0`. `movzx`/`movsx` = zero/sign-extend while loading (a `bool`/`char`/
`short` field becomes an `int`).

---

## Calling convention — the one-slide version

| | System V (Linux/Mac) | Windows x64 (MinGW, this repo) |
|---|---|---|
| int/ptr args | `rdi, rsi, rdx, rcx, r8, r9`, then stack | `rcx, rdx, r8, r9`, then stack |
| float args | `xmm0..xmm7` | `xmm0..xmm3` (and it "uses up" the int slot too) |
| return | `rax` (`rdx:rax` for 128-bit), `xmm0` for float | same |
| callee-saved | `rbx, rbp, r12-r15` | `rbx, rbp, rdi, rsi, r12-r15` |
| caller-saved (scratch) | `rax, rcx, rdx, rsi, rdi, r8-r11` | `rax, rcx, rdx, r8-r11` |
| stack alignment at `call` | 16-byte | 16-byte |
| extra | red zone (128 B below `rsp`) | **32-byte shadow space** the caller reserves for the callee |

**Reading tip:** the first thing a function does with `rdi` (SysV) / `rcx`
(Win64) is operate on argument 1. `this` is argument 1 for member functions.
Lesson 07 has the full detail.

---

## rflags — what `cmp`/`test` set

`cmp a, b` computes `a - b` and sets flags (doesn't store the result).
`test a, b` computes `a & b` and sets flags. Then a `jXX` / `cmovXX` /
`setXX` reads them:

| Flag | Set when | Jumps that read it |
|---|---|---|
| **ZF** (zero) | result == 0 | `je`/`jz`, `jne`/`jnz` |
| **SF** (sign) | result's top bit set (negative) | `js`, `jns` |
| **CF** (carry) | unsigned overflow / borrow | `jb`/`jc`, `jae`/`jnc` |
| **OF** (overflow) | signed overflow | `jo`, `jno` |
| combined | — | signed: `jl`/`jle`/`jg`/`jge`; unsigned: `jb`/`jbe`/`ja`/`jae` |

`jl` vs `jb`: `jl` (less) reads SF/OF = **signed**; `jb` (below) reads CF =
**unsigned**. Seeing `jb`/`ja` → the comparison is unsigned (`size_t`, a
pointer diff, a `& mask`).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — thinking `rsp` is a general register
`rsp` is the stack pointer. `push`/`pop`/`call`/`ret` and prologue/epilogue
move it. A `mov rax, rsp` is reading the stack pointer (e.g. to compute a
local's address), not a normal value.

### Trap 2 — `xor eax, eax` is "clever"
It's just the idiomatic "eax = 0" — smaller encoding than `mov eax, 0`, and
it breaks the false dependency on the old `rax`. Not a bug, not a trick.

### Trap 3 — ignoring the 32-bit zeroing rule
`mov eax, edi` followed by using `rax` — the top 32 bits are **zero**, not
garbage. This is how the compiler zero-extends an `int` to a pointer index.

### Trap 4 — `al`/`ax` writes
Writing `al` leaves `rax`'s other bits intact → a partial-register merge →
possible stall on older CPUs. Compilers use `movzx`/`movsx` to avoid it.

### Trap 5 — assuming SysV registers on this box
`./build.ps1 asm` here shows **Win64**: arg 1 in `rcx`, and functions often
`sub rsp, 40` (32 shadow + 8 align) even when they don't seem to need locals.

### Trap 6 — `rip`-relative addressing looks like a variable
`mov rax, QWORD PTR [rip + something]` = load a global / a constant from
`.rodata`. `lea rax, [rip + label]` = compute the address of a global/string.

---

## > **HFT relevance**

> - **Know arg 1 = `rdi` (SysV) / `rcx` (Win64)** so you can immediately see
>   "this function's first thing is dereferencing the order-book pointer".
> - **`jb`/`ja` vs `jl`/`jg`** tells you signedness — a `size_t` loop bound
>   vs an `int`; relevant for overflow reasoning and vectorization.
> - **`ymm` / `zmm` in the loop** = SIMD width actually achieved — confirm
>   `-march` did what you wanted (folder 33 lesson 12).
> - **`sub rsp, N` size** = frame size = how much stack this hot function
>   touches per call (matters for cache + `-fstack-usage` budgets).
> - **`lock` prefix / `xchg` / `mfence`** = atomics / memory ordering — the
>   asm shows the real synchronization (folder 27).

---

## Hands-on

```bash
./build.ps1 asm 34-ASSEMBLY/examples/01_simple_functions.s   # (it's a .s -- just cat it)
cat 34-ASSEMBLY/examples/01_simple_functions.s

# your platform's arg registers -- write a 3-arg function, read the asm:
echo 'long f(long a,long b,long c){return a*b-c;}' | g++ -O2 -S -masm=intel -xc++ - -o -
#   MinGW: imul rcx, rdx ; sub rcx, r8 ; mov rax, rcx    (args in rcx/rdx/r8)

# sub-register zeroing:
echo 'unsigned long g(int x){return (unsigned)x;}' | g++ -O2 -S -masm=intel -xc++ - -o -
#   `mov eax, ecx` -- writing eax zeroes rax's top 32 -> that IS the zero-extend
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`rsp` is a normal register" | it's the stack pointer; push/pop/call move it |
| "`xor eax,eax` is a trick" | idiomatic "= 0", smaller + breaks false dep |
| "writing `eax` leaves `rax` top garbage" | writing a 32-bit sub-reg **zeroes** the top 32 |
| "godbolt shows my calling convention" | SysV by default; this repo is Win64 |
| "`jl` and `jb` are the same" | `jl` signed (SF/OF), `jb` unsigned (CF) |
| "`[rip + x]` is weird" | RIP-relative: a global / constant load or address |

---

## Exercises

1. `mov eax, DWORD PTR [rcx + rdx*4]` — kaunsi C++ line (Win64 ABI), aur
   `eax` ke top 32 bits mein kya?

   <details><summary>Answer</summary>

   Win64: arg 1 = `rcx`, arg 2 = `rdx`. So this is `arr[i]` where `arr` is
   a pointer (arg 1, `rcx`) and `i` is an index (arg 2, `rdx`), element size
   4 → `int arr[]`, `int i`. Something like `int f(const int* arr, long i) {
   return arr[i]; }`. Because it writes `eax` (the 32-bit sub-register), the
   **top 32 bits of `rax` are zeroed** — so the loaded `int` is
   zero-extended into `rax`, ready to be used as an unsigned 64-bit value or
   a pointer offset without further work.
   </details>

2. Asm mein `test rax, rax` phir `je .L4` dikha. `cmp rax, 0` ki jagah `test`
   kyun, aur yeh kya check kar raha?

   <details><summary>Answer</summary>

   `test rax, rax` computes `rax & rax` (= `rax`) purely to set flags —
   **ZF is set iff `rax == 0`**, SF iff the top bit is set. It's the
   idiomatic "is this zero / is this null" check, preferred over `cmp rax, 0`
   because `test reg, reg` has a smaller encoding and no immediate. So `test
   rax, rax ; je .L4` = "if (rax == 0) goto .L4" — typically a null-pointer
   check, an `if (!x)`, an empty-container check, or a loop's `n <= 0` guard
   (paired with `jle`/`js`).
   </details>

3. Do functions: ek `jbe .L2` use karta hai, doosra `jle .L2` (same position,
   loop guard). Source mein kya farak?

   <details><summary>Answer</summary>

   `jbe` (jump if below-or-equal) reads **CF/ZF** → the comparison is
   **unsigned**. `jle` (jump if less-or-equal) reads **SF/OF/ZF** → **signed**.
   So the first function's loop bound / index type is unsigned (`size_t`,
   `unsigned`, `uint32_t`) — e.g. `for (size_t i = 0; i <= n; ++i)` or a
   `if (x <= limit)` with unsigned `x`. The second's is signed (`int`,
   `long`, `ssize_t`). This matters: signed overflow is UB (the compiler
   assumes it can't happen, which helps it prove trip counts and vectorize),
   unsigned wraps defined (sometimes making the compiler more conservative).
   The signedness is visible right there in the mnemonic.
   </details>

---

## Interview questions

1. The 16 GP registers — which holds the return value, which is the stack pointer, name 3 callee-saved.
2. The sub-register zeroing rule (`mov eax, ...` zeroes rax's top 32).
3. `xmm` vs `ymm` vs `zmm`, and `addss` vs `addps`.
4. Intel vs AT&T operand order.
5. `cmp` vs `test` — what each computes, what flags.
6. `jl` vs `jb` — signed vs unsigned, and what that tells you about the source.
7. SysV vs Win64 argument registers.

---

## Next
→ [`03-att-vs-intel-syntax.md`](03-att-vs-intel-syntax.md)
