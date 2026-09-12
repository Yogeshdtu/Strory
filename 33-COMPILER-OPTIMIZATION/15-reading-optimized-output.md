# 15 — Reading optimized assembly: verify your intent

## Prerequisites
- `02-godbolt-workflow.md`, and every lesson `03`–`14`
- Folder 34 (ASSEMBLY) goes deep; this lesson is the "compiler verification" angle

## Yeh topic abhi kyun
Yeh folder ka synthesis. Aapne compiler ko hint diya (`__restrict`, `inline`,
`[[unlikely]]`, `constexpr`, `-march`, PGO) — ab **verify** karo ki usne
wahi kiya. Optimized assembly padhna ek skill hai jo do sawaalon ka jawab
deta: "kya mera intent poora hua?" aur "yeh loop itna dheema kyun hai?".
Aapko asm likhni nahi aani chahiye — sirf padhni.

---

## The checklist (per hot function)

```bash
./build.ps1 asm 33-COMPILER-OPTIMIZATION/examples/<file>.cpp    # Intel, demangled
# or godbolt, or: g++ -O2 -S -masm=intel file.cpp -o -
```

| Look for | Good | Bad → go investigate |
|---|---|---|
| **loop body size** | tight, few instructions | bloated, spills (`mov [rsp+..], reg`) |
| **vectorization** | `ymm`/`xmm`, `vaddps`/`vfmadd...ps`, a horizontal-reduce tail | scalar `vaddss`/`addss` in the loop → `-fopt-info-vec-missed` |
| **`call` in the loop** | none (hot small callees inlined) | `call _Z...` → not inlined / cross-TU → header/`-flto` (lesson 3, 10) |
| **`call [reg]` / `call [rax+off]`** | none on the hot path | vtable / fn-ptr dispatch → devirt / `variant`/`switch` (lesson 7) |
| **branch vs `cmov`** | as intended (folder 31/32 nuance) | if you wanted a real branch and got `cmov` (or vice-versa) |
| **division** | `imul`+`shr` (constant divisor), or hoisted reciprocal | `div`/`idiv`/`divsd` in the loop → make divisor `constexpr` / `libdivide` (lesson 4) |
| **constants** | `imul eax, eax, 8` / `mov eax, 42` | a `mov reg, [rip+const]` load where a literal was expected → not folded (lesson 6) |
| **reloads** | invariant values held in registers | the same `mov reg, [mem]` every iteration → aliasing → `__restrict` (lesson 9) |
| **memory ops** | one load per needed value | extra loads/stores → spills (too many live values / over-unroll) |
| **cold code placement** | error/slow paths after `ret` (out-of-line) | rare handler inline in the loop body → `[[unlikely]]` (lesson 8) |

---

## Recognizing structures (folder 34 detail; quick version)

### A vectorized reduction
```asm
        vpxor   xmm0, xmm0, xmm0
.L3:    vpaddd  ymm0, ymm0, YMMWORD PTR [rdi+rax]   ; 8 ints/iter
        add     rax, 32
        cmp     rdx, rax
        jne     .L3
        ; ... vextracti128 / vpaddd / vmovd  -> horizontal reduce
        ; ... scalar tail loop for n % 8
```

### A scalar loop that should've vectorized
```asm
.L3:    movss   xmm1, DWORD PTR [rdi+rax*4]   ; one float
        addss   xmm0, xmm1                    ; scalar add
        add     rax, 1
        cmp     esi, eax
        jg      .L3
```
`movss`/`addss` (single) not `movups`/`addps` (packed) → **not vectorized**.
Run `-fopt-info-vec-missed` to find out why (aliasing? reduction? call?).

### An inlined vs non-inlined call
```asm
; inlined:  imul eax, eax, 3   /  shr edx, 3  /  add  ...    (body is here)
; not:      mov  edi, eax  /  call scale  /  (result in eax)
```

### A vtable dispatch
```asm
        mov     rax, QWORD PTR [rdi]       ; load vptr
        call    QWORD PTR [rax+16]         ; call vtable[2]  <- indirect, not inlined
```

### Division survived
```asm
        cdq
        idiv    ecx                        ; ~20-40 cyc, in the loop = bad
; vs constant divisor:
        imul    rax, rax, -1431655765      ; magic
        shr     ...                        ; reciprocal-multiply, ~4 cheap ops
```

---

## `-fverbose-asm` and dump files

