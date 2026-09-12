# 02 — Registers: GP, special, SIMD

## Prerequisites
- `01-how-cpu-works.md`
- `12-POINTERS` (addresses vs values), `05-OPERATORS` (bits)

## Yeh topic abhi kyun
Registers = CPU ke andar ki chhoti, super-fast storage jahan **actual
computation** hota. Har `a + b` pehle `a` aur `b` ko registers mein laata hai.
Inki ginti chhoti hai (~16 GP), aur compiler ka aadha kaam "kaunsi value kaunse
register mein, kab" (register allocation) hai. Yeh samajhna assembly padhne
(folder 34), calling conventions (folder 08), aur SIMD (file `10`) ke liye base.

---

## x86-64 general-purpose registers (16)

| 64-bit | 32-bit | 16 | 8 | Convention (System V AMD64 ABI) |
|---|---|---|---|---|
| `rax` | `eax` | `ax` | `al` | return value; `imul`/`div` implicit |
| `rbx` | `ebx` | `bx` | `bl` | callee-saved |
| `rcx` | `ecx` | `cx` | `cl` | 4th arg; shift count |
| `rdx` | `edx` | `dx` | `dl` | 3rd arg; `div` high half |
| `rsi` | `esi` | `si` | `sil` | 2nd arg |
| `rdi` | `edi` | `di` | `dil` | 1st arg |
| `rbp` | `ebp` | — | — | frame pointer (ya callee-saved GP) |
| `rsp` | `esp` | — | — | **stack pointer** (never general use) |
| `r8`–`r15` | `r8d`… | `r8w`… | `r8b`… | `r8`/`r9` = 5th/6th arg; `r12`–`r15` callee-saved |

- **64-bit ops zero the upper 32** of the destination (writing `eax` clears
  `rax[63:32]`) — but 8/16-bit writes **merge** (partial-register stall risk).
- `rsp` grows **down** (folder 08). `push` = `rsp -= 8; [rsp] = val`.

### Special registers
- **`rip`** — instruction pointer (file `01`). `lea rax, [rip+off]` = RIP-relative
  addressing (PIC, folder 24).
- **`rflags`** — condition flags: `ZF` (zero), `CF` (carry), `SF` (sign), `OF`
  (overflow), `PF` (parity). `cmp a, b` sets flags; `je`/`jl`/`cmovg` read them.
- **Segment registers** (`fs`, `gs`) — mostly vestigial on x86-64, but `fs`/`gs`
  base used for **thread-local storage** (folder 26) and the kernel's per-CPU
  data.
- **Model-Specific Registers (MSRs)** — `rdmsr`/`wrmsr` (ring 0): TSC frequency,
  performance counters, `MSR_LSTAR` (syscall entry, folder 29).

---

## SIMD registers (file `10`, `11`)

| Name | Width | Introduced | Lanes (float / double) |
|---|---|---|---|
| `xmm0`–`xmm15` | 128-bit | SSE | 4 f32 / 2 f64 |
| `ymm0`–`ymm15` | 256-bit | AVX | 8 f32 / 4 f64 |
| `zmm0`–`zmm31` | 512-bit | AVX-512 | 16 f32 / 8 f64 |

- `xmm` = low half of `ymm` = low quarter of `zmm` (aliased).
- AVX-512 also adds **mask registers** `k0`–`k7` (per-lane predication).
- **`mxcsr`** — SSE/AVX control/status (rounding mode, FP exception flags,
  flush-to-zero / denormals-are-zero — the last two matter for HFT, file `13`).
- **Example `07`** detects which of these your CPU supports (this box:
  AVX2 yes, AVX-512 no).

The old x87 FPU stack (`st0`–`st7`) still exists but is legacy — modern x86-64
does scalar float in `xmm` registers, not x87.

---

## Register pressure aur spilling

Sirf ~15 usable GP registers. Agar ek function ko ek waqt pe 20 live values
chahiye → compiler kuch ko **stack pe "spill"** karega (store), phir baad mein
reload. Har spill/reload = ek L1 access (~4-5 cycles) jahan register 0 hota.

```cpp
// bahut saare live locals + no inlining -> spills
double f(double a,double b,double c,double d,double e,double g,double h,double k) {
    double x1 = a*b, x2 = c*d, x3 = e*g, x4 = h*k;
    double y1 = x1+x2, y2 = x3+x4;
    // ... aur zyada intermediates ...
}
```

