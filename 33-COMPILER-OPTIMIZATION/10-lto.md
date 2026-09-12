# 10 — Link-Time Optimization (LTO)

## Prerequisites
- `24-COMPILATION-LINKING/` (TUs, linking, `24/15` LTO intro)
- `03-inlining.md`, `06-constant-folding.md`, `07-devirtualization.md`
- `06_lto_demo/` example

## Yeh topic abhi kyun
Normal compilation har `.cpp` (translation unit) ko **alag** optimize karta —
ek TU doosre ke functions ki definition nahi dekh sakta, to cross-TU inlining,
constant propagation, devirtualization sab ruke rehte. LTO is boundary ko
hatata hai: compile stage mein har object file mein IR (intermediate
representation) bhi jaati hai, aur **link stage** pe optimizer poore program
ko ek saath dekh ke optimize karta. HFT release builds mein standard.

---

## Normal build vs LTO

```
NORMAL:
  g++ -O2 -c a.cpp -> a.o   (a.cpp ke andar optimized; b.cpp ka kuch nahi pata)
  g++ -O2 -c b.cpp -> b.o
  g++ a.o b.o -> app        (linker: sirf symbol resolution, no optimization)

LTO:
  g++ -O2 -flto -c a.cpp -> a.o   (machine code + GIMPLE bytecode)
  g++ -O2 -flto -c b.cpp -> b.o
  g++ -O2 -flto a.o b.o -> app    (link stage: re-run the optimizer on the
                                   COMBINED IR -- cross-TU inline / const-prop /
                                   devirt / DCE / ICF, then codegen)
```

---

## Kya LTO unlock karta

| Optimization | Without LTO | With LTO |
|---|---|---|
| **cross-TU inlining** | impossible (no definition) | a hot function in `b.cpp` inlines into `a.cpp`'s loop |
| **cross-TU constant propagation** | no | a constant passed from `main` reaches a callee in another TU → clone/fold |
| **speculative devirtualization** | can't enumerate derived classes in other TUs | sees all overrides → devirt on the dominant type |
| **whole-program DCE** | unused `extern` functions kept (might be used elsewhere) | truly-unused functions/data removed → smaller binary |
| **identical code folding** (`-fipa-icf`) | per-TU | across TUs → dedupe template instantiations |
| **better inlining budget** | per-TU `inline-unit-growth` | whole-program view of hotness |
| **cross-TU alias analysis** | pessimistic at TU boundary | can prove non-aliasing from real call sites |

---

## Measured — example `06` (is box, ~2 GHz)