```bash
g++ -O2 -S -fverbose-asm file.cpp -o -          # asm with source-var comments
g++ -O2 -fopt-info-all file.cpp -o x 2>&1       # every pass's decisions
g++ -O2 -fdump-tree-optimized=/dev/stdout -c f.cpp   # GIMPLE after all tree passes
g++ -O2 -fdump-rtl-expand=/dev/stdout -c f.cpp       # early RTL
g++ -O2 -fdump-ipa-inline=/dev/stdout -c f.cpp       # inline decisions
objdump -dS --demangle a.out                         # asm interleaved with source (needs -g)
perf annotate -s myfunc                              # asm + sample % per instruction
```

`objdump -dS` (with `-g`) interleaves your C++ lines with the asm they became
— the fastest way to answer "which instructions did this line produce".

---

## A verification workflow

1. **Write the intent down**: "this loop should vectorize 8-wide, no `call`,
   `*scale` hoisted, error path out-of-line."
2. **Compile with the real flags**: `-O2 -march=<target>` (+ `-flto` context
   if relevant), `-g` for `objdump -dS`.
3. **Read the loop body**: check each item on the checklist against the intent.
4. **If a mismatch**: use `-fopt-info-*` / dump files to find *why*
   (aliasing, reduction, call, cost model, missing `-march`).
5. **Fix the cause** (a hint from lessons 3–13), recompile, re-read.
6. **Benchmark** to confirm the asm change is also a time change (lesson 14
   barriers, folder 32 methodology). Asm looking right but time not moving →
   you were bound by something else (memory, folder 32 lesson 13).
7. **Lock it in**: a godbolt permalink or a checked-in `.s` snapshot in the
   PR, so a future refactor / toolchain bump that regresses it is visible.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — reading `-O0` asm
Literal, verbose, meaningless for judging codegen. `-O2`/`-O3` only.

### Trap 2 — "there's a `call`, it's slow" without checking what it is
A `call memcpy` / `call memset` where you wrote a loop is usually the
compiler doing you a favour (optimized libc). A `call` to your tiny hot
helper is the problem.

### Trap 3 — assuming `cmov` is always better than a branch
For an unpredictable branch, yes. For a predictable one, `cmov` adds a
dependency (no speculation past it) and can be slower (folder 31 lesson 08).
Match the codegen to the branch's predictability.

### Trap 4 — chasing an asm "improvement" that doesn't move the clock
You got the loop to vectorize, `perf` shows the same time → it was
memory-bound (folder 32 lesson 13). Asm review must be paired with a
benchmark.

### Trap 5 — reading a stale build
Edited the source, forgot to rebuild, read old asm. Or read a different `-O`
than you ship. Script it (`./build.ps1 asm`).

### Trap 6 — over-indexing on instruction count
Fewer instructions ≠ faster (a `div` is 1 instruction, ~30 cycles). Think in
*latency* / *port pressure* / *dependency chains* (folder 31 lessons 04–09,
`llvm-mca`), not line count.

---

## > **HFT relevance**

> - **Every hot function gets an asm review before it ships** — order-book
>   update, matcher inner loop, feed parse. Written intent → checklist →
>   godbolt/`./build.ps1 asm`/`perf annotate`.
> - **The specific things to confirm**: no `call` on the hot path (inlined /
>   `-flto`), vectorized where it should be (`-march` + `__restrict`), no
>   `div`/`idiv` in the loop (constant divisors), invariants in registers
>   (no per-iteration reloads), cold paths out-of-line (`[[unlikely]]`),
>   constants folded (no config loads).
> - **Snapshot the asm in the PR** (godbolt link or a `.s` diff) — a
>   template refactor or GCC 14→15 bump that adds a `call` or drops
>   vectorization is then a visible regression.
> - **`perf annotate` in production triage** — the instruction eating the
>   cycles on a p99 spike is right there.
> - **Pair asm with a benchmark** — right-looking asm + unchanged time = you
>   were bound elsewhere (memory — folder 32). Fix that instead.

---

## Hands-on

