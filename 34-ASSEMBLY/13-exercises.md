# 13 — Exercises: reading assembly

## Prerequisites
- Poora folder 34 (`01`–`12`) + `examples/07_asm_puzzles.md`

## Kaise use karein
- **Part A** — "yeh assembly kis C++ se aayi?" (like `07_asm_puzzles.md`).
- **Part B** — verification: given a folder-33 technique + the asm, did it work?
- **Part C** — reasoning: given `perf annotate` / an asm shape, diagnose.
- **Part D** — hands-on: compile, read, compare. **Real observations** (what
  instructions, what width, any surprise).

> Intel syntax, `-O2`, unless noted. This repo's `./build.ps1 asm` = Win64
> ABI (arg 1 in `rcx`); godbolt default = System V (arg 1 in `rdi`).
> On MinGW `long` is 32-bit — use `int64_t` / godbolt for 64-bit `long`.

---

## Part A — Which C++?

### A1
```asm
f(unsigned long):
        lea     rax, [rdi + 7]
        and     rax, -8
        ret
```

<details><summary>Answer</summary>

`unsigned long f(unsigned long x) { return (x + 7) & ~7UL; }` — **round up
to a multiple of 8** (alignment). `+ 7` then `& -8` (= `& ~7`, clearing the
low 3 bits) is the standard "align up to 8" idiom. `& -16` → align to 16,
`& -4096` → align to a page. Seen everywhere in allocators, arena bump
pointers, SIMD peel loops. `lea` does the `+ 7` for free.
</details>

### A2
```asm
g(int*, int*, int):
        test    edx, edx
        jle     .L1
        movsx   rdx, edx
        xor     eax, eax
.L3:
        mov     ecx, DWORD PTR [rsi + rax*4]
        add     DWORD PTR [rdi + rax*4], ecx
        add     rax, 1
        cmp     rdx, rax
        jne     .L3
.L1:
        ret
```

<details><summary>Answer</summary>

`void g(int* a, int* b, int n) { for (int i = 0; i < n; ++i) a[i] += b[i]; }`
— element-wise add-into-`a`. Note it's **scalar** (`mov ecx, [rsi+rax*4]` /
`add [rdi+rax*4], ecx`, `add rax, 1`) not vectorized — because `a` and `b`
are plain `int*` and the compiler must assume they might overlap (no
`__restrict`, and this is a standalone function so it can't see the call
site). Add `int* __restrict a, const int* __restrict b` → this vectorizes to
`paddd`/`add rax, 16` (folder 33 lesson 09). The `movsx rdx, edx` sign-
extends `n` to 64 bits for the pointer arithmetic.
</details>

### A3
```asm
h(long):
        mov     rax, rdi
        sar     rax, 63
        xor     rdi, rax
        sub     rdi, rax
        mov     rax, rdi
        ret
```

<details><summary>Answer</summary>

