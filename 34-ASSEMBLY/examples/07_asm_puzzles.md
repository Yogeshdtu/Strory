# 07 — Assembly reading puzzles

"Yeh assembly kis C++ se aayi?" Har puzzle ka answer `<details>` mein.
Intel syntax, `-O2`, System V ABI (args: `rdi, rsi, rdx, rcx, r8, r9`;
return: `rax`/`eax`). Practice: godbolt pe apna guess likh ke verify karo.

---

## Puzzle 1

```asm
f(int):
        lea     eax, [rdi + rdi*2]
        add     eax, eax
        ret
```

<details><summary>Answer</summary>

`int f(int x) { return x * 6; }`

`lea eax, [rdi + rdi*2]` = `x + x*2` = `3x`. Then `add eax, eax` = `3x + 3x`
= `6x`. The compiler turned `* 6` into `lea` + `add` (two ~1-cycle ops)
instead of an `imul` (~3 cycles). Any small constant multiply gets this
treatment: `*3`, `*5`, `*9` are a single `lea`; `*6`, `*10`, `*12` are
`lea` + shift/add.
</details>

---

## Puzzle 2

```asm
g(int*):
        mov     eax, DWORD PTR [rdi]
        test    eax, eax
        jns     .L2
        neg     eax
.L2:
        ret
```

<details><summary>Answer</summary>

`int g(int* p) { int x = *p; return x < 0 ? -x : x; }` — i.e. `abs(*p)`.

