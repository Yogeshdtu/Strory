# 07 — Calling conventions: System V ABI (and Windows x64)

## Prerequisites
- `06-stack-frames.md`
- `24-COMPILATION-LINKING/` (ABI, `extern "C"`)

## Yeh topic abhi kyun
Calling convention = "arguments kis register/stack slot mein, return kahan,
kaunse registers preserve karne hain". Isse jaane bina aap asm mein "arg 1"
identify nahi kar sakte, aur `extern "C"` / FFI / mixed-toolchain bugs
samajh nahi aate. Do relevant hain: **System V** (Linux/Mac, godbolt default)
aur **Windows x64** (is repo ka MinGW).

---

## System V AMD64 ABI (Linux, macOS)

### Integer / pointer arguments
`rdi, rsi, rdx, rcx, r8, r9`, then right-to-left on the stack.

### Floating-point arguments
`xmm0 .. xmm7`, then stack.

### Return value
- integer/pointer: `rax` (and `rdx:rax` for 128-bit / a small struct)
- float/double: `xmm0` (`xmm1:xmm0` for a `{double,double}`)
- large struct: caller passes a hidden pointer in `rdi` (a "return slot"),
  and the other args shift right by one → `this` for a member returning a
  big struct is in `rsi`.

### Register preservation
| Callee-saved (function must restore) | Caller-saved / scratch |
|---|---|
| `rbx, rbp, r12, r13, r14, r15`, `rsp` | `rax, rcx, rdx, rsi, rdi, r8, r9, r10, r11`, all `xmm` |

### Stack
- 16-byte aligned **at the point of `call`** (so on entry, `rsp % 16 == 8`
  because `call` pushed 8).
- **Red zone**: 128 bytes below `rsp` that a leaf function may use without
  adjusting `rsp` (signal handlers respect it). Not on Windows.

### Member functions
`this` is the **first** argument → `rdi`. So `obj.method(a, b)` →
`rdi = &obj`, `rsi = a`, `rdx = b`.

---

## Windows x64 ABI (MinGW — this repo)

### Arguments (int OR float, positionally)
Slot 1→`rcx`/`xmm0`, 2→`rdx`/`xmm1`, 3→`r8`/`xmm2`, 4→`r9`/`xmm3`, then
stack. **A float in slot 1 uses `xmm0` AND consumes the `rcx` slot** (and for
varargs, the value goes in *both*).

### Return
Same as SysV: `rax` / `xmm0`. Large struct → hidden pointer in `rcx`, args
shift.

### Register preservation
| Callee-saved | Caller-saved |
|---|---|
| `rbx, rbp, rdi, rsi, r12-r15, rsp`, `xmm6-xmm15` | `rax, rcx, rdx, r8-r11`, `xmm0-xmm5` |

Note: **`rdi`, `rsi` are callee-saved here** (caller-saved in SysV), and
`xmm6-15` are callee-saved (all `xmm` are caller-saved in SysV).

### Stack
- 16-byte aligned at `call`.
- **Shadow space (home space)**: the *caller* allocates 32 bytes above the
  return address; the callee may spill its 4 register args there. This is why
  Win64 functions that call anything do `sub rsp, 0x28` (32 + 8).
- No red zone.

### `this`
First argument → `rcx`. `obj.method(a,b)` → `rcx = &obj`, `rdx = a`, `r8 = b`.

---

## Quick reference

| | System V | Windows x64 |
|---|---|---|
| int args | rdi rsi rdx rcx r8 r9 | rcx rdx r8 r9 |
| float args | xmm0-7 | xmm0-3 (positional w/ int slots) |
| return | rax / xmm0 | rax / xmm0 |
| `this` | rdi | rcx |
| callee-saved | rbx rbp r12-15 | rbx rbp rsi rdi r12-15 xmm6-15 |
| stack extra | 128B red zone | 32B shadow space |
| large-struct return | hidden ptr in rdi | hidden ptr in rcx |