`long h(long x) { return x < 0 ? -x : x; }` — **branchless `abs`**.
`sar rax, 63` broadcasts `x`'s sign bit across all 64 bits → `mask` is `0`
if `x >= 0`, `-1` (all ones) if `x < 0`. Then `x ^ mask` then `- mask`: if
`mask == 0` it's `x ^ 0 - 0 = x`; if `mask == -1` it's `~x - (-1) = ~x + 1 =
-x` (two's complement). No branch, no `cmov` even — pure arithmetic. This is
the canonical branchless absolute value.
</details>

### A4
```asm
k(int):
        cmp     edi, 4
        ja      .Ldefault
        mov     eax, edi
        jmp     [QWORD PTR .Ltab[0 + rax*8]]
.Ltab:
        .quad   .L4
        .quad   .L5
        .quad   .L6
        .quad   .L6
        .quad   .L7
```

<details><summary>Answer</summary>

A dense `switch` with 5 labelled cases (0–4) plus `default`, compiled to a
**jump table**. `cmp edi, 4 / ja .Ldefault` — range check (unsigned `ja`
catches both `> 4` and negative). `jmp [.Ltab + rax*8]` — indirect jump
through the table. Note `.Ltab` entries 2 and 3 both point to `.L6` → cases
2 and 3 have the **same body** (fall-through or identical code). Source:
`switch (x) { case 0: ...; case 1: ...; case 2: case 3: ...; case 4: ...;
default: ...; }`. One indirect jump, O(1) regardless of case count.
</details>

### A5
```asm
m(float const*, int):
        ...
.L3:
        vfmadd231ps ymm0, ymm1, YMMWORD PTR [rdi + rax]
        add     rax, 32
        cmp     rdx, rax
        jne     .L3
        ; ... horizontal reduce ...
```

<details><summary>Answer</summary>

`float m(const float* a, int n) { float s = 0; for (int i = 0; i < n; ++i)
s += k * a[i]; return s; }` — a **scaled sum**, compiled with `-ffast-math`
(or `#pragma omp simd reduction`) so the float reduction could be
reassociated, and with **FMA + AVX2** (`-march=x86-64-v3` or `-mfma -mavx2`).
`vfmadd231ps ymm0, ymm1, [rdi+rax]` = `ymm0 += ymm1 * mem` — 8 floats, fused
multiply-add, one instruction. `ymm1` holds the broadcast constant `k`.
`add rax, 32` = 8 floats/iter. Without `-ffast-math` you'd see a scalar
`vfmadd...ss` or `mulss/addss` chain (folder 33 lesson 05, 13).
</details>

### A6
```asm
p(std::atomic<int>*, int):
        mov     eax, esi
        lock xadd DWORD PTR [rdi], eax
        ret
```

<details><summary>Answer</summary>

`int p(std::atomic<int>* a, int delta) { return a->fetch_add(delta); }` —
an **atomic fetch-and-add**. `lock xadd [mem], eax` atomically does
`tmp = [mem]; [mem] = [mem] + eax; eax = tmp` — the `lock` prefix makes it a
single atomic RMW (locks the cache line / bus). `fetch_add` returns the
**old** value, which is why `xadd` (exchange-and-add) puts the previous
`[mem]` into `eax`. Any memory order ≤ `seq_cst` compiles to the same `lock
xadd` on x86 (x86-TSO — the `lock` prefix is already a full barrier; folder
27). A `relaxed` `fetch_add` is *also* `lock xadd` — x86 has no cheaper
atomic RMW.
</details>

---

## Part B — Verification

### B1
You added `__restrict` to `saxpy(float* y, const float* x, float a, int n)`.
The asm loop body is: `movss xmm1, [rsi+rax*4]` / `mulss xmm1, xmm2` /
`addss xmm1, [rdi+rax*4]` / `movss [rdi+rax*4], xmm1` / `add rax, 1`. Did
`__restrict` work? What now?

<details><summary>Answer</summary>

`__restrict` **removed the aliasing barrier** (otherwise the compiler would
insert a runtime overlap check + two versions, or refuse), but the loop is
still **scalar** (`ss` ops, `add rax, 1`). So `__restrict` was necessary but
not sufficient — something else is blocking vectorization. Most likely:
**`-march` is baseline** (no `-mavx`/`-msse` beyond SSE2 doesn't explain
scalar though)... more likely the **cost model** decided against it for the
given `n`, or there's an `-fno-tree-vectorize` / `-Os` in effect, or an
in-body issue. Run `g++ -O2 -fopt-info-vec-missed` — if it says "cost model"
try `-O3` or `-fvect-cost-model=cheap`; if it says nothing and you built
with `-Os`, switch to `-O2`; check no `-D_GLIBCXX_ASSERTIONS` bounds check.
Once it vectorizes you'll see `mulps`/`addps` (SSE) or `vfmadd...ps` (with
`-mfma`) and `add rax, 16`/`32`.
</details>

