# Examples — Folder 16 (OOP: Inheritance & Polymorphism)

| File | Lesson | Kya dikhata hai |
|---|---|---|
| `01_inheritance.cpp` | 01, 02 | `Vehicle`→`Car` — inherited members, `protected` access, construction order (Base first), upcast, layout (base at offset 0) |
| `02_virtual_functions.cpp` | 03, 05, 06 | `Shape` hierarchy, `override`, `describe()` (non-virtual) calling virtual `area()`/`name()`, `speak` (hides) vs `speakV` (overrides) |
| `03_vtable_layout.cpp` | 04 | `sizeof` +8 for first virtual, vptr at object start, `ov.vptr == ov2.vptr` (shared vtable), `Base*`→`Derived` vptr |
| `04_virtual_destructor.cpp` | 07 | ⚠️ **Non-virtual base dtor** → `~Derived()` skipped → counted-`new` shows **2 blocks leaked**; GoodBase (virtual) frees all. 1 intentional `-Wdelete-non-virtual-dtor` |
| `05_slicing.cpp` | 08 | `Manager`→`Employee` by value (sliced, `pay()`=30000) vs by ref (polymorphic, 100000); `vector<Employee>` slices, `vector<unique_ptr<Employee>>` doesn't |
| `06_diamond.cpp` | 09, 10 | Non-virtual diamond (2 `Device` subobjects, ambiguous `serial_`, bigger `sizeof`) vs `virtual` inheritance (1 shared `Device`, most-derived inits it) |
| `07_dispatch_benchmark.cpp` | 03, 12, 13 | **Measured** dispatch cost: virtual ~23 ns, direct ~2.2 ns, CRTP ~2.2 ns, `variant`+`visit` ~16 ns (per-call, `-O2`) |
| `08_crtp.cpp` | 13 | CRTP: `Printable<D>` mixin, `Ordered<D>` (5 comparison ops from `operator<`), static-dispatch `Strategy<D>` in a templated `backtest` |

## Compile / run

```bash
./build.ps1 16-OOP/examples/01_inheritance.cpp        # Windows
make FILE=16-OOP/examples/01_inheritance.cpp          # Linux/Mac/Git-Bash
```

Benchmark at **`-O2`**:

```bash
./build.ps1 fast 16-OOP/examples/07_dispatch_benchmark.cpp
```

## Measured — `07_dispatch_benchmark.cpp` (`-O2`, GCC 15.1, x86-64) — sample run

```
N = 100000 shapes, 500 reps  (per-call = total / (N*reps))

  virtual  (unique_ptr)        ~23   ns/call     (indirect call + no inline + pointer chase)
  direct   (monomorphic)       ~2.2  ns/call     (inlined to ~1-2 FLOPs)
  CRTP     (static poly)       ~2.2  ns/call     (compile-time dispatch -> inlined)
  std::variant + visit         ~16   ns/call     (jump table; contiguous, arms may not fully inline)

  virtual / direct : ~10x     variant / direct : ~7x     CRTP / direct : ~1x
```

Numbers vary by machine/compiler (~5-15x typical for virtual). Shape (direct ≈
CRTP ≪ variant < virtual) reproduces. `std::visit` here did NOT fully inline the
arms — still beats virtual (better locality, no vptr chase).

## Jaan-boojh kar warnings

- **`04_virtual_destructor.cpp`** — **jaan-boojh kar** 1 warning:
  `-Wdelete-non-virtual-dtor` at `delete p;` where `p` is `BadBase*` (polymorphic
  class, non-virtual destructor). The runtime demo (counted global `operator
  new`/`delete`, incl. `operator new[]` explicitly routed) prints
  `[LEAK] 2 block(s) never freed` for the bad case. Compiles + runs. All other
  files clean under `-Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Wpedantic`.

## Notes

- No `broken_on_purpose` file. `04` is a runtime-leak/UB demo (like folder 14/15's
  deliberate-bug examples), not a compile failure.
- MinGW-w64 has no ASan — `04`'s real diagnosis (`new-delete-type-mismatch` /
  leak stack) is on Linux/WSL: `g++ -fsanitize=address -g 04_virtual_destructor.cpp`.
- `06_diamond.cpp` uses `.` in the ASCII diagram (not `/ \`) to avoid the `//`
  line-continuation warning (`-Wcomment`).
- `07_dispatch_benchmark.cpp` uses `asm volatile("" : "+r"(acc) : : "memory")` to
  stop the loop being hoisted; the datasets are built with a runtime-decided
  type per element so the virtual path can't be devirtualized.
- `08_crtp.cpp` — the CRTP base types (`Printable<D>` etc.) are empty → EBO →
  no `sizeof` contribution; each derived type needs its own small constructor
  (aggregate init with an empty base needs `{{}, ...}`).
