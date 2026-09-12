# 13 — Build performance: PCH, unity builds, ccache, ninja, LTO cost

## Prerequisites
- `01-translation-units.md`, `03-include-guards-and-pragma.md`, `11-make.md`

## Yeh topic abhi kyun
C++ compile times legendary hain — ek bade project ka full build 10–60 minutes ho
sakta hai. Yeh directly developer productivity aur (HFT mein) "measure → tweak →
re-measure" loop ki speed hai. Kaaran samajhna (headers, templates) aur levers
pata hona (PCH, unity, ccache, ninja, header hygiene) zaroori hai.

---

## Kyun slow — the sources

| Cause | Kyun |
|---|---|
| **Header re-parsing** | `#include <vector>` har TU mein hazaron lines parse — 500 TUs → 500× |
| **Templates** | har instantiation compile + optimize; `<algorithm>`+`<vector>`+your generic code = lots |
| **Optimization** (`-O2`/`-O3`) | inlining, vectorization, LTO — CPU-heavy, superlinear in function size |
| **Debug info** (`-g`) | DWARF generation + bigger I/O |
| **Linking** | ek serial step; LTO linking especially (recompiles at link) |
| **Redundant work** | no incremental / bad deps → rebuild everything |

Rule of thumb: **compile time dominated by what the preprocessor feeds the parser.**
Headers are the lever.

---

## Lever 1 — header hygiene (biggest, free)

(File 03 recap, quantified.)

- **Forward declare** instead of `#include` where a pointer/reference/signature
  suffices.
- **pImpl** for any widely-included type that touches `<boost>`, `<asio>`, `<ssl>`,
  `<regex>`, protobuf, etc. — those includes move into one `.cpp`.
- **Don't put heavy STL in core headers.** `<regex>`, `<iostream>` (huge),
  `<random>`, `<chrono>` (moderate). Prefer `<cstdio>`, `<string_view>`,
  `<cstdint>`.
- **Split "everything" headers.** A `common.hpp` that pulls 40 headers and is
  included everywhere makes every TU pay for all 40.

Measure: `g++ -ftime-report` (per-pass timing), `-ftime-trace` (Clang; Chrome
`about:tracing` JSON — shows which headers/templates cost what).

---

## Lever 2 — precompiled headers (PCH)

```cmake
target_precompile_headers(engine PRIVATE
    <vector> <string> <memory> "common/types.hpp")
```

- Compiler parses the listed headers **once**, dumps the AST/state to a `.gch`/
  `.pch`, and reuses it for every TU.
- **Win:** big, stable, widely-included headers (STL, framework). 20–50% off a
  header-heavy build.
- **Costs:** the PCH must be rebuilt if any header in it (or a compile flag)
  changes → put only *stable* stuff in it. Every TU implicitly gets those includes
  (can hide missing `#include`s — IWYU discipline slips).

---

## Lever 3 — unity / jumbo builds

```cmake
set_target_properties(engine PROPERTIES UNITY_BUILD ON UNITY_BUILD_BATCH_SIZE 8)
```

CMake concatenates N `.cxx` into one TU (`#include "a.cxx"` `#include "b.cxx"`
...), compiles that.

- **Win:** headers parsed once per *batch* instead of per file; fewer TUs → less
  per-TU overhead; more inlining within a batch. Full builds can be 2–5× faster.
- **Costs:**
  - **Incremental builds worse** — touch one file → recompile its whole batch.
  - **ODR / name-collision landmines** — two files with a `static` helper of the
    same name, or `using namespace` at file scope, now clash (file 05).
  - Anonymous-namespace symbols from different files now share a TU.
- Practical: unity for CI/release full builds, normal for local incremental. Keep
  file-scope `static`/anon-ns names unique; no `using namespace` in `.cxx`.

---

## Lever 4 — ccache / sccache

```bash
export CXX="ccache g++"
# or CMake:  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
```

- Caches `(preprocessed source + flags) → object file`. Cache hit → copy the `.o`,
  skip compilation entirely.
- **Huge** for: switching branches, clean builds of unchanged code, CI with a
  shared cache. `ccache -s` shows hit rate.
- Needs deterministic builds (no `__DATE__`/`__TIME__` in hot headers, stable
  paths — `-fdebug-prefix-map`).
- `sccache` — like ccache but can use S3/redis for a shared team/CI cache, and
  handles MSVC.

---

## Lever 5 — Ninja instead of Make

```bash
cmake -S . -B build -G Ninja
```