### B2
You marked a leaf class `Circle final` and call `s.area()` where `s`'s static
type is `Circle&`. Asm: `mov rax, [rdi]` / `call [rax+16]`. Did
devirtualization happen?

<details><summary>Answer</summary>

**No** — `mov rax, [rdi]` (load vptr) + `call [rax+16]` (call through the
vtable) is still a **virtual dispatch**. `final` on the class should have let
the compiler fix the target (no derived class can override `area`), so
either: (1) `area` itself isn't `override`/isn't the one being called
through a base pointer the compiler can't prove is a `Circle`, (2) the
compiler couldn't see the full picture (definition of `area` in another TU,
no `-flto`), or (3) `s` is actually bound to a `Circle` via a `Shape&`
somewhere the analysis lost track. Fixes: ensure the `override` is also
visible (header / `-flto`), confirm the static type at the call site is
really `Circle` (not up-cast), and check with `-fdump-ipa-devirt`. With
`final` + visible definition + concrete-enough type, you should get a direct
`call Circle::area` or an inlined `imul` (folder 33 lesson 07).
</details>

### B3
You added `[[unlikely]]` to the error branch of a hot function. Binary got
*bigger* (`size` shows `.text` +200 bytes) but the hot loop is the same. Bug
or working?

<details><summary>Answer</summary>

**Working as intended.** `[[unlikely]]` moves the cold code (the error
handler) **out of line** — often into a separate `.text.unlikely` section —
so the *hot* part of `.text` gets *smaller / denser* (better I-cache), while
*total* `.text` can grow slightly because the moved code plus the extra
`jmp`s to reach it take a few bytes. What matters: (1) is the hot loop body
now straight-line with the error path reached by a rarely-taken forward
`jmp`? (`./build.ps1 asm` — yes if the handler's `call` sits after the loop
/ `ret`). (2) does `perf stat --topdown` show Frontend Bound / L1-icache
misses drop under load? If the hot loop is unchanged and there's no I-cache
pressure, the hint is cosmetic here — keep it only for genuinely-rare error
paths (folder 33 lesson 08).
</details>

---

## Part C — Reasoning

### C1
`perf annotate` on a hot function: 3% here, 2% there, then **52% on a single
`vdivps ymm0, ymm1, ymm2`**. Diagnose + fix path.

<details><summary>Answer</summary>

A **packed floating-point division** (`vdivps`, ~14-20+ cycles, very poorly
pipelined even vectorized) is dominating — 52% of the function's time. It's
dividing 8 lanes of `ymm1` by 8 lanes of `ymm2` per instruction, so it *is*
vectorized, but `divps` throughput is terrible. Fix path: (1) if `ymm2` (the
divisor) is **loop-invariant** (a broadcast constant), replace `x / d` with
`x * (1/d)` — compute the reciprocal once (`vrcpps` + one Newton-Raphson
step for accuracy, or a real `1.0f/d` scalar broadcast) → `vmulps` (~4 cyc,
fully pipelined). (2) if divisors are per-element but you can tolerate ~11-12
bits of precision, `vrcpps` + one NR iteration is ~3x faster than `vdivps`.
(3) restructure so the division happens once outside the loop. `perf
annotate` after → the 52% should collapse (folder 33 lesson 04, folder 31
lesson 09).
</details>

### C2
Asm of a hot loop: the body has `mov [rsp+0x20], r13` / `mov [rsp+0x28],
r14` / (work) / `mov r13, [rsp+0x20]` / `mov r14, [rsp+0x28]` every
iteration. What is this, root cause, fix?

<details><summary>Answer</summary>

