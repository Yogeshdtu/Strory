# 05 — Addressing modes: immediate, register, memory, indexed, scaled

## Prerequisites
- `04-common-instructions.md`
- `09-ARRAYS/03-array-indexing.md` (`a[i] == *(a+i)`)

## Yeh topic abhi kyun
Ek instruction ka operand kahan se aata hai — ye "addressing mode" hai. x86
ka memory operand ek chhota formula hai (`base + index*scale + disp`) jo
seedha C++ ke `struct.field`, `arr[i]`, `arr[i].field` ko map karta. Isse
padhna aata to aap asm se **data structure** wapas nikaal sakte ho.

---

## The modes

| Mode | Intel form | Meaning | Source it maps to |
|---|---|---|---|
| **immediate** | `mov rax, 42` | constant baked in | a literal / folded `constexpr` |
| **register** | `mov rax, rbx` | operand is a register | a variable in a register |
| **direct / RIP-relative** | `mov rax, [rip + g_x]` | a fixed global address | a global / static / string literal / `.rodata` constant |
| **register indirect** | `mov rax, [rbx]` | `*rbx` | `*p`, `p->first_field`, `obj.first_member` |
| **base + displacement** | `mov rax, [rbx + 16]` | `*(rbx + 16)` | `p->field` at offset 16, `obj.member` |
| **base + index** | `mov rax, [rbx + rcx]` | `*(rbx + rcx)` | `p[i]` for byte-sized elements |
| **base + index*scale** | `mov rax, [rbx + rcx*8]` | `*(rbx + rcx*8)` | `arr[i]` for 8-byte elements |
| **base + index*scale + disp** | `mov rax, [rbx + rcx*8 + 16]` | `*(rbx + rcx*8 + 16)` | `arr[i].field` / `s->arr[i]` |
| **index*scale + disp** (no base) | `mov rax, [rcx*4 + table]` | `*(table + rcx*4)` | `global_table[i]` |

`scale` ∈ {1, 2, 4, 8} only (element sizes 1/2/4/8 bytes). Bigger elements →
the compiler pre-multiplies the index (`imul`/`lea`) and uses scale 1.

---

## Reading data structures back out

```asm
; struct Order { int64 id; double px; int32 qty; ... };   sizeof 24, px at +8
mov  rax, [rdi + 8]          ; -> order->px          (offset 8)
mov  eax, [rdi + 16]         ; -> order->qty         (offset 16, 4-byte -> eax)

; std::vector<Order>: rdi = &v[0], rsi = i
lea  rax, [rsi + rsi*2]      ; rax = i*3   (24 = 3*8, compiler splits)
mov  rdx, [rdi + rax*8]      ; -> v[i].id            (i*24 + 0)
mov  rcx, [rdi + rax*8 + 8]  ; -> v[i].px            (i*24 + 8)

; 2D: int m[R][C]; m at rdi, i in rsi, j in rdx
mov  rax, rsi
imul rax, rax, C             ; i*C   (C not a power of 2)
add  rax, rdx                ; i*C + j
mov  eax, [rdi + rax*4]      ; -> m[i][j]
```

**Trick:** `[base + idx*8 + 8]` in a loop where `idx` increments → you're
walking an array of ≥16-byte structs, touching the field at offset 8. The
`+ 8` disp is the field; the `*8` (or a pre-multiply) is the stride.

---

## `lea` for addressing vs arithmetic

`lea` uses the **same** memory-operand syntax but produces the *address*, not
the contents:

```asm
lea  rax, [rdi + rsi*4]      ; rax = &arr[i]   (address, for later use / passing by ref)
lea  rax, [rdi + rdi*4 + 1]  ; rax = x*5 + 1   (pure arithmetic, no array involved)
lea  rax, [rip + msg]        ; rax = &"some string literal"
```
Context tells you which: if the result is then dereferenced (`mov [rax], ...`)
it was an address; if it's added/multiplied further, it was arithmetic.

---

## Element size from the instruction

```asm
movzx eax, BYTE  PTR [rdi + rsi]        ; 1-byte element (char/uint8/bool array)
movzx eax, WORD  PTR [rdi + rsi*2]      ; 2-byte (int16/char16)
mov   eax,       [rdi + rsi*4]          ; 4-byte (int/float)   -- DWORD implied by eax
mov   rax,       [rdi + rsi*8]          ; 8-byte (int64/double/pointer)
movss xmm0,      [rdi + rsi*4]          ; 4-byte float into xmm
movsd xmm0,      [rdi + rsi*8]          ; 8-byte double into xmm
movdqu xmm0,     [rdi + rsi]            ; 16 bytes (SIMD load)
vmovdqu ymm0,    [rdi + rsi]            ; 32 bytes (AVX load)
```
The scale usually equals the element size (so `idx` is the element index).
`scale 1` with a `movdqu`/`ymm` load → the loop increments the pointer/offset
by 16/32 (vectorized — lesson 09).

---

## Conflict-stride tell (folder 32 lesson 03)

`[rdi + rax]` where `rax += 4096` (or another power-of-two ≥ the L1 critical
stride) each iteration → column access / power-of-two-stride → cache-set
thrashing. Seeing a large constant added to the index/offset every iteration
= a strided access; check it against the cache geometry.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — reading `lea [x]` as a load
`lea` computes the address expression. Only `mov`/`add`/etc. with a `[..]`
operand actually touch memory.

### Trap 2 — assuming scale = element size
Usually true, but for structs > 8 bytes the compiler pre-multiplies the index
and uses scale 1 (or a small `lea` chain). `[rdi + rax*8]` where `rax` was
just `i*3` → 24-byte elements.

### Trap 3 — `[rip + x]` is not a stack access
It's a **global** / `.rodata` constant / string / `static`. Stack is
`[rsp + ..]` or `[rbp - ..]`.

