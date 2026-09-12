# 12 — CMake: targets, properties, `find_package`, generator expressions

## Prerequisites
- `11-make.md`
- [`examples/06_cmake_project/`](examples/06_cmake_project/)

## Yeh topic abhi kyun
Make Makefiles handwrite karta — platform-specific, tedious for big projects.
**CMake** ek **meta-build system** hai: aap targets aur unke relationships
describe karte ho, CMake platform ke native build (Make / Ninja / VS / Xcode)
generate karta hai. "Modern CMake" ka core idea — **targets carry their own usage
requirements** — samajhna zaroori hai; warna CMake ek global-variable mess ban
jaata hai.

---

## Mental model

```
CMakeLists.txt  ──►  cmake -S . -B build  ──►  build/Makefile (ya build.ninja / .sln)
                                          ──►  cmake --build build  ──►  binaries
```

- **Configure step** (`cmake -S . -B build`) — `CMakeLists.txt` padho, compiler
  detect, options resolve, native build files generate. Ek baar (ya jab
  `CMakeLists.txt` badle).
- **Build step** (`cmake --build build`) — native tool (make/ninja) chalao.
- **Out-of-source** — sab kuch `build/` mein, source tree saaf. `rm -rf build` =
  full clean.

---

## Skeleton

```cmake
cmake_minimum_required(VERSION 3.16)
project(myengine VERSION 1.0 LANGUAGES CXX)

add_library(engine STATIC src/engine.cxx src/book.cxx)
target_include_directories(engine PUBLIC include)
target_compile_features(engine PUBLIC cxx_std_20)

add_executable(app src/main.cxx)
target_link_libraries(app PRIVATE engine)
```

Bas. `app` ko `include/` ya `cxx_std_20` explicitly nahi dena pada — `engine` ke
`PUBLIC` properties **propagate** hue.

---

## Targets + usage requirements — the core idea

Ek **target** (`add_library` / `add_executable`) pe aap properties set karte ho.
Har property teen "scopes" mein aa sakti:

| Scope | Kis pe apply | Consumers (jo link karein) ko propagate? |
|---|---|---|
| **`PRIVATE`** | sirf is target ki apni build | ❌ |
| **`INTERFACE`** | sirf consumers | ✅ (is target ko nahi) |
| **`PUBLIC`** | dono | ✅ |

```cmake
target_include_directories(engine
    PUBLIC  include            # engine ki build + consumers dono ko chahiye
    PRIVATE src/internal)      # sirf engine.cxx compile karte waqt

target_compile_definitions(engine PUBLIC ENGINE_V2)
target_compile_options(engine PRIVATE -Wall -Wextra)
target_link_libraries(engine PUBLIC fmt::fmt PRIVATE spdlog::spdlog)
```

`target_link_libraries(app PRIVATE engine)` — `app` ko `engine` ke saare `PUBLIC`/
`INTERFACE` include dirs, definitions, compile features, aur transitive link libs
**automatically** mil jaate. Yeh "modern CMake" hai — **no global
`include_directories()` / `add_definitions()`**, sab target-scoped.

---

## Common target commands

```cmake
target_sources(app PRIVATE src/extra.cxx)
target_include_directories(t PUBLIC/PRIVATE/INTERFACE <dir>)
target_compile_features(t PUBLIC cxx_std_20)          # "consumers need >= C++20"
target_compile_definitions(t PUBLIC FOO=1)
target_compile_options(t PRIVATE -O3 -march=native)
target_link_libraries(t PUBLIC/PRIVATE <lib|target>)
target_link_options(t PRIVATE -static)
target_precompile_headers(t PRIVATE <pch.hpp>)        # file 13
set_target_properties(t PROPERTIES CXX_STANDARD 20 POSITION_INDEPENDENT_CODE ON)
```

Library types: `STATIC` (`.a`), `SHARED` (`.so`/`.dll`), `OBJECT` (just `.o`s),
`INTERFACE` (header-only — no sources, only usage requirements), `MODULE`
(plugin, `dlopen`-only).

---

## `option()` and configuration

```cmake
option(ENGINE_FAST_PATH "Enable fast code path" OFF)
if(ENGINE_FAST_PATH)
    target_compile_definitions(engine PUBLIC ENGINE_FAST_PATH)
endif()
```

```bash
cmake -S . -B build -DENGINE_FAST_PATH=ON -DCMAKE_BUILD_TYPE=Release
```

- `-D<VAR>=<value>` — cache variables set/override.
- `CMAKE_BUILD_TYPE` — `Debug` / `Release` / `RelWithDebInfo` / `MinSizeRel`
  (single-config generators). Multi-config (Ninja Multi-Config, VS): `cmake
  --build build --config Release`.
- `cmake -LH build` — list all cache options with help.
- `ccmake` / `cmake-gui` — interactive.

---

## `find_package` — external dependencies

