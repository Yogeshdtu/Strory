# 10 — Inline assembly: GCC extended asm, constraints, when it's needed (rarely)

## Prerequisites
- `02`–`09` of this folder
- `33-COMPILER-OPTIMIZATION/14-preventing-optimization.md` (the barrier)
- `06_inline_asm.cpp` example

## Yeh topic abhi kyun
Kabhi-kabhi aapko ek instruction chahiye jiska koi intrinsic ya builtin
nahi (`cpuid`, `rdtsc` variants, `pause`, port I/O in a driver), ya aapko
the **zero-instruction optimization barrier** (`DoNotOptimize`) chahiye. Uske
liye GCC extended inline asm. Par **99% cases mein aapko iski zaroorat nahi**
— intrinsics (`<immintrin.h>`, `__builtin_*`) better hain. Yeh lesson syntax
+ "kab" batata, taaki aap padh sako aur galti se corruption na karo.

---

## Syntax (GNU extended asm)

```cpp
asm [volatile] ( "assembly template"
                 : output operands        // "constraint"(cexpr), ...
                 : input operands         // "constraint"(cexpr), ...
                 : clobbers );            // "reg", "cc", "memory", ...
```

- **template** — the literal asm; `%0`, `%1`, ... refer to operands in
  declaration order (outputs first). `%%reg` for a literal register name.
- **outputs** — `"=r"(x)` write-only into a register bound to `x`; `"+r"(x)`
  read-write; `"=m"(x)` a memory operand.
- **inputs** — `"r"(a)` in a register; `"i"(5)` an immediate; `"m"(w)` memory.
- **clobbers** — registers the asm trashes that aren't operands; `"cc"` =
  flags; `"memory"` = "this asm may read/write arbitrary memory" (an
  optimization barrier for surrounding loads/stores).

### Constraint letters (x86)
| | meaning |
|---|---|
| `r` | any GP register | `m` | memory |
| `i` | immediate integer | `n` | known immediate |
| `a`,`b`,`c`,`d` | `rax`,`rbx`,`rcx`,`rdx` specifically | `S`,`D` | `rsi`,`rdi` |
| `x` | any `xmm` | `v` | any `xmm`/`ymm`/`zmm` |
| `=` | write-only output | `+` | read-write output |
| `&` | "early-clobber" — written before all inputs are consumed |

---

## `volatile` and ordering

- Without `volatile`, the compiler may **delete** the asm if its outputs are
  unused, or **hoist/CSE** it (if it looks pure). Use `volatile` when the asm
  has a side effect the compiler can't see (`rdtsc`, `pause`, MMIO).
- `asm volatile` still may be **reordered** relative to *unrelated* code.
  Add `"memory"` clobber to also fence loads/stores around it.
- It is **not** a CPU fence — `asm volatile("" ::: "memory")` is a
  *compiler* barrier only. For a hardware fence you need `mfence`/`lfence`/
  `sfence` in the template (folder 27, folder 34 lesson 11).

---

## The barrier (this repo's `keep()` / `DoNotOptimize`)

```cpp
template <class T> void DoNotOptimize(T const& v) {
    asm volatile("" : : "r,m"(v) : "memory");     // "someone reads v" -> compute it
}
template <class T> void DoNotOptimize(T& v) {
    asm volatile("" : "+r,m"(v) : : "memory");     // read-write escape
}
void ClobberMemory() { asm volatile("" : : : "memory"); }   // "all memory changed"
```
Empty template → **zero instructions**. The constraints/clobber are pure
information to the optimizer (folder 33 lesson 14). Note: `"+r,m"` can hit
"impossible constraint" if the compiler proved `v` is a constant with no
storage — bind it to a genuine runtime variable, or use `"+r"` alone.

---

## Legitimate uses (roughly in order of how often)

1. **The barrier** — `DoNotOptimize` / `ClobberMemory` (above). Every
   micro-benchmark.
2. **`cpuid`** — no direct intrinsic that's this raw (`__get_cpuid` exists but
   the inline form is common). `06_inline_asm.cpp`.
3. **`rdtsc` / `rdtscp` variants with specific fencing** — `__rdtsc()`
   exists, but you may want `lfence; rdtsc; lfence` inlined precisely
   (folder 34 lesson 11).
