# 46 — INTERVIEW PREPARATION (PHASE 33)

## Prerequisites
Ideally folders 01–44

## Yeh folder kyun
**Spec ka rule: HFT interviews pe seedha jump mat karo. Build toward them.**

Isliye yeh question banks **layered** hain — beginner se HFT tak. Har layer pe
apne aap ko test karo.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-how-hft-interviews-work.md` | Process, rounds, kya expect karein, timelines |
| 02 | `02-cpp-basics-questions.md` | **Layer 1:** types, variables, control flow, functions |
| 03 | `03-pointers-memory-questions.md` | **Layer 2:** pointers, references, memory, RAII |
| 04 | `04-oop-questions.md` | **Layer 3:** classes, inheritance, vtables, virtual destructors |
| 05 | `05-stl-questions.md` | **Layer 4:** containers, iterators, algorithms, complexity |
| 06 | `06-templates-questions.md` | **Layer 5:** templates, SFINAE, concepts, CRTP |
| 07 | `07-move-semantics-questions.md` | **Layer 6:** value categories, move, forwarding, Rule of 5 |
| 08 | `08-concurrency-questions.md` | **Layer 7:** threads, mutexes, condition variables, deadlock |
| 09 | `09-memory-model-questions.md` | **Layer 8:** atomics, memory ordering, happens-before |
| 10 | `10-linux-questions.md` | **Layer 9:** syscalls, processes, scheduling, mmap |
| 11 | `11-networking-questions.md` | **Layer 10:** TCP/UDP, multicast, epoll, kernel bypass |
| 12 | `12-cpu-cache-questions.md` | **Layer 11:** pipelines, branch prediction, cache, false sharing |
| 13 | `13-performance-questions.md` | **Layer 12:** profiling, optimization, latency vs throughput |
| 14 | `14-hft-architecture-questions.md` | **Layer 13:** system design — feed handler, order book, full pipeline |
| 15 | `15-system-design-rounds.md` | Design a matching engine / feed handler / risk system |
| 16 | `16-brainteasers-and-probability.md` | Optiver/Jane Street style puzzles, expected value, market making games |
| 17 | `17-trick-questions.md` | Classic traps aur unke sahi jawab |
| 18 | `18-behavioural.md` | Behavioural round, projects discuss karna |
| 19 | `19-mock-interviews.md` | Full mock interview scripts with rubrics |
| 20 | `20-resume-and-projects.md` | HFT ke liye resume, kaunse projects highlight karein |

## Examples

| Path | Kya |
|---|---|
| `examples/*.cpp` | 8 classic interview **coding problems** — clean solution + assertion `main()` + "what the interviewer is testing" + HFT angle. `./build.ps1 folder 46-INTERVIEW-PREP` → **8/8 OK** (strict). |
| `examples/design/` | 4 worked **system-design** answers (feed handler, matching engine, risk gateway, full tick-to-trade) following the `15` arc |
| `examples/mocks/` | 4 full **mock-interview transcripts** with rubrics (C++ deep-dive, latency/systems, system design, market-making game) |

Coding problems: `01` reverse linked list · `02` LRU cache · `03` **lock-free
SPSC ring** · `04` fixed-point price parse · `05` object pool · `06` L2
top-of-book · `07` atoi edge cases / overflow · `08` O(1) moving average +
division-free signal.

## Time
Ongoing

## Status
✅ **COMPLETE.** 20 lessons (`01`–`20`) + 8 verified coding examples + 4
design docs + 4 mock transcripts. Layered question banks `02`–`14`
(Layer 1 types → Layer 13 tick-to-trade architecture), plus `15`
(system design), `16` (brainteasers/probability), `17` (C++ trick
questions), `18` (behavioural), `19` (mock scripts + rubrics), `20`
(resume). `./build.ps1 folder 46-INTERVIEW-PREP` → 8/8 OK under strict
warnings.

## Next
→ [`../47-CODING-PROBLEMS/00-README.md`](../47-CODING-PROBLEMS/00-README.md)
