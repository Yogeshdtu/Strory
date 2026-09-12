# Examples — Folder 24 (Compilation, linking & build systems)

Yeh folder ke examples zyadatar **multi-file projects** hain — ek `.cpp` akele
compile karke kuch nahi seekhoge; poora build process (compile → archive/shared →
link → inspect) hi lesson hai. Har directory ka apna `build.sh` / `build.ps1` /
`README.md` hai.

| # | Kya | Lesson(s) | Build |
|---|---|---|---|
| `01_multi_file_project/` | Header/source separation, 4 TUs → 3 `.o` → link, cross-TU symbol resolution, separate compilation | 01, 03, 05 | `./build.sh` |
| `02_odr_violation/` | ⚠️ **jaan-boojh ke galat.** DEMO 1: non-inline function in header → linker "multiple definition". DEMO 2: `struct` ke 2 layouts → silent ODR, `-flto -Wodr` pakadta hai | 04 | `./build.sh` (DEMO 1 ka link FAIL expected) |
| `03_static_library/` | `.a` archive banana, `ar rcs`, member selection (unused `.o` link nahi hota), `nm`/`size`, static-link properties | 09 | `./build.sh` |
| `04_shared_library/` | `.so`/`.dll` banana, `-fPIC`, export macros, loader search path, PLT/GOT indirection cost, per-process library state | 09 | `./build.sh` |
| `05_makefile_project/` | Proper Makefile: pattern rules, `wildcard`/`patsubst`, `-MMD -MP` auto-dependency tracking, order-only prereqs, `.PHONY`, incremental builds | 11 | `make` / `mingw32-make` |
| `06_cmake_project/` | Modern CMake: targets + `PUBLIC`/`PRIVATE`/`INTERFACE` usage requirements, `option()`, `target_compile_definitions`, generator expressions, out-of-source build, `install()` | 12 | `./build.sh` (cmake chahiye) |
| `07_binary_inspection.sh` | `file` / `size` / `nm` / `c++filt` / `objdump -d`/`-h`/`-p` / `readelf` / `strings` / `strip` ka guided tour ek chhote binary pe | 08, 14 | `bash 07_binary_inspection.sh` |
| `08_preprocessor_demo.cpp` | Self-contained: object/function macros, `#`/`##`, `__VA_ARGS__`/`__VA_OPT__`, predefined macros, `__has_include`, `_Pragma`, **X-macros**, aur macro traps (missing parens, double-eval) — **measured** | 02, 03 | `./build.ps1 24-COMPILATION-LINKING/examples/08_preprocessor_demo.cpp` |
| `09_linkage_storage.cpp` | Self-contained: anonymous namespace / file-`static` (internal linkage), `extern const`, **`inline` variable** (C++17), `extern` decl+def, **`constinit`** (C++20), `static` local (init guard), **`thread_local`** (2 threads) | 05, 06 | `./build.ps1 24-COMPILATION-LINKING/examples/09_linkage_storage.cpp` |

## `.cxx` / `.hpp` kyun (aur `.cpp` nahi)

Repo ke global compile-checks (`./build.ps1 folder`, `./build.ps1 checkall`) har
`*.cpp` file ko **akele** `g++ file.cpp -o exe` se compile+link karte hain. Multi-
file projects (jinme `main` doosri TU ke functions call karta) aise akele link
nahi honge, aur `02_odr_violation` toh jaan-boojh ke fail hota hai. Isliye
directory examples `.cxx` + `.hpp` use karte hain — same as folder 22 ka modules
demo. Real projects `.cpp`/`.cc` use karte hain; yahan sirf repo ke checker ko
dodge karne ke liye `.cxx` hai. Sirf `08`/`09` top-level `.cpp` hain (self-
contained, folder-check unhe verify karta).

## Ek nazar mein

```bash
# self-contained (folder check inhe dekhta hai):
./build.ps1 24-COMPILATION-LINKING/examples/08_preprocessor_demo.cpp
./build.ps1 24-COMPILATION-LINKING/examples/09_linkage_storage.cpp   # -pthread

# multi-file (har directory ka apna build):
cd 24-COMPILATION-LINKING/examples/01_multi_file_project && ./build.sh
cd ../02_odr_violation   && ./build.sh      # DEMO 1 link fail = seekh
cd ../03_static_library  && ./build.sh
cd ../04_shared_library  && ./build.sh
cd ../05_makefile_project && mingw32-make run
cd ../06_cmake_project   && ./build.sh
bash 24-COMPILATION-LINKING/examples/07_binary_inspection.sh
```

## Verified (GCC 15.1.0 / binutils / CMake 4.0.2, this box)

- `01` — 3 TUs compile, link, run: `count_primes<=100 = 25` (cross-TU call resolves).
- `02` — DEMO 1: `ld: multiple definition of 'venue_name()'`. DEMO 2: plain link OK
  with `sizeof(Config)` **12 vs 16**; `-flto -Wodr` → `warning: 'struct Config'
  violates the C++ One Definition Rule`.
- `03` — `libcalc.a` with 3 members; `nm app` shows `calc::add/mul/dot/build_id`
  but **not** `calc::huge_unused` (member not pulled).
- `04` — `app.exe` depends on `greet.dll` (`objdump -p` shows it); rebuild library
  only → app output changes without relinking app.
- `05` — `make` no-op = "Nothing to be done"; `touch util.cxx` → only `util.o` +
  link; `touch include/engine.hpp` → all `.o` rebuild (auto-deps).
- `06` — configure/build/run; `-DENGINE_FAST_PATH=ON` → `flavor: fast-path`;
  `cmake --install` → `stage/{bin,lib,include}`.
- `08`/`09` — `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
  -Wcast-align -Wunused -Wnull-dereference -Wdouble-promotion` clean.
