# 02 — Compiler Explorer (godbolt) workflow

## Prerequisites
- `01-optimization-levels.md`
- Basic assembly reading intro (folder 34 aage detail dega; abhi surface OK)

## Yeh topic abhi kyun
Compiler ke saath kaam karne ka matlab hai **use dekhna** — usne aapke code
ka kya banaya. **Compiler Explorer** (godbolt.org) yeh instant karta:
source ek taraf, uski assembly doosri taraf, colour-linked, live. Yeh is
poore folder ka daily tool hai. Local pe `./build.ps1 asm` aur `-S` wahi
karte hain offline.

---

## Kya check karte ho (har baar)

1. **Mera loop vectorize hua?** — `ymm`/`xmm` registers, `vaddps`/`vfmadd`.
   Nahi → `vaddss` (scalar).
2. **Mera function inline hua?** — caller mein `call` nahi dikhna chahiye
   (hot small function ke liye).
3. **Branch bana ya `cmov`?** — `jne`/`jl` vs `cmovne`. Folder 31/32 ka
   sabak: `-O2` aksar `cmov` bana deta.
4. **Yeh division / modulo asli `div` hai?** — ya compiler ne
   reciprocal-multiply (`imul` + `shr`) mein badal diya (constant divisor).
5. **Yeh call zaroori tha?** — ya compiler `memset`/`memcpy`/`__memmove`
   emit kar raha jahan aapne loop likha tha (aksar theek — libc optimized).
6. **Constant fold hua?** — `return 5*8+2` → `mov eax, 42`.
7. **Instruction count** — do implementations ka rough size compare.

---

## godbolt.org — the interface

```
┌─────────────────────┬──────────────────────────────┐
│  source (left)      │  assembly (right)            │
│                     │                              │
│  compiler: x86-64   │  colour bands link source    │
│  gcc 15.1           │  line <-> asm lines          │
│  flags: -O2 -std=.. │                              │
└─────────────────────┴──────────────────────────────┘
```

Key features:
- **Compiler dropdown** — GCC / Clang / MSVC / ICX, every version. Compare
  the same code across compilers/versions side by side (add another pane).
- **Flags box** — `-O2 -std=c++20 -march=native -fopt-info-vec` etc.
- **Colour linking** — click a source line → its asm highlights (and vice
  versa). This is how you find "which instructions did this line become".
- **"Add tool"** → `llvm-mca` (static pipeline / throughput estimate),
  `-fopt-info` output, preprocessor, AST, GIMPLE/LLVM-IR, execution.
- **Filters** (top of asm pane) — hide directives (`.cfi_*`, `.p2align`),
  demangle names, hide comments, "Compile to binary" (real bytes + reloc).
- **Diff view** — two compilation panes, godbolt shows the asm diff.
- **Share** — permalink; put it in a code review or a bug report.
- **`//` markers** — `// godbolt:` no; but you can `#define` toggles and
  flip them to A/B.

---

## Local equivalents (offline, this repo)

```bash
# Intel-syntax, demangled, first 80 lines (repo helper):
./build.ps1 asm 33-COMPILER-OPTIMIZATION/examples/03_vectorization.cpp

# raw:
g++ -std=c++20 -O2 -S -masm=intel file.cpp -o -              # to stdout
g++ -std=c++20 -O2 -S file.cpp -o file.s                     # AT&T, to file
g++ -std=c++20 -O2 -S -masm=intel file.cpp -o - | c++filt    # demangle

# just one function's asm from an object file:
g++ -O2 -c file.cpp -o file.o
objdump -d -M intel --demangle file.o | awk '/<_Z.*myfunc.*>:/,/^$/'

# what vectorized / why not:
g++ -O2 -fopt-info-vec        file.cpp -o x    # optimized: loop vectorized...
g++ -O2 -fopt-info-vec-missed file.cpp -o x    # missed: not vectorized because...

# GIMPLE (GCC's mid-level IR) -- see the transforms:
g++ -O2 -fdump-tree-optimized=/dev/stdout -c file.cpp

# static pipeline estimate for a snippet:
g++ -O2 -S file.cpp -o - | llvm-mca -mcpu=native
```

