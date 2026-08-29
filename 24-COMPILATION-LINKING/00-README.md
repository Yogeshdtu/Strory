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
⏳ **Yeh folder abhi syllabus stage pe hai.** Upar ki file list poori plan hai —
content agle batch mein aayega.

## Next
→ [`../25-OBJECT-MODEL/00-README.md`](../25-OBJECT-MODEL/00-README.md)
