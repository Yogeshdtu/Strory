# 05 — GCC / Clang flags cheatsheet

Deep: folders `24-COMPILATION-LINKING`, `33-COMPILER-OPTIMIZATION`, `35-PROFILING`.
This repo's `build.ps1` / `Makefile` bake most of these in.

---

## Standard & warnings (always on)

```
-std=c++20            # or c++23; -std=c++2b for bleeding edge
-Wall -Wextra         # baseline — turn these ON, always
-Wpedantic            # ISO strictness
-Wshadow              # a var shadows an outer one
-Wconversion -Wsign-conversion   # implicit narrowing / signedness changes
-Wcast-align          # cast increases required alignment
-Wnull-dereference -Wdouble-promotion
-Werror               # warnings become errors (CI)
-Wno-error=deprecated-declarations   # selective escape hatch
```

This repo's strict set (`build.ps1` `$DEBUG_FLAGS`):
`-std=c++20 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
-Wcast-align -Wunused -Wnull-dereference -Wdouble-promotion -g -O0`

---

## Optimization levels

| Flag | Meaning |
|---|---|
| `-O0` | none — fast compile, debuggable, **benchmarks meaningless** |
| `-O1` | light |
| `-O2` | **the production default** — full opt, no size/UB gambles |
| `-O3` | `-O2` + aggressive vectorization/unroll — sometimes slower, always measure |
| `-Os` | optimize for size |
| `-Og` | opt that preserves debuggability (better than `-O0` for stepping) |
| `-Ofast` | `-O3 -ffast-math …` — **breaks IEEE**; know what you're trading |

Benchmark rule: `-O2` (or `-O3`), realistic data, and stop dead-code elimination
from deleting your result (`DoNotOptimize` / `volatile` sink). (folder 43/02)

---

## Target / arch

```
-march=native         # use THIS machine's ISA (AVX2, BMI2, …) — not portable
-march=x86-64-v3      # portable baseline ~= Haswell+ (AVX2)
-mtune=native         # schedule for this CPU, keep ISA portable
-mno-vzeroupper       # niche AVX/SSE transition tuning
-fno-omit-frame-pointer   # keep %rbp -> usable stack traces in perf/prod
```

---

## Debug info & sanitizers

```
-g                       # DWARF debug info (safe with -O2: -O2 -g)
-ggdb3                   # max detail incl. macros
-fsanitize=address       # ASan — heap/stack OOB, UAF, leaks (~2x slow)
-fsanitize=undefined     # UBSan — signed overflow, bad shifts, misalign, ...
-fsanitize=thread        # TSan — data races (~5-15x; not with ASan)
-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all
-fstack-protector-all    # stack canaries (when no ASan available, e.g. MinGW)
-D_GLIBCXX_ASSERTIONS    # libstdc++ bounds/precondition checks (cheap)
-D_GLIBCXX_DEBUG         # heavy STL debug mode (iterator invalidation, SWO) — ABI-incompatible
```

MinGW/Windows: no libasan/libubsan/libtsan → use Linux/WSL for sanitizers, or
`-D_GLIBCXX_ASSERTIONS -fstack-protector-all` as a partial local net. (folder 45/06)

---

## Codegen knobs (know the trade-off — folder 43/13)

```
-flto                    # link-time optimization (cross-TU inlining, devirt) — slower link
-fno-exceptions -fno-rtti# smaller/faster if you truly don't use them
-ffast-math              # reassoc/FTZ/no-NaN — non-deterministic sums; opt-in per-file
-funroll-loops           # sometimes helps, sometimes I-cache pressure
-fno-plt                 # cheaper external calls (PIC)
-fvisibility=hidden      # smaller dynamic symbol table, better inlining across a lib
-fno-semantic-interposition
-static / -static-libgcc -static-libstdc++   # ship without runtime deps
```

---

## Inspecting output

```
g++ -S -masm=intel -O2 f.cpp -o -           # assembly (this repo: build.ps1 asm)
g++ -E f.cpp                                 # preprocessor only (build.ps1 pp)
g++ -fverbose-asm -S ...                     # asm with source annotations
g++ -Wa,-adhln -g -c f.cpp                   # interleaved source+asm
objdump -dC --source a.out                   # disassemble a built binary + demangle
nm -C --defined-only a.out                   # symbols
c++filt _ZN3fooEv                            # demangle a name
g++ -### f.cpp                               # show the exact sub-commands
g++ -Q --help=optimizers -O2 | grep enabled  # what -O2 actually turns on
```

---

## Linking

```
-c                       # compile only, emit .o
-I dir  -L dir  -l name  # include path / lib path / link libfoo
-pthread                 # threads (compile + link)
-Wl,--as-needed          # drop unused DT_NEEDED
-Wl,-rpath,'$ORIGIN'     # find .so next to the binary
-Wl,--gc-sections -ffunction-sections -fdata-sections   # strip unused sections
-Wl,-Map=out.map         # link map (what pulled in what)
```

---

## The three builds you actually use

```
# dev / debug
g++ -std=c++20 -Wall -Wextra -Wshadow -g -O0 f.cpp -o f

# CI correctness (Linux)
g++ -std=c++20 -Wall -Wextra -Werror -O1 -g \
    -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all f.cpp -o f

# production / benchmark
g++ -std=c++20 -O2 -g -march=x86-64-v3 -flto -fno-omit-frame-pointer f.cpp -o f
```

## Next
→ [`06-gdb-commands.md`](06-gdb-commands.md)
