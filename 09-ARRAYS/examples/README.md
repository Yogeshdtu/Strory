# Examples — Folder 09 (Arrays)

| File | Lesson | Kya dikhata hai |
|---|---|---|
| `01_array_basics.cpp` | 01–04 | Declaration, init forms, partial-init zeroing, 3 traversal styles, contiguous layout (addresses) |
| `02_array_decay.cpp` | 05, 07 | Array → pointer decay, **`sizeof` trap** (intentional `-Wsizeof-array-argument`), size-param sum, reference-to-array |
| `03_2d_arrays.cpp` | 06 | Row-major layout with offsets, `m[i]` as 1D, `int(*)[4]`, flat 1D indexing |
| `04_std_array.cpp` | 08 | `std::array` API, `.at()` throw, value semantics, no-decay, STL algos, zero overhead |
| `05_span_demo.cpp` | 09 | One function on C array / `std::array` / `std::vector`, modify, subspan, dangling |
| `06_oob_asan.cpp` | 03, 11 | ⚠️ **Deliberate OOB** — raw arrays (silent on MinGW) + `std::vector[]` (aborts via `_GLIBCXX_ASSERTIONS`) |
| `07_aos_vs_soa.cpp` | 10 | AoS vs SoA — **measured ~4x** for single-field access (`-O2`) |

## Compile / run

```bash
./build.ps1 09-ARRAYS/examples/01_array_basics.cpp        # Windows
make FILE=09-ARRAYS/examples/01_array_basics.cpp          # Linux/Mac/Git-Bash
```

**Benchmark (`07`) — `-O2`:**
```bash
./build.ps1 fast 09-ARRAYS/examples/07_aos_vs_soa.cpp
g++ -std=c++20 -O3 -march=native 09-ARRAYS/examples/07_aos_vs_soa.cpp -o aos3 && ./aos3
```

## Sanitizers / hardening

⚠️ **Yeh course ka MinGW-w64 toolchain (`C:\mingw64`) mein `libasan`/`libubsan`
NAHI hai** — `-fsanitize=address` / `-fsanitize=undefined` **link fail** karte hain.

- `./build.ps1 san <file>` — ASan try karta hai, na milne pe **`-D_GLIBCXX_ASSERTIONS`
  + `-fstack-protector-all`** pe fall back karta hai (STL container `[]` bounds +
  stack canaries; **raw C arrays pe nahi**).
- Full coverage (raw arrays bhi) ke liye: **Linux / macOS / Clang-on-Windows / WSL**
  pe `-fsanitize=address,undefined`.
- Is MinGW build pe `_GLIBCXX_ASSERTIONS` **default ON** hai — `std::vector[5]` on a
  size-5 vector program ko abort kar deta hai (bina kisi flag ke).

## Jaan-boojh kar warnings

- **`02_array_decay.cpp`** — `-Wsizeof-array-argument` (`sizeof` on a decayed array
  parameter). Yehi lesson hai. Baaki file clean.
- **`06_oob_asan.cpp`** — deliberate OOB. Compile clean (`-O0`); plain run shows
  BUG 1–3 (raw, silent), then BUG 4 (`std::vector[5]`) **aborts** with a clear
  `_GLIBCXX_ASSERTIONS` message. `checkall` / `make folder` only compile → "OK".

Koi `*_broken_on_purpose.cpp` nahi — sab 7 compile hote hain.

## Measured results (aapke machine pe alag)

GCC 15.1.0, `-O2`, x86-64:

| Benchmark | Result |
|---|---|
| `07` — AoS vs SoA, TASK A (touch 1 of 6 fields) | SoA **~4.4x faster** (~542 vs ~124 ms) |
| `07` — AoS vs SoA, TASK B (`x += vx`, 2 of 6) | SoA **~2.7x faster** (~502 vs ~183 ms) |
| (lesson 10) column-major vs row-major (folder 07) | ~8x |
