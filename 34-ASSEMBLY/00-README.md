# 34 — ASSEMBLY (reading, not writing) (PHASE 22)

## Prerequisites
`33-COMPILER-OPTIMIZATION`

## Yeh folder kyun
Aapko assembly **likhni** nahi hai. Aapko usse **padhni** aani chahiye — taaki verify
kar sako ki compiler ne wahi kiya jo aap chahte the.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-why-read-assembly.md` | Verification, optimization, debugging — teen wajah |
| 02 | `02-x86-64-basics.md` | Registers, sizes, calling convention overview |
| 03 | `03-att-vs-intel-syntax.md` | Dono syntax, kaunsa kab dikhta hai |
| 04 | `04-common-instructions.md` | `mov`, `lea`, `add`, `cmp`, `jmp`, `call`, `ret`, `test` |
| 05 | `05-addressing-modes.md` | Immediate, register, memory, indexed, scaled |
| 06 | `06-stack-frames.md` | **Prologue/epilogue**, `rbp`/`rsp`, local variables, frame pointers |
| 07 | `07-calling-conventions.md` | **System V ABI** — argument registers, return, callee/caller saved |
| 08 | `08-recognizing-patterns.md` | Loops, branches, function calls, virtual calls — assembly mein pehchano |
| 09 | `09-simd-assembly.md` | SSE/AVX instructions padhna |
| 10 | `10-inline-assembly.md` | GCC inline asm, constraints, kab zaroori hai (rarely) |
| 11 | `11-rdtsc-and-timing.md` | `rdtsc`, `rdtscp`, serialization, cycle counting |
| 12 | `12-disassembly-tools.md` | `objdump -d`, `perf annotate`, `gdb disassemble` |
| 13 | `13-exercises.md` | 'Yeh assembly kis C++ se aayi?' puzzles |

## Examples

| File | Kya |
|---|---|
| `examples/01_simple_functions.s` | Basic function assembly |
| `examples/02_reading_loops.cpp` | Loop patterns |
| `examples/03_virtual_call_asm.cpp` | Virtual call assembly mein |
| `examples/04_stack_frame.cpp` | Frame layout |
| `examples/05_rdtsc.cpp` | Cycle counting |
| `examples/06_inline_asm.cpp` | Inline assembly examples |
| `examples/07_asm_puzzles.md` | Reading exercises |

## Time
2 hafte

## Status
⏳ **Yeh folder abhi syllabus stage pe hai.** Upar ki file list poori plan hai —
content agle batch mein aayega.

## Next
→ [`../35-PROFILING-BENCHMARKING/00-README.md`](../35-PROFILING-BENCHMARKING/00-README.md)