- Faster null builds (no re-stat storm), better parallel scheduling, minimal
  rebuild graph, cleaner output. Bade projects ka default.
- `ninja -j$(nproc)`, `ninja -t graph`, `ninja -d stats`.

---

## Lever 6 — linker choice

```bash
g++ ... -fuse-ld=lld        # LLVM lld — much faster than GNU bfd ld
g++ ... -fuse-ld=mold       # mold — fastest; multi-threaded
```

Linking is a serial bottleneck, especially with debug info / many objects. `mold`
/ `lld` can turn a 30s link into 2s.

---

## Lever 7 — parallelism + hardware

- `make -j$(nproc)` / `ninja` (auto). Watch RAM — `-O2 -g` + LTO can use GBs per
  compile process; `-j` too high → swap → slower.
- `-flto=thin` (Clang) / `-flto=<N>` (GCC) — parallel LTO.
- Distributed: `distcc` / `icecc` (compile), sccache (cache) across a build farm.

---

## The LTO / `-O3` cost (file 15 preview)

- **`-flto`** — recompiles at link time with whole-program view. Link step goes
  from seconds to minutes on big projects. Great for release, painful for
  iterate. `-flto=<N>` parallelizes; ThinLTO (Clang) is designed for this.
- **`-O3` / `-march=native`** — more aggressive inlining/vectorization → longer
  compiles, bigger `.o`. Often only marginal runtime gain over `-O2`; profile
  before paying.

Practical: `-O0 -g` (or `-Og`) for local dev/debug, `-O2 -g` (`RelWithDebInfo`)
for perf work and CI, `-O2/-O3 -flto -march=native` for the production artifact.

---

## Measuring

```bash
g++ -ftime-report file.cpp -c            # GCC: time per compiler pass
clang++ -ftime-trace file.cpp -c         # Clang: JSON -> chrome://tracing
                                          #   which headers/templates dominate
time make -j$(nproc)                      # wall time
ninja -d stats                            # ninja internals
ccache -s                                 # cache hit rate
/usr/bin/time -v g++ ...                  # peak RSS (RAM) per compile
```

Find the 5 costliest headers (`-ftime-trace` / `include-what-you-use` /
`-H` to dump the include tree) and fix those first.

---

## > **HFT relevance**
> - **Iteration loop speed = research/optimization throughput.** The
>   measure→profile→change→re-measure loop (folder 35) is only as fast as your
>   incremental build. A 2-minute rebuild kills flow.
> - **Two build configs**: fast local (`-O0 -g -fuse-ld=mold`, ccache, Ninja,
>   normal TUs) and slow-but-fast-runtime CI/prod (`-O2/-O3 -flto -march=native
>   -fno-exceptions`, unity for the full build). Don't force `-flto` locally.
> - **Header hygiene on core types** (Order, Book, MdPacket, config) — these are
>   in nearly every TU; keeping them minimal + `constexpr` + no heavy includes is
>   worth real effort.
> - **PCH for the STL subset + stable internal headers**; keep volatile stuff out
>   of it.
> - **Deterministic builds** (no `__DATE__` in headers, `-fdebug-prefix-map`,
>   pinned toolchain) → ccache hits + reproducible artifacts + no accidental ODR
>   drift (file 04).
> - **`mold`/`lld`** — a cheap, large win; adopt it.

---

## Hands-on

```bash
# how heavy is a header?
echo '#include <regex>' > /tmp/h.cpp && echo 'int main(){}' >> /tmp/h.cpp
time g++ -std=c++20 -O2 -c /tmp/h.cpp -o /tmp/h.o
g++ -std=c++20 -E /tmp/h.cpp | wc -l          # expanded line count
g++ -std=c++20 -H -c /tmp/h.cpp -o /dev/null 2>&1 | head    # include tree

# per-pass timing
g++ -std=c++20 -O2 -ftime-report -c 24-COMPILATION-LINKING/examples/09_linkage_storage.cpp -o /dev/null

# ccache effect (if installed)
ccache -C; CXX="ccache g++" ; time $CXX -O2 -c /tmp/h.cpp -o /tmp/h.o  # miss
time $CXX -O2 -c /tmp/h.cpp -o /tmp/h.o                                  # hit (fast)
```

Compare `time g++ -O0` vs `-O2` vs `-O2 -flto` on `examples/05_makefile_project`.

---

## ⚠️ Traps

### Trap 1 — `-O3`/`-flto` for local dev
Slow rebuilds, marginal runtime benefit for debugging. Use `-O0 -g` / `-Og`
locally; reserve LTO for the release artifact.

