# 10 — Modules

## Prerequisites
- [`09-coroutines.md`](09-coroutines.md), folder 21 files 14–15 (two-phase lookup, instantiation & bloat)
- Folder 24 preview (compilation / linking)
- `examples/05_modules_demo/` (a working multi-file module)

## Yeh topic abhi kyun
`#include` **textual** hai — har `.cpp` header ko dobara parse karta, macros leak
karte, include order matter karta, aur ek header alag TUs mein alag dikh sakta
(ODR traps). **Modules** (C++20) isse replace karte: interface **ek baar** compile
hoke ek binary artifact banti, jise `import` karte. Bade codebases (trading
systems!) mein incremental build time bahut ghatta.

`examples/05_modules_demo/` mein ek chhota working module hai (GCC 15 `-fmodules-ts`
pe verified).

---

## The pieces

```cpp
// geometry.ixx  -- a module INTERFACE unit
module;                          // optional: global module fragment
#include <cmath>                 // classic includes go HERE (before `export module`)

export module geometry;          // this unit defines module `geometry`

export struct Point { double x, y; };                  // exported -> visible to importers
export double distance(Point a, Point b);              // exported declaration
double dot(Point a, Point b);                          // NOT exported -> module-private

export double norm(Point p) { return distance({0,0}, p); }
```

```cpp
// main.cxx  -- a consumer
import geometry;                  // brings in ONLY exported names (Point, distance, norm)
#include <cstdio>                 // #include still works alongside import

int main() { std::printf("%.2f\n", distance({3,4}, {0,0})); }   // -> 5.00
// dot(...) -> ERROR: not exported
```

Unit kinds:
- **Module interface unit** — has `export module name;`. Compiled to a **BMI**
  (Binary Module Interface: GCC `.gcm`, Clang `.pcm`, MSVC `.ifc`).
- **Module implementation unit** — `module name;` (no `export`). Splits big
  modules; its non-exported entities are module-private.
- **Module partitions** — `export module name:part;` — internal sub-units of one
  module.
- **Header units** — `import <vector>;` — a legacy header consumed as a module
  (needs the header pre-compiled).
- **`import std;`** (C++23) — the whole standard library as one module.

Build order: a module's BMI must be built **before** anything that imports it →
the build system needs dependency scanning. `.cpp` files compile in any order;
module interfaces don't.

---

## `#include` vs `import`