---

## Reading a call site

```asm
; SysV:  foo(x, y, &z);   x in eax-ish, computed into edi; y into esi; &z into rdx
        mov   edi, ...        ; arg 1
        mov   esi, ...        ; arg 2
        lea   rdx, [rsp+8]    ; arg 3 = &local
        call  foo
        ; result now in rax / eax

; member:  book.apply(update)
        mov   rdi, rbx        ; arg 1 = this  (&book, kept in rbx)
        mov   rsi, ...        ; arg 2 = update
        call  Book::apply
```

To find what a function does with its args: SysV → first uses of `rdi`
(`edi`), `rsi`; Win64 → `rcx`, `rdx`.

---

## `extern "C"` and name mangling

C++ mangles names (`_ZN4Book5applyE...`); C doesn't. The **calling
convention is the same** for `extern "C"` functions (SysV/Win64 as above) —
`extern "C"` only changes the *symbol name* and disables overloading. So a C
library and C++ code interoperate fine on the ABI; the mismatch people hit is
usually name mangling or a struct-layout / `enum`-size difference, not the
convention.

`__attribute__((sysv_abi))` / `((ms_abi))` force a specific convention on a
function (for interop across the two on the same binary — rare, e.g. UEFI).

---

## Varargs (`printf`-style)

- SysV: `al` holds the **number of `xmm` registers used** for float args (so
  the callee knows how many to save). You'll see `mov eax, N` (or `xor eax,
  eax` for no floats) before `call printf`.
- Win64: each vararg goes in **both** its int register and `xmm` slot;
  floats are also copied to the int register. The 32-byte shadow space plus
  more is used to spill them into a contiguous array the callee can walk.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — assuming SysV registers on this box
godbolt default = SysV (`rdi` = arg 1). This repo's `./build.ps1 asm` =
Win64 (`rcx` = arg 1). Read the right one.

### Trap 2 — forgetting `this` is arg 1
A member function's asm starts by dereferencing `rdi` (SysV) / `rcx` (Win64)
— that's `this`, and the "real" arguments are shifted by one.

### Trap 3 — large struct return
`Big f()` → the caller passes a hidden return-slot pointer as arg 0, shifting
everything. `this` for a member returning `Big` is in `rsi` (SysV). Prefer
returning by out-param or small types; NRVO/RVO (folder 18) also matter.

### Trap 4 — `xmm6-15` clobbered on Win64
They're **callee-saved** on Windows. Hand-written asm / naked functions that
use them must save/restore. (SysV: all `xmm` are scratch.)

### Trap 5 — red zone on Windows
There is none. Code (or hand asm) that writes below `rsp` without adjusting
it will be clobbered by an interrupt / a `call`.

### Trap 6 — `al` before `printf` looks weird
SysV varargs: `al` = count of `xmm` args. `xor eax, eax` = "no float
varargs". Not a bug.

---

## > **HFT relevance**

> - **Identify `this` / arg 1 instantly** — `rcx` (this box) — so you can
>   read "the matcher's inner function takes the book pointer and an update".
> - **Callee-saved `push`es** in the prologue = the long-lived pointers the
>   hot function keeps in registers across its loop (folder 34 lesson 06).
> - **Large-struct returns** add a hidden pointer + a copy — return small
>   PODs, out-params, or rely on RVO; the asm shows the hidden arg.
> - **`extern "C"` for the exchange/venue C API** — same ABI, just unmangled
>   names; the interop bugs are struct layout / `enum` width, check with
>   `static_assert` (folder 11/25).
> - **Pin the toolchain** — a compiler that changes struct-passing rules
>   (rare, but ABI breaks happen) silently corrupts FFI.

---

## Hands-on

```bash
# this box (Win64): arg 1 = rcx
echo 'long f(long a, long b, long c, long d, long e){ return a+b*c-d/e; }' \
 | g++ -O2 -S -masm=intel -xc++ - -o -