```cmake
find_package(Threads REQUIRED)
find_package(fmt 9 CONFIG REQUIRED)
find_package(Boost 1.80 REQUIRED COMPONENTS system)

target_link_libraries(app PRIVATE
    Threads::Threads
    fmt::fmt
    Boost::system)
```

- **Imported targets** (`fmt::fmt`, `Boost::system`) — inme already include dirs,
  compile flags, transitive deps bake hote → bas link karo.
- Two modes: **Config mode** (`<pkg>Config.cmake` jo library ship karti — preferred,
  modern) aur **Module mode** (`Find<pkg>.cmake` jo CMake ya aap ship karte —
  legacy).
- Dependency management aage: **FetchContent** (build-time download+build),
  **vcpkg** / **Conan** (package managers). HFT shops aksar vendored + pinned.

---

## Generator expressions — `$<...>`

Configure time pe **nahi**, generate time pe evaluate hote — multi-config aur
per-target logic ke liye:

```cmake
target_compile_options(engine PRIVATE
    $<$<CONFIG:Release>:-O3 -march=native>
    $<$<CONFIG:Debug>:-O0 -fsanitize=address>
    $<$<CXX_COMPILER_ID:GNU>:-fno-plt>)

target_compile_definitions(app PRIVATE
    $<$<BOOL:${ENABLE_TELEMETRY}>:WITH_TELEMETRY>)

target_include_directories(engine PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>   # jab build kar rahe
    $<INSTALL_INTERFACE:include>)                            # jab installed use ho
```

`plain if(CMAKE_BUILD_TYPE STREQUAL "Release")` sirf single-config pe kaam karta;
generator expressions multi-config (VS/Ninja MC) mein sahi rehte.

---

## Install / export / package

```cmake
include(GNUInstallDirs)
install(TARGETS app engine
        EXPORT engineTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT engineTargets NAMESPACE engine:: DESTINATION lib/cmake/engine)
```

```bash
cmake --install build --prefix /opt/engine
```

`install(EXPORT ...)` → doosre CMake projects `find_package(engine)` +
`engine::engine` link kar sakte.

---

## Andar kya hota hai

- Configure step: CMake compiler ko chhote test programs se probe karta (which C++
  features, which flags work), `CMakeCache.txt` mein sab cache, phir chosen
  generator ke liye build files emit.
- `target_link_libraries` ek **directed graph** banata (INTERFACE properties
  edges pe). Generate time pe CMake har target ke liye final flags/includes/libs
  ko graph traverse karke compute karta, phir Makefile/ninja rules likhta.
- Generator expressions ek mini-language hai jo generate step pe evaluate hoti —
  isliye woh `${CMAKE_BUILD_TYPE}` jaise configure-time vars pe fully rely nahi
  karti (multi-config ke liye).
- Ninja generator (`-G Ninja`) — faster, better parallelism, minimal rebuilds
  vs Make. Bade projects ka default choice.

---

## > **HFT relevance**
> - **One `CMakeLists.txt`, one flag set** — `-O2/-O3 -march=native -flto
>   -fno-exceptions -fno-rtti` etc. target properties mein, sabhi TUs + vendored
>   libs consistent (file 04 ODR discipline). Per-target overrides via generator
>   expressions (`$<CONFIG:...>`), not ad-hoc.
> - **`-G Ninja`** — HFT codebases bade hote; Ninja ki incremental/parallel speed
>   dev iteration ke liye matter karta.
> - **Static, vendored, pinned deps** — `find_package` config-mode against
>   in-tree vendored builds, ya FetchContent with pinned tags; no "whatever's on
>   the system". Reproducibility + ABI control.
> - **`RelWithDebInfo` for production** — `-O2 -g` so you get optimized code *and*
>   usable stack traces / `perf` symbols (folder 35, 45). Separate the debug info
>   (`objcopy --only-keep-debug`) for deploy.
> - **`INTERFACE` libraries** for header-only internal utilities (SPSC queue,
>   fixed-point types) — usage requirements without a compiled artifact.
> - **Custom targets** for `perf`/`asm`/`benchmark` runs — `add_custom_target(bench
>   COMMAND $<TARGET_FILE:app> --bench)`.

---

## Hands-on

```bash
cd 24-COMPILATION-LINKING/examples/06_cmake_project && ./build.sh

# manually:
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release          # (Windows: add -G "MinGW Makefiles")
cmake --build build
./build/app            # or build/app.exe
cmake -S . -B build -DENGINE_FAST_PATH=ON               # flip option
cmake --build build && ./build/app                       # "flavor: fast-path"
cmake --build build --verbose                            # see compiler commands
cmake -LH build                                          # list options
cmake --install build --prefix ./stage                   # stage/{bin,lib,include}
```

Try: remove `target_link_libraries(app PRIVATE engine)` → `app` loses the include
dir + link → build error. Change `PUBLIC include` to `PRIVATE include` → `app` can
no longer find `engine.hpp`.

---

## ⚠️ Traps

