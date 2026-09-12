# 03 — AT&T vs Intel syntax

## Prerequisites
- `02-x86-64-basics.md`

## Yeh topic abhi kyun
Same machine code do tareeke se likha/dikhaya jaata hai. Aapko dono padhni
aani chahiye kyunki aap dono se milenge: **Intel** (godbolt option, MSVC,
`objdump -M intel`, is repo ka `./build.ps1 asm`) aur **AT&T** (GCC/Clang ka
default `-S`, `objdump` ka default, `gdb` ka default, `perf` output). Fark
mechanical hai — 5 rules.

---

## The 5 differences

| | AT&T | Intel |
|---|---|---|
| **operand order** | `op src, dest` | `op dest, src` |
| **register prefix** | `%rax` | `rax` |
| **immediate prefix** | `$5`, `$0x10` | `5`, `0x10` |
| **memory** | `disp(base, index, scale)` → `8(%rdi,%rsi,4)` | `[base + index*scale + disp]` → `[rdi + rsi*4 + 8]` |
| **size suffix** | on the mnemonic: `movq`, `movl`, `movw`, `movb` | on the operand: `mov QWORD PTR`, `DWORD PTR`, ... |

Everything else (mnemonics, flags, jump conditions) is the same.

---

## Side by side

```
C:  rax = rbx
AT&T:   movq  %rbx, %rax          Intel:  mov  rax, rbx

C:  rax = 5
AT&T:   movq  $5, %rax            Intel:  mov  rax, 5

C:  eax = arr[i]   (arr in rdi, i in rsi, int elements)
AT&T:   movl  (%rdi,%rsi,4), %eax Intel:  mov  eax, DWORD PTR [rdi + rsi*4]

C:  rax = a + b*2 + 8
AT&T:   leaq  8(%rdi,%rsi,2), %rax Intel: lea  rax, [rdi + rsi*2 + 8]

C:  *p += 1   (p in rdi)
AT&T:   addl  $1, (%rdi)          Intel:  add  DWORD PTR [rdi], 1

C:  if (rax == 0) goto L
AT&T:   testq %rax, %rax          Intel:  test rax, rax
        je    L                            je   L
```

---

## Memory operand: the general form

Both mean **`base + index*scale + displacement`**, scale ∈ {1,2,4,8}:

```
AT&T:   disp(base, index, scale)
Intel:  [base + index*scale + disp]

  4(%rax)              [rax + 4]                  ; struct field at offset 4
  (%rax,%rcx,8)        [rax + rcx*8]              ; long array: base + i*8
  16(%rax,%rcx,4)      [rax + rcx*4 + 16]         ; arr[i] where arr is at offset 16
  (,%rcx,4)            [rcx*4]                     ; index*4, no base (rare)
  symbol(%rip)         [rip + symbol]             ; RIP-relative: a global/constant
```

Any of base/index/disp can be absent. `scale` only applies to `index`.

---

## Size suffixes (AT&T only)

AT&T puts the size on the mnemonic when it's ambiguous:

| suffix | bytes | Intel `PTR` |
|---|---|---|
| `b` | 1 | `BYTE` |
| `w` | 2 | `WORD` |
| `l` | 4 | `DWORD` |
| `q` | 8 | `QWORD` |

`movl $0, (%rdi)` == `mov DWORD PTR [rdi], 0`. When a register operand makes
the size unambiguous (`mov %eax, (%rdi)` → clearly 4 bytes) the suffix is
optional and often omitted.

Extension mnemonics carry two sizes: `movzbl` = move-zero-extend byte→long
(`movzx eax, BYTE PTR [..]`), `movslq` = move-sign-extend long→quad
(`movsxd rax, DWORD PTR [..]`).

---

## Getting each

```bash
# Intel:
g++ -O2 -S -masm=intel file.cpp -o -
objdump -d -M intel --demangle a.out
./build.ps1 asm 34-ASSEMBLY/examples/<file>.cpp        # this repo -- Intel
gdb)  set disassembly-flavor intel

# AT&T (the defaults):
g++ -O2 -S file.cpp -o -
objdump -d --demangle a.out
gdb)  disassemble                                       # AT&T unless you set intel
perf annotate                                           # AT&T
```

godbolt: the assembly pane's filter toolbar has an "Intel asm syntax" toggle
(on by default).

---

## Which to use

- **Learning / reviewing your own code** → **Intel** (`dest, src` reads like
  assignment; `[...]` memory is clearer). This folder uses Intel.
- **`perf annotate` / `gdb` / random `objdump` output you didn't control** →
  you'll get **AT&T**; know the 5 rules so you're not blocked.