Compiler flags/hints jo help karte: `-O2`+ (good allocator), keeping functions
small (inlining frees the callee's register needs), not taking addresses of
locals unnecessarily (`&x` forces `x` to memory). `./build.ps1 asm` mein bahut
`mov [rsp+..], reg` / `mov reg, [rsp+..]` = spilling.

### Register renaming (file `06` preview)
The 16 **architectural** registers are a fiction — the CPU has a **physical
register file** of 100-300+ entries and *renames* `rax` to a different physical
register each time you write it. Yeh "write-after-write" / "write-after-read"
false dependencies ko todta, aur OoO execution enable karta. Tumhare code mein
`rax` ko baar-baar reuse karna isi liye theek hai.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — 8/16-bit partial register writes
`mov al, 5` (8-bit) `rax` ke upper bits **merge** karta → CPU ko purani `rax`
value ke saath dependency banani padti (partial-register stall). `movzx eax, ...`
(zero-extend to 32, which clears the top) prefer. Compilers isse handle karte,
par hand asm mein trap.

### Trap 2 — `rsp` ko general register maan lena
`rsp` = stack pointer. Usse arithmetic ke liye use karna → stack corrupt, next
`call`/`ret`/signal delivery crash. `rbp` bhi frame pointer role mein sensitive
(par `-fomit-frame-pointer` se woh free ho jaata).

### Trap 3 — "zyada local variables = zyada registers = tez"
Ulta — register pressure badhta, spills aate. Compiler ko chhote scopes, chhote
functions, aur clear dependency structure do.

### Trap 4 — `&local` lena jab zaroori nahi
`int x = 5; foo(&x);` — ab `x` **memory mein** rehna padta (address escape),
register mein nahi. Pass by value / `const&` jab possible.

### Trap 5 — SIMD register aliasing bhoolna
`ymm0` likhne se `xmm0` bhi change hota (low half). AVX/SSE mix karne pe
**transition penalty** (dirty upper state) — `vzeroupper` chahiye jab AVX code
se SSE-only code mein jao. Compilers `-mavx` ke saath handle karte.

### Trap 6 — thread-local access "free" maan lena
`thread_local` variable ka access `fs:` / `gs:`-relative load hai (folder 26) —
usually cheap, par ek extra indirection vs a plain global. Hot path pe count.

---

## > **HFT relevance**

> - **Keep hot functions small so they fit the register file** — inlining the
>   whole tick→order path lets the compiler allocate across it without spills at
>   every call boundary.
> - **Avoid taking addresses of hot locals** — `&x` forces memory; pass values.
> - **`mxcsr`: set flush-to-zero + denormals-are-zero** (`_MM_SET_FLUSH_ZERO_MODE`,
>   `_MM_SET_DENORMALS_ZERO_MODE`) so a stray denormal float doesn't cost
>   ~100+ cycles in a microcode assist (file `13`).
> - **`vzeroupper`** — if you hand-write AVX and then call SSE library code,
>   emit it (or let `-mavx` do it) to avoid the SSE↔AVX transition stall.
> - **Watch spills in the hot loop** — `./build.ps1 asm` and grep for
>   `mov ..., [rsp` / `mov [rsp`, `[rbp-`; each is a hidden L1 access.

---

## Hands-on

```bash
# spilling dekho: bahut live doubles, no inline
cat > /tmp/spill.cpp <<'EOF'
__attribute__((noinline))
double f(double* p){
  double a=p[0]*p[1], b=p[2]*p[3], c=p[4]*p[5], d=p[6]*p[7];
  double e=p[8]*p[9], g=p[10]*p[11], h=p[12]*p[13], k=p[14]*p[15];
  return (a+b)*(c+d) + (e+g)*(h+k) + a*c*e*g + b*d*h*k;
}
EOF
g++ -std=c++20 -O2 -S -masm=intel /tmp/spill.cpp -o - | grep -E 'rsp|xmm' | head -30

# example 07: is box ke SIMD registers (AVX2 yes, AVX-512 no)
./build.ps1 fast 31-CPU-ARCHITECTURE/examples/07_cpu_info.cpp
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "16 registers = fixed limit" | architectural 16; physical 100-300+ via renaming |
| "zyada locals = tez" | register pressure → spills to stack |
| "`&x` free hai" | forces `x` into memory for its lifetime |
| "8-bit write cheap" | partial-register merge stall; use 32-bit / `movzx` |
| "`xmm`/`ymm` alag registers" | `xmm` = low half of `ymm`; mixing → `vzeroupper` |
| "`thread_local` = global" | extra `fs:`/`gs:` indirection |

---

## Exercises

1. `mov eax, 1` ke baad `rax` ki value kya? `mov al, 1` ke baad?

   <details><summary>Answer</summary>

   `mov eax, 1`: 32-bit write **zero-extends** — `rax` = `0x0000_0000_0000_0001`,
   upper 32 bits cleared. `mov al, 1`: 8-bit write **merges** — `rax` =
   `(old_rax & ~0xFF) | 1`, upper 56 bits unchanged. The merge creates a
   dependency on the old `rax` value (partial-register stall on some µarchs);
   the 32-bit form breaks that dependency, which is why compilers prefer
   `movzx`/32-bit ops.
   </details>

2. Ek function `foo(int x)` mein tum `int arr[3] = {x, x+1, x+2}; bar(arr);`
   likhte ho. `x` register mein reh sakta hai?

   <details><summary>Answer</summary>

   `x` khud shayad register mein rahe (used to compute the array), par `arr`
   **stack pe** allocate hoga (address `&arr[0]` `bar` ko pass ho raha — escape).
   Teen stores + a `lea` for the pointer. Agar `bar` inline ho jaye aur woh
   `arr` ke elements ko sirf padhe, compiler `arr` ko eliminate karke values
   registers mein rakh sakta (SROA — scalar replacement of aggregates). `-O2` +
   inlining is key.
   </details>

3. Register renaming kaise ek "write-after-write" hazard todta hai? Example:
   `rax = a; use(rax); rax = b; use(rax);`

   <details><summary>Answer</summary>

   Without renaming, the second `rax = b` would have to wait for the first
   `use(rax)` to finish reading the old value (WAR), and can't be reordered
   before it. With renaming, the CPU assigns `rax = b` a **new physical
   register** — the two `rax` values coexist. Now the second `rax = b` and its
   `use` can execute in parallel with (or before) the first pair, limited only
   by real data dependencies. This is what makes 4 independent accumulator
   chains (example `02`) run ~4× faster than one.
   </details>

4. `_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON)` — HFT code ise startup pe kyun
   set karta?

   <details><summary>Answer</summary>

   Denormal (subnormal) floating-point numbers — tiny values near zero — are
   handled by a **microcode assist** on many x86 CPUs, costing ~100-200+ cycles
   per operation instead of ~4. A market-data feed or a strategy that produces a
   denormal (e.g. an exponential decay that underflows) suddenly gets a random
   100× slowdown on those ops. Flush-to-zero + denormals-are-zero mode makes the
   FPU treat denormals as 0 — a tiny accuracy loss for a huge, predictable
   latency win. Set it once at thread startup via `mxcsr`.
   </details>

5. `./build.ps1 asm` output mein `mov QWORD PTR [rsp+24], rax` aur 5 lines baad
   `mov rax, QWORD PTR [rsp+24]` dikhta hai hot loop mein. Kya ho raha, kaise
   kam karo?

   <details><summary>Answer</summary>

   Register **spill and reload** — the compiler ran out of registers, stored a
   live value to the stack, and reloaded it when needed again. Each is a ~4-5
   cycle L1 access where a register would be free. Fixes: reduce the number of
   simultaneously-live values (smaller expressions, tighter scopes), inline the
   function so callee-saved register overhead disappears, avoid `&` on locals,
   split the hot loop so fewer things are live at once, or (last resort)
   restructure so the dependency chain is shorter.
   </details>

---

## Interview questions

1. x86-64 GP registers — count, the `rax`/`eax`/`ax`/`al` sub-register scheme,
   what a 32-bit write does to the upper 32.
2. `rflags` — which flags, what sets them, what reads them.
3. Architectural vs physical registers — register renaming, what hazard it breaks.
4. Register spilling — when it happens, its cost, how to reduce it.
5. `xmm`/`ymm`/`zmm` — widths, aliasing, `vzeroupper`.
6. `fs`/`gs` on x86-64 — the one thing they're still used for.
7. `mxcsr` flush-to-zero / denormals-are-zero — the HFT reason.

---

## Next
→ [`03-instruction-set.md`](03-instruction-set.md)
