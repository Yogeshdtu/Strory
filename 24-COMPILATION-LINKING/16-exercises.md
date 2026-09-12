# 16 — Exercises: compilation, linking & build systems

## Prerequisites
- Poora folder 24 (files 01–15)

## Yeh file kya hai
Practice — diagnosis, "fix the build", design, aur ek multi-file project challenge.
Answers `<details>` mein. Yeh folder ke examples (`examples/01_*` … `examples/07_*`,
`examples/08_*.cpp`, `examples/09_*.cpp`) reference hain.

---

## Part A — Diagnose the error

### A1
```
main.o: in function `main':
undefined reference to `md::decode(std::span<std::byte const, 18446744073709551615ul>)'
```
The library defines `md::decode(std::span<const std::byte>)`. Same thing?
<details><summary>Answer</summary>

Yes, same signature (the `18446…ul` is just `std::dynamic_extent`). So this is a
**link-line** problem, not a signature mismatch: the object/library that *defines*
`md::decode` isn't on the link line (or a library is before the object that uses
it). Add it / fix the order (`main.o ... -lmd`).
</details>

### A2
```
b.o:(.text+0x0): multiple definition of `parse(std::string_view)'
a.o:(.text+0x0): first defined here
```
<details><summary>Answer</summary>

A non-`inline` function `parse` is **defined in a header** that both `a.cxx` and
`b.cxx` include. Fix: mark it `inline`, or put only the declaration in the header
and the body in one `.cpp`. (File 04.)
</details>

### A3
```
undefined reference to `vtable for Strategy'
```
<details><summary>Answer</summary>

`Strategy` has virtual functions declared but its **key function** (typically the
first non-inline non-pure virtual, often `~Strategy`) isn't defined. Define at
least one non-inline virtual in `strategy.cpp` (e.g. `Strategy::~Strategy() =
default;`), or make the class abstract with `= 0`. (File 10.)
</details>

### A4
Link succeeds. At runtime: `./app: error while loading shared libraries:
libfeed.so.1: cannot open shared object file`. It linked against `libfeed.so`
fine.
<details><summary>Answer</summary>

Link-time search (`-L`) and runtime search are different. The loader looks in
RUNPATH / `LD_LIBRARY_PATH` / `ldconfig` cache, not your `-L` dir. Fix: link with
`-Wl,-rpath,'$ORIGIN'` and ship `libfeed.so.1` next to the binary, or install it
where `ldconfig` sees it, or static-link. (File 09.)
</details>

### A5
Two `.cpp` compile fine separately. Together in a **unity build**, you get
`redefinition of 'static void init()'`.
<details><summary>Answer</summary>

Both files have a file-scope `static void init()` (internal linkage — fine as
separate TUs). Unity build concatenates them into one TU → two `init()` in the
same TU → redefinition. Rename to unique names, or move each into its own named
namespace. (File 13.)
</details>

### A6
```
undefined reference to `foo(std::__cxx11::basic_string<char, ...>)'
```
<details><summary>Answer</summary>

`_GLIBCXX_USE_CXX11_ABI` mismatch: one TU/library built with the new `std::string`
ABI (`std::__cxx11::string`), the other with the old COW one → `std::string`
mangles differently → symbols don't match. Rebuild everything with the same ABI
setting. (Files 04, 07, 10.)
</details>

---

## Part B — Fix the build

### B1
```make
build/app: $(OBJS)
	g++ $(OBJS) -o $@
build/%.o: src/%.cxx
	g++ -std=c++20 -O2 -c $< -o $@
