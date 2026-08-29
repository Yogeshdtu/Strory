# 14 — MEMORY MANAGEMENT (PHASE 5)

## Prerequisites
`12-POINTERS`, `13-REFERENCES`

## Yeh folder kyun
Ab aap memory ko **khud manage** karoge. Yeh C++ ki sabse bada power hai aur
sabse bada khatra bhi.

Aur yahan **allocation cost** samjhenge — jo HFT low-latency ka core problem hai.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-memory-layout-revisited.md` | Text/data/bss/heap/stack — ab code ke saath, `/proc/PID/maps` |
| 02 | `02-stack-deep-dive.md` | Stack frames, growth direction, stack limits, stack overflow, `ulimit` |
| 03 | `03-heap-deep-dive.md` | Heap kya hai, allocator ka kaam, `malloc` internals ki jhalak |
| 04 | `04-new-and-delete.md` | `new`/`delete`, `new[]`/`delete[]`, mixing = UB, `nothrow` variants |
| 05 | `05-memory-leaks.md` | Leaks kya hain, kaise dhoondhein, Valgrind aur ASan |
| 06 | `06-use-after-free.md` | Use-after-free, double-free, ASan demos |
| 07 | `07-static-and-thread-local.md` | Static storage duration, `thread_local`, initialization order |
| 08 | `08-allocation-cost.md` | **Allocation kitni mehngi hai** — measured, jitter, page faults, **HFT relevance** |
| 09 | `09-fragmentation.md` | Internal/external fragmentation, long-running processes ka problem |
| 10 | `10-custom-allocation-intro.md` | Placement new intro, arena/pool allocators ka pehla parichay |
| 11 | `11-memory-tools.md` | Valgrind, ASan, MSan, LSan, heaptrack — practical usage |
| 12 | `12-exercises.md` | Practice + leak hunting |

## Examples

| File | Kya |
|---|---|
| `examples/01_memory_layout.cpp` | Saare segments ke addresses |
| `examples/02_stack_vs_heap.cpp` | Allocation speed comparison |
| `examples/03_new_delete.cpp` | Manual memory management |
| `examples/04_memory_leak.cpp` | ⚠️ Leak — Valgrind/LSan se pakdo |
| `examples/05_use_after_free.cpp` | ⚠️ UAF — ASan se pakdo |
| `examples/06_allocation_benchmark.cpp` | Allocation latency distribution (p50/p99) — HFT style |
| `examples/07_simple_pool.cpp` | Ek chhota fixed-size memory pool |

## Time
1–2 hafte

## Status
⏳ **Yeh folder abhi syllabus stage pe hai.** Upar ki file list poori plan hai —
content agle batch mein aayega.

## Next
→ [`../15-CLASSES/00-README.md`](../15-CLASSES/00-README.md)
