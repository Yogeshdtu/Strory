# 03 — Instruction set: ISA, x86-64, CISC vs RISC, microcode

## Prerequisites
- `02-registers.md`
- `24-COMPILATION-LINKING` (assembler: asm → machine code)

## Yeh topic abhi kyun
**ISA** (Instruction Set Architecture) = CPU aur software ke beech ka contract:
kaunse instructions, kaunse registers, memory model, kaise encode hote. Tumhara
compiler ISA ki taraf compile karta; CPU ISA ko implement karti. HFT ke liye
samajhna zaroori: kaunsa operation ek instruction hai (sasta) vs kai (mehnga),
aur "microcode" wale rare instructions (jinse door rehte hain).

---

## ISA kya define karta

| Aspect | x86-64 |
|---|---|
| **Registers** | 16 GP × 64-bit, 16/32 SIMD, flags, RIP (file `02`) |
| **Instructions** | ~1000+ mnemonics (`mov`, `add`, `imul`, `vaddps`, `lock cmpxchg`, `rdtsc`...) |
| **Encoding** | variable length, **1–15 bytes** (prefixes + opcode + ModR/M + SIB + disp + imm) |
| **Addressing modes** | `[base + index*scale + disp]` — one instruction can do address math |
| **Memory model** | **x86-TSO** — strong ordering; only store→load can reorder (folder 27) |
| **Endianness** | little-endian |
| **Calling convention** | ABI, not ISA — System V AMD64 on Linux (folder 08) |

Ek **µarch** (microarchitecture — Zen 4, Golden Cove, ...) ISA ka ek
**implementation** hai: pipeline depth, port count, cache sizes, predictors —
sab µarch-specific (file `15`), ISA same rehta.

---

## CISC vs RISC

| | CISC (x86) | RISC (ARM, RISC-V) |
|---|---|---|
| Instructions | many, complex, variable-length | few, simple, fixed-length (usually 4 B) |
| Memory access | any instruction can (`add [mem], reg`) | only `load`/`store` (load-store architecture) |
| Decode | hard (variable length, prefixes) | easy (fixed, parallel) |
| Code density | higher | lower |
| Registers | 16 GP | 31 GP (ARM64) |

**Modern reality:** the CISC/RISC line is blurry. x86 CPUs decode complex
instructions into **µops** (micro-operations) that look RISC-like, and the OoO
core (file `06`) schedules µops. ARM decode is simpler so ARM can be wider/
lower-power at the front-end, but both have deep OoO backends.

### µops and macro-fusion / micro-fusion
- **Decode:** `add rax, [rbx]` → 1 fused µop that both loads and adds (**micro-
  fusion** — 1 slot in the ROB, splits to 2 in execution).
- **Macro-fusion:** `cmp rax, rbx` + `je label` → **1 µop** (compare-and-branch).
  The compiler emits them adjacent so the CPU fuses them. Yeh why "compare then
  branch" is basically free.
- `./build.ps1 asm` shows the instructions; the µop count is µarch-specific
  (uops.info / `llvm-mca` / `perf` for real µop counts).

---

## Microcode — the slow path

Most instructions are **hardwired** (fast, fixed µop count). But some complex/
rare ones are implemented by a **microcode ROM** — a tiny program of µops the
CPU runs:

| Microcoded / assist-heavy | Approx cost |
|---|---|
| `div` / `idiv` (64-bit) | ~20–45 cycles, not pipelined (file `09`) |
| `rep movsb` / `rep stosb` (short) | overhead; `memcpy`/`memset` use these for big blocks (fast-string) |
| `gather` / `scatter` (AVX2/512) | many µops; often not worth it |
| **denormal float** result/input | microcode assist ~100–200 cycles (file `13`, file `02`) |
| `cpuid`, `rdmsr`, `wrmsr`, `xsave` | serializing, slow |
| x87 transcendentals (`fsin`, `fyl2x`) | 50–150+ cycles (avoid; use libm / poly approx) |
| sub-normal / NaN-heavy FP paths | assists |