4. **`pause`** (spin hint) — `_mm_pause()` intrinsic exists; the inline form
   `asm volatile("pause" ::: "memory")` is equivalent.
5. **Reading/writing MSRs, port I/O, control registers** — kernel/driver only
   (`rdmsr`, `wrmsr`, `in`, `out`, `cli`/`sti`).
6. **A tiny hot sequence the compiler consistently mis-schedules** — very
   rare, must be measured, and revisited on every compiler upgrade.
7. **Naked functions / trampolines / context switches** — `__attribute__
   ((naked))`, hand-written entry stubs.

---

## When NOT to (almost always)

- **A SIMD kernel** → intrinsics (`<immintrin.h>`). Portable, the compiler
  schedules/allocates registers, retargets to NEON/SVE. Hand asm here is a
  maintenance and correctness liability.
- **"To make it faster"** → measure first; the compiler usually wins.
  Restructure the C++ (folder 33) before reaching for asm.
- **Bit tricks** → `__builtin_popcount`, `__builtin_clz`, `__builtin_ctz`,
  `__builtin_bswap`, `__builtin_add_overflow`, `std::rotl` — all lower to
  the right instruction and are portable.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — missing a clobber
The asm trashes `rcx` but you didn't list it → the compiler thinks `rcx`
still holds its value → **silent corruption**, and it shows up only at `-O2`
when `rcx` was carrying something live. List every non-operand register the
template writes, plus `"cc"` if it sets flags.

### Trap 2 — missing `"memory"`
Your asm writes to `*ptr` via `"r"(ptr)` but you didn't say `"memory"` → the
compiler may keep a stale cached copy of `*ptr` in a register across the asm.

### Trap 3 — no `volatile` on a side-effecting asm
`rdtsc` without `volatile` → the compiler CSEs two reads into one, or moves
it out of your timing region. Always `volatile` for `rdtsc`/`pause`/MMIO.

### Trap 4 — Intel-syntax template in a default (AT&T) build
`asm("lea %0, [%1+8]")` fails to assemble under default AT&T. Use AT&T
(`lea 8(%1), %0`), or `asm(".intel_syntax noprefix\n lea %0,[%1+8]\n
.att_syntax")`, or compile with `-masm=intel` (and then it breaks on Linux).
Prefer AT&T in inline asm for portability.

### Trap 5 — `"+r,m"` "impossible constraint"
Happens when the operand is a proven constant / has no storage at `-O2`.
Bind it to a real runtime lvalue, or drop the `,m` (`"+r"`).

### Trap 6 — assuming `asm volatile("" ::: "memory")` is a CPU fence
It's a **compiler** barrier. For hardware ordering you need an actual fence
instruction (`mfence`) or a `lock`ed op (folder 27).

---

## > **HFT relevance**

> - **`DoNotOptimize` / `ClobberMemory`** — the only inline asm most HFT code
>   needs, in every internal benchmark (folder 33 lesson 14).
> - **`rdtsc` with precise fencing** for tick-to-trade instrumentation
>   (lesson 11) — sometimes hand-inlined to control exactly where the
>   `lfence`s land.
> - **`pause`** in spin-wait loops (folder 28) — `_mm_pause()` is fine.
> - **Everything else → intrinsics.** A hand-asm SIMD kernel in a trading
>   codebase is a liability: not portable across the fleet's CPU
>   generations, breaks on toolchain upgrades, and the compiler's scheduler
>   is usually better. Use `<immintrin.h>` + `target`/multi-versioning
>   (folder 31/11, folder 33/12).
> - **Audit any inline asm on review** — a missing clobber is a
>   heisenbug that only appears under a specific register-allocation.

---

## Hands-on