The repo's `./build.ps1 asm` is the fast path; godbolt is better for
comparing versions/compilers and for sharing.

---

## A worked reading

Source:
```cpp
int sum(const int* p, int n) {
    int s = 0;
    for (int i = 0; i < n; ++i) s += p[i];
    return s;
}
```
`-O2 -march=native` asm (Intel, trimmed):
```asm
sum(int const*, int):
        test    esi, esi
        jle     .L4                 ; n <= 0 -> return 0
        ...
        vpxor   xmm0, xmm0, xmm0    ; vector accumulator = 0
.L3:
        vpaddd  ymm0, ymm0, YMMWORD PTR [rdi+rax]   ; 8 ints at a time  <- VECTORIZED
        add     rax, 32
        cmp     rdx, rax
        jne     .L3
        vextracti128 xmm1, ymm0, 0x1               ; horizontal reduce
        vpaddd  xmm0, xmm0, xmm1
        ...                                        ; scalar tail for n % 8
```
Reading it: `vpaddd ymm0, ..., [rdi+rax]` = "add 8 packed 32-bit ints from
memory into ymm0" → the loop **vectorized** (8-wide). The `vextracti128 +
vpaddd` after = horizontal reduction of the 8 lanes. Then a scalar tail for
the leftover `n % 8`. If instead you saw `add eax, [rdi+rax*4]` in the loop
→ **scalar**, and you'd go find out why (`-fopt-info-vec-missed`).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — godbolt default flags
Fresh pane = **no `-O`** → you're reading `-O0` garbage. Always set
`-O2`/`-O3` first. And `-std=c++20` (or whatever) — default may be old.

### Trap 2 — reading `-O0` and concluding "compiler is dumb"
`-O0` is deliberately literal. Set `-O2` before judging codegen.

### Trap 3 — the function got deleted (empty asm)
If a function is `static`/anonymous-namespace and unused, or fully inlined
into its only caller, its standalone asm vanishes. Mark it
`[[gnu::used]]` / make it `extern` / call it from a visible `main`, or look
at the caller.

### Trap 4 — forgetting the sink
`int f() { int x = expensive(); return 0; }` → `expensive()` may vanish
(dead). To see real codegen, `return x;` or use a `DoNotOptimize` barrier
(lesson 14).

### Trap 5 — `-march=native` on godbolt
godbolt's `native` = *godbolt's server's* CPU, not yours. Pick the explicit
arch (`-march=haswell` / `-mavx2` / `-march=x86-64-v3`) for reproducible,
target-relevant output.

### Trap 6 — comparing across noise
Small source changes shuffle labels/`.p2align`. Use godbolt's diff view, or
compare *structure* (which instructions in the loop body), not line-for-line.

---

## > **HFT relevance**

> - **Every hot function gets an asm review.** Order-book update, matching
>   inner loop, parse routine — paste to godbolt (or `./build.ps1 asm`),
>   confirm: no surprise `call`, vectorized where expected, no `div` in the
>   critical path, branch vs `cmov` as intended.
> - **Version-diff on every toolchain bump.** GCC 13→15 with identical flags
>   can change the hot loop. Diff the asm; re-benchmark.
> - **Put godbolt permalinks in code review.** "This template refactor keeps
>   the same 12-instruction loop body — <link>" is a real review artifact.
> - **`llvm-mca` for critical-path budgeting** — folder 31 lesson 09.
> - **Check the constants got folded** — a `price * tick_size` where
>   `tick_size` is `constexpr` should be an `imul`/shift, not a memory load.

---

## Hands-on

```bash
# 1. paste examples/03_vectorization.cpp into godbolt.org, flags: -O2 -std=c++20
#    click the map_vec loop -> see vpaddps/vmulps; click map_scalar -> vmulss
# 2. add -march=native -> 16-byte (xmm) becomes 32-byte (ymm)
# 3. add a second compiler pane: clang 18, same flags -> diff the loop
# 4. local: ./build.ps1 asm 33-COMPILER-OPTIMIZATION/examples/02_inlining_demo.cpp
#    grep for `call` -- present in the noinline path, absent in the inline path
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "godbolt default is fine" | no `-O` by default → set `-O2` first |
| "empty asm = compiler broke" | function inlined/dead; look at the caller |
| "`-march=native` on godbolt = my CPU" | godbolt's server CPU; name the arch |
| "asm diff line-for-line" | labels/padding shift; compare loop-body structure |
| "one look is enough" | re-check after every refactor + toolchain bump |
| "if it's slow, read -O0 asm" | read `-O2`/`-O3` asm — that's what ships |