`mov eax, [rdi]` load `*p`. `test eax, eax` sets flags from `eax`. `jns` =
"jump if not sign" (i.e. if `eax >= 0`) → skip the negate. Otherwise
`neg eax` (two's-complement negate). Note: this is the branchy form; `-O2`
often emits the branchless `abs` idiom instead (`mov edx, eax; sar edx, 31;
xor eax, edx; sub eax, edx`) — if you see *that*, it's still `abs`.
</details>

---

## Puzzle 3

```asm
h(unsigned int):
        mov     eax, edi
        shr     eax, 1
        and     eax, 1431655765          ; 0x55555555
        sub     edi, eax
        mov     eax, edi
        and     eax, 858993459           ; 0x33333333
        shr     edi, 2
        and     edi, 858993459
        add     eax, edi
        ...
        imul    eax, eax, 16843009       ; 0x01010101
        shr     eax, 24
        ret
```

<details><summary>Answer</summary>

`int h(unsigned x) { return __builtin_popcount(x); }` (population count —
number of set bits) — the classic SWAR bit-twiddling implementation, emitted
when the target has **no `popcnt` instruction** (baseline `x86-64` /
without `-mpopcnt` / `-msse4.2` / `-march=x86-64-v2`+).

The magic constants `0x55555555`, `0x33333333`, `0x0F0F0F0F`, `0x01010101`
are the popcount tell. **With `-mpopcnt`** this whole thing becomes one
`popcnt eax, edi`. Seeing the SWAR version = "add `-march` to get the
hardware instruction" (lesson 09, folder 33 lesson 12).
</details>

---

## Puzzle 4

```asm
k(int*, int):
        test    esi, esi
        jle     .L9
        lea     eax, [rsi-1]
        cmp     esi, 7
        jbe     .L10                     ; small n -> scalar path
        ...
        pxor    xmm0, xmm0
.L4:
        movdqu  xmm1, XMMWORD PTR [rdi+rax]
        paddd   xmm0, xmm1
        add     rax, 16
        cmp     rdx, rax
        jne     .L4
        ; ... horizontal add of xmm0's 4 lanes ...
.L10:
        ; scalar tail: add up the remaining n % 4 elements
        ...
```

<details><summary>Answer</summary>

`int sum(int* a, int n) { int s = 0; for (int i = 0; i < n; ++i) s += a[i];
return s; }` — **auto-vectorized** at `-O2` (GCC 12+).

Reading the structure: `test/jle` entry guard (n ≤ 0). `cmp esi, 7 / jbe` —
if n is small, jump to a scalar path (vectorizing 3 elements isn't worth the
setup). `movdqu` (unaligned 128-bit load) + `paddd` (packed add of 4×
32-bit ints) + `add rax, 16` (not 4!) → **4 ints per iteration** = SSE
vectorized. Then a horizontal reduction of the 4 lanes, then a scalar tail
loop for `n % 4`. With `-march=native` the `xmm` becomes `ymm` and `add rax,
16` becomes `32` (8 ints/iter).
</details>

---

## Puzzle 5

```asm
m(Base*):
        mov     rax, QWORD PTR [rdi]
        jmp     [QWORD PTR [rax+16]]
```

<details><summary>Answer</summary>

`long m(Base* b) { return b->virt_fn(); }` where `virt_fn` is a **virtual
function** — and this is a **tail call** of it.

`mov rax, [rdi]` — load the vptr (first 8 bytes of the object). `[rax+16]`
— the 3rd entry in the vtable (slot 0 might be the type_info/offset, or it's
the 3rd virtual). `jmp [...]` (not `call`) — because `m` does nothing after
`virt_fn()` returns except return its value, so the compiler tail-calls:
`virt_fn` will `ret` straight back to `m`'s caller. Indirect `jmp`/`call`
through `[reg+off]` = **virtual dispatch** — not inlined, BTB-predicted
(lesson 08, folder 33 lesson 07).
</details>

---

## Puzzle 6

```asm
p(long):
        movabs  rdx, -6148914691236517205    ; 0xAAAAAAAAAAAAAAAB
        mov     rax, rdi
        imul    rdx                          ; rdx:rax = rdi * magic
        mov     rax, rdx
        sar     rax, 1
        mov     rcx, rdi
        sar     rcx, 63
        sub     rax, rcx
        ret
```

<details><summary>Answer</summary>

`long p(long x) { return x / 3; }` — **division by a constant, done as a
reciprocal multiply**.

There is no `idiv` (which would be ~20-40 cycles). Instead: multiply by a
precomputed "magic number" (≈ 2⁶⁵/3), take the high 64 bits of the 128-bit
product (`rdx` after `imul`), shift, and a correction for negative operands
(`sar rcx, 63` produces 0 or -1 = the sign, then `sub`). This is what every
`x / <constant>` and `x % <constant>` compiles to. If you see a real `idiv`
in a hot loop → the divisor isn't a compile-time constant → make it one, or
use `libdivide` (lesson 06, folder 33 lesson 04).
</details>

---

## Puzzle 7

```asm
q(char*):
        mov     rax, rdi
.L2:
        cmp     BYTE PTR [rax], 0
        jne     .L3
        ...
.L3:
        add     rax, 1
        jmp     .L2
```
(simplified; real GCC vectorizes `strlen`)

<details><summary>Answer</summary>

`size_t q(const char* s) { const char* p = s; while (*p) ++p; return p - s; }`
— a hand-rolled `strlen`.

`cmp BYTE PTR [rax], 0` — compare the byte at `rax` with `\0`. `jne` —
keep going while non-zero. `add rax, 1` — `++p`. Backward `jmp .L2` — the
loop. The trip count is unknown until the sentinel is hit → **not
vectorizable in the naive form** (the real libc/GCC `strlen` uses SSE with
careful page-boundary handling). If you wrote this, GCC at `-O2` may just
replace it with `call strlen` — seeing `call strlen` where you wrote a loop
is the compiler recognizing the idiom.
</details>

---

## Puzzle 8

```asm
r(int):
        sub     rsp, 40
        mov     DWORD PTR [rsp+12], edi
        ...
        lea     rdi, [rsp+12]
        call    something(int*)
        ...
        add     rsp, 40
        ret
```

<details><summary>Answer</summary>

`int r(int x) { int local = x; something(&local); return local; }` — a
function that **takes the address of a local**.

`sub rsp, 40` — allocate a stack frame (there's a local that must live in
memory because its address escapes to `something`). `mov [rsp+12], edi` —
store the parameter into the local's slot. `lea rdi, [rsp+12]` — compute
`&local`. `call something` — pass it. `add rsp, 40 / ret` — epilogue. The
`sub rsp` / `add rsp` pair without `push rbp` = a stack frame with the
**frame pointer omitted** (`-O2` default; `-fno-omit-frame-pointer` adds
`push rbp / mov rbp, rsp`). Contrast: a leaf function with no
address-taken locals has neither (lesson 06).
</details>

---

## Puzzle 9

```asm
s(int, int):
        mov     eax, esi
        xor     edx, edx
        div     ecx                     ; (in a version where args are in different regs)
        ...
```
vs the same source with a different constant — no `div` at all. What's the
source, and why the difference?

<details><summary>Answer</summary>

`int s(int a, int b) { return a % b; }` — **runtime modulo**.

`div ecx` (or `idiv`) appears because `b` is a **runtime value** — the
compiler can't precompute a reciprocal-multiply magic number for an unknown
divisor. `xor edx, edx` clears the high dividend word before `div`. If the
source were `a % 7` (constant), you'd see the magic-multiply sequence from
puzzle 6 (plus a multiply-back-and-subtract to get the remainder) and **no
`div`**. Lesson: `div`/`idiv` in a hot loop = a non-constant divisor =
~20-40 cycles each; hoist a `libdivide` divider or restructure (folder 33
lesson 04/15).
</details>

---

## Puzzle 10

```asm
t():
        mov     eax, 3628800
        ret
```

<details><summary>Answer</summary>

`int t() { int r = 1; for (int i = 1; i <= 10; ++i) r *= i; return r; }` —
or `constexpr int t() { return factorial(10); }`, or `int t() { return
1*2*3*4*5*6*7*8*9*10; }`.

The whole loop / call has been **constant-folded** — the compiler evaluated
`10!` = 3628800 at compile time and the function is just `mov eax,
3628800 ; ret`. Any computation whose inputs are all compile-time-known and
whose result is used gets this. It's also why a benchmark loop with a known
bound and an unused (or closed-form) result compiles to nothing — you need
`DoNotOptimize` on an *opaque* input and the output (folder 33 lesson 14).
</details>

---

## Next
→ [`../lessons` continue: `08-recognizing-patterns.md`](../08-recognizing-patterns.md), then folder 35.
