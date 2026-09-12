# Examples — Folder 12 (Pointers)

| File | Lesson | Kya dikhata hai |
|---|---|---|
| `01_basic_pointers.cpp` | 01, 02, 03 | `&`, `*`, modify-through, address-as-number, `sizeof` (all 8), re-pointing |
| `02_pointer_arithmetic.cpp` | 05, 06 | `p+n` scales by `sizeof(*p)`, `arr[i]==*(arr+i)`, pointer walk, subtraction, `char*` byte view |
| `03_const_pointers.cpp` | 07 | `const int*` vs `int* const` vs `const int* const`, reading declarations, API const-correctness |
| `04_pointers_structs.cpp` | 08 | `->` vs `(*p).`, `Order*`/`const Order*` params, nullptr guards, linked-list walk |
| `05_function_pointers.cpp` | 11 | Declaring, calling, callbacks, dispatch table (`std::array` of fp), capture-less lambda → fp |
| `06_dangling_pointer.cpp` | 12, 13 | ⚠️ **Deliberate dangling/UAF bugs** (2 intentional warnings) — use-after-return, use-after-free, vector realloc, scope end + fixes |
| `07_pointer_diagrams.cpp` | 01, 03, 09 | Memory-state table at each step (`p = &a`, `*p = ...`, re-point, `**ppx`) |
| `08_swap_via_pointers.cpp` | — (bridge to folder 13) | by-value (broken) vs by-pointer vs by-reference vs `std::swap` |

## Compile / run

```bash
./build.ps1 12-POINTERS/examples/01_basic_pointers.cpp        # Windows
make FILE=12-POINTERS/examples/01_basic_pointers.cpp          # Linux/Mac/Git-Bash
```

## Jaan-boojh kar warnings

- **`06_dangling_pointer.cpp`** — **jaan-boojh kar** 2 warnings deta hai:
  `-Wreturn-local-addr` (`return &x` of a local) aur `-Wdangling-pointer`
  (pointer to a block-scoped local used after the block). Wahi lesson hai —
  compiler kuch dangling cases khud pakad leta hai. File compile + run dono
  karti hai (UB, par usually completes). Baaki 7 files clean compile karti hain.

## Sanitizers

⚠️ **MinGW-w64 (`C:\mingw64`) mein `libasan`/`libubsan` NAHI** — ASan/UBSan link
fail karte hain. `06_dangling_pointer.cpp` ka asli diagnosis (heap-use-after-free
/ stack-use-after-return with exact line) **Linux / macOS / Clang / WSL** pe:

```bash
g++ -std=c++20 -fsanitize=address,undefined -g 06_dangling_pointer.cpp -o dp && ./dp
```

`./build.ps1 san <file>` MinGW pe `-D_GLIBCXX_ASSERTIONS + -fstack-protector-all`
pe fall back karta hai (STL bounds + canaries; raw dangling nahi pakadta).

## Notes

- No `broken_on_purpose` file. `06_dangling_pointer.cpp` is a runtime-UB demo
  (like folder 09's `06_oob_asan.cpp`), not a compile failure.
- `05_function_pointers.cpp` uses a capture-less lambda → `int(*)(int,int)`
  conversion (valid). A capturing lambda would NOT convert — see lesson 11.
