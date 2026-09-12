# 06 — Stack frames: prologue/epilogue, `rbp`/`rsp`, locals, frame pointers

## Prerequisites
- `08-FUNCTIONS/05-the-call-stack.md`
- `14-MEMORY/03-stack-deep-dive.md`
- `04_stack_frame.cpp` example

## Yeh topic abhi kyun
Har non-trivial function ke asm ki pehli aur aakhri lines **prologue** aur
**epilogue** hoti hain — jo stack frame set/undo karti hain. Inhe pehchano to
aap turant jaan jaate: kitne locals, frame pointer hai ya nahi, kaunse
callee-saved registers use hue, aur recursion mein stack kitna badhta.

---

## The stack (grows DOWN)

```
higher addresses
  ...
  [ caller's frame        ]
  [ arg 7, 8, ... on stack ]   (first 6 int args are in registers)
  [ return address         ]   <- pushed by `call`
  [ saved rbp (if used)    ]   <- pushed by prologue
  [ callee-saved regs      ]   (rbx, r12-r15 if the function uses them)
  [ local variables        ]
  [ spill slots            ]
  [ outgoing args / shadow  ]   (Win64: 32-byte shadow space for callees)
rsp ->  (top of stack)
lower addresses
```

`rsp` always points at the current top. `call` pushes the return address and
jumps; `ret` pops it.

---

## Prologue / epilogue — two forms

### With frame pointer (`-O0`, or `-fno-omit-frame-pointer`)
```asm
foo:
        push  rbp                ; save caller's frame pointer
        mov   rbp, rsp           ; rbp = start of THIS frame
        sub   rsp, 48            ; allocate 48 bytes of locals/spills
        ...
        ; locals accessed as [rbp - 8], [rbp - 16], ...
        ...
        leave                   ; == mov rsp, rbp ; pop rbp
        ret
```
`rbp` is a stable anchor for the whole function → locals are at fixed
`[rbp - N]` offsets, and a debugger/profiler can walk the chain of saved
`rbp`s to unwind the call stack.

### Frame pointer omitted (`-O2` default)
```asm
foo:
        sub   rsp, 40           ; allocate; rbp is now a free general register
        ...
        ; locals accessed as [rsp + 8], [rsp + 16], ... (rsp-relative)
        ...
        add   rsp, 40
        ret
```
One fewer `push`/`pop`, one more usable register (`rbp`). Downside: stack
unwinding needs the `.eh_frame` / `.pdata` unwind tables (CFI directives) —
`perf` without frame pointers falls back to DWARF or LBR (folder 35).

### Leaf function, no big locals
```asm
add3:
        lea   rax, [rdi + rsi]
        add   rax, rdx
        ret
```
**No `sub rsp` at all** — everything in registers, no frame. (Leaf = calls
nothing. On Win64, even a leaf may `sub rsp, 40` for shadow space if it makes
a call; a true leaf doesn't.)

---

## Reading a frame

| You see | It means |
|---|---|
| `push rbp / mov rbp, rsp` | frame-pointer build (`-O0` or `-fno-omit-frame-pointer`) |
| `sub rsp, N` (no `push rbp`) | `-O2`, frame pointer omitted, N bytes of frame |
| `sub rsp, 0x8xx` (big) | a big local array / struct (2 KiB in `04_stack_frame.cpp`) |
| `push rbx` / `push r12` ... | the function uses callee-saved regs → it has long-lived values (loop pointers, `this`) |
| `mov [rsp + k], reg` then later `mov reg, [rsp + k]` | a **spill** — a value pushed to memory because registers ran out |
| `[rbp - N]` accesses | frame-pointer-relative locals |
| `[rsp + N]` accesses | rsp-relative locals / outgoing args |
| `lea rdi, [rsp + k]` before a `call` | passing `&local` to another function (address escaped) |
| `sub rsp, 0x28` in almost every function | Win64 shadow space (32) + alignment (8) |
| `leave` / `add rsp, N ; pop ...` / `ret` | epilogue |
| stack-canary: `mov rax, fs:[0x28]` at entry, check before `ret` | `-fstack-protector` |

---

## Spills = register pressure

`-O2` keeps hot values in registers. When there are more live values than
registers (16 GP, minus reserved), some **spill** to the stack:

```asm
        mov   QWORD PTR [rsp + 8], rax     ; spill rax
        ... use other registers ...
        mov   rax, QWORD PTR [rsp + 8]     ; reload
```
A hot loop full of `mov [rsp+..], reg` / `mov reg, [rsp+..]` pairs = register
pressure = often caused by **over-unrolling**, **large structs by value**,
**too many live accumulators**, or `-O3` being greedy. Fixes: fewer
accumulators, pass by `const&`, `-fno-unroll-loops` / lower `--param`.

---

## Recursion and stack growth

Each recursive call = a new frame = `rsp` moves down by the frame size.
`04_stack_frame.cpp`'s `show_depth` prints `&local` at each depth — the
addresses **decrease** by the frame size per level. Deep/unbounded recursion
→ `rsp` runs into the guard page → `SIGSEGV` (stack overflow — folder 08
`05_stack_overflow`).

**Tail call** (`return f(...)` with nothing after) → `-O2` reuses the current
frame and `jmp`s → **no stack growth** (`04_stack_frame.cpp` `sum_to` becomes
a loop). `factorial` (`return n * f(n-1)` — a `mul` after the call) is **not**
a tail call → real recursion, real frames.

---

## Windows x64 specifics (this repo)

- **Shadow space**: the caller reserves 32 bytes above the return address for
  the callee to spill its 4 register args. You'll see `sub rsp, 40` (32 + 8
  align) or `sub rsp, 0x28` in functions that call anything.