### Trap 2 — unity build without checking for name clashes
Two files with `static void init()` or `using namespace std;` → compile errors or
ODR surprises when merged. Unique file-scope names, no `using namespace` in `.cxx`.

### Trap 3 — PCH with volatile headers
PCH includes a header you edit often → PCH rebuilds every time → net loss. Only
stable headers in the PCH.

### Trap 4 — `file(GLOB)` + expecting new files to build
(File 12) — no re-configure, silently not compiled.

### Trap 5 — `-j` too high → thrash
Each `-O2 -g`/LTO compile can take 1–4 GB. `-j$(nproc)` on a 16 GB box with heavy
TUs → swap → slower than `-j4`.

### Trap 6 — `__DATE__` / `__TIME__` in a common header
Kills ccache (every build differs) and reproducibility. Inject build metadata via
one generated `.cpp`, not a widely-included header.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "compile time = optimizer" | Mostly header parsing + templates; optimizer matters at `-O2+` |
| "unity build is a free speedup" | Faster full builds, *worse* incrementals, ODR/name-clash risk |
| "PCH always helps" | Only for stable, heavy, widely-included headers |
| "`-O3` builds are just a bit slower" | Superlinear; and often no runtime gain over `-O2` — profile |
| "linking is instant" | Serial bottleneck; `mold`/`lld` can 10× it; LTO link = minutes |
| "adding an include is harmless" | If it's a core header, every TU now pays for it |

---

## Exercises

1. **Estimate:** a header `core.hpp` includes `<regex>` (adds ~1s to each TU's
   parse) and is included by 300 of 400 TUs. You remove `<regex>` (moving it to 5
   `.cpp`s). Rough full-build time saved?

   <details><summary>Answer</summary>

   ~300 × 1s = ~300 CPU-seconds of parsing removed. With `-j16`, roughly
   300/16 ≈ ~19s of wall time off the full build (more if it was also pulling in
   `<locale>` etc.). Plus: editing `core.hpp` no longer forces a near-full
   rebuild.
   </details>

2. **Unity risk:** `a.cxx` and `b.cxx` each have `namespace { int g_count = 0; }`
   and `static void reset() { g_count = 0; }`. Unity-build them together. What
   happens?

   <details><summary>Answer</summary>

   In one TU now: two `reset()` in the same anonymous namespace → redefinition
   error (or, if signatures differ enough, a subtle wrong-`g_count` bug). Unity
   builds require unique file-scope names; give them `a_reset`/`b_reset` or move
   into named namespaces.
   </details>

3. **ccache miss:** every clean build is a full recompile even though the code is
   identical. One likely cause in the headers.

   <details><summary>Answer</summary>

   Something non-deterministic in a widely-included header — `__DATE__`/`__TIME__`,
   an absolute build path baked into a string, or `__FILE__` used in a way that
   differs. ccache keys on preprocessed output; make it deterministic
   (`-fdebug-prefix-map`, generate build metadata separately).
   </details>

4. **Which config where:** match `-O0 -g` / `-O2 -g` / `-O2 -flto -march=native`
   to: (a) local debugging, (b) perf profiling & CI, (c) production deploy binary.

   <details><summary>Answer</summary>

   (a) `-O0 -g` (or `-Og`) — fast builds, faithful debugging. (b) `-O2 -g`
   (RelWithDebInfo) — realistic performance + usable symbols for `perf`. (c) `-O2
   -flto -march=native` (+ `-fno-exceptions` etc.) — max runtime perf, slow build
   is fine for one artifact.
   </details>

5. **Linker win:** a project's link step is 25s (GNU bfd ld, `-g`). One flag to
   try, expected effect.

   <details><summary>Answer</summary>

   `-fuse-ld=mold` (or `-fuse-ld=lld`). mold is multi-threaded and designed for
   large link jobs — the 25s link often drops to a couple of seconds, with no
   change to the output.
   </details>

---

## Interview questions

1. C++ compile time kis cheez se dominated hota — aur main lever kya?
2. Precompiled headers — kaise kaam karte, kya rakho / kya nahi?
3. Unity/jumbo build — win aur do costs.
4. ccache — kya cache karta, kab huge win, kya break karta?
5. `-ftime-report` / `-ftime-trace` — kya batate?
6. `-flto` / `-O3` ki build-time cost — local dev pe kyun avoid?
7. `mold`/`lld` — kyun, kis step pe farq?
8. pImpl build time ke liye kya deta?

---

## Next
→ [`14-binary-tools.md`](14-binary-tools.md)