---

## Exercises

1. Godbolt pe `int rem(int x) { return x % 7; }`, `-O2`. `idiv` dikhta hai?
   Ab `int rem(int x, int d) { return x % d; }` — ab kya, aur kyun farak?

   <details><summary>Answer</summary>

   `x % 7` (constant divisor): **no `idiv`** — the compiler emits a
   reciprocal-multiply sequence: `imul` by a magic constant, some shifts, a
   subtract → ~4-5 cheap instructions instead of a ~20-40 cycle `idiv`.
   `x % d` (runtime divisor): **`idiv` appears** — the compiler can't
   precompute the magic number for an unknown `d`, so it must use the real
   division instruction. Lesson: keep divisors compile-time-constant where
   you can (folder 31 lesson 09's "division problem"); for a runtime-but-
   loop-invariant divisor, hoist a `libdivide` object.
   </details>

2. Ek `std::vector<int> v; v.push_back(x);` ko godbolt pe daalo, `-O2`. Kitne
   instructions, aur kaunsa branch "grow" path ke liye hai?

   <details><summary>Answer</summary>

   You'll see: load `size` and `capacity` (or `_M_finish` / `_M_end_of_storage`
   pointers), `cmp` them, a `je`/`jne` — the **fast path** (capacity left):
   store `x` at `_M_finish`, `add` 8 (or 4) to `_M_finish`, done (~4-5
   instructions). The **slow path** (`_M_finish == _M_end_of_storage`): a
   `call` to `_M_realloc_insert` / `_M_realloc_append` — reallocate, move,
   free. The compiler lays the grow path out-of-line (cold). This is why
   `reserve()` matters: it makes every `push_back` take only the ~5-instruction
   fast path.
   </details>

3. `./build.ps1 asm` on `examples/05_branch_hints.cpp`. `process_hinted` mein
   `slow_path` ka `call` kahan hai — loop body mein ya bahar? `process_plain`
   mein?

   <details><summary>Answer</summary>

   In **`process_hinted`** (with `[[unlikely]]` on the `b == 0` branch), the
   `call slow_path` is emitted **out of line** — after the function's main
   body / the loop, reached by a forward `jmp` taken only when `b == 0`, then
   a `jmp` back. The hot path (the `else`) is straight-line fall-through.
   In **`process_plain`** the compiler still usually guesses the `== 0` branch
   is unlikely (it's an equality against a literal, and static heuristics
   favour "not equal"), so the layout is often similar — which is why the
   measured speedup is small (~1.3×). The hint makes the layout *reliable*
   and matters more in a big function with many such checks. Confirm by
   grepping the asm for `slow_path` and seeing whether it sits between the
   loop and `ret` (out-of-line) or inside the loop label range.
   </details>

---

## Interview questions

1. Compiler Explorer — 5 things you check on a hot function's asm.
2. `-fopt-info-vec` vs `-fopt-info-vec-missed` — kya batate.
3. godbolt pe function ka asm gayab — 2 reasons + fixes.
4. Local `-S` command aur `objdump -d` — kab kaunsa.
5. `llvm-mca` — kya deta jo raw asm nahi.
6. Toolchain version bump pe asm review kyun.
7. A vectorized loop ki asm signature (ymm + vpaddd + horizontal reduce + tail).

---

## Next
→ [`03-inlining.md`](03-inlining.md)
