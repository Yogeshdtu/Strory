# 17 — RAII & SMART POINTERS (PHASE 7)

## Prerequisites
`16-OOP`, `14-MEMORY`

## Yeh folder kyun
**RAII C++ ka sabse important idiom hai.** Constructor mein resource lo,
destructor mein chhodo. Bas.

Yeh aapne folder 02 lesson 06 mein scope ke saath dekha tha. Ab poora.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-what-is-a-resource.md` | Memory, files, sockets, locks — sab resources hain |
| 02 | `02-raii-idiom.md` | **RAII** — acquire in ctor, release in dtor, scope-based cleanup |
| 03 | `03-why-raii-works.md` | Exception safety, early return, guaranteed destruction |
| 04 | `04-unique-ptr.md` | `std::unique_ptr` — exclusive ownership, move-only, `make_unique` |
| 05 | `05-shared-ptr.md` | `std::shared_ptr` — reference counting, **atomic refcount ki cost** |
| 06 | `06-weak-ptr.md` | `std::weak_ptr` — cycles todna, `lock()` |
| 07 | `07-custom-deleters.md` | Custom deleters, C APIs ko wrap karna (FILE*, socket) |
| 08 | `08-ownership-semantics.md` | Ownership ka poora model — kaun delete karega |
| 09 | `09-raii-for-other-resources.md` | File RAII, lock RAII, socket RAII — apne wrappers likhna |
| 10 | `10-rule-of-zero.md` | **Rule of Zero** — special members likhne ki zarurat hi na pade |
| 11 | `11-smart-pointer-performance.md` | `unique_ptr` zero-cost hai, `shared_ptr` nahi — **HFT relevance** |
| 12 | `12-exercises.md` | Practice + RAII wrappers likhna |

## Examples

| File | Kya |
|---|---|
| `examples/01_raii_basics.cpp` | Scope-based cleanup |
| `examples/02_unique_ptr.cpp` | `unique_ptr` full |
| `examples/03_shared_ptr.cpp` | Refcounting dekhna |
| `examples/04_weak_ptr.cpp` | Cycle todna |
| `examples/05_custom_deleter.cpp` | FILE* ko RAII mein wrap karna |
| `examples/06_exception_safety.cpp` | Exception ke saath bhi cleanup hota hai |
| `examples/07_smartptr_benchmark.cpp` | raw vs unique vs shared — cost |

## Time
1–2 hafte

## Status
✅ **COMPLETE** (Batch 7 — PHASE 7). 11 lessons + exercises + 7 compile-verified
examples. What-is-a-resource, RAII idiom, why it works (stack unwinding +
exception safety + the 5 gaps where dtors don't run), `unique_ptr` (zero-cost),
`shared_ptr` (atomic refcount + control block), `weak_ptr` (cycles), custom
deleters (EBO / size table), ownership semantics, RAII wrappers for fds/locks/
sockets, **Rule of Zero**, smart-pointer performance. Measured: `unique_ptr` ==
raw (0.99x); `shared_ptr` copy ~**90x** a raw copy (`07_smartptr_benchmark.cpp`).

## Next
→ [`../18-COPY-MOVE/00-README.md`](../18-COPY-MOVE/00-README.md)