#   a=rcx b=rdx c=r8 d=r9 e=[rsp+0x28]   (5th arg on the stack, past shadow space)

# member function -- `this` is arg 1:
echo 'struct S{long v; long get(long k) const { return v + k; }};' \
 | g++ -O2 -S -masm=intel -std=c++20 -xc++ - -o -
#   S::get: uses [rcx] (this->v) + rdx (k)

# godbolt for SysV: same code, args in rdi/rsi/rdx/rcx/r8
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "arg 1 is always `rdi`" | SysV yes; Win64 (this repo) it's `rcx` |
| "`this` is separate from args" | it's argument 1 |
| "large struct return is free" | hidden pointer arg + a copy; args shift |
| "all `xmm` are scratch" | on Win64 `xmm6-15` are callee-saved |
| "`extern \"C\"` changes the calling convention" | only the symbol name / overloading |
| "the red zone is universal" | SysV only; no red zone on Windows |

---

## Exercises

1. `S::method` (member, Win64) ka asm `mov rax, [rcx + 8]` se shuru hota,
   phir `add rax, rdx` use karta. Source likho.

   <details><summary>Answer</summary>

   Win64: arg 1 = `rcx` = `this`, arg 2 = `rdx` = the first real parameter.
   `[rcx + 8]` = `this->` the member at offset 8 (the second 8-byte member,
   or the first after an 8-byte member). `add rax, rdx` adds the parameter.
   So: `struct S { <8 bytes>; long y; ...; long method(long k) const {
   return y + k; } };` — `S::method` returns `this->y + k`. The `+ 8` offset
   is the tell that `y` isn't the first member.
   </details>

2. Ek function ka asm: entry pe `mov eax, 0` phir `call printf`. Kya, aur
   kaunsi ABI?

   <details><summary>Answer</summary>

   **System V** varargs convention: before a variadic call, `al` must hold
   the **number of vector (xmm) registers used for floating-point
   arguments**. `mov eax, 0` (or `xor eax, eax`) means "**no** float/double
   varargs are being passed" — so `printf("%d %s", ...)` with only integer
   and pointer args. If it were `printf("%f", x)` you'd see `mov eax, 1`
   (one xmm arg). On Win64 you wouldn't see this — floats there go in both
   the int and xmm registers and there's no `al` count.
   </details>

3. `Big make()` (returns a 64-byte struct by value, SysV). Caller ka asm
   `make()` ko kaise call karega, aur `this` kahan hota agar yeh `Obj::make()`
   member hota?

   <details><summary>Answer</summary>

   Returning a 64-byte struct by value (too big for `rdx:rax`): the **caller
   allocates space for the result** (a local slot) and passes its address as
   a **hidden first argument in `rdi`**. `make()` writes the result through
   that pointer and returns the same pointer in `rax`. Caller: `lea rdi,
   [rsp + <slot>]` ; `call make` ; then use `[rsp + <slot>]`. If it were
   `Obj::make()` (a member), the hidden return pointer still takes `rdi`, so
   **`this` shifts to `rsi`**, and the first real parameter (if any) to
   `rdx`. This is why returning big structs by value costs a hidden pointer
   + a copy; RVO/NRVO (folder 18) elide the copy when the compiler can, but
   the ABI shape is still "write through the caller's pointer".
   </details>

---

## Interview questions

1. SysV integer arg registers, float arg registers, return register.
2. Win64 arg registers, and 2 differences from SysV (rcx-first, shadow space, rsi/rdi/xmm6-15 callee-saved).
3. Where is `this`?
4. How a large struct is returned by value.
5. Callee-saved vs caller-saved — what each means for reading a prologue.
6. `extern "C"` — does it change the calling convention?
7. SysV varargs and the `al` register.

---

## Next
→ [`08-recognizing-patterns.md`](08-recognizing-patterns.md)
