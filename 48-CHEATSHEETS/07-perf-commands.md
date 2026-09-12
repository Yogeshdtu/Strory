# 07 — perf / valgrind / sanitizers cheatsheet

Deep: folders `35-PROFILING-BENCHMARKING`, `45-DEBUGGING` (06, 07, 11).
Linux tools. Build with `-g -fno-omit-frame-pointer` (or `--call-graph dwarf`).

---

## `perf` — sampling profiler

```bash
perf stat ./prog                       # cycles, insns, IPC, branch-miss, cache-miss
perf stat -d ./prog                    # + L1/LLC detail
perf stat -e task-clock,context-switches,cpu-migrations,page-faults ./prog
perf stat -r 10 ./prog                 # 10 runs + stddev

perf record -g ./prog                  # sample with call graphs (frame pointers)
perf record --call-graph dwarf ./prog  # call graphs w/o frame pointers (heavier)
perf record -e cache-misses -g ./prog
perf report                            # interactive; annotate with 'a'
perf annotate -s hot_func              # source+asm with sample %
perf script | stackcollapse-perf.pl | flamegraph.pl > fg.svg

perf top                               # live system-wide
perf list                              # all events
```

### Reading `perf stat`

| Signal | Likely problem | Fix direction |
|---|---|---|
| IPC < ~1.0 | stalled — memory or mispredict bound | check cache-miss / branch-miss next |
| branch-misses high (>2–3%) | unpredictable branch in a hot loop | branchless / sort / restructure |
| LLC-load-misses high | working set > cache, pointer chasing | shrink data, SoA, tile, prefetch |
| page-faults climbing | allocating on the hot path | pool / arena / prefault + `mlockall` |
| context-switches high | lock contention / oversubscription | reduce sharing, pin, isolate |
| frontend-bound | I-cache / decode | hot-cold split, less inlining |

---

## `perf` for latency bugs (folder 45/11)

```bash
perf trace ./prog                      # strace-like, low overhead — syscall storms
perf trace -s ./prog                   # syscall summary (count + time)
perf sched record ./prog ; perf sched latency   # scheduler wait / off-CPU
perf c2c record ./prog ; perf c2c report        # false sharing (HITM cache lines)
perf record -e sched:sched_switch -g ./prog     # who preempts the hot thread
# off-CPU (bcc):  offcputime -p <pid>
```

---

## `valgrind` (folder 45/07) — no Linux libasan? use this

```bash
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./prog
#   definitely lost | indirectly lost | possibly lost | still reachable
valgrind --tool=helgrind ./prog        # data races + lock-order (noisy)
valgrind --tool=drd ./prog             # races, lighter than helgrind
valgrind --tool=massif ./prog ; ms_print massif.out.*   # heap over time
valgrind --tool=cachegrind ./prog      # simulated cache miss counts (no HW needed)
valgrind --tool=callgrind ./prog ; kcachegrind   # call-graph profile
```

~10–50× slowdown. Great for a definitive answer when HW sanitizers aren't available.

---

## Sanitizers (compile-time; folder 45/06)

```bash
# ASan — OOB (heap/stack/global), use-after-free/return/scope, leaks
g++ -O1 -g -fsanitize=address -fno-omit-frame-pointer f.cpp && ./a.out
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1 ./a.out

# UBSan — signed overflow, /0, bad shift, misaligned, bad enum/bool, null deref
g++ -O1 -g -fsanitize=undefined -fno-sanitize-recover=all f.cpp && ./a.out

# TSan — data races, lock-order inversions  (NOT with ASan; ~5-15x)
g++ -O1 -g -fsanitize=thread f.cpp && ./a.out

# MSan (Clang only) — reads of uninitialized memory (needs instrumented libs)
clang++ -O1 -g -fsanitize=memory -fno-omit-frame-pointer f.cpp
```

Combine ASan+UBSan freely. **CI golden build:**
`-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all`.

| Tool | Catches | Misses | Cost |
|---|---|---|---|
| ASan | heap/stack OOB, UAF/UAR, leaks | uninit reads, races | ~2× |
| UBSan | int overflow, UB arithmetic, misalign | memory-safety | ~1.2× |
| TSan | data races, lock-order | non-racy logic bugs | ~5–15× |
| valgrind memcheck | leaks, invalid r/w, uninit | data races | ~10–30× |

---

## Quick benchmark harness (no external dep)

```cpp
#include <chrono>
auto t0 = std::chrono::steady_clock::now();
for (int i = 0; i < N; ++i) { auto r = work(i); asm volatile("" :: "r"(r) : "memory"); }
auto t1 = std::chrono::steady_clock::now();
double ns_per = std::chrono::duration<double,std::nano>(t1 - t0).count() / N;
```

`asm volatile("" :: "r"(r) : "memory")` = "don't optimize this away". Run at `-O2`.
Warm up, take the min of several runs, keep the workload realistic. (folder 43/02)

---

## Other useful

```bash
ltrace ./prog                 # library calls
strace -c -f ./prog           # syscall count/time summary
/usr/bin/time -v ./prog       # max RSS, page faults, ctx switches
hyperfine './prog'            # statistical CLI benchmarking
taskset -c 2 ./prog           # pin to CPU 2
chrt -f 80 ./prog             # SCHED_FIFO prio 80  (careful)
```

## Next
→ [`08-linux-tuning-checklist.md`](08-linux-tuning-checklist.md)
