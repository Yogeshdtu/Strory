# Examples — Folder 17 (RAII & Smart Pointers)

| File | Lesson | Kya dikhata hai |
|---|---|---|
| `01_raii_basics.cpp` | 01, 02 | `TraceGuard` — ctor acquire / dtor release; cleanup on normal exit, early return, nested scopes, loop; reverse order |
| `02_unique_ptr.cpp` | 04, 08, 10 | `make_unique`, move-only (copy = error), pass/return ownership, `reset`/`release`, `unique_ptr<int[]>`, container, `sizeof` == 8 |
| `03_shared_ptr.cpp` | 05 | refcount lifecycle (`use_count`), `make_shared` (1 alloc) vs `shared_ptr(new)` (2 allocs) via counted `operator new`, `sizeof` == 16 |
| `04_weak_ptr.cpp` | 06 | ⚠️ `shared_ptr` cycle → 2 blocks **leaked** (counter); `weak_ptr` back-edge → clean; `lock()` / `expired()` after destruction |
| `05_custom_deleter.cpp` | 07, 09 | `FILE*` wrapped in `unique_ptr<FILE, FileCloser>`; size table (8 / 8 / 16 / 16); `shared_ptr<FILE>` with a lambda deleter; fd via fn-ptr deleter |
| `06_exception_safety.cpp` | 03 | raw `new`/`delete` + `throw` → **2 blocks leaked**; `unique_ptr` + `throw` → 0 (counted `operator new`) |
| `07_smartptr_benchmark.cpp` | 11 | **Measured**: `unique_ptr` == raw (0.99x); `make_shared` ~raw; `shared_ptr(new)` ~1.7x; `shared_ptr` copy ~**90x** a raw copy |

## Compile / run

```bash
./build.ps1 17-RAII/examples/01_raii_basics.cpp        # Windows
make FILE=17-RAII/examples/01_raii_basics.cpp          # Linux/Mac/Git-Bash
```

Benchmark at **`-O2`**:

```bash
./build.ps1 fast 17-RAII/examples/07_smartptr_benchmark.cpp
```

## Measured — `07_smartptr_benchmark.cpp` (`-O2`, GCC 15.1, x86-64) — sample run

```
A) create + destroy one Obj
  raw new + delete           ~146 ns/op
  make_unique<Obj>           ~144 ns/op   -> unique_ptr / raw : 0.99x   (ZERO overhead)
  make_shared<Obj>           ~140 ns/op   -> ~raw (+ control block)
  shared_ptr<Obj>(new Obj)   ~244 ns/op   -> ~1.74x make_shared  (2 allocations)

B) copy an existing pointer in a hot loop
  raw pointer copy            ~0.4 ns/op
  unique_ptr .get()           ~0.7 ns/op   (a mov)
  shared_ptr copy + destroy   ~34  ns/op   -> ~90x a raw copy   (atomic inc+dec)
```

Numbers vary by machine/allocator (~alloc dominates A). The **shape** —
`unique_ptr` == raw, `shared_ptr` copy ≫ raw copy — reproduces everywhere.

## Runtime-visible bugs (not compile failures)

- **`04_weak_ptr.cpp`** — `bad::run()` deliberately builds a `shared_ptr` cycle;
  the counted `operator new`/`delete` reports `outstanding 2  (LEAK -- no dtors
  ran)`. `good::run()` (weak_ptr back-edge) reports `outstanding 0`.
- **`06_exception_safety.cpp`** — `rawVersion(true)` throws mid-function and
  leaks 2 blocks (`outstanding=2  <- LEAK`); `raiiVersion(true)` throws and
  leaks 0 (`unique_ptr` dtors run during unwinding).

Both use a global `operator new` / `operator delete` (+ `operator new[]` /
`operator delete[]` where relevant) counter — the same technique as folder 14,
since MinGW-w64 has no ASan/LSan.

## Notes

- No `broken_on_purpose` file. `04` and `06` are runtime-leak demos, not compile
  failures.
- `05_custom_deleter.cpp` writes `raii_demo.txt` / `raii_demo2.txt` in the CWD
  and `std::remove`s them.
- `05` — `FileCloser` prints to stdout (`std::puts`) on close so you can see the
  RAII cleanup fire.