### Trap 4 — disp is always a struct field
Usually, but it's also loop-unroll offsets (`[rdi]`, `[rdi+8]`, `[rdi+16]`,
`[rdi+24]` in one iteration = unrolled x4), and negative disps are local
variables (`[rbp - 4]`).

### Trap 5 — missing a two-level indirection
`mov rax, [rdi]` then `mov rdx, [rax + 8]` = `p->q->field` — a pointer
chase (folder 32). Two dependent loads.

### Trap 6 — segment prefixes (`fs:`/`gs:`)
`mov rax, fs:[0x28]` = thread-local storage / stack canary. `gs:`/`fs:` =
TLS base. Not a normal memory access; don't try to trace the address.

---

## > **HFT relevance**

> - **Recover the struct layout from the loop** — `[rdi + rax*32 + 8]` tells
>   you the hot object is 32 bytes and you're touching the field at +8. Cross
>   check against your `offsetof` expectations and `sizeof` `static_assert`s
>   (folder 32 lesson 02).
> - **Spot the stride** — a big constant added to the offset each iteration =
>   strided/column access = a cache problem (folder 32 lesson 03/05).
> - **Two dependent `[..]` loads** = pointer chase = folder 32 lesson 08.
> - **`movdqu`/`vmov...` with pointer += 16/32** = vectorized; confirm the
>   width matches your `-march` (folder 33 lesson 12).
> - **`fs:`/`gs:`** = TLS — a `thread_local` access in the hot path (folder
>   26); consider a `Context&` instead.

---

## Hands-on

```bash
./build.ps1 asm 34-ASSEMBLY/examples/02_reading_loops.cpp | grep -nE "\[r|lea"
#   sum_matrix: `imul rax, ..., <cols>` then `[rdi + rax*4]`  -> m[i*cols + j]

echo 'struct P{long a; double b; int c;};
double f(const P* v, long i){ return v[i].b; }' \
 | g++ -O2 -S -masm=intel -std=c++20 -xc++ - -o -
#   -> lea/imul for i * sizeof(P), then movsd xmm0, [base + off + 8]  (b at +8)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`lea [x]` loads x" | it computes the address x |
| "scale = element size always" | structs > 8 B: index pre-multiplied, scale 1 |
| "`[rip + x]` is a stack access" | it's a global/`.rodata`/string |
| "disp = struct field" | also unroll offsets, negative = locals |
| "one `[..]` = one indirection" | `[[rdi]+8]` chained = pointer chase (2 loads) |
| "`fs:[..]` is normal memory" | TLS / stack canary — don't trace it |

---

## Exercises

1. `mov rax, [rdi + rsi*8]` in a loop where `rsi` goes 0,1,2,... — element
   type aur access pattern?

   <details><summary>Answer</summary>

   `base + index*8` → **8-byte elements** (`int64_t`, `double`, or a
   pointer), and `rsi` is the **element index** incrementing by 1 → a
   **sequential, contiguous** scan of an array (`rdi` = base). Cache-
   friendly: ~1 miss per 8 elements (64-byte line / 8 bytes), prefetcher
   loves it. If instead you saw `rsi` incrementing by a large constant, or a
   pre-multiply making the effective stride huge, that'd be strided/column
   access (a cache problem).
   </details>

2. `imul rdx, rdx, 40` / `mov eax, [rcx + rdx + 12]` — struct size? which
   field?

   <details><summary>Answer</summary>

   `imul rdx, rdx, 40` → `rdx = i * 40` → **`sizeof(struct) == 40`** and
   `rdx` was the element index `i`. Then `[rcx + rdx + 12]` = `base + i*40 +
   12` → accessing the field at **offset 12** within the 40-byte struct,
   loaded into `eax` (32-bit) so it's a **4-byte field** (an `int` /
   `float` / `int32_t`). Something like `struct S { ...; int32_t x; ...; };
   /* x at offset 12 */` and the code is `arr[i].x`. The `imul` (not a
   shift/`lea`) means 40 isn't a nice power-of-two composite — it's a real
   multiply per iteration unless the compiler hoisted it into a running
   pointer.
   </details>

3. `mov rax, [rbx]` / `test rax, rax` / `je .Ldone` / `mov rbx, [rax + 8]` /
   `jmp .Lloop` — poori shape describe karo.

   <details><summary>Answer</summary>

   A **singly-linked-list traversal**. `rbx` holds the current node pointer
   (callee-saved → live across the loop). `mov rax, [rbx]` — hmm, actually
   re-reading: `[rbx]` loads the first field, `test/je` checks it for null
   → this looks like the loop reads `node` (in `rbx`), does something, then
   `mov rbx, [rax + 8]` advances to `next` (at offset 8 of whatever `rax`
   points to). The tell is: **load a pointer field, null-check it, advance
   to it, jump back** — classic `while (node) { ...; node = node->next; }`.
   Each `mov rbx, [rax + 8]` is a **dependent** load (the next iteration
   can't start until it completes) → zero memory-level parallelism → every
   not-cached node is a full latency stall (folder 32 lesson 08). If this is
   hot, replace the list with a flat array / arena.
   </details>

---

## Interview questions

1. The memory-operand formula (`base + index*scale + disp`) and the scale values.
2. How `arr[i].field` for a 24-byte struct shows up in asm.
3. `lea [x]` vs `mov [x]` — address vs contents.
4. Recovering `sizeof(struct)` and a field offset from a loop's addressing.
5. `[rip + x]` — what kind of storage.
6. `fs:`/`gs:` prefix — what it accesses.
7. Two chained `[..]` loads — what construct, why slow.

---

## Next
→ [`06-stack-frames.md`](06-stack-frames.md)
