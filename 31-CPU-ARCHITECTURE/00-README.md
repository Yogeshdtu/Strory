# 31 — CPU ARCHITECTURE (PHASE 20)

## Prerequisites
`29-LINUX-SYSTEMS`, `05-OPERATORS`

## Yeh folder kyun
Ab tak CPU ek black box thi. Ab uske andar dekhenge.

Har cheez pehle **simple** explain hogi, phir gehrai mein — spec ka rule.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-how-cpu-works.md` | Fetch-decode-execute, clock, transistors se instructions tak |
| 02 | `02-registers.md` | General purpose, special, SIMD registers, x86-64 register set |
| 03 | `03-instruction-set.md` | ISA, x86-64 basics, CISC vs RISC, microcode |
| 04 | `04-pipelining.md` | **Pipeline** — stages, throughput vs latency, hazards |
| 05 | `05-superscalar.md` | Multiple execution units, ILP, ports |
| 06 | `06-out-of-order-execution.md` | **OoO** — reorder buffer, register renaming, reservation stations |
| 07 | `07-branch-prediction.md` | **Branch prediction** — predictors, BTB, misprediction cost (measured) |
| 08 | `08-speculative-execution.md` | Speculation, rollback, Spectre/Meltdown ka basic idea |
| 09 | `09-instruction-latency-throughput.md` | Har instruction ki cost, dependency chains, Agner Fog tables |
| 10 | `10-simd-basics.md` | **SIMD** — SSE, AVX, AVX-512, vector registers |
| 11 | `11-simd-intrinsics.md` | Intrinsics likhna, auto-vectorization vs manual |
| 12 | `12-hyperthreading.md` | SMT, resource sharing, **HFT mein kyun disable karte hain** |
| 13 | `13-frequency-and-power.md` | Turbo, C-states, P-states, thermal throttling, **HFT mein sab disable** |
| 14 | `14-numa-architecture.md` | Multi-socket, interconnect, memory locality |
| 15 | `15-cpu-differences.md` | Intel vs AMD, generation differences, what matters |
| 16 | `16-exercises.md` | Practice + microbenchmarks |

## Examples

| File | Kya |
|---|---|
| `examples/01_instruction_timing.cpp` | Instruction latency measure karo |
| `examples/02_pipeline_stall.cpp` | Dependency chain ka asar |
| `examples/03_branch_prediction.cpp` | Sorted vs random — measured |
| `examples/04_branch_free.cpp` | Branchless alternative |
| `examples/05_simd_basics.cpp` | SSE/AVX intrinsics |
| `examples/06_autovectorization.cpp` | Compiler kab vectorize karta hai |
| `examples/07_cpu_info.cpp` | CPUID se features detect karo |
| `examples/08_rdtsc_timing.cpp` | Cycle-accurate timing |

## Time
3 hafte

## Status
⏳ **Yeh folder abhi syllabus stage pe hai.** Upar ki file list poori plan hai —
content agle batch mein aayega.

## Next
→ [`../32-CACHE-MEMORY-PERFORMANCE/00-README.md`](../32-CACHE-MEMORY-PERFORMANCE/00-README.md)