**Register spills inside the hot loop** — `r13` and `r14` are being parked
to stack slots and reloaded every iteration because the loop has more
simultaneously-live values than fit in the ~13 usable GP registers. Root
causes (most common): the loop was **over-unrolled** (many partial
accumulators + their addresses live at once — `-O3` or a `#pragma unroll`),
a **large struct held/passed by value**, or **too many manual accumulator
variables**. Each spill/reload pair is ~2 L1 accesses + adds stack traffic;
in a tight loop that's a real fraction of the time. Fixes: reduce the number
of live values (fewer accumulators — let the compiler pick, folder 33 lesson
04), lower the unroll factor (`-fno-unroll-loops` or a smaller `#pragma
unroll`), pass big objects by `const&`, or split the loop (fission) so each
part has fewer live values. Re-read the asm — the `[rsp+..]` churn should be
gone.
</details>

### C3
Two builds of the same hot loop. Build A: `vaddps ymm` (8-wide). Build B:
`vaddps xmm` (4-wide). Same source, only the compile flags differ. Name the
likely flag difference and which is faster.

<details><summary>Answer</summary>

Build A has **AVX/AVX2 enabled** (`-march=x86-64-v3` / `-mavx2` / a
`__attribute__((target("avx2")))`), so the vectorizer used 256-bit `ymm`
(8 floats). Build B is **SSE-only** (baseline `x86-64` / `-msse2` /
`-mno-avx`), so it's limited to 128-bit `xmm` (4 floats). **Build A is
faster per iteration** if the loop is compute-bound (twice the lanes → ~2x
throughput on the arithmetic). Caveats (folder 31/13, 33/12): if the loop is
**memory-bandwidth-bound** (working set >> LLC), both hit the same wall and
the width doesn't matter; and if Build A had gone all the way to **AVX-512
`zmm`**, the frequency downclock could make it a net loss system-wide. For
this xmm-vs-ymm case, ymm is the win — pin `-march` to your production CPU's
level so you get it (folder 33 lesson 12).
</details>

---

## Part D — Hands-on (compile, read, observe)

### D1 — The loop-shape gallery
`./build.ps1 asm 34-ASSEMBLY/examples/02_reading_loops.cpp`. For each of the
6 functions, write down: (a) is there an entry guard? (b) where's the
backward branch? (c) is it vectorized (packed op + `add ptr, 16/32`)? (d)
any `call`? (e) for `sum_if_positive`, is it `cmovg` (if-converted) or a real
`jg`? Compare with the header comments in the file.

### D2 — ABI comparison
Take a 5-argument function and read its asm both ways: `./build.ps1 asm`
(this box, Win64 → args `rcx, rdx, r8, r9, [rsp+0x28]`) and godbolt (SysV →
`rdi, rsi, rdx, rcx, r8`). Note which register is arg 1, where the 5th arg
lives, and — for a member function — where `this` is.

### D3 — `__restrict` before/after
Write `void triad(float* c, const float* a, const float* b, float k, int n)`
as `[[gnu::noinline]]`. `./build.ps1 asm` it, then add `__restrict` to all
three pointers and diff. Report: scalar→packed, `add rax, 1`→`add rax, 16`,
and run `-fopt-info-vec-missed` before/after to see the "possible aliasing"
message disappear.

### D4 — Devirtualization ladder
`./build.ps1 asm 34-ASSEMBLY/examples/03_virtual_call_asm.cpp`. For each of
`via_virtual`, `via_direct`, `via_crtp`, `via_known_concrete`: is there a
`call`? indirect (`call [rax+..]`) or direct? inlined to an `imul`? Match to
the file's expected output. Then try `-flto` on a 2-file split and see if
`via_virtual`-style calls devirtualize.

### D5 — `rdtsc` flavours in asm and in ns
`./build.ps1 asm 34-ASSEMBLY/examples/05_rdtsc.cpp` — find the `rdtsc`,
`rdtscp`, and `lfence` instructions in `tsc_plain` / `tsc_lfenced` /
`tsc_p`. Then `./build.ps1 fast` it and record: calibration (ticks/ns),
plain vs fenced self-cost. Explain why plain is ~1 tick and fenced ~20.