```
Editing a header never triggers a rebuild. Fix.
<details><summary>Answer</summary>

No dependency tracking for headers. Add `-MMD -MP` to the compile flags and
`-include $(OBJS:.o=.d)` to the Makefile. Now editing `engine.hpp` rebuilds every
`.o` that includes it. (File 11.)
</details>

### B2
```cmake
add_library(engine src/engine.cxx)
include_directories(include)
add_executable(app src/main.cxx)
target_link_libraries(app engine)
```
`app` sometimes can't find `engine.hpp` depending on build dir. Modernize.
<details><summary>Answer</summary>

```cmake
add_library(engine STATIC src/engine.cxx)
target_include_directories(engine PUBLIC include)
target_compile_features(engine PUBLIC cxx_std_20)
add_executable(app src/main.cxx)
target_link_libraries(app PRIVATE engine)
```
Drop global `include_directories`; make `include` a `PUBLIC` usage requirement of
`engine` so it propagates to `app` via the link. (File 12.)
</details>

### B3
A widely-included `core.hpp` has:
```cpp
#include <regex>
#include "session.hpp"     // pulls <asio>
inline std::vector<Rule> g_rules;
int rule_count = 0;
```
Two problems.
<details><summary>Answer</summary>

(1) `int rule_count = 0;` — non-`inline` variable **defined in a header** → ODR /
`multiple definition` across TUs. Make it `inline int rule_count = 0;` (or
`extern` + one `.cpp` def). (2) `<regex>` and `<asio>` in a core header make every
TU pay for them — move them behind a pImpl in `session.cpp`, forward-declare what
`core.hpp` needs. (Files 04, 03, 13.)
</details>

### B4
`g++ -lcalc main.o util.o -o app` → `undefined reference to calc::add`. `libcalc.a`
is fine.
<details><summary>Answer</summary>

Library before its users. GNU ld processes left-to-right and only pulls archive
members to satisfy *already-seen* undefined symbols. `g++ main.o util.o -lcalc -o
app`. (Files 09, 10.)
</details>

### B5
Static-init crash: `Config g_cfg = load(g_logger);` in `cfg.cxx`, `Logger
g_logger;` in `log.cxx`. Fix without a global mutex.
<details><summary>Answer</summary>

Static init order across TUs is unspecified — `g_cfg` may init before `g_logger`.
Use construct-on-first-use: `Logger& logger(){ static Logger l; return l; }` and
have `load()` call `logger()`. Now `g_logger`'s replacement is initialized on
first use, guaranteed before `g_cfg` touches it. (Or `constinit` if
const-initializable.) (File 06.)
</details>

---

## Part C — Design / explain

### C1
Explain why templates and `inline` functions can live in headers (included by
many TUs) without a "multiple definition" link error, but a plain function
can't.
<details><summary>Answer</summary>

Plain function → **strong** symbol; two of them → collision. Template
instantiations and `inline` functions → **weak / COMDAT (vague linkage)** symbols;
the linker is told "many identical copies may exist, keep one, discard the rest."
That's sound only because ODR rule 2 requires all those definitions to be
token-for-token identical. (File 04.)
</details>

### C2
Your team is choosing static vs dynamic linking for a low-latency engine. Give
the case for static in terms of (a) call overhead, (b) optimization, (c) latency
determinism, (d) ops — and name one place you'd still use a shared library.
<details><summary>Answer</summary>

(a) No PLT/GOT indirection — cross-library calls become direct `call rel32`.
(b) LTO can inline/devirtualize library code into the hot path (impossible across
a `.so`). (c) No lazy first-call resolver fault mid-session, no loader work at
start. (d) One immutable artifact — `scp` and run, trivial rollback, no
`LD_LIBRARY_PATH`/version-drift surprises. Still shared: hot-swappable strategy
plugins loaded via `dlopen` + `extern "C"` entry points. (File 09.)
</details>

### C3
Design a stable binary C++ API you ship to another team who use a different
compiler. What language/ABI constraints, and why?
<details><summary>Answer</summary>

Expose only `extern "C"` functions with C-compatible types (integers, pointers,
POD structs with explicit layout / fixed-width fields). No C++ classes,
`std::string`, templates, or exceptions across the boundary. Use opaque handles
(`typedef struct Engine Engine;` + `Engine* engine_create()`) so your layout can
change without breaking them (pImpl at the ABI level). Match the calling
convention. Reason: C++ name mangling and class/vtable/stdlib layout are not
stable across compilers or even library versions; the C ABI is. (Files 07, 03.)
</details>

### C4
Explain the difference between what LTO does and what putting code in a header as
`inline` does. When would you need LTO specifically?
<details><summary>Answer</summary>

`inline` in a header makes a function's body visible to every TU that includes it,
so each TU's optimizer can inline it — but you have to *put* it in the header
(and pay the parse cost everywhere). LTO keeps code in its `.cxx`, emits compiler
IR into the `.o`, and does the cross-module optimization at **link time** with a
whole-program view — inlining, devirtualization, DCE, ICF across files you never
touched. You need LTO when the hot code legitimately lives in `.cxx` files (large
functions, third-party static libs you can't header-ify) and you want the
optimizer to see across those boundaries. (Files 01, 15.)
</details>

### C5
A `bloaty` run shows 55% of your binary's `.text` is one template class
instantiated for ~40 types. Two concrete mitigations and their trade-offs.
<details><summary>Answer</summary>

(1) `extern template class Foo<CommonType>;` in the header + one explicit
`template class Foo<CommonType>;` in a `.cpp` for the handful of common types —
stops every TU re-emitting them; trade-off: you must enumerate the common
instantiations, and rare ones still get emitted per-TU. (2) Factor the
type-independent logic out of the template into a non-template base (or a
type-erased core) that all instantiations call — big code-size cut; trade-off:
an indirection / lost inlining on the extracted path, so keep the truly hot bits
templated. Also: `-Wl,--icf=all` (lld/gold) folds identical instantiations.
(Folder 21; files 08, 14.)
</details>

---

## Part D — Challenge

### D1 — Build a small library three ways

Take a tiny library (`libseq`: `seq::next_id()`, `seq::fnv1a(std::string_view)`,
`seq::reverse(std::string)`) and a `demo` program that uses it. Produce **all
three** builds, each with its own script, and compare:

1. **Multi-file, hand-linked** — `g++ -c` each `.cxx` → `.o`, then link. Add
   `-MMD` and show that touching a header rebuilds the right `.o`.
2. **Static library** — `ar rcs libseq.a *.o`, link `demo` against it. Add an
   unused function `seq::unused_big()` and show with `nm demo` that it's **not**
   pulled in.
3. **Shared library** — `-fPIC -shared` → `libseq.so`/`.dll`. Show `ldd`/`objdump
   -p` dependency, and that rebuilding only the library changes `demo`'s output
   without relinking `demo`.

Then:
- `size` all three `demo` binaries. Explain the differences.
- `objdump -d -C demo | grep -A3 '@plt'` on the shared build — find the PLT stubs
  the static build doesn't have.
- Build the static one **with `-flto`** and check whether `seq::fnv1a` got inlined
  into `demo`'s hot loop (`objdump -d`).

<details><summary>Hints</summary>

- Use `.cxx`/`.hpp` if you keep it inside this repo's `examples/` (so `checkall`
  skips it), or work in `/tmp` with `.cpp`.
- Model each build on `examples/03_static_library` and `examples/04_shared_library`
  — their `build.sh` scripts already do most of this.
- For the LTO check: `g++ -O2 -flto -c seq.cxx demo.cxx && g++ -O2 -flto *.o -o
  demo`, then look for a `call` to `fnv1a` in `main` (present = not inlined,
  absent/loop-body-has-the-hash = inlined).
- `nm -C demo | grep unused_big` — empty on the static build (member not pulled),
  present on the shared build (the whole `.so` is loaded).
</details>

### D2 — Reproduce and fix a silent ODR bug

Recreate `examples/02_odr_violation`'s DEMO 2 from scratch: two `.cxx` with a
`struct Settings` of different layout, a shared `describe()` inline function with
two bodies, and a `main` that calls both. Confirm:
- It **links with no error**.
- `sizeof(Settings)` printed from each TU differs.
- `g++ -flto -Wodr *.cxx -o app` prints the ODR warning and names the first
  differing field.

Then fix it (one shared `settings.hpp`, `#pragma once`, included by both) and
confirm the sizes agree and `-Wodr` is silent. Write two sentences on how this
same bug shows up in real projects (hint: build flags).

<details><summary>What you should observe</summary>

Plain link: silent success, mismatched `sizeof`. `-flto -Wodr`: `warning: type
'struct Settings' violates the C++ One Definition Rule` + a note pointing at the
first differing member. After the fix: identical `sizeof`, no warning. Real-world
trigger: the "same" struct compiled with different `-D` flags (`-DWITH_X` adds a
member) or different stdlib settings (`-D_GLIBCXX_DEBUG` changes `std::vector`
layout) in different TUs / a vendored library.
</details>

---

## Next
→ [`../25-OBJECT-MODEL/00-README.md`](../25-OBJECT-MODEL/00-README.md)
