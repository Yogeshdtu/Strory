# 45 — DEBUGGING

## Prerequisites
`14-MEMORY`, `26-CONCURRENCY`

## Yeh folder kyun
Debugging ek skill hai jo alag se seekhni padti hai. Aap iska use poore course mein
karoge — isliye ise jaldi skim kar lo, phir zarurat pe wapas aao.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-debugging-mindset.md` | Scientific method, hypothesis → test, bisection |
| 02 | `02-gdb-basics.md` | Breakpoints, stepping, `print`, `backtrace`, `info locals` |
| 03 | `03-gdb-advanced.md` | Watchpoints, conditional breakpoints, `tbreak`, scripting |
| 04 | `04-debugging-crashes.md` | Segfaults, core dumps, `ulimit -c`, post-mortem analysis |
| 05 | `05-debugging-optimized.md` | `-O2` ke saath debug karna, inlined frames, `-Og` |
| 06 | `06-sanitizers-practical.md` | ASan, UBSan, TSan, MSan — real workflows |
| 07 | `07-valgrind.md` | memcheck, helgrind, DRD, massif |
| 08 | `08-debugging-multithreaded.md` | Race conditions, deadlocks, TSan, thread inspection in gdb |
| 09 | `09-reverse-debugging.md` | `rr` — record aur replay, backwards stepping |
| 10 | `10-logging-strategy.md` | Effective logging, levels, structured logs, **HFT: async logging** |
| 11 | `11-perf-for-debugging.md` | perf se performance bugs dhoondhna |
| 12 | `12-common-bug-patterns.md` | **Poora catalog** — memory, concurrency, logic, integer, lifetime |
| 13 | `13-exercises.md` | **Buggy programs — inhe fix karo** |

## Examples

| File | Kya | Chalta? |
|---|---|---|
| `examples/01_gdb_practice.cpp` | GDB practice program (break/step/watch/bt/finish) — **real transcripts** lessons 02–05 mein | ✅ |
| `examples/02_segfault_debug.cpp` | ⚠️ NULL-deref crash + core-dump workflow | ❌ SIGSEGV |
| `examples/03_memory_bugs.cpp` | ⚠️ Menu: leak / UAF / OOB / double-free / uninit / stack-UAR — sanitizers se pakdo | ⚠️ UB per mode |
| `examples/04_race_debug.cpp` | ⚠️ Data race (4 threads, plain `long`); `--safe` = atomic — TSan | ⚠️ wrong total |
| `examples/05_deadlock_debug.cpp` | ⚠️ AB/BA deadlock; default safe, `--deadlock` hangs — gdb `thread apply all bt` | ✅ / ⏸ |
| `examples/06_buggy_programs/` | 10 "find & fix" drills (off-by-one, uninit, unsigned wrap, dangling view, use-after-move, iterator invalidation, int overflow, rule-of-three double-free, precedence, data race) | compile ✅ / misbehave ⚠️ |

## Time
1–2 hafte

## Status
✅ **COMPLETE.** 13 lessons + 5 standalone examples + 10 buggy programs.
`./build.ps1 folder 45-DEBUGGING` → 5/5 OK (strict warnings); buggy programs
`checkall` mein compile-clean. gdb transcripts is box (MinGW GCC 15.1.0,
GDB 16.3) ke **real** output; sanitizer/valgrind/rr Linux-only, expected
output examples ke neeche.

## Next
→ [`../46-INTERVIEW-PREP/00-README.md`](../46-INTERVIEW-PREP/00-README.md)
