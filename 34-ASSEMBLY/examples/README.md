# Examples — Folder 34 (assembly — reading, not writing)

> These are **read-the-assembly** demos. The `.cpp` files compile + run (so
> you can verify behaviour), but the point is `./build.ps1 asm <file>` — look
> at what the compiler produced. `01_simple_functions.s` and
> `07_asm_puzzles.md` are reference documents (not compiled).
>
> `./build.ps1 folder 34-ASSEMBLY` → **5/5 OK** (the `.cpp` files), strict flags.

## The box these were measured on

**AMD Ryzen 7 4700U** (Zen 2), ~2.0 GHz. **Windows x64 ABI** (arg 1 in
`rcx`, 32-byte shadow space) — godbolt's default is System V (`rdi`). MinGW
`long` is **32-bit** (LLP64). Plain `-O2` = SSE2 baseline.

## Files

| File | Lesson(s) | Kya |
|---|---|---|
| `01_simple_functions.s` | 02, 04, 05 | hand-annotated Intel asm for `add`/`muladd`/`max2`/`sum_n`/`is_even` — `lea` as arithmetic, `cmov` (branchless), entry guard, strength reduction, pointer-compare loop. Reference; not compiled. |
| `02_reading_loops.cpp` | 08 | 6 loop shapes: counted+vectorized reduction, `while (*p)` sentinel walk, `do-while` (collatz), nested (matrix sum), early-`break` (`find_first`), data-dependent `if` (→ `cmovg` at `-O2`). `./build.ps1 asm` and match. |
| `03_virtual_call_asm.cpp` | 07 (this folder), 33/07 | `via_virtual` (`mov rax,[rcx]` + `call [rax+..]` — indirect) vs `via_direct` / `via_crtp` (inlined `imul`, no call) vs `via_known_concrete` (`final` + concrete type → devirtualized). `sizeof` shows the vptr adds 8. |
| `04_stack_frame.cpp` | 06, 07 | `add3` (leaf, no frame) / `sum_local_buffer` (256-long local → big `sub rsp`) / `caller` (`call add3`) / `factorial` (real recursion) / `sum_to` (tail call → `jmp` loop at `-O2`). Runtime: frame addresses shrink with recursion depth. |
| `05_rdtsc.cpp` | 11 | `__rdtsc` / `lfence;rdtsc;lfence` / `rdtscp+lfence` — asm + measured self-cost. **This box: calibration ~2.0 ticks/ns; plain ~1 tick (~0.5 ns); fenced ~20 ticks (~10 ns); 100M dependent-LCG loop ~1.98 ticks/iter.** |
| `06_inline_asm.cpp` | 10 | GNU extended asm: the `DoNotOptimize` barrier (zero instructions), `cpuid` (vendor = **AuthenticAMD**, SSE2/AVX bits), hand-written `rdtsc` (EDX:EAX combine), a `lea`-trick (`x*5+1` in one instruction, AT&T syntax), `pause`. Constraints + clobbers explained. |
| `07_asm_puzzles.md` | 08 | 10 "which C++ produced this asm?" puzzles with answers — `x*6` via `lea`, `abs`, SWAR `popcount`, auto-vectorized sum, virtual tail-call, reciprocal-multiply division, `strlen`, address-of-local frame, runtime `%`, `10!` constant-folded. Reference. |

## Notes / jaan-boojh kar cheezein

- **No `keep()` in `02`/`03`/`04`.** The functions are non-`static` → the
  compiler emits their standalone assembly regardless of whether `main`'s
  calls constant-fold. `printf` of every result is a sufficient sink. (A
  `keep()` on a compile-time-constant value hit "impossible constraint in
  'asm'" at `-O2` — the barrier needs a genuine runtime lvalue. `05`/`06`
  keep their barriers because they wrap real runtime values.)
- **Win64 ABI on this box.** `./build.ps1 asm` shows arg 1 in `rcx` (not
  `rdi`), `sub rsp, 0x28` in call-making functions (shadow space +
  alignment), `rsi`/`rdi` push/pop in the prologue when used (they're
  callee-saved here). godbolt shows System V — read the right one (lesson 07).
- **`06`'s `lea` inline asm is AT&T syntax** (`lea 1(%1,%1,4), %0`) — the
  default GCC inline-asm dialect. Intel form needs `-masm=intel`. Noted in
  the file.
- **`05` numbers are this-box (~2 GHz Zen 2).** Ratios port (plain rdtsc ≈
  free, fenced ≈ 20 ticks); absolutes don't. Frequency-lock the production
  box (folder 31 lesson 13).
- **No `broken_on_purpose` file.** Every `.cpp` is a correct program.
- The 5 `.cpp` compile clean under `-std=c++20 -Wall -Wextra -Wpedantic
  -Wshadow -Wconversion -Wsign-conversion -Wcast-align -Wnull-dereference
  -Wdouble-promotion`.