`hot_transform` (5-op integer hash) defined in `mathx.cxx`, called once per
element in `main.cxx`'s throughput loop:
```
  NO   -flto : 1.75 ns/elem
  WITH -flto : 0.76 ns/elem      -> ~2.3x
```
Without LTO: an opaque `call` per element (call/ret overhead + the optimizer
can't schedule/fold across it). With LTO: `hot_transform` inlined into the
loop → call gone, the 5 ops scheduled/unrolled with the surrounding code.

(An earlier version of this example used a *carried* loop — `acc =
hot_transform(acc ^ i)` — and LTO showed **no** difference, because the loop
was latency-bound on the hash critical path and the ~2-cycle call overlapped
with the OoO engine. The throughput loop is where the cross-TU call actually
costs. That's a Rule-2 nuance worth knowing: LTO helps where the call is on
the throughput path, not where it's hidden behind a dependency chain.)

---

## Flavours

| Flag | What |
|---|---|
| `-flto` | full LTO — whole program merged, best results, slowest link, most RAM |
| `-flto=N` / `-flto=auto` | parallel LTO — N jobs at link (GCC `auto` = nproc) |
| `-flto=thin` (Clang) / GCC WHOPR | **ThinLTO** — per-TU summaries + a global call graph; imports only what's needed. ~10x faster link than full LTO, ~90% of the benefit. Default choice for big projects. |
| `-ffat-lto-objects` | object files carry both real code *and* IR → usable by a non-LTO link too (bigger `.o`) |
| `-fno-fat-lto-objects` | IR only (default on GCC) — `.o` unusable without `-flto` at link |

Link with the **same** `-O` and `-flto` flags you compiled with. Use the
compiler driver (`g++`/`clang++`) to link, not `ld` directly (it needs the
LTO plugin — `gcc` sets it up; raw `ld` needs `-plugin`).

---

## Costs / gotchas

- **Link time + RAM** — full LTO on a large binary can take minutes and GBs.
  ThinLTO / `-flto=auto` fixes most of it.
- **All TUs must use `-flto`** — a mix of LTO and non-LTO objects → the
  non-LTO ones are opaque (no benefit for them), and you need
  `-ffat-lto-objects` or it may fail.
- **Flag consistency** — different `-O`, `-march`, `-fno-strict-aliasing`
  across TUs under LTO → the linker picks *one* (usually the last / a
  merge), which can silently change behaviour. Build the whole project with
  one flag set.
- **ODR violations become loud (good) or miscompiles (bad)** — LTO merges
  definitions; a real ODR violation that was "harmless" per-TU can now
  miscompile or trip `-Wodr` (folder 24 lesson 02). LTO is a great ODR
  detector.
- **Debugging** — inlined-across-TU frames, `<optimized out>` more common.
  `-g` still works; `-flto -g` keeps DWARF.
- **Static libs** — build them `-flto` too, or they stay opaque. System libs
  (libc, libstdc++) are usually not LTO — that's fine, they're pre-optimized.
- **Symbol visibility matters more** — mark internal symbols
  `-fvisibility=hidden` / anonymous namespace so LTO can DCE / not export them.

---

## When LTO gives little

- A project that's already **one big TU** (unity build) — LTO has nothing to
  merge.
- Header-only hot code — already visible everywhere.
- A binary dominated by one hot loop that doesn't cross TU boundaries.
- I/O-bound / syscall-bound programs — the CPU codegen isn't the limit.

Typical real-world gain: **~2-10% on a large C++ binary**, more on codebases
with lots of small cross-TU calls (and it shrinks the binary, which helps
I-cache/startup). Not a magic 2x — that's example `06`'s micro-case.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — mixing LTO and non-LTO objects
Half the binary gets no benefit and you may hit link errors. All-or-nothing
per target; `-ffat-lto-objects` if you must mix.

### Trap 2 — inconsistent flags under LTO
`-O2` in one TU, `-O3` in another, `-ffast-math` in a third → LTO merges and
one wins. Silent. Pin flags project-wide.

### Trap 3 — full `-flto` on a huge codebase
10-minute links, OOM. Use `-flto=auto` (GCC) / `-flto=thin` (Clang).

### Trap 4 — linking LTO objects with `ld` directly
Needs the plugin. Link via `g++`/`clang++`.

### Trap 5 — expecting LTO to fix bad architecture
LTO inlines and folds; it doesn't restructure your data or fix a pointer-
chasing hot loop. Do the algorithm/layout work first (folder 32).

### Trap 6 — LTO exposing an ODR bug as a miscompile
Two different `struct Config` in two TUs (folder 24 `02`) — per-TU it
"worked"; under LTO the merged definition miscompiles. Fix the ODR
violation; `-Wodr` helps.

---

## > **HFT relevance**

> - **Release build = `-O2`/`-O3` + `-march=<target>` + `-flto` (or thin) +
>   PGO + `-g`.** Folder 24 lesson 15, folder 33 lessons 11-12.
> - **LTO's big win for HFT is cross-TU inlining + devirt** — the matcher
>   calling into the book, the feed handler calling the parser, adapters
>   calling framework code — all become inlinable, so the optimizer sees the
>   whole hot path.
> - **Build your static libs with `-flto`** — an in-house order-book lib
>   linked without LTO stays a wall.
> - **`-fvisibility=hidden` + anon namespaces** so LTO can DCE aggressively
>   and not export internal symbols.
> - **Pin the flag set and toolchain version** — LTO merges are flag- and
>   version-sensitive; re-benchmark on any change.
> - **LTO is also an ODR / one-definition auditor** — turn on `-Wodr`.

---

## Hands-on

```bash
cd 33-COMPILER-OPTIMIZATION/examples/06_lto_demo
./build.ps1        # (or ./build.sh)  -> NO LTO vs WITH LTO ns/elem

# see the call vanish:
g++ -std=c++20 -O2       -c main.cxx mathx.cxx && g++ main.o mathx.o -o nolto
g++ -std=c++20 -O2 -flto -c main.cxx mathx.cxx && g++ -O2 -flto main.o mathx.o -o lto
objdump -d --demangle nolto | grep -A15 'main>:' | grep call     # call hot_transform
objdump -d --demangle lto   | grep -A15 'main>:' | grep call     # gone (inlined)

# binary size:
size nolto lto

# thin/parallel:
g++ -O2 -flto=auto  ...        # GCC parallel
clang++ -O2 -flto=thin ...     # ThinLTO
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "LTO = 2x faster" | typically 2-10% on a big binary; example 06's 2.3x is a micro-case |
| "just add `-flto` to the link" | all TUs must compile with it; use the driver to link |
| "flag mismatch across TUs is fine" | LTO merges, one wins, silently |
| "full LTO always" | huge projects: `-flto=auto` / ThinLTO |
| "LTO fixes slow loops" | it inlines/folds; layout/algorithm is still on you |
| "LTO and ODR bugs coexist" | LTO can turn a latent ODR bug into a miscompile — fix it |

---

## Exercises

1. Tumhara project: `main.cpp` + `book.cpp` + `parser.cpp` + a prebuilt
   `libvenue.a` (no LTO). Tum `-flto` add karte ho. Kaunse cross-TU
   optimizations milenge, kaunse nahi?

   <details><summary>Answer</summary>

   **Get**: cross-TU inlining / const-prop / devirt / DCE **among
   `main.cpp`, `book.cpp`, `parser.cpp`** (all compiled `-flto`). E.g. a hot
   `parser::decode()` called from `main` can now inline; a `virtual` in
   `book` with all overrides in these 3 TUs can be speculatively
   devirtualized. **Don't get**: anything involving `libvenue.a` — it was
   built without `-flto`, so its `.o` members have no IR → they stay opaque
   `call`s, and functions *you* pass into it can't be inlined there. Fix:
   rebuild `libvenue.a` with `-flto` (and matching `-O`/`-march`), or if
   it's third-party and hot, ask for an LTO build or a header-only version.
   </details>

2. LTO on karne ke baad ek test fail hone laga jo pehle pass tha, aur
   `-Wodr` warnings aa rahe. Kya likely, kaise fix?

   <details><summary>Answer</summary>

   A latent **ODR violation** that per-TU compilation tolerated. Most common:
   the same class/struct/`inline` function defined differently in two headers/
   TUs — different member order, a field present in one and not the other,
   different `enum` values, a macro flipping a `#ifdef` in one TU only
   (folder 24 `02`: `sizeof(Config)` 12 vs 16). Per-TU each object was
   internally consistent; LTO merges to *one* definition and now some code
   uses the wrong layout → miscompile → test fails. Fix: find the divergent
   definition (`-Wodr` points at it; also diff the two headers, check macro
   guards / build flags per TU), make it identical everywhere (one header,
   one flag set), rebuild. LTO surfaced a real bug that was always there.
   </details>

3. LTO se link ab 4 minute leta hai aur CI timeout kar raha. 3 options.

   <details><summary>Answer</summary>

   (1) **`-flto=auto` (GCC) / `-flto=thin` (Clang)** — parallel / incremental
   LTO. ThinLTO especially: per-TU summaries, imports only hot cross-module
   functions → often ~2-4x faster link than full LTO for ~90% of the gain.
   (2) **LTO only the hot targets** — the latency-critical binary gets
   `-flto`; tools/tests/one-off binaries don't. (3) **LTO cache** —
   ThinLTO's `-Wl,--thinlto-cache-dir=...` (or GCC's incremental) so
   unchanged modules aren't re-optimized every CI run. (4) more cores / more
   RAM on the LTO link job; (5) split into fewer, larger link units. For a
   trading binary, ThinLTO + a cache is the usual answer — keep the
   optimization, cut the link time.
   </details>

---

## Interview questions

1. LTO — what boundary it removes, and where the optimizer runs.
2. 4 optimizations LTO unlocks (cross-TU inline, const-prop, devirt, DCE/ICF).
3. Full LTO vs ThinLTO — the trade-off.
4. Why all TUs must be built with `-flto`, and flag consistency.
5. LTO and ODR violations — why LTO exposes them.
6. Typical real-world LTO gain (and why example 06's 2.3x is a micro-case).
7. LTO on a large codebase timing out — 3 mitigations.

---

## Next
→ [`11-pgo.md`](11-pgo.md)
