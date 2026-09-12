# 47 — CODING PROBLEMS

## Prerequisites
Depends on the problem set — har file apne prerequisites list karti hai (usually
the matching course folders). Broadly: folders `01`–`46`.

## Yeh folder kyun
Graded problem sets — **250 problems** across 10 themes. Har course folder ke baad
yahan aakar us topic ki practice karo. Yeh woh muscle hai jo interview mein kaam
aati hai: khud likhna, test karna, complexity bolna, trap pehchaanna.

**Kaise use karo:** har problem ke saath ek `Pattern:` hint aur ek `<details>`
block hai (approach + complexity). **Pehle khud solve karo** — scratch file,
`./build.ps1 scratch.cpp`. Atak jao → `<details>` kholo. Phir bhi → poore code
ke liye `11-solutions/`.

## Is folder ki files

| # | File | Problems | Kya |
|---|------|:---:|--------------|
| 01 | `01-basics-problems.md` | 30 | variables, loops, functions, integer math, bit tricks |
| 02 | `02-arrays-strings-problems.md` | 40 | two-pointer, sliding window, prefix sum, hash map, in-place |
| 03 | `03-pointers-memory-problems.md` | 25 | raw pointers, allocators, smart pointers, linked structures, lifetime |
| 04 | `04-oop-design-problems.md` | 20 | class design, Rule of 3/5, virtual dispatch, RAII, type erasure, CRTP |
| 05 | `05-stl-problems.md` | 35 | containers, algorithms, iterators, invalidation, allocators |
| 06 | `06-templates-problems.md` | 20 | traits, SFINAE/concepts, tag dispatch, CRTP, variadics, mini-`tuple`/`variant` |
| 07 | `07-concurrency-problems.md` | 25 | threads, mutex, CV, futures, thread pool, false sharing, deadlock |
| 08 | `08-lock-free-problems.md` | 15 | atomics, CAS, memory ordering, ABA, SPSC/MPSC, seqlock, reclamation |
| 09 | `09-optimization-problems.md` | 20 | **"optimize this"** — slow code + a target ratio/latency, grounded in folders 32/43 |
| 10 | `10-hft-problems.md` | 20 | wire parsing, order book ops, matching, feed gaps, risk gate, timer wheel |
| 11 | `11-solutions/` | — | full worked code for the problems that need it + how-to-practice |

## Examples

`examples/*.cpp` — **10 runnable reference solutions**, ek har category se, har
ek assertion-tested aur strict-warning-clean. `examples/README.md` mein mapping.

```bash
./build.ps1 folder 47-CODING-PROBLEMS      # 10/10 OK
./build.ps1 fast examples/09_optimize_row_vs_col.cpp   # the benchmark (needs -O2)
```

## Time
Ongoing — yeh folder ek reference/practice bank hai, ek baar mein padhne ki
cheez nahi. Har course phase ke baad relevant file pe 5–10 problems karo.

## Status
✅ **COMPLETE** — 250 problems (10 themed files), 10 per-category solution
writeups, 10 verified runnable examples.

## Next
→ [`../48-CHEATSHEETS/00-README.md`](../48-CHEATSHEETS/00-README.md)
