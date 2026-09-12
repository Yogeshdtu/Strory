# Examples — Folder 25 (Object model & UB)

| File | Lesson(s) | Kya dikhata hai |
|---|---|---|
| `01_lifetime_demo.cpp` | 01, 02 | Object lifetime: automatic (scope), manual in a raw `alignas` buffer (placement new + explicit dtor), **storage reuse** (one buffer, 3 objects one after another), a different type in the same slot; `alive` counter → 0 |
| `02_static_init_fiasco/` | 04 | ⚠️ **Fiasco reproduced.** Two link orders → two outputs — order B prints `BADLOG[1/(null)]` (the `std::string` member's ctor hadn't run) and `lines = 0`. The **fixed** (construct-on-first-use) side is identical both ways. `.cxx` so the repo `*.cpp` check skips it — build with `./build.sh` |
| `03_temporaries.cpp` | 05 | Full-expression rule (`~Loud` before the `;`), `const T&` lifetime extension, transitive extension, and **4 non-extension cases** — returning a bound ref, reference member from a ctor-arg temp, `string_view` into a temporary, C++20 range-for over `f().member`. Watch **when `~Loud` prints** |
| `04_type_properties.cpp` | 06, 07 | `is_trivial` / `is_trivially_copyable` / `is_standard_layout` / `is_aggregate` / `has_unique_object_representations` matrix for 9 types; which operation each unlocks (`memcpy`, byte round-trip, `memcmp`, `offsetof`). **Measured: `memcmp == -1` without `memset` first** (padding bytes) |
| `05_placement_new.cpp` | 09 | Manual construct/destruct in `alignas` storage, storage reuse, `std::launder` note, and a `FixedOptional<T>` (no heap, `std::launder` on every access, `alive` → 0). `sizeof(FixedOptional<Widget>) == 48` vs `Widget` 40 |
| `06_strict_aliasing.cpp` | 10, 11, 16 | float↔bits via `reinterpret_cast` (UB) vs `memcpy` / `std::bit_cast` (legal); and **`aliasing_trap`** — `-O0`: `delta` reflects the real change; **`-O2 -fstrict-aliasing`: `delta == 0`** (compiler reused the load, "knowing" `int*`/`long*` don't alias) |
| `07_vtable_inspect.cpp` | 13, 12 | vptr = first 8 bytes; same type → same vtable, different type → different; dispatch goes through the vtable; **multiple inheritance → two vptr**, `Printable` subobject at offset 8, base-cast **changes the pointer value**. ⚠️ ABI-specific, learning only |
| `08_ub_examples.cpp` | 15, 16 | A UB catalog (most `#if 0`, read-only) + 3 cases that **run**: `overflow_check(INT_MAX)` (GCC folds `x+1>x` to `1` at every `-O`; only `-fwrapv` gives `0`), a null-check the optimizer deletes at `-O2`, an infinite-loop-forward-progress note |

## Compile / run

```bash
./build.ps1 25-OBJECT-MODEL/examples/01_lifetime_demo.cpp     # Windows (debug -O0 + heavy warnings)
make FILE=25-OBJECT-MODEL/examples/01_lifetime_demo.cpp       # Linux/Mac/Git-Bash
```

**Divergence demos** need specific flags:
```bash
# strict aliasing — compare delta:
g++ -std=c++20 -O0                        25-OBJECT-MODEL/examples/06_strict_aliasing.cpp -o sa  && ./sa
g++ -std=c++20 -O2 -fstrict-aliasing      25-OBJECT-MODEL/examples/06_strict_aliasing.cpp -o sa  && ./sa   # delta == 0
g++ -std=c++20 -O2 -fno-strict-aliasing   25-OBJECT-MODEL/examples/06_strict_aliasing.cpp -o sa  && ./sa

# signed overflow fold:
g++ -std=c++20 -O2         25-OBJECT-MODEL/examples/08_ub_examples.cpp -o ub  && ./ub   # "1. ... = 1"
g++ -std=c++20 -O2 -fwrapv 25-OBJECT-MODEL/examples/08_ub_examples.cpp -o ub  && ./ub   # "1. ... = 0"
```

The **static-init fiasco** builds separately (not via `./build.ps1 folder` /
`checkall`, which only see `*.cpp`):
```bash
cd 25-OBJECT-MODEL/examples/02_static_init_fiasco && ./build.sh
```

**UB hunting** (Linux — MinGW has no libasan/libubsan):
```bash
g++ -std=c++20 -O1 -g -fsanitize=address,undefined 25-OBJECT-MODEL/examples/08_ub_examples.cpp -o ub && ./ub
```

## Measured (GCC 15.1.0, this box)

### `04_type_properties.cpp`
```
type                    trivial triv-copy std-layout aggregate uniq-obj-rep
struct{int,int}            1        1          1          1          1
+ user default ctor        0        1          1          0          1
struct{char,int} (pad)     1        1          1          1          0
mixed public/private        0        1          0          0          1
struct{int,std::string}    0        0          1          1          0
struct{virtual ~}          0        0          0          0          0
Derived (data in base+der)  1        1          0          1          1
...
memcmp WITHOUT memset first: -1     <- padding bytes differed
```

### `06_strict_aliasing.cpp` (`aliasing_trap`)
```
-O0                     : delta = 287453909   (real change)
-O2 -fstrict-aliasing   : delta = 0           (load reused — UB biting)
-O2 -fno-strict-aliasing: delta = 287453909   (real)
```

### `08_ub_examples.cpp`
```
overflow_check(INT_MAX): 1  at -O0 and -O2  (GCC folds x+1>x -> 1)
                         0  only with -fwrapv (signed overflow -> wrap)
```

### `07_vtable_inspect.cpp`
```
c1, c2 (both Circle) -> same vtable pointer
s1 (Square)          -> different vtable pointer
Both : Shape, Printable -> two vptr; Printable subobject at offset 8;
                           (Printable*)&b != (Shape*)&b  (pointer value shifts by 8)
```

## Notes / jaan-boojh kar cheezein

- **No `broken_on_purpose` file.** `08_ub_examples.cpp` keeps most UB behind
  `#if 0` (read-only catalog) and runs only the "surprising at `-O2`" cases
  cleanly.
- `02_static_init_fiasco/` uses `.cxx` / `.hpp` on purpose so the repo's `*.cpp`
  compile-check skips it (it's a two-link-order demo, not a single TU). Build it
  with its own `build.sh`.
- `07_vtable_inspect.cpp` reads the vptr via `memcpy` (the `char`/byte aliasing
  exception) — the exact vtable slot layout is **not** walked (impl-defined,
  crash risk). It's Itanium-ABI-specific and for learning only.
- All top-level `.cpp` compile clean under `-Wall -Wextra -Wpedantic -Wshadow
  -Wconversion -Wsign-conversion -Wcast-align -Wunused -Wnull-dereference
  -Wdouble-promotion` (`./build.ps1 folder 25-OBJECT-MODEL`).
