# Examples — Folder 09 (Arrays)

| File | Lesson | Kya dikhata hai |
|---|---|---|
| `01_array_basics.cpp` | 01–04 | Declaration, init ke tareeke, adhoore init pe zeros, 3 traversal styles, contiguous layout (addresses ke saath) |
| `02_array_decay.cpp` | 05, 07 | Array → pointer decay, **`sizeof` trap** (jaan-boojh kar `-Wsizeof-array-argument`), size-param wala sum, reference-to-array |
| `03_2d_arrays.cpp` | 06 | Offsets ke saath row-major layout, `m[i]` ko 1D ki tarah, `int(*)[4]`, flat 1D indexing |
| `04_std_array.cpp` | 08 | `std::array` API, `.at()` ka exception, value semantics, decay nahi, STL algorithms, zero overhead |
| `05_span_demo.cpp` | 09 | Ek function C array / `std::array` / `std::vector` pe, badalna, subspan, dangling |
| `06_oob_asan.cpp` | 03, 11 | ⚠️ **Jaan-boojh kar OOB** — raw arrays (MinGW pe chupchaap) + `std::vector[]` (debug build mein `_GLIBCXX_ASSERTIONS` se abort) |
| `07_aos_vs_soa.cpp` | 10 | AoS vs SoA — single-field access pe **kai guna farq naapa** (`-O2`) |

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

⚠️ **Is course ke MinGW-w64 toolchain (`C:\mingw64`) mein `libasan`/`libubsan` NAHI hai** —
`-fsanitize=address` / `-fsanitize=undefined` pe **link fail** hota hai.

- `./build.ps1 san <file>` — pehle ASan try karta hai, na mile to **`-D_GLIBCXX_ASSERTIONS` +
  `-fstack-protector-all`** pe aa jaata hai (STL container `[]` ke bounds + stack canaries;
  **raw C arrays pe nahi**).
- Poori coverage (raw arrays bhi) ke liye: **Linux / macOS / Clang-on-Windows / WSL** pe
  `-fsanitize=address,undefined`.
- GCC 16.2 pe `_GLIBCXX_ASSERTIONS` **sirf `-O0` pe default ON** hai — debug build mein size-5
  vector pe `v[5]` program ko abort kar deta hai. `-O2` pe macro off hai aur wahi OOB chupchaap
  UB ban jaata hai — optimized test builds mein `-D_GLIBCXX_ASSERTIONS` khud lagao (lesson 11).

## Jaan-boojh kar warnings

- **`02_array_decay.cpp`** — `-Wsizeof-array-argument` (decay ho chuke array parameter pe `sizeof`).
  Yahi lesson hai. Baaki file clean.
- **`06_oob_asan.cpp`** — jaan-boojh kar OOB. Compile clean (`-O0`); normal run pe pehle BUG 1–3
  (raw, chupchaap) dikhte hain, phir BUG 4 (`std::vector[5]`) saaf `_GLIBCXX_ASSERTIONS` message ke
  saath **abort** karta hai. `-O2` (`fast`) pe BUG 4 abort nahi karta (upar dekho). `checkall` /
  `make folder` sirf compile karte hain → "OK".

Koi `*_broken_on_purpose.cpp` nahi — saare 7 compile hote hain.

## Naape hue results (aapki machine pe alag honge)

`-O2`, x86-64, Windows (MinGW):

| Benchmark | GCC 15.1 | GCC 16.2 (3 runs) |
|---|---|---|
| `07` — AoS vs SoA, TASK A (6 mein se 1 field) | SoA **~4.4x** tez (~542 vs ~124 ms) | SoA **5.4–5.8x** tez (488–566 vs 89–98 ms) |
| `07` — AoS vs SoA, TASK B (`x += vx`, 6 mein se 2) | SoA **~2.7x** tez (~502 vs ~183 ms) | SoA **4.6–4.9x** tez (489–546 vs 106–111 ms) |
| (lesson 10) column-major vs row-major (folder 07) | ~8x | — (dobara nahi naapa) |

Compiler upgrade pe gap badha — GCC 16.2 ne SoA loops ko behtar vectorize kiya (lesson 10).