**HFT rule:** know which of your operations are microcoded. `div` → reciprocal
multiply or a shift. Transcendentals → polynomial approximations (or precomputed
tables). Denormals → flush-to-zero mode. `gather` → restructure to contiguous
(SoA, folder 32).

---

## Instruction categories you'll see (folder 34 goes deep)

| Category | Examples |
|---|---|
| Data movement | `mov`, `lea` (address calc, no memory access!), `push`/`pop`, `movzx`/`movsx` |
| Integer ALU | `add`, `sub`, `imul`, `inc`, `and`/`or`/`xor`, `shl`/`shr`/`sar`, `neg` |
| Integer div | `div`, `idiv` — the expensive one |
| Compare/branch | `cmp`, `test`, `je`/`jne`/`jl`/`jg`..., `jmp`, `call`/`ret` |
| Conditional move | `cmove`, `cmovg`... — **branchless** (file `07`, `08`; example `03`) |
| SIMD | `movdqu`, `paddd`, `pmulld`, `vaddps`, `vfmadd...`, `pshufb`, `pcmpeqd` (files `10`, `11`) |
| Bit manipulation | `popcnt`, `lzcnt`/`tzcnt`, `bsf`/`bsr`, `pdep`/`pext` (BMI2), `bt`/`bts` |
| Atomics | `lock add`, `lock cmpxchg`, `xchg` (implicit lock), `mfence`/`lfence`/`sfence` (folder 27) |
| Timing | `rdtsc`, `rdtscp`, `rdpmc` (file `08`; example `08`) |
| System | `syscall`, `hlt`, `cli`/`sti`, `invlpg`, `wbinvd` (ring 0) |

`lea` is a favourite compiler trick: `lea rax, [rdi + rsi*4 + 8]` computes
`rdi + rsi*4 + 8` in **one ALU op, no memory touch** — used for arithmetic, not
just addresses (`x*5` → `lea rax, [rdi + rdi*4]`).

---

## Extensions and `-march`

x86-64 baseline (2003) = SSE2. Since then: SSE3/SSSE3/SSE4, **AVX** (2011),
**AVX2 + FMA + BMI2** (2013), **AVX-512** (2016, fragmented). Also `POPCNT`,
`AES-NI`, `SHA`, `RDRAND`, `RDSEED`, `ADX`, ...

```bash
gcc -march=native -Q --help=target | grep -E 'sse|avx|bmi|fma' | grep enabled   # is box
gcc -march=x86-64-v2   # SSE4.2 + POPCNT baseline (2009+)
gcc -march=x86-64-v3   # + AVX2 + FMA + BMI2 (2013+; the common HFT baseline)
gcc -march=x86-64-v4   # + AVX-512
```

**Example `07`** reads these bits at runtime via CPUID. `-march=native` bakes in
*this* box's set → binary #UD-crashes on an older CPU (file `15`).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — integer `/` and `%` in a hot loop
Compiles to `div`/`idiv` — ~20–45 cycles, not pipelined (file `09`). Power-of-two
→ shift/mask (compiler does it if the divisor is a constant). Non-power-of-two
constant → compiler emits a **reciprocal multiply** (magic number). Runtime
divisor → you do it: cache `1.0/d` and multiply, or Lemire's fast-mod.

### Trap 2 — `std::pow`, `std::exp`, `std::sin` on the hot path
`libm` calls, tens–hundreds of cycles, and a function-call boundary. Polynomial
approximation, precomputed table, or restructure to avoid.

### Trap 3 — assuming `memcpy(x, y, 5)` is a `rep movsb`
Small fixed-size `memcpy` → the compiler inlines it as 1–2 `mov`s. Big/unknown
size → `rep movsb` (fast on modern CPUs with ERMS) or a SIMD loop. `-O2` handles
it well; don't hand-roll byte loops.