```bash
./build.ps1 fast 34-ASSEMBLY/examples/06_inline_asm.cpp     # barrier / cpuid / rdtsc / lea / pause
./build.ps1 asm  34-ASSEMBLY/examples/06_inline_asm.cpp     # DoNotOptimize -> ZERO instructions

# a missing-clobber bug demo (don't ship this):
echo 'long f(long a){ long r; asm("mov %1,%%rcx; imul $3,%%rcx; mov %%rcx,%0"
      : "=r"(r) : "r"(a) /* MISSING: "rcx" clobber, "cc" */); return r; }' \
 | g++ -O2 -S -masm=intel -xc++ - -o -
#   at -O2 with rcx live elsewhere, this corrupts. Correct: add : "rcx","cc"
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "inline asm makes it faster" | measure; the compiler usually wins; restructure C++ first |
| "I need asm for SIMD" | use `<immintrin.h>` intrinsics — portable, scheduled for you |
| "`volatile` isn't needed" | it is, for any side-effecting asm (rdtsc/pause/MMIO) |
| "the clobber list is optional" | a missing clobber = silent `-O2` corruption |
| "`asm volatile(\"\":::\"memory\")` fences the CPU" | compiler barrier only; need `mfence` for hardware |
| "Intel syntax in the template is fine" | default build is AT&T; use AT&T or `.intel_syntax` blocks |

---

## Exercises

1. `asm volatile("rdtsc" : "=a"(lo), "=d"(hi));` — kaunse registers implied,
   `volatile` kyun, aur ek missing safety kya?

   <details><summary>Answer</summary>

   `"=a"(lo)` binds the output `lo` to `eax`, `"=d"(hi)` binds `hi` to `edx`
   — because `rdtsc` writes the low 32 bits of the counter to `eax` and the
   high 32 to `edx` (that's the instruction's contract). `volatile` is
   required because `rdtsc` has no visible operands the compiler can track —
   without it, two `rdtsc` reads look identical (pure) and get CSE'd into
   one, or the read is hoisted/sunk out of your timing region. **Missing
   safety**: no `lfence` / serialization — `rdtsc` can execute out-of-order,
   so the timestamp may not bracket the code you think it does. For precise
   short-interval timing, add `_mm_lfence()` (or use `rdtscp` + `lfence`)
   around it (lesson 11).
   </details>

2. Kisi ne `DoNotOptimize` ko `asm volatile("" : "=r"(v) : :)` likha (`=`
   instead of `+`, no `"memory"`). Do problems.

   <details><summary>Answer</summary>

   (1) **`"=r"` is write-only** — it tells the compiler the asm *produces* a
   fresh value for `v` and doesn't read the old one. So the computation that
   set `v` before the barrier is now **dead** (its result is "overwritten" by
   the asm) → the compiler can DCE exactly what you were trying to protect.
   You need `"+r"` (read-write): "the asm reads `v`, so compute it". (2) **No
   `"memory"` clobber** — surrounding stores (e.g. a fill loop whose "use" is
   the memory it wrote) can still be eliminated, because the barrier doesn't
   claim to touch memory. For `DoNotOptimize` on a value use `"+r,m"(v)` (and
   optionally `"memory"`); for `ClobberMemory` use `"" ::: "memory"`.
   </details>

3. Inline asm mein `imul $10, %%rax` likha bina `%%rax` ko clobber ya operand
   banaye. `-O0` pe test pass, `-O2` pe random wrong answers. Kyun?

   <details><summary>Answer</summary>

   The template writes `%%rax` (a literal register) but `rax` is **not
   listed** as an output operand or in the clobber list. At `-O0` the
   compiler keeps almost nothing in registers across statements, so `rax`
   happens to be free when the asm runs → it "works" by luck. At `-O2` the
   register allocator keeps live values in registers — it may have put a
   loop counter, an accumulator, or `this` in `rax` right before your asm,
   and it doesn't know the asm destroys it → after the asm, code uses the
   clobbered `rax` as if it still held the old value → wrong results, and
   only for whatever register pressure / allocation the `-O2` build happened
   to produce (so it's flaky). Fix: make `rax` an operand (`"+a"(x)` /
   `"=a"(out)`), or if it's genuinely scratch, add it to the clobber list:
   `: : ... : "rax", "cc"`. `imul` also sets flags → `"cc"` too.
   </details>

---

## Interview questions

1. GNU extended asm — the 4 sections (template, outputs, inputs, clobbers).
2. `"=r"` vs `"+r"` vs `"=m"` — meanings.
3. `"memory"` clobber and `"cc"` — what each tells the compiler.
4. When is `volatile` on `asm` required?
5. `asm volatile("" ::: "memory")` — compiler barrier vs CPU fence.
6. A missing clobber — why it's a `-O2`-only, flaky bug.
7. When to use inline asm vs an intrinsic (and the default answer).

---

## Next
→ [`11-rdtsc-and-timing.md`](11-rdtsc-and-timing.md)