### Trap 1 — global `include_directories()` / `add_definitions()`
Old style — leaks to every target, order-dependent, hard to reason about. Use
`target_*` commands with explicit scope.

### Trap 2 — wrong scope keyword
`target_include_directories(engine PRIVATE include)` when consumers need those
headers → they get "file not found". Public headers → `PUBLIC`.

### Trap 3 — editing generated files in `build/`
They're regenerated. Edit `CMakeLists.txt`, re-configure.

### Trap 4 — `CMAKE_CXX_FLAGS` vs `target_compile_options`
`set(CMAKE_CXX_FLAGS "-O3")` is global and clobbers user flags. Prefer
`target_compile_options` with generator expressions.

### Trap 5 — `file(GLOB ...)` for sources
```cmake
file(GLOB SRCS src/*.cxx)     # ⚠️ new files don't trigger re-configure -> not built
```
List sources explicitly, or `file(GLOB ... CONFIGURE_DEPENDS)` (still discouraged
for large projects).

### Trap 6 — forgetting `CMAKE_BUILD_TYPE` (single-config)
No build type → no `-O` / `-g` at all (empty flags) → "why is it slow / no debug
info". Set `Release` / `RelWithDebInfo`.

### Trap 7 — `cmake .` in the source tree
Pollutes the source with `CMakeCache.txt`, `CMakeFiles/`. Always `-S . -B build`.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "CMake ek build system hai" | Meta-build — generates Make/Ninja/VS; those build |
| "properties global variables se set karo" | Per-target with `PUBLIC`/`PRIVATE`/`INTERFACE` scope |
| "`PUBLIC`/`PRIVATE` sirf visibility labels hain" | They control **propagation** of usage requirements to consumers |
| "`if(CONFIG STREQUAL Release)` = generator expr" | Plain `if` is configure-time; `$<CONFIG:Release>` works for multi-config too |
| "`file(GLOB)` sources fine hai" | New files won't be picked up without re-configure |
| "no `CMAKE_BUILD_TYPE` → defaults to Release" | Defaults to empty (no `-O`, no `-g`) on single-config |

---

## Exercises

1. **Scope:** `engine` needs `include/` to compile its own `.cxx` AND its
   consumers include `engine.hpp`. `src/detail/` is only used inside `engine.cxx`.
   Write the `target_include_directories` call.

   <details><summary>Answer</summary>

   ```cmake
   target_include_directories(engine
       PUBLIC  include
       PRIVATE src/detail)
   ```
   </details>

2. **Propagation:** `app` links `engine` (PRIVATE), `engine` links `fmt::fmt`
   (PUBLIC) and `spdlog::spdlog` (PRIVATE). Which of `fmt`, `spdlog` does `app`
   link against?

   <details><summary>Answer</summary>

   `app` links `fmt` transitively (PUBLIC on `engine` propagates to `engine`'s
   consumers). `spdlog` is PRIVATE to `engine` — `app` does not link it (and
   shouldn't need `spdlog` headers).
   </details>

3. **Generator expr:** add `-O3 -march=native` only in Release, `-fsanitize=address`
   only in Debug, for target `engine`.

   <details><summary>Answer</summary>

   ```cmake
   target_compile_options(engine PRIVATE
       $<$<CONFIG:Release>:-O3;-march=native>
       $<$<CONFIG:Debug>:-fsanitize=address>)
   target_link_options(engine PRIVATE
       $<$<CONFIG:Debug>:-fsanitize=address>)
   ```
   </details>

4. **`find_package`:** you want fmt ≥ 9, failing hard if absent, linked into `app`.
   Three lines.

   <details><summary>Answer</summary>

   ```cmake
   find_package(fmt 9 CONFIG REQUIRED)
   target_link_libraries(app PRIVATE fmt::fmt)
   # (the imported target fmt::fmt carries its include dirs + flags automatically)
   ```
   </details>

5. **Why out-of-source:** what does `rm -rf build` give you that `make clean` in a
   handwritten Makefile might not?

   <details><summary>Answer</summary>

   A guaranteed-clean state: every generated file (objects, deps, CMake cache,
   moc/generated sources, stale build rules) lives under `build/`, so removing it
   can't leave anything behind. `make clean` only removes what its `clean` recipe
   lists — easy to miss generated files or stale rules.
   </details>

---

## Interview questions

1. CMake — meta-build system ka matlab, configure vs build step.
2. Target + usage requirements — `PUBLIC`/`PRIVATE`/`INTERFACE` kya control karta?
3. `target_link_libraries(app PRIVATE engine)` se `app` ko kya-kya milta?
4. Generator expression `$<...>` — configure-time `if` se kyun/kab behtar?
5. `find_package` — imported target, config vs module mode.
6. `INTERFACE` library kis ke liye?
7. Out-of-source build ka fayda.
8. `file(GLOB)` for sources — kyun avoid?

---

## Next
→ [`13-build-performance.md`](13-build-performance.md)
