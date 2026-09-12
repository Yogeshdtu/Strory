# 06 — A proper CMakeLists.txt

**Lesson:** 12 (cmake)

## Layout
```
include/engine.hpp
src/engine.cxx  src/main.cxx
CMakeLists.txt
build/          <- generated (out-of-source)
stage/          <- `cmake --install` output
```

## Use
```bash
./build.sh                            # configure + build + toggle option + install

# ya manually:
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release        # configure (once)
cmake --build build                                   # build
cmake --build build --target clean                    # clean
./build/app                                           # run  (Windows: build/app.exe)
cmake -S . -B build -DENGINE_FAST_PATH=ON             # flip an option, reconfigure
cmake --install build --prefix ./stage               # install
```
Windows/MinGW: `cmake -S . -B build -G "MinGW Makefiles" ...`

## Concepts

### Targets, not files
CMake mein aap **targets** declare karte hain (`add_library`, `add_executable`)
aur unpe **properties** set karte ho. CMake se hi Makefiles / Ninja / VS solution
generate hota hai. Aap rules nahi likhte — target relationships likhte ho.

```
add_library(engine STATIC src/engine.cxx)     # target: engine
add_executable(app src/main.cxx)              # target: app
target_link_libraries(app PRIVATE engine)     # app engine par depend
```

### `PUBLIC` / `PRIVATE` / `INTERFACE` — usage requirements ka dil
```
target_include_directories(engine PUBLIC include)
target_compile_features(engine PUBLIC cxx_std_20)
```
| Keyword | Kis pe apply | Consumers ko propagate? |
|---|---|---|
| `PRIVATE` | sirf `engine` ki apni build | ❌ |
| `INTERFACE` | sirf consumers (`app`) | ✅ (engine ko nahi) |
| `PUBLIC` | dono | ✅ |

`engine` ki `include/` `PUBLIC` hai → `app` ko `target_include_directories`
khud nahi likhna padta, `target_link_libraries(app PRIVATE engine)` se
`include/` + `cxx_std_20` + `ENGINE_FAST_PATH` sab **propagate** ho jaate hain.
Yeh "modern CMake" ka core idea hai — **transitive usage requirements**.

### `option()` + `target_compile_definitions`
```
option(ENGINE_FAST_PATH "..." OFF)
if(ENGINE_FAST_PATH)
    target_compile_definitions(engine PUBLIC ENGINE_FAST_PATH)
endif()
```
`cmake -B build -DENGINE_FAST_PATH=ON` → `engine.cxx` mein `#ifdef
ENGINE_FAST_PATH` live → `build_flavor()` "fast-path" lauta ta hai. `PUBLIC` isliye
`app` bhi wahi define dekhta hai (agar use karta).

### Generator expressions
```
target_compile_options(engine PRIVATE
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Debug>:-O0>)
```
`$<...>` **configure time pe nahi, generate time pe** evaluate hote — multi-config
generators (VS, Ninja Multi-Config) mein "Release me -O3, Debug me -O0" ek hi
build tree mein. Plain `if(CMAKE_BUILD_TYPE STREQUAL Release)` single-config pe hi
kaam karta.

### Out-of-source build
`cmake -S . -B build` — saara generated stuff `build/` mein, source tree saaf.
`rm -rf build` = full clean. Kabhi `cmake .` source dir mein mat chalao.

### `install()`
```
install(TARGETS app engine RUNTIME DESTINATION bin ARCHIVE DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
```
`cmake --install build --prefix ./stage` → `stage/bin/app`, `stage/lib/libengine.a`,
`stage/include/engine.hpp`. Packaging / `find_package` ka base.

## Try karo

- `cmake -B build -DENGINE_FAST_PATH=ON` phir `cmake --build build` → `./build/app`
  ab "flavor: fast-path".
- `cmake -B build -DCMAKE_BUILD_TYPE=Debug` → generator expr `-O0` chunta hai.
- `cmake --build build --verbose` → actual compiler command lines.
- `cmake -B build -G Ninja` (agar ninja ho) → same project, alag backend.
- `target_link_libraries(app PRIVATE engine)` ko hatao → `undefined reference` /
  include error (usage requirements gaye).