- **Unwind info** is `.pdata`/`.xdata` (not DWARF `.eh_frame`). `push`/`sub`
  in the prologue must match declared unwind codes.
- **`rsi`, `rdi` are callee-saved** on Win64 (caller-saved on SysV) — so a
  Win64 function that uses them `push`es them in the prologue.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — expecting `push rbp` at `-O2`
Frame pointer is omitted by default. `sub rsp, N` alone is the frame. Add
`-fno-omit-frame-pointer` if you need `rbp` chains (profiling).

### Trap 2 — `sub rsp, 0x28` = "has locals"
On Win64 that's often just shadow space + alignment for making calls, not
real locals. A big `sub rsp, 0x820` = real locals.

### Trap 3 — reading spills as the algorithm
`mov [rsp+8], rax` mid-function isn't storing a result — it's parking a
register. High spill density = a codegen smell, not intent.

### Trap 4 — `[rbp - 4]` in an `-O2` build
If you see `rbp`-relative locals at `-O2`, someone passed
`-fno-omit-frame-pointer` (or it's a function that takes `&local` a lot).
Fine — just note it.

### Trap 5 — assuming stack grows up
It grows **down**. `sub rsp` allocates; `add rsp` frees. Local addresses in
a recursion **decrease** with depth.

### Trap 6 — ignoring the stack canary check
`-fstack-protector` adds a `fs:[0x28]` load at entry and a compare + `call
__stack_chk_fail` before `ret`. That's not your logic; it's the buffer-
overflow guard.

---

## > **HFT relevance**

> - **Frame size** (`sub rsp, N`) of a hot function = stack bytes touched per
>   call = a small cache concern, and a `-fstack-usage` budget item. A
>   surprise big frame = a large local array / struct-by-value → move it to a
>   pool / pass by ref.
> - **Spills in the hot loop** = register pressure → the loop is doing memory
>   traffic it shouldn't. Reduce live values, don't over-unroll (folder 33
>   lesson 04).
> - **`-fno-omit-frame-pointer` on release** — many HFT shops keep frame
>   pointers on so `perf`/production profilers get cheap, accurate stacks
>   (the ~1 register cost is worth the observability — folder 35).
> - **Tail-call → loop** — a recursive helper that's tail-recursive costs no
>   stack; confirm the `jmp` (not `call`) in the asm.
> - **`callee-saved push`es** tell you which pointers the function keeps live
>   across a loop — usually the book/iterator/`this`.

---

## Hands-on

```bash
./build.ps1 asm 34-ASSEMBLY/examples/04_stack_frame.cpp
#   add3            : no `sub rsp`
#   sum_local_buffer: `sub rsp, <big>` (256 longs)
#   factorial       : `call` to itself
#   sum_to          : `jmp` (tail call -> loop), no `call`

# frame pointer on vs off:
g++ -O2                        -S -masm=intel 34-ASSEMBLY/examples/04_stack_frame.cpp -o - | sed -n '/caller/,/ret/p'
g++ -O2 -fno-omit-frame-pointer -S -masm=intel 34-ASSEMBLY/examples/04_stack_frame.cpp -o - | sed -n '/caller/,/ret/p'

# frame sizes per function:
g++ -O2 -fstack-usage -c 34-ASSEMBLY/examples/04_stack_frame.cpp -o /dev/null && cat 04_stack_frame.su

./build.ps1 fast 34-ASSEMBLY/examples/04_stack_frame.cpp   # runtime: frame addrs shrink with depth
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "every function has `push rbp`" | omitted at `-O2`; `sub rsp, N` is the frame |
| "`sub rsp, 0x28` means locals" | on Win64 often just shadow space + alignment |
| "spills are part of the logic" | parked registers; a codegen smell |
| "stack grows up" | grows down; `sub rsp` allocates |
| "recursion is free" | a frame per call; `rsp` decreases; overflow at the guard page |
| "tail recursion still uses frames" | `-O2` turns it into a `jmp` loop |

---

## Exercises

1. Ek `-O2` function ka asm: `push rbx` / `push r12` / `sub rsp, 40` ... `add
   rsp, 40` / `pop r12` / `pop rbx` / `ret`. Kya infer karte ho?

   <details><summary>Answer</summary>

   The function **uses `rbx` and `r12`** (callee-saved) and therefore had to
   save/restore them → it holds **two long-lived values across the body** —
   almost certainly a loop with two pointers (current + end), or `this` + an
   accumulator, or two nested-loop induction pointers. It **calls something**
   (the `sub rsp, 40` is shadow space + alignment for a call — a pure leaf
   with no big locals wouldn't need it). No huge `sub rsp` → **no large local
   arrays/structs**; whatever spills exist fit in a few slots. `pop`s are in
   reverse `push` order (LIFO), then `ret`. This is the shape of a typical
   "iterate a range, call a helper per element, accumulate" function.
   </details>

2. `perf` ek hot loop ke andar `mov QWORD PTR [rsp+0x10], rax` aur `mov rax,
   QWORD PTR [rsp+0x10]` dono pe samples dikha raha hai. Kya, kaise fix?

   <details><summary>Answer</summary>

   A **register spill inside the hot loop** — `rax` is being pushed to a
   stack slot and reloaded because the loop has more simultaneously-live
   values than the ~13 usable GP registers. Each spill/reload is an L1 access
   (~4-5 cyc) and adds stack traffic. Common causes: the loop was
   **over-unrolled** (`-O3` / `#pragma unroll 16` → many partial accumulators
   + addresses live at once), a **big struct passed/held by value**, or **too
   many manual accumulators**. Fixes: fewer accumulators (or let the compiler
   choose — folder 33 lesson 04), `-fno-unroll-loops` or a smaller unroll
   factor, pass large objects by `const&`, split the loop (fission) so each
   part has fewer live values. Re-check the asm — the `[rsp+..]` churn should
   drop.
   </details>