| | `#include "foo.hpp"` | `import foo;` |
|---|---|---|
| mechanism | textual paste, re-parsed **per TU** | BMI built **once**, loaded per importer |
| macros | leak in and out; order matters | **not** exported (a module doesn't leak its `#define`s); macros before `import` don't affect the module |
| include order | can change meaning | irrelevant |
| ODR across TUs | a header seen differently in two TUs → silent ODR violation | one canonical definition |
| private helpers | need anonymous namespace / `static` | non-`export`ed names are private automatically |
| incremental build | touch a widely-included header → everything recompiles | touch a module impl unit → only that unit + direct importers |
| template instantiation | re-instantiated per TU (folder 21 file 15) | instantiated in the module, shared |

---

## What modules do **not** change

- **Linking** — modules still produce object files linked normally. `export`
  affects *name visibility to importers*, not linkage in the linker sense.
- **The ABI** — a module's exported class has the same layout as a `#include`d
  one.
- **Runtime** — zero runtime effect. It's purely a compilation-model change.
- You can **mix** — a module can `#include` legacy headers (in the global module
  fragment), and legacy code can't `import` a module without being compiled as
  C++20 with module support.

---

## The state of tooling (2024–26)

- **GCC** — `-fmodules-ts` (becoming `-std=c++20`-implied); a `gcm.cache/`
  directory holds BMIs. Works for hand-built examples; CMake support via
  `CXX_MODULES` is recent.
- **Clang** — `-fmodules` / `-fmodule-file=`; explicit BMI management.
- **MSVC** — most mature; `.ixx` recognized natively, good MSBuild integration.
- **CMake ≥ 3.28** — `target_sources(... FILE_SET CXX_MODULES ...)` +
  `import std` support in 3.30+.
- **Build2** — designed around modules.

The friction is **build-system integration and dependency scanning**, not the
language feature. For a new project on a modern toolchain, `import std;` alone
can cut clean build times substantially.

---

## Andar kya hota hai

- The compiler parses the interface unit once, type-checks it, and serializes a
  structured representation (the AST-ish BMI) — declarations, exported names,
  template patterns, inline function bodies. Importers **load** this instead of
  re-parsing text.
- Because there's no textual inclusion, the preprocessor state of the importer
  doesn't reach the module and vice versa — macros are contained. This kills a
  whole category of "works in TU A, breaks in TU B because a macro was defined
  first" bugs.
- Template instantiations triggered *inside* the module are done once and stored
  in the BMI; importers reuse them → less of the per-TU instantiation cost from
  folder 21 file 15 (though instantiations triggered *by the importer* still
  happen in the importer).
- Non-exported names have **module linkage** — visible across the module's units,
  invisible outside — so `dot()` in `examples/05` stays private without an
  anonymous namespace.
- BMIs are **compiler- and flag-specific** — you rebuild them when the compiler
  version or relevant flags change; they're a build artifact, not a distributable.

> **HFT relevance:** the payoff is **developer latency**, not program latency —
> trading codebases are large, template-heavy (folder 21 file 16), and header
> coupling makes a one-line change trigger a multi-minute rebuild. Modules
> (especially `import std;` and modularizing the big internal "everything"
> headers) cut incremental and clean build times, and remove macro-leak / ODR /
> include-order classes of bug. Runtime is unaffected — the generated code is
> identical. The blocker is build-system integration; teams adopt it
> incrementally (modularize new components, keep legacy headers behind the global
> module fragment) as CMake/GCC/Clang support solidifies. It changes nothing
> about the hot path; it changes how fast you can iterate on it.

---

## Hands-on

```bash
cd 22-MODERN-CPP/examples/05_modules_demo
./build.sh        # or .\build.ps1  on Windows
```

Manually:
```bash
g++ -std=c++20 -fmodules-ts -c geometry.ixx -o geometry.o   # builds gcm.cache/geometry.gcm + geometry.o
g++ -std=c++20 -fmodules-ts main.cxx geometry.o -o demo
./demo
```

Add a module implementation unit (`geometry_impl.cxx` with `module geometry;` and
the out-of-line definition of `distance`), and a private helper only that unit
uses. Confirm the importer can't see it.

---

## ⚠️ Traps

### Trap 1 — `#include` after `export module`
```cpp
export module geometry;
#include <cmath>          // ⚠️ include AFTER `export module` -> the header's names become part of your module. Put it in the global module fragment (before `export module`)
```

### Trap 2 — build order
```cpp
// Compiling main.cxx before geometry.ixx -> "failed to read compiled module". The BMI must exist first.
```

### Trap 3 — expecting macros to cross the boundary
```cpp
#define DEBUG 1
import mymod;   // mymod does NOT see DEBUG. Modules don't import/export macros. Pass config via constexpr/template args
```

### Trap 4 — shipping BMIs
```cpp
// A .gcm/.pcm/.ifc is tied to the exact compiler + flags. Don't distribute it; rebuild from the interface source
```

### Trap 5 — assuming `import` changes linkage/ABI/runtime
```cpp
// It's a compilation-model change only. Same object files, same layout, same runtime code
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Modules are a drop-in replacement for `#include`" | Real build-order + tooling work; the wins are compile time + hygiene |
| "Modules make the program faster" | Zero runtime effect — same generated code |
| "A module exports its macros" | Modules never import/export macros — a deliberate hygiene guarantee |
| "You distribute the BMI" | It's a local build artifact, compiler/flag-specific — rebuild from source |
| "`export` is about linker linkage" | It's about *name visibility to importers*; linker linkage is unchanged |

---

## Exercises

1. **Contained macros:** you have `#define max(a,b) ...` (a classic header sin).
   Why can't it break a `import`ed module?

   <details><summary>Answer</summary>

   Modules don't participate in the preprocessor across the boundary — the
   importer's macros are not visible inside the module, and the module's macros
   don't leak out. `max` defined before `import mymod;` has no effect on
   `mymod`'s use of `std::max`. With `#include` it would rewrite the header's
   text.
   </details>

2. **Private without `static`:** in `examples/05`, `dot()` has no `export` and no
   anonymous namespace. Why is it still invisible to `main.cxx`?

   <details><summary>Answer</summary>

   Names in a module that aren't `export`ed have **module linkage** — visible
   within the module's units, invisible to importers. The module boundary does
   what an anonymous namespace / `static` did for headers, automatically.
   </details>

3. **Build graph:** module `A` exports; module `B` `import A;`; `main.cpp`
   `import B;`. What's the compile order?

   <details><summary>Answer</summary>

   `A.ixx` → BMI first. Then `B.ixx` (needs `A`'s BMI) → BMI. Then `main.cpp`
   (needs `B`'s BMI). Object files then link in any order. This dependency chain
   is what the build system must scan for.
   </details>

4. **`import std;`:** what does it buy over `#include <vector>` + `<algorithm>` +
   ... in a large project?

   <details><summary>Answer</summary>

   The entire standard library is parsed **once** into a module BMI instead of
   every TU re-parsing tens of thousands of lines of `std` headers. On a big
   codebase that's a large clean-build and incremental-build time reduction, plus
   no `std` macro/include-order surprises.
   </details>

5. **Migration:** how do you adopt modules in an existing trading codebase
   without a big-bang rewrite?

   <details><summary>Answer</summary>

   Incrementally: keep legacy headers behind each module's **global module
   fragment** (`module; #include "legacy.hpp"` before `export module`); write
   **new** components as modules; convert the biggest, most-included internal
   "umbrella" headers to modules first (they cause the most rebuild churn); adopt
   `import std;` where the toolchain supports it. Legacy `#include` code and
   modules coexist.
   </details>

---

## Interview questions

1. `#include` vs `import` — mechanism, macros, ODR, incremental build?
2. Module interface unit vs implementation unit vs partition?
3. BMI (`.gcm`/`.pcm`/`.ifc`) kya hai — distribute kar sakte? (nahi)
4. Non-exported name ka linkage — anonymous namespace ki zaroorat kyun nahi?
5. Modules runtime / ABI / linking pe kya asar? (koi nahi)
6. Bade codebase mein modules ka fayda — kya blocker hai (tooling)?

---

## Next
→ [`11-spaceship-operator.md`](11-spaceship-operator.md)