### Trap 4 — `-march=native` in a shipped binary
Runs only on ≥ this µarch. Build for the deployment floor (`x86-64-v3` is a safe
HFT baseline); use function multiversioning (`target_clones`) + a startup CPUID
assert (example `07`) for the rest.

### Trap 5 — thinking one C++ operator = one instruction
`a % b` (2 instructions + `div`), `a * b` where both are `int64` (1 `imul`),
`v.at(i)` (bounds check branch + load), `shared_ptr` copy (atomic increment +
possible atomic decrement + branch). Look at the asm.

### Trap 6 — serializing instructions in the hot path
`cpuid`, `mfence`, `lock`-prefixed ops, `rdtscp` (partially) drain/serialize the
pipeline. One per timestamp is fine; a `lock` op per hot iteration is a
contended-atomic smell (folder 28).

---

## > **HFT relevance**

> - **Audit the hot path's instruction mix** — `./build.ps1 asm`, `llvm-mca`,
>   `perf annotate`. Look for `div`/`idiv`, `call` to `libm`, `rep` string ops,
>   `gather`/`scatter`, and unexpected spills (file `02`).
> - **Replace division:** constant divisor → let the compiler; runtime divisor →
>   reciprocal-multiply (`d_inv = 1.0/d` once, then `x * d_inv`), or integer
>   fast-mod (Lemire).
> - **Replace transcendentals:** a degree-3–5 polynomial (Chebyshev / minimax)
>   for the input range you actually use, or a lookup table + interpolation.
> - **Pin `-march`** to your deployment floor (`x86-64-v3` / `-march=znver3` /
>   `-march=icelake-server` as appropriate), with a runtime CPUID check.
> - **Fuse-friendly code:** `if (a == b)` right before the branch it guards lets
>   the CPU macro-fuse `cmp` + `je` into one µop — cheap. Don't compute the
>   condition far from its use.

---

## Hands-on