- **Kernel / glibc / distro sources** → AT&T (that's their convention).

Don't fight it — recognize which one you're looking at (`%`/`$` prefixes and
`op src, dest` = AT&T) and read accordingly.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — reading AT&T as Intel (operands reversed)
`mov %rbx, %rax` in AT&T is `rax = rbx`. If you read it Intel-style you'll
think `rbx = rax` — the exact opposite. The `%` prefix is your signal:
`%` → AT&T → **src first**.

### Trap 2 — `$` vs no-`$`
AT&T: `mov $5, %rax` (immediate 5) vs `mov 5, %rax` (load from **address**
5!). The `$` matters. Intel disambiguates with `[]`.

### Trap 3 — missing the size on an AT&T memory-only op
`add $1, (%rdi)` — is it 1, 4, or 8 bytes? Ambiguous without the suffix. GCC
always emits `addl`/`addq`; if a disassembler drops it, cross-check.

### Trap 4 — `objdump` default is AT&T
Forgot `-M intel` → you're reading AT&T. Add it, or adjust.

### Trap 5 — `gdb` default is AT&T
`set disassembly-flavor intel` in your `.gdbinit` if you prefer Intel.

### Trap 6 — thinking the syntaxes are different instruction sets
They're not — identical machine code, identical mnemonics, only the
*textual presentation* differs.

---

## > **HFT relevance**

> - **`perf annotate` gives you AT&T** — production triage. You must read
>   `movl (%rdi,%rax,4), %edx` fluently to find the hot load.
> - **Code review / godbolt links → Intel** — clearer for "did the loop
>   vectorize / is there a `div`".
> - Just be fast at spotting which one you have and flipping the operand
>   order in your head.

---

## Hands-on

```bash
# same function, both syntaxes:
echo 'int f(const int* a, long i){ return a[i] * 3 + 1; }' > /tmp/f.cpp
g++ -O2 -S -masm=intel /tmp/f.cpp -o -    # Intel:  lea eax, [rdx + rdx*2] ; ... [rcx + ...]
g++ -O2 -S            /tmp/f.cpp -o -    # AT&T:   leal (%rdx,%rdx,2), %eax ; ...

# force gdb to Intel:
gdb -ex 'set disassembly-flavor intel' -ex 'disassemble f' -batch ./a.out
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "AT&T and Intel are different ISAs" | same machine code; only text presentation |
| "`mov %a, %b` (AT&T) = b→a" | AT&T is src,dest → it's **a→b** |
| "`mov 5, %rax` loads the value 5" | AT&T: that's address 5; the value is `$5` |
| "objdump gives Intel" | default is AT&T; add `-M intel` |
| "gdb shows Intel" | default AT&T; `set disassembly-flavor intel` |
| "the suffix on the mnemonic is decoration" | in AT&T it's the operand size (b/w/l/q) |

---

## Exercises

1. `leaq (%rdi,%rdi,4), %rax` — Intel mein likho aur batao C++ kya.

   <details><summary>Answer</summary>

   Intel: `lea rax, [rdi + rdi*4]`. Meaning: `rax = rdi + rdi*4 = rdi * 5`.
   Source: `long f(long x) { return x * 5; }` — the compiler turned `* 5`
   into a single `lea` (address-generation unit, ~1 cycle) instead of an
   `imul` (~3 cycles). `lea` with `base == index` and `scale` gives you
   `x*(scale+1)`: scale 1→`x*2`, 2→`x*3`, 4→`x*5`, 8→`x*9`.
   </details>

2. AT&T: `cmpl $10, -4(%rbp)` / `jg .L3`. Intel form + what it does.

   <details><summary>Answer</summary>

   Intel: `cmp DWORD PTR [rbp - 4], 10` / `jg .L3`. It compares a **4-byte
   local variable** at `[rbp - 4]` (a stack slot, frame-pointer-relative —
   so this is a `-O0` or `-fno-omit-frame-pointer` build) against the
   immediate `10`, then **jumps if the local is (signed) greater than 10**
   (`jg` reads SF/OF/ZF). Source: something like `if (local > 10) { ... }`
   where `local` is an `int`. The `l` suffix on `cmpl` and the `$` on `$10`
   are the AT&T tells.
   </details>

3. Tum `perf annotate` output dekh rahe ho aur ek line hai:
   `mov 0x8(%rbx),%rax`. Kya ho raha, aur ye kaunsi syntax hai?

   <details><summary>Answer</summary>

   **AT&T syntax** (no `%` on... wait, there IS `%rbx`/`%rax`, and no `[]` —
   that's AT&T). `mov 0x8(%rbx), %rax` = Intel `mov rax, QWORD PTR [rbx +
   8]` = "load the 8-byte value at offset 8 from the object pointed to by
   `rbx` into `rax`". Since `rbx` is callee-saved, it's probably holding a
   long-lived pointer (a `this`, an iterator, a node pointer) across the
   loop; offset 8 is the second 8-byte field (e.g. `node->next` if `next`
   is the second member, or the second field of a struct). If this line has
   a high sample %, that load is a likely cache miss — check what `rbx`
   points at and whether it's a pointer-chase (folder 32).
   </details>

---

## Interview questions

1. The 5 differences between AT&T and Intel syntax.
2. `mov %rbx, %rax` (AT&T) — which register gets what value?
3. AT&T memory operand `disp(base,index,scale)` → the Intel form and the formula.
4. AT&T size suffixes b/w/l/q → bytes.
5. `$5` vs `5` in AT&T.
6. Which tools give which syntax by default (gcc -S, objdump, gdb, perf, godbolt).
7. Are AT&T and Intel different instruction sets? (No — presentation only.)

---

## Next
→ [`04-common-instructions.md`](04-common-instructions.md)