3. Do versions of a recursive `sum(node*)`: ek mein har call pe `call sum`,
   doosre mein `jmp sum` (loop). Source mein kya farak hai?

   <details><summary>Answer</summary>

   The `jmp sum` version is **tail-recursive**: `return sum(node->next,
   acc + node->val);` — the recursive call is the last thing, its result is
   returned directly, so `-O2` reuses the current frame and `jmp`s (→ a loop,
   O(1) stack). The `call sum` version does work **after** the recursive
   call: `return node->val + sum(node->next);` — the `+` happens once `sum`
   returns, so each call needs its own frame to hold `node->val` while the
   recursion runs → O(depth) stack, and a deep list → stack overflow. Rewrite
   the second as the first (thread an accumulator) to get the tail-call
   optimization, or just use an explicit loop.
   </details>

---

## Interview questions

1. The stack layout of a frame (return addr, saved rbp, callee-saved, locals, spills).
2. Prologue/epilogue with and without a frame pointer — the instructions.
3. Why `-O2` omits the frame pointer, and the cost (unwinding / profiling).
4. Register spills — how they look, what causes them, how to reduce.
5. Tail call vs real recursion — the asm difference, the source difference.
6. Win64 shadow space — what it is, why you see `sub rsp, 40` everywhere.
7. Stack canary — the instructions `-fstack-protector` adds.

---

## Next
→ [`07-calling-conventions.md`](07-calling-conventions.md)
