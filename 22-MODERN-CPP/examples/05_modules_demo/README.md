# 05 — modules demo (multi-file)

A minimal C++20 **module**: an interface unit + a consumer.

| File | Role |
|---|---|
| `geometry.ixx` | module **interface** unit — `export module geometry;`, exports `Point`, `distance`, `norm`, `cosAngle`; `dot` is module-private |
| `main.cxx` | consumer — `import geometry;` |

The files use `.ixx` / `.cxx` extensions on purpose so the repo's `*.cpp`
compile-check (`./build.ps1 folder` / `checkall`) **skips** them — a module
interface unit and an `import`ing TU don't compile under a plain
`g++ -std=c++20 file.cpp`.

## Build & run

```bash
./build.sh                 # Linux / Mac / Git-Bash
```
```powershell
.\build.ps1                # Windows PowerShell
```

Manually:
```bash
g++ -std=c++20 -fmodules-ts -c geometry.ixx -o geometry.o
g++ -std=c++20 -fmodules-ts main.cxx geometry.o -o modules_demo
./modules_demo
```

Verified on MinGW-w64 ucrt **GCC 15.1.0** with `-fmodules-ts`. Clang uses
`-fmodules` (+ `-fbuiltin-module-map` for header units); MSVC uses `/std:c++20`
and `.ixx` natively.

## What it shows

- `export module` / `import` vs `#include`: the interface is parsed once into a
  binary module artifact (GCC: a `.gcm` in `gcm.cache/`), not re-textually-included
  per TU → faster incremental builds, no macro leakage, no include-order
  fragility, no header-driven ODR traps.
- Only `export`ed names are visible to importers; `dot()` stays private with no
  anonymous namespace needed.

See [`../../10-modules.md`](../../10-modules.md) for the full lesson.