### D6 — Constant folding
Compile `int f() { int s = 0; for (int i = 1; i <= 20; ++i) s += i*i; return
s; }` and read `f`'s asm. It should be `mov eax, <constant> ; ret`. Now make
the bound come from `argv` (opaque) and add `DoNotOptimize` — the loop
reappears. This is folder 33 lesson 06/14, seen in asm.

---

## Challenge

Open-ended — answer key nahi. Har claim ke saath asli `objdump` / `-S` output ka tukda paste karo.

### Challenge 1 — bina source ke reverse engineering
Ek dost (ya ek hafte baad aap khud) ke liye 5 functions `-O2` pe ek object file mein compile
karo, source chhupa do: (1) `clamp`, (2) popcount wala loop, (3) struct array pe field sum,
(4) dense `switch` (8+ cases), (5) ek virtual call. Sirf `objdump -d -M intel` padh ke yeh
nikaalo:
- struct ka `sizeof` aur field offsets (file 05 — addressing modes se)
- loop kitni baar chalta hai — formula
- kaunsa branch `cmov` ban gaya, kaunsa jump table (file 08)
- virtual call ka vtable slot index (file 08, folder 25)

Aakhir mein source se milao — kahan galat andaaza lagaya, aur kyun?

### Challenge 2 — optimization levels ka zoo
Ek hi function (array pe `sum += a[i] * b[i]`) ko `-O0`, `-O1`, `-O2`, `-O3`, aur
`-O3 -march=native` pe compile karo. Table banao: instructions ki ginti, branches, stack
spills, aur vectorized hai ya nahi (packed `ps`/`pd` suffix, `ymm`/`zmm` register — file 09).
Har do levels ke beech **ek line** mein batao ki kya badla aur kyun.

### Challenge 3 — calibrated `rdtsc` timer
`examples/05_rdtsc.cpp` se aage badho: `rdtscp` pe ek chhota timer banao jo startup pe TSC ko
`steady_clock` ke against ns mein calibrate kare (file 11). Naapo:
1. Timer ka apna overhead (khali start/stop ka ns) — best, p50, p99.
2. `cpuid` se serialize karne ki kimat vs bina serialize.
3. `steady_clock::now()` ka overhead isi machine pe.

Batao kab `rdtsc` sahi tool hai aur kab `clock_gettime`/`steady_clock` (core hopping,
invariant TSC flag).

---

## Interview questions (folder-wide)

1. Three reasons to read assembly; what you do NOT need to know.
2. The 16 GP registers, the sub-register zeroing rule, `xmm`/`ymm`/`zmm`.
3. AT&T vs Intel — the 5 differences; which tools give which.
4. The ~20 common instructions; `lea` vs `mov [..]`; `cmp`/`test`.
5. Memory-operand formula; recovering `sizeof`/field offsets from a loop.
6. Stack frame: prologue/epilogue with/without frame pointer; spills; tail calls.
7. SysV vs Win64 calling conventions; where `this` is; large-struct return.
8. Asm shapes: `if` / if-converted / loop / `while` / `switch` (dense & sparse) / call / virtual call / constant-folded / division.
9. SIMD asm: scalar vs packed suffix; width from `add ptr, N`; the reduce cluster; `vzeroupper`; `vgather`.
10. Inline asm: the 4 sections; `"=r"`/`"+r"`/`"memory"`/`"cc"`; missing-clobber bug; when to use vs intrinsics.
11. `rdtsc`: what it counts; fencing; ticks vs cycles vs ns; core hopping; when to use `clock_gettime` instead.
12. Tools: `-S` vs `objdump -dS` vs `perf annotate` vs `gdb` vs `addr2line` vs `llvm-mca` — which for which question.

---

## Next
→ [`../35-PROFILING-BENCHMARKING/00-README.md`](../35-PROFILING-BENCHMARKING/00-README.md)