```bash
# read every folder-33 example's hot asm against its stated intent:
for f in 33-COMPILER-OPTIMIZATION/examples/0[1-8]_*.cpp; do
  echo "== $f =="; ./build.ps1 asm "$f" | grep -nE "call|ymm|xmm|mulps|mulss|idiv|imul|cmov|jne" | head -20
done

# source-interleaved:
g++ -std=c++20 -O2 -g -c 33-COMPILER-OPTIMIZATION/examples/03_vectorization.cpp -o /tmp/v.o
objdump -dS --demangle /tmp/v.o | sed -n '/map_vec/,/ret/p'

# why not vectorized / not inlined:
g++ -O2 -fopt-info-vec-missed -fopt-info-inline-missed 33-COMPILER-OPTIMIZATION/examples/04_aliasing_restrict.cpp -o /tmp/x 2>&1 | head
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "read `-O0` asm to see what the code does" | read `-O2`/`-O3` — that's what runs |
| "any `call` = slow" | `call memcpy` is fine; `call my_hot_helper` isn't |
| "fewer instructions = faster" | think latency / ports / dependency chains |
| "`cmov` beats a branch" | only for unpredictable branches |
| "asm looks right, so it's faster" | confirm with a benchmark — could be memory-bound |
| "I'll eyeball it once" | snapshot it; re-check on refactor + toolchain bump |

---

## Exercises

1. `./build.ps1 asm` on `03_vectorization.cpp`. `map_vec` ke loop body mein
   kya dikhega, aur `map_scalar` mein? `reduce_try` mein?

   <details><summary>Answer</summary>

   **`map_vec`**: `movups xmm, [rsi+rax]` (load 4 floats) / `mulps` (square) /
   `mulps` by 0.5 / `addps` (+ a[i]) / `addps` (+ bump broadcast) / `movups
   [rdi+rax], xmm` / `add rax, 16` / loop — plus an unrolled copy or two, and
   a scalar `n % 4` tail. Packed (`ps`) throughout → **vectorized** (SSE2,
   16-byte; add `-march=native` → `ymm`, 32-byte). **`map_scalar`**: `movss`
   / `mulss` / `mulss` / `addss` / `addss` / `movss` / `add rax, 4` — one
   element per iteration, scalar (`ss`), because of
   `optimize("no-tree-vectorize")`. **`reduce_try`** at plain `-O2`: a scalar
   `addss xmm0, [rsi+rax]` chain (one accumulator) — **not** vectorized
   (float reduction reassociation not allowed); with `-ffast-math` you'd see
   4 `addps` partial-sum accumulators + a horizontal reduce.
   </details>

2. Ek hot function ki asm mein loop body ke andar `call __divdi3` (ya
   `idiv`) dikh raha hai. Diagnose + fix path.

   <details><summary>Answer</summary>

   There's an **integer division by a runtime (non-constant) value** in the
   hot loop (`__divdi3` is the libc 64-bit divide helper on targets without a
   hardware 64-bit `idiv`; `idiv` is the x86-64 instruction — ~20-40+ cycles,
   not pipelined). Diagnose: find the `/` or `%` in the source line
   (`objdump -dS` maps it). Fix options, best first: (1) if the divisor is
   **loop-invariant**, hoist it — compute `1.0/d` once (float) or build a
   `libdivide::divider<uint64_t>` once, use multiply-shift in the loop
   (folder 31 lesson 09). (2) if it's a **power of two**, use `>>` / `&`
   (and make the compiler see it — `x / (1u << k)` with unsigned). (3) if
   it's actually **compile-time constant** but hidden behind a `const`
   parameter, make it `constexpr` / a template non-type param → the compiler
   emits reciprocal-multiply. (4) restructure to divide once outside the
   loop. Re-read the asm to confirm the `idiv`/`__divdi3` is gone.
   </details>

3. Tumne `__restrict` + `-march=native` add kiya, asm ab clean 8-wide
   `vfmadd...ps` dikhata hai, par benchmark 0% tez. Ab kya?

   <details><summary>Answer</summary>

   The asm is exactly what you wanted — the loop is now compute-optimal — but
   the time didn't move, so **the ALU was never the bottleneck**. Almost
   certainly **memory-bound** (folder 32 lesson 13): the working set is
   larger than LLC and the loop is limited by DRAM/L3 bandwidth; SIMD
   reads/writes the same bytes, just issuing fewer instructions to do it, so
   wall time is unchanged. Confirm with `perf stat --topdown` (Backend →
   Memory Bound dominant) and `perf stat -e
   offcore_requests_outstanding.all_data_rd`. The fix is not more SIMD —
   it's fewer bytes (smaller element types, `float`→`half` if precision
   allows), fewer passes (fusion), blocking so the working set fits L2/L3, or
   NT stores for write-only output. Keep the `__restrict`/`-march` (they're
   free and correct), but the next optimization is a folder-32 one.
   </details>

---

## Interview questions

1. The asm-review checklist for a hot function — name 6 things you check.
2. Vectorized vs scalar loop — the asm signature of each.
3. A `call` in the hot loop — the two very different cases (`memcpy` vs your helper).
4. `idiv`/`__divdi3` in a loop — diagnosis and fix path.
5. `objdump -dS` — what it gives you that plain `-S` doesn't.
6. Why asm review must be paired with a benchmark.
7. Right-looking asm, unchanged time — what does that tell you?

---

## Next
→ [`16-exercises.md`](16-exercises.md)
