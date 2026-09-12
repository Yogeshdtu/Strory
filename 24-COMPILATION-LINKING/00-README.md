# 24 — COMPILATION, LINKING & BUILD SYSTEMS (PHASE 14)

## Prerequisites
`08-FUNCTIONS`, `23-ERROR-HANDLING`

## Yeh folder kyun
Folder 01 aur 02 mein pipeline ka overview mila tha. Ab **poori gehrai** mein —
kyunki bade projects mein build system hi aapka roz ka sangharsh hai.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-translation-units.md` | TU kya hai, compilation model, separate compilation |
| 02 | `02-preprocessor-deep.md` | Macros, conditional compilation, `#pragma`, predefined macros, macro traps |
| 03 | `03-include-guards-and-pragma.md` | Guards vs `#pragma once`, include hygiene, IWYU |
| 04 | `04-odr-deep.md` | **One Definition Rule** poora — kya break karta hai, kaise pakde |
| 05 | `05-linkage.md` | Internal/external/module linkage, `static`, anonymous namespaces |
| 06 | `06-storage-specifiers.md` | `static`, `extern`, `inline` variables (C++17), `thread_local` |
| 07 | `07-name-mangling.md` | Mangling, `extern "C"`, ABI compatibility, `c++filt` |
| 08 | `08-object-files-elf.md` | **ELF format** — sections, symbol tables, relocations |
| 09 | `09-static-vs-dynamic-linking.md` | `.a` vs `.so`, PLT/GOT, `LD_LIBRARY_PATH`, **HFT: static kyun** |
| 10 | `10-linker-errors.md` | Har common linker error aur uska fix |
| 11 | `11-make.md` | Make basics, rules, variables, patterns, dependency tracking |
| 12 | `12-cmake.md` | **CMake deep** — targets, properties, find_package, generator expressions |
| 13 | `13-build-performance.md` | Compile time optimization, PCH, unity builds, ccache, ninja |
| 14 | `14-binary-tools.md` | `nm`, `objdump`, `readelf`, `ldd`, `strings`, `size`, `strip` |
| 15 | `15-lto-and-pgo.md` | Link-Time Optimization, Profile-Guided Optimization |
| 16 | `16-exercises.md` | Practice + multi-file project banao |

## Examples

| File | Kya |
|---|---|
| `examples/01_multi_file_project/` | Header + source separation |
| `examples/02_odr_violation/` | ⚠️ ODR break karke dekho |
| `examples/03_static_library/` | `.a` banao aur link karo |
| `examples/04_shared_library/` | `.so` banao aur link karo |
| `examples/05_makefile_project/` | Ek proper Makefile |
| `examples/06_cmake_project/` | Ek proper CMakeLists.txt |
| `examples/07_binary_inspection.sh` | nm/objdump/readelf ka tour |

## Time
2 hafte

## Status
✅ **COMPLETE (Batch 8 — PHASE 14).** 15 lessons (`01`–`15`) + `16-exercises.md`
+ examples: 6 multi-file directories (`01`–`06`), `07_binary_inspection.sh`, aur
2 self-contained `.cpp` (`08_preprocessor_demo`, `09_linkage_storage`).

- **`.cxx`/`.hpp`** for the directory examples so the repo's `*.cpp`
  compile-check skips them; each builds via its own `build.sh` / `build.ps1` and
  is verified manually. `08`/`09` are `.cpp` and pass `./build.ps1 folder
  24-COMPILATION-LINKING` under the full warning set.
- `01_multi_file_project` — 3 TUs → link → `count_primes<=100 = 25` (cross-TU
  call resolves).
- `02_odr_violation` — DEMO 1: `ld: multiple definition of venue_name()`.
  DEMO 2: plain link OK with `sizeof(Config)` **12 vs 16**; `g++ -flto -Wodr`
  catches it and names the first differing field.
- `03_static_library` — `ar rcs libcalc.a` (3 members); `nm app` shows
  `calc::add/mul/dot/build_id` but **not** `calc::huge_unused` (member not
  pulled from the archive).
- `04_shared_library` — `greet.dll` + import lib; `objdump -p app.exe` shows the
  DLL dependency; rebuild library only → app output changes without relinking.
- `05_makefile_project` — pattern rules + `-MMD -MP` auto-deps: `make` no-op =
  "Nothing to be done"; `touch util.cxx` → only `util.o` + link; `touch
  include/engine.hpp` → **all** `.o` rebuild. (`OS`-aware `.exe` suffix so Make
  isn't always stale on Windows.)
- `06_cmake_project` — CMake 4.0.2: configure/build/run; `-DENGINE_FAST_PATH=ON`
  → `flavor: fast-path`; `cmake --install` → `stage/{bin,lib,include}`.

**Coverage:** translation units + compilation model · preprocessor deep (macros,
`#`/`##`, `__VA_OPT__`, X-macros, predefined macros, `_Pragma`, traps) · include
guards / `#pragma once` / IWYU / self-contained headers / pImpl · **ODR deep**
(loud "multiple definition" vs silent IFNDR, `-Wodr`, `_GLIBCXX` flag drift) ·
linkage (internal/external/module, `static`, anon namespaces, visibility) ·
storage (`static`, `extern`, `inline` variables, `thread_local`, `constinit`,
init-order fiasco) · name mangling / `extern "C"` / ABI / `c++filt` · object
files & ELF (sections, `.symtab`, relocations, `.bss`) · **static vs dynamic
linking** (`.a` member selection, PLT/GOT cost, `-fPIC`, RUNPATH, HFT: static) ·
every common linker error + fix · Make (rules, vars, patterns, `-MMD -MP`,
order-only prereqs, `.PHONY`) · CMake (targets, `PUBLIC`/`PRIVATE`/`INTERFACE`
usage requirements, `find_package`, generator expressions, install) · build
performance (header hygiene, PCH, unity builds, ccache, ninja, `mold`/`lld`) ·
binary tools (`nm`/`objdump`/`readelf`/`ldd`/`strings`/`size`/`strip`/`addr2line`)
· **LTO and PGO** (whole-program opt, profile-guided, the HFT release config).

## Next
→ [`../25-OBJECT-MODEL/00-README.md`](../25-OBJECT-MODEL/00-README.md)