```bash
# what one C++ operator becomes
cat > /tmp/ops.cpp <<'EOF'
int   rem(int a,int b){ return a % b; }             // -> idiv
long  mulc(long a){ return a * 10; }                // -> lea/imul, no div
int   divc(int a){ return a / 7; }                  // -> reciprocal multiply, no div
int   pc(unsigned x){ return __builtin_popcount(x);}// -> popcnt (with -mpopcnt)
double p(double a){ return __builtin_exp(a); }      // -> call exp
EOF
g++ -std=c++20 -O2 -S -masm=intel /tmp/ops.cpp -o - | grep -E 'rem\(|mulc\(|divc\(|pc\(|p\(|idiv|imul|lea|popcnt|call'

# this box's enabled ISA extensions
g++ -march=native -Q --help=target 2>/dev/null | grep -E '(avx2|fma|bmi2|avx512).*\[enabled\]'
./build.ps1 fast 31-CPU-ARCHITECTURE/examples/07_cpu_info.cpp
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "har C++ operator = 1 instruction" | `%` = idiv (~30 cyc); `pow` = a `call`; `.at()` = branch + load |
| "CISC = slow, RISC = fast" | both decode to µops + deep OoO; front-end differences |
| "microcode is old x86 baggage only" | `div`, denormals, `gather`, transcendentals still hit it |
| "`-march=native` is the release setting" | crashes on older CPUs; build for the floor + CPUID check |
| "`memcpy` small = slow rep movsb" | inlined as 1-2 `mov`s for small fixed sizes |
| "macro-fusion is automatic regardless" | needs `cmp`/`test` immediately before the `j*` |

---

## Exercises

1. `unsigned h = x % 1024;` vs `unsigned h = x % 1000;` — kaunsa `div` emit
   karega, kaunsa nahi, aur kyun?

   <details><summary>Answer</summary>

   `x % 1024` (power of two) → `x & 1023`, a single `and` — no `div`. `x % 1000`
   (constant, not power of two) → the compiler emits a **reciprocal multiply**:
   `(x * magic) >> shift` then multiply-back-and-subtract — ~4-5 cheap
   instructions, still no `div`. You only get an actual `div`/`idiv` when the
   divisor is a **runtime value** the compiler can't see. So `x % v` (v a
   variable) → `idiv` ~20-30 cycles; hoist `v` to a reciprocal if it's loop-
   invariant.
   </details>

2. Assembly mein `cmp rdi, rsi` ke turant baad `jl .L4` hai. CPU ispe kya
   optimization karta?

   <details><summary>Answer</summary>

   **Macro-fusion:** the decoder fuses the `cmp` + `jl` into a **single µop**
   that both compares and conditionally branches — it occupies one slot in the
   reorder buffer and one execution port. This is why "compare then branch" is
   essentially one operation. It only works when the `j*` immediately follows a
   fusable `cmp`/`test`/`add`/`sub`/`and`/`inc`/`dec` — so keep the condition
   computation adjacent to the branch.
   </details>

3. Tumhare pricing code mein `std::exp(-lambda * t)` har tick pe call hota hai.
   Do alternatives batao.

   <details><summary>Answer</summary>

   (1) **Polynomial / rational approximation** of `exp` over the range of
   `-lambda*t` you actually see (e.g. a minimax degree-4 poly, or the standard
   `exp` = `2^(x/ln2)` split into integer + fractional with a small poly on the
   fraction) — a few FMAs, ~10-20 cycles, no call, vectorizable. (2) **Lookup
   table** on `t` (if `lambda` is fixed and `t` is quantized to tick intervals)
   with linear interpolation. (3) If it feeds a comparison, sometimes you can
   compare in log-space and skip `exp` entirely. Measure the accuracy you
   actually need — full `libm` precision is rarely required for a trading
   signal.
   </details>

4. `-march=x86-64-v3` kya guarantee karta, aur ek HFT firm ise apna build
   baseline kyun rakhti?

   <details><summary>Answer</summary>

   `x86-64-v3` = SSE4.2 + **AVX + AVX2 + FMA + BMI1/BMI2 + LZCNT + MOVBE** — the
   feature set common to essentially every server CPU since ~2013 (Haswell / Zen
   1). Building to it means: (a) the auto-vectorizer can use 256-bit AVX2 and
   FMA everywhere (not just baseline SSE2), (b) `popcnt`, `tzcnt`, `pdep`/`pext`
   for bit tricks, (c) the binary still runs on any of the firm's boxes and DR
   sites without per-machine builds. AVX-512 (`v4`) is deliberately *not* the
   baseline — it's fragmented across vendors/generations and can cause frequency
   downclocking (file `13`).
   </details>

5. Ek instruction "serializing" hoti hai iska kya matlab, aur `mfence` /
   `cpuid` / `lock`-prefixed op mein se hot loop mein kaunsa sabse suspicious?

   <details><summary>Answer</summary>

   Serializing ≈ the CPU must complete all older instructions (and often drain
   the store buffer) before the serializing instruction executes, and no younger
   instruction starts early — it kills OoO overlap around that point.
   `mfence` (~20-30+ cycles) appears legitimately in `seq_cst` fences / some
   atomics (folder 27) — one per hand-off is acceptable. `cpuid` should never be
   in a hot loop (it's a startup / calibration thing). A **`lock`-prefixed op
   per hot iteration** is the most suspicious: it means you have a contended
   atomic on the fast path — the thing folder 28 exists to remove.
   </details>

---

## Interview questions

1. ISA vs microarchitecture — what each fixes, give an example of each.
2. CISC vs RISC today — why the distinction is blurry (µops).
3. Macro-fusion vs micro-fusion — what fuses, the benefit.
4. Microcode — 4 operations that hit it and their rough cost.
5. `lea` — why compilers use it for non-address arithmetic.
6. `-march=x86-64-v2/v3/v4` — what each level adds; the HFT baseline.
7. How would you get rid of a runtime integer division on the hot path?

---

## Next
→ [`04-pipelining.md`](04-pipelining.md)
